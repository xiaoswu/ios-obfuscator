/**
 * @file PropertyNameStrategy.cpp
 * @brief 属性名混淆策略实现
 *
 * 该策略负责混淆：
 * 1. @property 声明中的属性名
 * 2. 点语法访问 self.property
 * 3. 成员变量 _property
 * 4. 属性的 getter/setter 方法（同步处理）
 * 5. KVO/KVC 字符串中的属性名
 *
 * 保留不混淆：
 * - IBOutlet/IBInspectable 属性
 * - 白名单中的属性
 * - 系统属性（delegate、dataSource等）
 */

#include "strategies/PropertyNameStrategy.h"
#include "core/ConfigManager.h"
#include "core/SymbolTable.h"
#include "core/NameGenerator.h"
#include "core/Logger.h"
#include "core/AhoCorasick.h"
#include "core/ReplacementManager.h"
#include "core/SystemProperties.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include <sstream>
#include <algorithm>
#include <unordered_set>

using namespace clang;
using namespace clang::ast_matchers;

namespace obfuscator {

void PropertyNameObfuscationStrategy::analyze(ASTContext& context) {
    LOG_INFO("Starting property name analysis");

    // 匹配所有ObjC属性声明
    finder_.addMatcher(
        objcPropertyDecl().bind("property"),
        this
    );

    finder_.matchAST(context);

    // 【性能优化】构建 Aho-Corasick 多模式匹配器
    // 在分析阶段完成后，一次性构建所有属性的模式匹配器
    buildPatternMatcher();

    LOG_INFO("Property name analysis: " + std::to_string(propertiesToObfuscate_.size()) + " properties");
}

void PropertyNameObfuscationStrategy::run(const MatchFinder::MatchResult& result) {
    if (const auto* prop = result.Nodes.getNodeAs<ObjCPropertyDecl>("property")) {
        handleProperty(prop);
    }
}

void PropertyNameObfuscationStrategy::handleProperty(const ObjCPropertyDecl* propDecl) {
    if (!propDecl) {
        return;
    }

    // 【双重过滤】从源头过滤系统类属性和系统属性名
    // 1. 检查属性是否在系统框架中声明
    auto& SM = propDecl->getASTContext().getSourceManager();
    if (SM.isInSystemHeader(propDecl->getLocation())) {
        return;  // 系统类的属性，不收集也不混淆
    }

    std::string propName = propDecl->getNameAsString();
    if (propName.empty()) {
        return;
    }

    // 2. 检查属性名是否是系统属性名
    // 这样用户代码中声明了系统属性同名（如 x、y、height）的属性时，也不会被混淆
    if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
        return;  // 系统属性名，不收集也不混淆
    }

    // 获取所属类名
    currentClass_ = getClassNameFromProperty(propDecl);
    if (currentClass_.empty()) {
        return;
    }

    // 检查是否应该跳过此属性
    // 【重要】对于白名单属性，虽然不混淆属性名，但仍需将 getter/setter 映射添加到 SymbolTable
    // 这样 MethodNameStrategy 就可以跳过这些 getter/setter 方法
    bool isWhitelistedProperty = false;
    std::string propNameStr = propDecl->getNameAsString();

    // 检查是否是白名单属性
    if (isWhitelisted(propNameStr, "property")) {
        isWhitelistedProperty = true;
    }

    // 对于非白名单属性，执行其他跳过检查
    if (!isWhitelistedProperty && shouldSkipProperty(propDecl)) {
        return;
    }

    // 如果是白名单属性，创建不混淆的映射并保存到 SymbolTable
    if (isWhitelistedProperty) {
        // 创建映射：原始名 = 混淆名（表示不混淆）
        PropertyMethodMapping mapping;
        mapping.originalPropertyName = propNameStr;
        mapping.obfuscatedPropertyName = propNameStr;  // 不混淆

        // 判断是否布尔属性
        bool isBool = isBooleanProperty(propDecl);

        // Getter 映射（不混淆）
        std::string actualGetter = propDecl->getGetterName().getAsString();
        mapping.originalGetterName = actualGetter;
        mapping.obfuscatedGetterName = actualGetter;  // 不混淆
        mapping.hasCustomGetter = (actualGetter != propNameStr);

        // Setter 映射（只读属性没有 setter）
        bool isReadonly = propDecl->isReadOnly();
        if (!isReadonly) {
            std::string actualSetter = propDecl->getSetterName().getAsString();
            mapping.originalSetterName = actualSetter;
            mapping.obfuscatedSetterName = actualSetter;  // 不混淆
            mapping.hasCustomSetter = true;  // 使用实际的 setter 名称

            // 确保 setter 包含冒号
            if (!mapping.obfuscatedSetterName.empty() && mapping.obfuscatedSetterName.back() != ':') {
                mapping.obfuscatedSetterName += ":";
            }
        } else {
            mapping.originalSetterName = "";
            mapping.obfuscatedSetterName = "";
            mapping.hasCustomSetter = false;
        }

        mapping.isBoolean = isBool;

        // 保存到 SymbolTable
        symbolTable_->addPropertyMapping(mapping);

        LOG_INFO("Added whitelist property mapping: " + propNameStr + " -> " + propNameStr + " (not obfuscated)");
        return;  // 白名单属性不混淆，直接返回
    }

    // 生成唯一key
    std::string key = getPropertyKey(currentClass_, propName);
    if (propertiesToObfuscate_.find(key) != propertiesToObfuscate_.end()) {
        return;  // 已处理过
    }

    // 生成或复用混淆后的属性名（同名属性使用相同混淆名）
    std::string obfuscatedName;
    auto it = globalPropertyObfuscationCache_.find(propName);
    if (it != globalPropertyObfuscationCache_.end()) {
        // 已有同名属性的混淆名，复用
        obfuscatedName = it->second;
        LOG_INFO("[CACHE] Using cached obfuscated name for '" + propName + "' -> '" + obfuscatedName + "' in class " + currentClass_);
    } else {
        // 首次遇到此属性名，生成新混淆名并缓存
        obfuscatedName = nameGenerator_->generate(propName, "propertyName");
        globalPropertyObfuscationCache_[propName] = obfuscatedName;
        LOG_INFO("[NEW] Generated obfuscated name for '" + propName + "' -> '" + obfuscatedName + "' in class " + currentClass_);
    }

    PropertyInfo info;
    info.originalName = propName;
    info.obfuscatedName = obfuscatedName;
    info.className = currentClass_;

    // 判断是否布尔属性
    info.isBoolean = isBooleanProperty(propDecl);

    // 计算默认的 getter/setter 名称（用于判断是否有自定义 getter/setter）
    std::string actualGetter = propDecl->getGetterName().getAsString();
    std::string actualSetter = propDecl->getSetterName().getAsString();
    std::string defaultGetter = propName;

    // 默认 setter 格式: set + 首字母大写 + :
    std::string defaultSetter = "set" + capitalizeFirstLetter(propName) + ":";

    // 【修复】检查源码中是否有显式指定的 getter/setter
    // 即使名称与默认值相同，只要在源码中显式指定了，就应该混淆
    // 使用 getter/setter 名称的位置来判断
    bool hasExplicitGetter = (propDecl->getGetterNameLoc().isValid() &&
                              propDecl->getGetterNameLoc() != propDecl->getLocation());
    bool hasExplicitSetter = (propDecl->getSetterNameLoc().isValid() &&
                              propDecl->getSetterNameLoc() != propDecl->getLocation());

    // Getter 处理
    if (actualGetter != defaultGetter || hasExplicitGetter) {
        info.hasCustomGetter = true;
        info.originalGetterName = actualGetter;
        // 为自定义 getter 生成混淆后的名称
        // 保留自定义 getter 的结构（如 is 前缀），使用属性的混淆名
        if (hasIsPrefix(actualGetter)) {
            // 布尔类型的自定义 getter（isXxx），使用属性的混淆名
            // is + 首字母大写的属性名
            info.getterName = "is" + capitalizeFirstLetter(obfuscatedName);
        } else {
            // 自定义 getter 但不以 is 开头，使用属性的混淆名生成 getter
            // 不添加 is 前缀，因为原始 getter 没有这个前缀
            info.getterName = generateGetterName(obfuscatedName, false);
        }
    } else {
        // 使用默认 getter 名称（getter 等于属性名）
        info.hasCustomGetter = false;
        // 检查是否需要添加 is 前缀
        // 只有当属性名不以 is 开头，但 getter 以 is 开头时（自定义 getter），才需要添加 is
        // 如果属性名本身就以 is 开头（如 isValid），则不需要额外添加
        bool propertyHasIsPrefix = hasIsPrefix(propName);
        bool getterHasIsPrefix = hasIsPrefix(actualGetter);
        // 只有 getter 以 is 开头但属性名不以 is 开头时，才在混淆后添加 is
        bool shouldAddIs = getterHasIsPrefix && !propertyHasIsPrefix;
        info.getterName = generateGetterName(obfuscatedName, shouldAddIs);
    }

    // Setter 处理
    if (actualSetter != defaultSetter || hasExplicitSetter) {
        info.hasCustomSetter = true;
        info.originalSetterName = actualSetter;
        // 自定义 setter 应该使用属性的混淆名，生成标准 setter 格式
        // 这样可以确保 getter/setter 与属性名保持一致
        info.setterName = generateSetterName(obfuscatedName);
    } else {
        // 使用默认 setter 名称（基于混淆后的属性名）
        info.hasCustomSetter = false;
        info.setterName = generateSetterName(obfuscatedName);
    }

    info.ivarName = generateIvarName(obfuscatedName);

    // 检查是否只读
    info.isReadonly = propDecl->isReadOnly();

    // 检查是否 IBOutlet
    info.isIBOutlet = isIBOutletProperty(propDecl);

    // 存储属性信息
    propertiesToObfuscate_[key] = info;
    nameMapping_[propName] = obfuscatedName;

    // 添加到符号表
    symbolTable_->addSymbol(propName, SymbolType::PROPERTY, false, currentClass_);

    // ========== 保存属性 getter/setter 映射到 SymbolTable ==========
    // 供方法名混淆策略使用，确保属性相关方法正确混淆
    PropertyMethodMapping mapping;
    mapping.originalPropertyName = propName;
    mapping.obfuscatedPropertyName = obfuscatedName;

    // Getter 映射
    if (info.hasCustomGetter) {
        mapping.originalGetterName = info.originalGetterName;
        mapping.hasCustomGetter = true;
    } else {
        // 默认 getter: 属性名本身
        mapping.originalGetterName = propName;
        mapping.hasCustomGetter = false;
    }
    mapping.obfuscatedGetterName = info.getterName;

    // Setter 映射（只读属性没有 setter）
    if (!info.isReadonly) {
        if (info.hasCustomSetter) {
            mapping.originalSetterName = info.originalSetterName;
            mapping.hasCustomSetter = true;
        } else {
            // 默认 setter: set + 首字母大写 + :
            mapping.originalSetterName = "set" + capitalizeFirstLetter(propName) + ":";
            mapping.hasCustomSetter = false;
        }
        // 【修复】确保 obfuscatedSetterName 包含冒号（与 originalSetterName 格式一致）
        // 这样 MethodNameStrategy 在处理消息表达式时不会重复添加冒号
        mapping.obfuscatedSetterName = info.setterName;
        if (!mapping.obfuscatedSetterName.empty() && mapping.obfuscatedSetterName.back() != ':') {
            mapping.obfuscatedSetterName += ":";
        }
    } else {
        // 只读属性，设置空 setter
        mapping.originalSetterName = "";
        mapping.obfuscatedSetterName = "";
        mapping.hasCustomSetter = false;
    }

    mapping.isBoolean = info.isBoolean;

    // 保存到 SymbolTable
    symbolTable_->addPropertyMapping(mapping);
}

bool PropertyNameObfuscationStrategy::shouldSkipProperty(const ObjCPropertyDecl* propDecl) const {
    if (!propDecl) {
        return true;
    }

    std::string propName = propDecl->getNameAsString();

    // 检查白名单
    if (isWhitelisted(propName, "property")) {
        return true;
    }

    // 检查系统属性列表（如 completionBlock, size, state 等常见系统属性名）
    // 即使在用户类中声明，也跳过这些常见系统属性名
    if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
        return true;
    }

    // 检查 IBOutlet
    if (isIBOutletProperty(propDecl)) {
        return true;
    }

    // 检查是否在系统类中声明
    std::string className = getClassNameFromProperty(propDecl);
    if (isSystemSymbol(className)) {
        // 【重要】区分系统类原生属性和 category 中的自定义属性
        // 如果属性是在 category 中声明的（而不是系统类原生属性），应该混淆
        if (const DeclContext* ctx = propDecl->getDeclContext()) {
            if (const auto* category = dyn_cast<ObjCCategoryDecl>(ctx)) {
                // 这是 category 中的属性，是用户自定义的，应该混淆
                return false;
            }
        }
        // 系统类原生属性，不混淆
        return true;
    }

    return false;
}

bool PropertyNameObfuscationStrategy::isIBOutletProperty(const ObjCPropertyDecl* propDecl) const {
    if (!propDecl) {
        return false;
    }

    // 检查是否有 IBOutlet 属性
    for (const auto* attr : propDecl->attrs()) {
        if (isa<IBOutletAttr>(attr)) {
            return true;
        }
        if (isa<IBOutletCollectionAttr>(attr)) {
            return true;
        }
    }

    return false;
}

bool PropertyNameObfuscationStrategy::isBooleanProperty(const ObjCPropertyDecl* propDecl) const {
    if (!propDecl) {
        return false;
    }

    QualType type = propDecl->getType();

    // 检查类型是否为 BOOL 或 bool
    std::string typeStr = type.getAsString();
    if (typeStr == "BOOL" || typeStr == "_Bool" || typeStr == "bool") {
        return true;
    }

    return false;
}

bool PropertyNameObfuscationStrategy::isSystemProperty(const std::string& propName) const {
    // O(1) 查找
    return SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end();
}

std::string PropertyNameObfuscationStrategy::generateGetterName(const std::string& obfuscatedName, bool isBoolean) {
    if (isBoolean) {
        // 布尔属性保留 is 前缀
        return "is" + obfuscatedName.substr(0, 1).append(
            obfuscatedName.length() > 1 ? obfuscatedName.substr(1) : "");
    }
    return obfuscatedName;
}

// ============================================================================
// 工具函数：首字母大写（用于生成 setter/getter 名称）
// ============================================================================
std::string PropertyNameObfuscationStrategy::capitalizeFirstLetter(const std::string& str) {
    if (str.empty()) {
        return str;
    }
    std::string result = str;
    if (result[0] >= 'a' && result[0] <= 'z') {
        result[0] -= 32;  // 首字母大写（ASCII）
    }
    return result;
}

// ============================================================================
// 【P1 性能优化】字符串操作工具函数
// ============================================================================
bool PropertyNameObfuscationStrategy::hasIsPrefix(const std::string& str) {
    // O(1) 检查，避免 substr 分配
    return str.size() >= 2 && str[0] == 'i' && str[1] == 's';
}

std::string PropertyNameObfuscationStrategy::removeIsPrefix(const std::string& str) {
    // 移除 "is" 前缀，如果存在
    return hasIsPrefix(str) ? str.substr(2) : str;
}

std::string PropertyNameObfuscationStrategy::removeUnderscore(const std::string& str) {
    // 移除下划线前缀，如果存在
    return str.empty() || str[0] != '_' ? str : str.substr(1);
}

std::string PropertyNameObfuscationStrategy::generateSetterName(const std::string& obfuscatedName) {
    // setter 格式: set + 首字母大写 + :
    return "set" + capitalizeFirstLetter(obfuscatedName) + ":";
}

std::string PropertyNameObfuscationStrategy::generateIvarName(const std::string& obfuscatedName) {
    // 成员变量格式: _ + 属性名
    return "_" + obfuscatedName;
}

// 【新架构】收集所有替换项
void PropertyNameObfuscationStrategy::collectReplacements(ASTContext& context, ReplacementManager& manager) {
    LOG_INFO("Collecting property name replacements");

    if (propertiesToObfuscate_.empty()) {
        LOG_INFO("No properties to obfuscate");
        return;
    }

    // 收集属性相关的替换项
    collectPropertyReplacements(context, manager);

    // 收集 KVO/KVC 字符串中的属性名替换
    collectKeyPathReplacements(context, manager);

    LOG_INFO("Property name replacement collection completed");
}

namespace {
    // 【P1 性能优化】局部字符串操作辅助函数
    auto hasIsPrefixLocal = [](const std::string& str) -> bool {
        return str.size() >= 2 && str[0] == 'i' && str[1] == 's';
    };

    // 替换项结构（使用头文件中定义的 PropertyReplacementType）
    struct PropertyReplacementItem {
        SourceLocation loc;
        std::string original;
        std::string replacement;
        obfuscator::PropertyReplacementType type;
        unsigned length;
    };

    // 获取属性名从 ObjCPropertyRefExpr
    std::string getPropertyNameFromExpr(const ObjCPropertyRefExpr* propRef) {
        if (!propRef) {
            return "";
        }

        // 首先尝试获取显式属性声明
        if (const auto* prop = propRef->getExplicitProperty()) {
            return prop->getNameAsString();
        }

        // 对于隐式属性（点语法访问），从 getter 方法名推断属性名
        if (propRef->isImplicitProperty()) {
            if (const auto* getter = propRef->getImplicitPropertyGetter()) {
                std::string getterName = getter->getNameAsString();
                // 处理 is 前缀的布尔属性
                if (hasIsPrefixLocal(getterName)) {
                    std::string baseName = getterName.substr(2);  // 移除 is 前缀
                    // 【修复】首字母小写: isCancelled -> cancelled
                    if (!baseName.empty() && baseName[0] >= 'A' && baseName[0] <= 'Z') {
                        baseName[0] = baseName[0] + 32;
                    }
                    return baseName;
                }
                return getterName;
            }
        }

        return "";
    }
}

void PropertyNameObfuscationStrategy::collectPropertyReplacements(ASTContext& context, ReplacementManager& manager) {
    const SourceManager& SM = context.getSourceManager();

    // 收集所有替换项
    std::vector<PropertyReplacementItem> replacements;

    // 统一的属性收集器
    class PropertyCollector : public MatchFinder::MatchCallback {
    public:
        PropertyCollector(
            const SourceManager& sm,
            const std::map<std::string, PropertyInfo>& properties,
            std::vector<PropertyReplacementItem>& replacements,
            const PropertyNameObfuscationStrategy* strategy)
            : SM_(sm), properties_(properties), replacements_(replacements), strategy_(strategy), context_(nullptr) {
            // 性能优化：构建按属性名的索引，实现 O(1) 查找
            buildNameIndex();
        }

        void run(const MatchFinder::MatchResult& result) override {
            // 保存 ASTContext 供后续使用
            context_ = result.Context;
            // 处理属性声明
            if (const auto* prop = result.Nodes.getNodeAs<ObjCPropertyDecl>("property")) {
                handlePropertyDecl(prop);
            }
            // 处理成员表达式（包含点语法 self.propertyName）
            else if (const auto* memberRef = result.Nodes.getNodeAs<MemberExpr>("memberRef")) {
                handleMemberRef(memberRef);
            }
            // 处理 ObjCIvarRefExpr (成员变量访问如 _propertyName)
            else if (const auto* ivarExpr = result.Nodes.getNodeAs<Expr>("ivarExpr")) {
                handleIvarRef(ivarExpr);
            }
            // 处理 ObjCIvarDecl (成员变量声明如 { NSString *_propertyName; })
            else if (const auto* ivarDecl = result.Nodes.getNodeAs<ObjCIvarDecl>("ivarDecl")) {
                handleIvarDecl(ivarDecl);
            }
            // 处理 @synthesize 和 @dynamic
            else if (const auto* propImpl = result.Nodes.getNodeAs<ObjCPropertyImplDecl>("propertyImpl")) {
                handlePropertyImpl(propImpl);
            }
            // 处理 anyDecl，过滤出 ObjCPropertyImplDecl（因为 ASTMatchers.h 没有直接提供 matcher）
            else if (const auto* anyDecl = result.Nodes.getNodeAs<Decl>("anyDecl")) {
                if (const auto* propImpl = dyn_cast<ObjCPropertyImplDecl>(anyDecl)) {
                    handlePropertyImpl(propImpl);
                }
            }
            // 【新增】处理 anyExpr，过滤出 PseudoObjectExpr 和 ObjCPropertyRefExpr
            else if (const auto* anyExpr = result.Nodes.getNodeAs<Expr>("anyExpr")) {
                // 只处理 PseudoObjectExpr 和 ObjCPropertyRefExpr
                if (dyn_cast<PseudoObjectExpr>(anyExpr) || dyn_cast<ObjCPropertyRefExpr>(anyExpr)) {
                    handlePropertyRefExpr(anyExpr);
                }
            }
            // 【移除】ObjCMessageExpr 处理与 MethodNameStrategy 冲突
        }

    private:
        void handlePropertyDecl(const ObjCPropertyDecl* prop) {
            if (!prop) return;

            std::string propName = prop->getNameAsString();
            std::string className = getClassName(prop);

            // 首先尝试按完整 key (className::propertyName) 查找
            std::string key = className + "::" + propName;
            auto it = properties_.find(key);
            if (it != properties_.end()) {
                handlePropertyWithName(prop, it->second);
                return;
            }

            // 性能优化：使用 nameIndex_ 进行 O(1) 跨类查找（替代原来的 O(n) 线性搜索）
            auto nameIt = nameIndex_.find(propName);
            if (nameIt != nameIndex_.end()) {
                handlePropertyWithName(prop, *(nameIt->second));
            }
        }

        void handlePropertyWithName(const ObjCPropertyDecl* prop, const PropertyInfo& info) {
            SourceLocation loc = prop->getLocation();
            if (loc.isValid() && !SM_.isInSystemHeader(loc)) {
                // 【关键修复】只替换主文件中的属性声明
                // 当处理 .m 文件时，.h 文件中的属性声明也会被匹配到（通过 #import）
                // 我们应该只替换主文件中的属性声明，避免重复替换
                // .h 文件中的属性声明会在处理 .h 文件时被替换
                if (!SM_.isInMainFile(loc)) {
                    return;  // 跳过非主文件中的属性声明
                }

                // 1. 替换属性名
                replacements_.push_back({
                    loc, info.originalName, info.obfuscatedName,
                    obfuscator::PropertyReplacementType::PropertyName,
                    static_cast<unsigned>(info.originalName.length())
                });

                // 2. 【修复】替换自定义 getter 名称
                if (info.hasCustomGetter && !info.originalGetterName.empty()) {
                    SourceLocation getterLoc = prop->getGetterNameLoc();
                    if (getterLoc.isValid() && !SM_.isInSystemHeader(getterLoc)) {
                        // 使用 Lexer 获取源代码中的 getter 名称
                        llvm::StringRef getterText = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(getterLoc),
                            SM_, context_->getLangOpts());
                        if (!getterText.empty() && getterText.str() == info.originalGetterName) {
                            replacements_.push_back({
                                getterLoc, info.originalGetterName, info.getterName,
                                obfuscator::PropertyReplacementType::PropertyName,
                                static_cast<unsigned>(info.originalGetterName.length())
                            });
                        }
                    }
                }

                // 3. 【修复】替换自定义 setter 名称
                if (info.hasCustomSetter && !info.originalSetterName.empty()) {
                    SourceLocation setterLoc = prop->getSetterNameLoc();
                    if (setterLoc.isValid() && !SM_.isInSystemHeader(setterLoc)) {
                        // 使用 Lexer 获取源代码中的 setter 名称
                        llvm::StringRef setterText = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(setterLoc),
                            SM_, context_->getLangOpts());
                        if (!setterText.empty()) {
                            std::string setterInSource = setterText.str();

                            // 【修复】lexer 获取的可能不包含冒号，需要标准化比较
                            // 移除两边的冒号进行比较
                            std::string setterInSourceNormalized = setterInSource;
                            if (!setterInSourceNormalized.empty() && setterInSourceNormalized.back() == ':') {
                                setterInSourceNormalized.pop_back();
                            }
                            std::string originalSetterNormalized = info.originalSetterName;
                            if (!originalSetterNormalized.empty() && originalSetterNormalized.back() == ':') {
                                originalSetterNormalized.pop_back();
                            }

                            if (setterInSourceNormalized == originalSetterNormalized) {
                                // 【修复】确定要替换的文本
                                // 源代码中的 setter 名称（可能包含冒号）
                                std::string textToReplace = setterInSource;

                                // 替换文本不带冒号（因为源代码可能已经有冒号了）
                                std::string replacementText = info.setterName;
                                if (!replacementText.empty() && replacementText.back() == ':') {
                                    replacementText.pop_back();
                                }

                                replacements_.push_back({
                                    setterLoc, textToReplace, replacementText,
                                    obfuscator::PropertyReplacementType::PropertyName,
                                    static_cast<unsigned>(textToReplace.length())
                                });
                            }
                        }
                    }
                }
            }
        }

        // 【新增】处理 @synthesize 和 @dynamic
        void handlePropertyImpl(const ObjCPropertyImplDecl* propImpl) {
            if (!propImpl) return;

            // 获取属性声明
            const ObjCPropertyDecl* prop = propImpl->getPropertyDecl();
            if (!prop) return;

            std::string propName = prop->getNameAsString();

            // 查找属性
            auto it = nameIndex_.find(propName);
            if (it == nameIndex_.end()) return;

            const PropertyInfo& info = *(it->second);

            // 检查是否是系统属性
            if (strategy_ && strategy_->isSystemSymbol(propName)) {
                return;  // 系统属性不处理
            }

            // 【关键修复】检测自动合成的属性
            // 对于自动合成的属性（没有显式 @synthesize），不应该在这里处理 ivar
            // 因为属性声明会由 handlePropertyDecl 处理
            SourceLocation propLoc = prop->getLocation();
            SourceLocation implLoc = propImpl->getLocation();

            // 如果 @synthesize 的位置与属性声明的位置相同或非常接近，则很可能是自动合成
            // 【修复】对于跨文件的 @synthesize（属性在 .h，@synthesize 在 .m），不能简单地认为是自动合成
            bool isAutoSynthesized = false;

            // 检查是否是跨文件的 @synthesize
            bool isCrossFile = false;
            if (propLoc.isValid() && implLoc.isValid() && SM_.getFileID(propLoc) != SM_.getFileID(implLoc)) {
                isCrossFile = true;
            }

            if (propLoc.isValid() && implLoc.isValid() && SM_.getFileID(propLoc) == SM_.getFileID(implLoc)) {
                unsigned propOffset = SM_.getFileOffset(propLoc);
                unsigned implOffset = SM_.getFileOffset(implLoc);
                // 如果 @synthesize 位置与属性位置相差小于 50 字符，认为是自动合成
                if (implOffset >= propOffset && implOffset < propOffset + 50) {
                    isAutoSynthesized = true;
                }
            } else if (!isCrossFile) {
                // 只有在同一文件内，且位置无效时，才认为是自动合成
                // 对于跨文件的 @synthesize（如属性在 .h，@synthesize 在 .m），不认为是自动合成
                if (!implLoc.isValid() || !propLoc.isValid()) {
                    isAutoSynthesized = true;
                }
            }

            // 【新增】替换 @synthesize 或 @dynamic 语句中的属性名
            // implLoc 指向 @synthesize/@dynamic 后面的属性名位置
            if (implLoc.isValid() && !SM_.isInSystemHeader(implLoc)) {
                // 【调试】获取源代码中的实际文本
                llvm::StringRef implText = Lexer::getSourceText(
                    CharSourceRange::getTokenRange(implLoc),
                    SM_, context_->getLangOpts());
                std::string implNameInSource = implText.str();

                // 检查这个位置是否与属性声明位置相同（避免重复替换）
                bool isSameLocation = false;
                if (propLoc.isValid() && SM_.getFileID(propLoc) == SM_.getFileID(implLoc)) {
                    unsigned propOffset = SM_.getFileOffset(propLoc);
                    unsigned implOffset = SM_.getFileOffset(implLoc);
                    if (implOffset == propOffset) {
                        isSameLocation = true;
                    }
                }

                // 只有当位置不同时才替换（避免与 handlePropertyDecl 重复）
                if (!isSameLocation) {
                    replacements_.push_back({
                        implLoc, propName, info.obfuscatedName,
                        obfuscator::PropertyReplacementType::PropertyName,
                        static_cast<unsigned>(propName.length())
                    });
                }
            }

            // 判断是 @synthesize 还是 @dynamic
            if (propImpl->getPropertyImplementation() == ObjCPropertyImplDecl::Synthesize) {
                // 【关键】跳过自动合成的属性
                if (isAutoSynthesized) {
                    return;  // 自动合成的属性，不处理 ivar，让 handlePropertyDecl 处理
                }

                // @synthesize propName; 或 @synthesize propName = _ivar;
                // 处理成员变量名的混淆

                // 获取 @synthesize 语句中 ivar 名的位置
                // 注意：getPropertyIvarDeclLoc() 返回 @synthesize 语句中等号后面 ivar 名的位置
                SourceLocation ivarLocInSynthesize = propImpl->getPropertyIvarDeclLoc();

                if (ivarLocInSynthesize.isValid() && !SM_.isInSystemHeader(ivarLocInSynthesize)) {
                    // 使用 Lexer 获取源代码中 ivar 标识符的实际文本
                    llvm::StringRef ivarText = Lexer::getSourceText(
                        CharSourceRange::getTokenRange(ivarLocInSynthesize),
                        SM_, context_->getLangOpts());

                    if (!ivarText.empty()) {
                        std::string ivarNameInSource = ivarText.str();

                        // 【关键修复】检查 ivar 位置是否与属性名位置相同
                        // 如果 @synthesize propName; (没有显式 ivar)，Clang 的 getPropertyIvarDeclLoc()
                        // 可能返回属性名的位置（指向 propName），导致重复替换
                        bool isSameLocation = false;
                        if (SM_.getFileID(ivarLocInSynthesize) == SM_.getFileID(implLoc)) {
                            unsigned ivarOffset = SM_.getFileOffset(ivarLocInSynthesize);
                            unsigned implOffset = SM_.getFileOffset(implLoc);
                            if (ivarOffset == implOffset) {
                                isSameLocation = true;
                            }
                        }

                        // 只有当位置不同时才替换（避免重复替换）
                        if (!isSameLocation) {
                            // 替换 ivar 名为混淆后的名字
                            // 无论 @synthesize 中指定的是什么 ivar 名，都替换为 _obfuscatedPropName
                            std::string obfuscatedIvar = "_" + info.obfuscatedName;
                            replacements_.push_back({
                                ivarLocInSynthesize, ivarNameInSource, obfuscatedIvar,
                                obfuscator::PropertyReplacementType::IvarRef,
                                static_cast<unsigned>(ivarNameInSource.length())
                            });
                        }
                    }
                }
                // 【修复】对于 @synthesize propName; (没有显式指定 ivar)，不添加额外的 ivar 替换
                // 因为 auto-generated ivar (_propName) 不会在源代码中出现
                // 只有 @synthesize propName = _ivar; 才需要替换 ivar 名（在上面的 if 块中已处理）
            }
            // @dynamic 的属性名已经在上面处理过了
        }

        void handleMemberRef(const MemberExpr* memberRef) {
            // 【关键修复】对于 Objective-C 点语法，需要通过 getMemberDecl() 获取属性声明
            // 而不是将 MemberExpr 直接转换为 ObjCPropertyRefExpr
            const auto* memberDecl = memberRef->getMemberDecl();
            const ObjCPropertyDecl* propDecl = dyn_cast<ObjCPropertyDecl>(memberDecl);

            std::string propName;
            if (propDecl) {
                propName = propDecl->getNameAsString();
            } else {
                // 【修复】只处理真正的 @property 声明
                // 如果没有 @property 声明，说明这是方法调用而非属性访问
                // 应该由 MethodNameStrategy 处理，这里跳过
                return;
            }

            if (propName.empty()) {
                return;  // 属性名为空，跳过
            }

            // 获取被引用的属性声明，检查是否是系统属性
            std::string className;
            if (propDecl) {
                className = getClassName(propDecl);
                // 如果属性在系统类中声明，需要进一步检查
                if (strategy_ && strategy_->isSystemSymbol(className)) {
                    // 【重要】区分系统类原生属性和 category 中的自定义属性
                    // 如果属性是在 category 中声明的（而不是系统类原生属性），应该混淆
                    if (const DeclContext* ctx = propDecl->getDeclContext()) {
                        if (const auto* category = dyn_cast<ObjCCategoryDecl>(ctx)) {
                            // 这是 category 中的属性，是用户自定义的，应该混淆
                            // 不跳过，继续执行下面的混淆逻辑
                        } else {
                            // 系统类原生属性，不替换
                            return;
                        }
                    } else {
                        // 系统类原生属性，不替换
                        return;
                    }
                }
            }

            // 检查是否是常见系统属性名（作为备用检查）
            if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
                return;  // 跳过系统属性
            }

            // 性能优化：使用 nameIndex_ 进行 O(1) 查找
            auto it = nameIndex_.find(propName);
            if (it == nameIndex_.end()) {
                // 【修复】在属性列表中找不到，尝试在 SymbolTable 中查找方法符号之前
                // 先检查是否是系统属性，如果是系统属性则跳过
                // 这防止了系统属性（如 UIView.center）被误认为是虚拟属性而被混淆
                if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
                    LOG_INFO("Skipping system property (not in nameIndex): " + propName);
                    return;  // 系统属性，跳过混淆
                }

                // 【新增】在属性列表中找不到，尝试在 SymbolTable 中查找方法符号
                // 这处理了虚拟属性（手动实现的 setter/getter）
                if (strategy_ && strategy_->getSymbolTable()) {
                    const auto* methodInfo = strategy_->getSymbolTable()->findMethodSymbol(propName);
                    if (methodInfo && methodInfo->isGetter) {
                        // 找到对应的方法 getter，创建临时的 PropertyInfo 进行处理
                        PropertyInfo tempInfo;
                        tempInfo.originalName = propName;
                        tempInfo.obfuscatedName = methodInfo->obfuscatedName;
                        tempInfo.hasCustomGetter = true;
                        tempInfo.getterName = methodInfo->obfuscatedName;
                        tempInfo.setterName = "";  // 未知 setter

                        // 执行替换逻辑（与下面的逻辑类似）
                        SourceLocation memberLoc = memberRef->getMemberNameInfo().getLoc();
                        SourceLocation spellingLoc = SM_.getSpellingLoc(memberLoc);
                        SourceLocation loc = spellingLoc.isValid() ? spellingLoc : memberLoc;

                        if (loc.isValid() && !SM_.isInSystemHeader(loc)) {
                            replacements_.push_back({
                                loc, propName, tempInfo.obfuscatedName,
                                obfuscator::PropertyReplacementType::PropertyRef,
                                static_cast<unsigned>(propName.length())
                            });
                            LOG_INFO("VIRTUAL PROPERTY REPLACEMENT: " + propName + " -> " + tempInfo.obfuscatedName);
                        }
                        return;  // 处理完成
                    }
                }
                return;  // 找不到，跳过
            }

            // 找到了属性信息
            const PropertyInfo& info = *(it->second);

            // 【修复】检查是否是系统属性名
            // 即使在 nameIndex_ 中找到了，也可能是其他类的同名属性（如 Masonry.center）
            // 系统属性不应该被混淆
            if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
                LOG_INFO("Skipping system property (found in nameIndex): " + propName);
                return;  // 系统属性，跳过混淆
            }

            // 【关键修复】正确获取成员标识符的位置
            // 对于点语法 self.property，getMemberLoc() 可能返回错误的位置
            // 我们需要使用 getMemberNameInfo().getLoc() 来获取准确的成员名位置
            SourceLocation memberLoc = memberRef->getMemberNameInfo().getLoc();

            // 使用 spelling location 获取源代码中的实际位置
            SourceLocation spellingLoc = SM_.getSpellingLoc(memberLoc);
            SourceLocation loc = spellingLoc.isValid() ? spellingLoc : memberLoc;

            // 【关键修复】使用 Lexer 获取源代码中的实际文本
            llvm::StringRef sourceText = Lexer::getSourceText(
                CharSourceRange::getTokenRange(loc),
                SM_, context_->getLangOpts());

            std::string nameInSource = !sourceText.empty() ? sourceText.str() : propName;

            // 【验证】检查获取的文本是否与属性名匹配
            // 如果不匹配，说明位置计算错误，需要跳过此替换
            if (!sourceText.empty() && sourceText.str() != propName) {
                // 位置不匹配，跳过此替换
                LOG_WARNING("Position mismatch: expected '" + propName + "', got '" + sourceText.str() + "', skipping replacement");
                // 跳过后续处理
            } else if (loc.isValid() && !SM_.isInSystemHeader(loc)) {
                // 【调试】记录所有 MemberExpr 替换
                std::string filePath = SM_.getFilename(loc).str();
                std::string fileName = filePath.substr(filePath.find_last_of("/") + 1);
                unsigned offset = SM_.getFileOffset(loc);
                LOG_INFO("MEMBEREXPR REPLACEMENT in " + fileName +
                         ": offset=" + std::to_string(offset) +
                         ", propName='" + propName + "'" +
                         ", nameInSource='" + nameInSource + "'" +
                         ", obfuscatedName='" + info.obfuscatedName + "'");

                // 检查是否使用了自定义 getter
                if (info.hasCustomGetter && !info.originalGetterName.empty() &&
                    nameInSource == info.originalGetterName) {
                    // 源代码中使用的是自定义 getter 名称，替换它
                    replacements_.push_back({
                        loc, nameInSource, info.getterName,
                        obfuscator::PropertyReplacementType::PropertyRef,
                        static_cast<unsigned>(nameInSource.length())
                    });
                } else {
                    // 使用默认的属性名替换
                    replacements_.push_back({
                        loc, propName, info.obfuscatedName,
                        obfuscator::PropertyReplacementType::PropertyRef,
                        static_cast<unsigned>(propName.length())
                    });
                }
            }
        }

        // ========================================================================
        // 【P2-2 重构】handlePropertyRefExpr 的辅助方法
        // ========================================================================

        // 解析表达式，提取 ObjCPropertyRefExpr
        struct ResolvedPropertyExpr {
            const ObjCPropertyRefExpr* propRef;
            bool isPseudoObject;
            const Expr* syntacticExpr;
        };
        ResolvedPropertyExpr resolvePropertyExpression(const Expr* expr) {
            ResolvedPropertyExpr result{nullptr, false, expr};

            // 首先尝试直接转换为 ObjCPropertyRefExpr
            result.propRef = dyn_cast<ObjCPropertyRefExpr>(expr);

            // 如果不是直接的 ObjCPropertyRefExpr，检查是否是 PseudoObjectExpr
            if (!result.propRef) {
                const auto* pseudoObj = dyn_cast<PseudoObjectExpr>(expr);
                if (pseudoObj) {
                    result.isPseudoObject = true;
                    result.syntacticExpr = pseudoObj;

                    const Expr* syntacticForm = pseudoObj->getSyntacticForm();
                    if (syntacticForm) {
                        // 优先处理 ObjCPropertyRefExpr
                        if (const auto* propertyRefExpr = dyn_cast<ObjCPropertyRefExpr>(syntacticForm)) {
                            result.propRef = propertyRefExpr;
                        }
                        // 处理 BinaryOperator（赋值语句）
                        else if (const auto* binaryOp = dyn_cast<BinaryOperator>(syntacticForm)) {
                            const Expr* lhs = binaryOp->getLHS();
                            if (lhs) {
                                if (const auto* lhsPseudo = dyn_cast<PseudoObjectExpr>(lhs)) {
                                    const Expr* lhsSyntacticForm = lhsPseudo->getSyntacticForm();
                                    if (const auto* lhsPropertyRef = dyn_cast<ObjCPropertyRefExpr>(lhsSyntacticForm)) {
                                        result.propRef = lhsPropertyRef;
                                    }
                                } else if (const auto* lhsPropertyRef = dyn_cast<ObjCPropertyRefExpr>(lhs)) {
                                    result.propRef = lhsPropertyRef;
                                }
                            }
                        }
                    }

                    // 如果从语法表达式没找到，尝试从语义表达式中查找
                    if (!result.propRef) {
                        for (const auto* semExpr : pseudoObj->semantics()) {
                            if (semExpr) {
                                result.propRef = dyn_cast<ObjCPropertyRefExpr>(semExpr);
                                if (result.propRef) break;
                            }
                        }
                    }
                }
            }

            return result;
        }

        // 检查是否是系统属性访问
        bool isSystemPropertyAccess(const ObjCPropertyRefExpr* propRef, const std::string& propName) {
            if (!propRef) return false;

            // 检查常见系统属性名
            if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
                return true;
            }

            // 检查属性声明是否在系统类中
            if (const ObjCPropertyDecl* propDecl = propRef->getExplicitProperty()) {
                std::string className = getClassName(propDecl);
                if (strategy_ && strategy_->isSystemSymbol(className)) {
                    // 区分系统类原生属性和 category 中的自定义属性
                    if (const DeclContext* ctx = propDecl->getDeclContext()) {
                        if (const auto* category = dyn_cast<ObjCCategoryDecl>(ctx)) {
                            return false;  // category 中的属性，应该混淆
                        }
                    }
                    return true;  // 系统类原生属性，不混淆
                }
            }

            return false;
        }

        // 检查是否是跨类属性访问
        bool isCrossClassProperty(const PropertyInfo& info, const ObjCPropertyRefExpr* propRef) {
            if (!context_ || !propRef) return false;

            QualType receiverType = propRef->getReceiverType(*context_);
            if (const auto* objType = receiverType->getAs<ObjCObjectType>()) {
                if (const ObjCInterfaceDecl* ifaceDecl = objType->getInterface()) {
                    std::string currentClassName = ifaceDecl->getNameAsString();
                    return !info.className.empty() && info.className != currentClassName;
                }
            }
            return false;
        }

        // 查找属性替换的位置
        SourceLocation findPropertyLocation(const ObjCPropertyRefExpr* propRef,
                                            bool isPseudoObject,
                                            const Expr* syntacticExpr,
                                            const std::string& propName) {
            SourceLocation loc;

            if (isPseudoObject && syntacticExpr) {
                // 遍历子表达式查找 MemberExpr
                for (const auto* child : syntacticExpr->children()) {
                    if (const auto* memberExpr = dyn_cast_or_null<MemberExpr>(child)) {
                        SourceLocation nameLoc = memberExpr->getMemberNameInfo().getLoc();
                        llvm::StringRef sourceText = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(nameLoc),
                            SM_, context_->getLangOpts());

                        if (!sourceText.empty() && sourceText.str() == propName) {
                            loc = nameLoc;
                            break;
                        }
                    }
                }
            }

            // 如果没找到，使用属性表达式的位置
            if (!loc.isValid() && propRef) {
                loc = propRef->getLocation();
            }

            // 使用 spelling location 获取源代码中的实际位置
            SourceLocation spellingLoc = SM_.getSpellingLoc(loc);
            return spellingLoc.isValid() ? spellingLoc : loc;
        }

        // 添加属性替换到列表
        void addPropertyReplacement(SourceLocation loc, const std::string& propName,
                                  const PropertyInfo& info) {
            if (!loc.isValid() || SM_.isInSystemHeader(loc)) {
                return;
            }

            llvm::StringRef sourceText = Lexer::getSourceText(
                CharSourceRange::getTokenRange(loc),
                SM_, context_->getLangOpts());

            std::string nameInSource = !sourceText.empty() ? sourceText.str() : propName;

            // 检查是否使用了自定义 getter
            if (info.hasCustomGetter && !info.originalGetterName.empty() &&
                nameInSource == info.originalGetterName) {
                replacements_.push_back({
                    loc, nameInSource, info.getterName,
                    obfuscator::PropertyReplacementType::PropertyRef,
                    static_cast<unsigned>(nameInSource.length())
                });
            } else {
                replacements_.push_back({
                    loc, propName, info.obfuscatedName,
                    obfuscator::PropertyReplacementType::PropertyRef,
                    static_cast<unsigned>(propName.length())
                });
            }
        }

        // ========================================================================

        void handlePropertyRefExpr(const Expr* expr) {
            if (!expr) return;

            // 1. 解析表达式，提取属性引用
            ResolvedPropertyExpr resolved = resolvePropertyExpression(expr);
            if (!resolved.propRef) return;

            // 2. 获取属性名
            std::string propName = getPropertyNameFromExpr(resolved.propRef);
            if (propName.empty()) return;

            // 3. 检查是否是系统属性访问
            if (isSystemPropertyAccess(resolved.propRef, propName)) {
                return;
            }

            // 4. 查找属性信息
            auto it = nameIndex_.find(propName);
            if (it != nameIndex_.end()) {
                const PropertyInfo& info = *(it->second);

                // 5. 检查是否跨类属性访问
                if (isCrossClassProperty(info, resolved.propRef)) {
                    return;
                }

                // 6. 查找位置并添加替换
                SourceLocation loc = findPropertyLocation(
                    resolved.propRef, resolved.isPseudoObject,
                    resolved.syntacticExpr, propName);
                addPropertyReplacement(loc, propName, info);
            }
            // 7. 如果没找到属性声明，尝试在 SymbolTable 中查找同名方法
            else {
                if (strategy_ && strategy_->getSymbolTable()) {
                    const auto* methodInfo = strategy_->getSymbolTable()->findMethodSymbol(propName);
                    if (methodInfo && methodInfo->isGetter) {
                        SourceLocation loc = resolved.propRef->getLocation();
                        if (loc.isValid() && !SM_.isInSystemHeader(loc)) {
                            SourceLocation workLoc = loc;
                            // 对于 PseudoObjectExpr，尝试找到 MemberExpr 位置
                            if (resolved.isPseudoObject && resolved.syntacticExpr) {
                                for (const auto* child : resolved.syntacticExpr->children()) {
                                    if (const auto* memberExpr = dyn_cast_or_null<MemberExpr>(child)) {
                                        SourceLocation nameLoc = memberExpr->getMemberNameInfo().getLoc();
                                        if (nameLoc.isValid()) {
                                            workLoc = nameLoc;
                                            break;
                                        }
                                    }
                                }
                            }
                            SourceLocation spellingLoc = SM_.getSpellingLoc(workLoc);
                            workLoc = spellingLoc.isValid() ? spellingLoc : workLoc;

                            replacements_.push_back({
                                workLoc, propName, methodInfo->obfuscatedName,
                                obfuscator::PropertyReplacementType::PropertyRef,
                                static_cast<unsigned>(propName.length())
                            });
                        }
                    }
                }
            }
        }

        void handleMessageExpr(const ObjCMessageExpr* messageExpr) {
            if (!messageExpr) return;

            // 只处理无参数的消息表达式（可能是属性 getter 调用）
            if (messageExpr->getNumArgs() > 0) {
                return;  // 有参数，不是简单的 getter 调用
            }

            // 【关键修复】跳过类方法调用
            // 类方法调用如 [HWSDKTestClass10 sharedInstance] 不应该由属性混淆策略处理
            if (messageExpr->isClassMessage()) {
                return;  // 类方法，跳过
            }

            // 【关键修复】跳过实例方法调用，除非它匹配已知的属性 getter
            // 普通实例方法调用不应该由属性混淆策略处理
            // 只处理那些在属性列表中定义的自定义 getter 或默认 getter

            // 获取选择器名称
            std::string selectorName = messageExpr->getSelector().getAsString();
            if (selectorName.empty()) return;

            // 查找是否有属性使用了这个 getter
            // 遍历所有属性，检查是否有属性的自定义 getter 与这个选择器匹配
            for (const auto& entry : nameIndex_) {
                const std::string& propName = entry.first;
                const PropertyInfo& info = *(entry.second);

                // 检查是否是属性的自定义 getter
                if (info.hasCustomGetter && !info.originalGetterName.empty() &&
                    selectorName == info.originalGetterName) {
                    // 找到了！这是一个对属性 getter 的调用
                    SourceLocation selectorLoc = messageExpr->getSelectorStartLoc();
                    SourceLocation spellingLoc = SM_.getSpellingLoc(selectorLoc);
                    SourceLocation workLoc = spellingLoc.isValid() ? spellingLoc : selectorLoc;

                    if (workLoc.isValid() && !SM_.isInSystemHeader(workLoc)) {
                        // 获取源代码中的实际文本
                        llvm::StringRef sourceText = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(workLoc),
                            SM_, context_->getLangOpts());

                        std::string nameInSource = !sourceText.empty() ? sourceText.str() : selectorName;

                        // 替换 getter 名称
                        replacements_.push_back({
                            workLoc, nameInSource, info.getterName,
                            obfuscator::PropertyReplacementType::PropertyRef,
                            static_cast<unsigned>(nameInSource.length())
                        });
                    }
                    return;  // 找到匹配后立即返回
                }

                // 同时检查默认 getter（属性名）
                if (selectorName == propName) {
                    SourceLocation selectorLoc = messageExpr->getSelectorStartLoc();
                    SourceLocation spellingLoc = SM_.getSpellingLoc(selectorLoc);
                    SourceLocation workLoc = spellingLoc.isValid() ? spellingLoc : selectorLoc;

                    if (workLoc.isValid() && !SM_.isInSystemHeader(workLoc)) {
                        // 替换 getter 名称
                        replacements_.push_back({
                            workLoc, propName, info.obfuscatedName,
                            obfuscator::PropertyReplacementType::PropertyRef,
                            static_cast<unsigned>(propName.length())
                        });
                    }
                    return;
                }
            }
        }

        void handleIvarRef(const Expr* expr) {
            // 处理 ObjCIvarRefExpr (成员变量访问如 _propertyName)
            const auto* ivarRef = dyn_cast<ObjCIvarRefExpr>(expr);
            if (!ivarRef) return;

            // 获取成员变量声明
            const ObjCIvarDecl* ivarDecl = ivarRef->getDecl();
            if (!ivarDecl) return;

            std::string ivarName = ivarDecl->getNameAsString();
            std::string propName = PropertyNameObfuscationStrategy::removeUnderscore(ivarName);
            if (propName == ivarName) {
                return;  // 没有下划线前缀，跳过
            }

            // 性能优化：使用 nameIndex_ 进行 O(1) 查找，替代原来的 O(n) 线性搜索
            auto it = nameIndex_.find(propName);
            if (it != nameIndex_.end()) {
                const PropertyInfo& info = *(it->second);
                SourceLocation loc = ivarRef->getLocation();
                if (loc.isValid() && !SM_.isInSystemHeader(loc)) {
                    replacements_.push_back({
                        loc, ivarName, info.ivarName,
                        obfuscator::PropertyReplacementType::IvarRef,
                        static_cast<unsigned>(ivarName.length())
                    });
                }
            }
        }

        void handleIvarDecl(const ObjCIvarDecl* ivarDecl) {
            // 处理成员变量声明 { Type *_propertyName; }
            if (!ivarDecl) return;

            std::string ivarName = ivarDecl->getNameAsString();
            std::string propName = PropertyNameObfuscationStrategy::removeUnderscore(ivarName);
            if (propName == ivarName) {
                return;  // 没有下划线前缀，跳过
            }

            // 【关键修复】跳过自动合成的 ivar（@property 自动生成的）
            // 自动合成的 ivar 不应该在这里处理，因为属性声明会由 handlePropertyDecl 处理

            // 方法1：检查 getSynthesize() - 适用于显式 @synthesize
            if (ivarDecl->getSynthesize()) {
                return;  // 跳过 @property 显式合成的 ivar
            }

            // 方法2：检查是否有对应的属性
            auto it = nameIndex_.find(propName);
            if (it != nameIndex_.end()) {
                // 【关键修复】检查 ivar 的 location 是否与 property 的 location 相同
                // 对于自动合成的 ivar，Clang 会将 ivar 的位置设置为与属性名相同的位置
                // 在这种情况下，我们不应该在这里处理 ivar，因为属性声明会由 handlePropertyDecl 处理
                const DeclContext* ctx = ivarDecl->getDeclContext();
                const ObjCPropertyDecl* property = nullptr;

                // 查找对应的属性声明
                if (const auto* interface = dyn_cast<ObjCInterfaceDecl>(ctx)) {
                    for (const auto* prop : interface->properties()) {
                        if (prop->getNameAsString() == propName) {
                            property = prop;
                            break;
                        }
                    }
                } else if (const auto* category = dyn_cast<ObjCCategoryDecl>(ctx)) {
                    for (const auto* prop : category->properties()) {
                        if (prop->getNameAsString() == propName) {
                            property = prop;
                            break;
                        }
                    }
                }

                if (property) {
                    SourceLocation propLoc = property->getLocation();
                    SourceLocation ivarLoc = ivarDecl->getLocation();

                    if (SM_.getFileID(propLoc) == SM_.getFileID(ivarLoc)) {
                        unsigned propOffset = SM_.getFileOffset(propLoc);
                        unsigned ivarOffset = SM_.getFileOffset(ivarLoc);

                        // 【关键】如果 ivar 的位置与属性的位置相同（或非常接近），则跳过
                        // 这是因为自动合成的 ivar 没有独立的声明，它的位置就是属性名的位置
                        if (ivarOffset == propOffset || (ivarOffset >= propOffset && ivarOffset < propOffset + 20)) {
                            return;  // 跳过自动合成的 ivar
                        }
                    }
                }
            }

            // 性能优化：使用 nameIndex_ 进行 O(1) 查找
            it = nameIndex_.find(propName);
            if (it != nameIndex_.end()) {
                const PropertyInfo& info = *(it->second);
                SourceLocation loc = ivarDecl->getLocation();

                // 检查是否在系统头文件中
                if (loc.isValid() && !SM_.isInSystemHeader(loc)) {
                    // 替换成员变量声明: _propertyName -> _obfuscatedName
                    std::string obfuscatedIvar = "_" + info.obfuscatedName;

                    replacements_.push_back({
                        loc, ivarName, obfuscatedIvar,
                        obfuscator::PropertyReplacementType::IvarRef,
                        static_cast<unsigned>(ivarName.length())
                    });
                }
            }
        }

        std::string getClassName(const ObjCPropertyDecl* prop) const {
            // 使用外部类的统一方法
            if (strategy_) {
                return strategy_->getClassNameFromProperty(prop);
            }
            return "";
        }

        // 性能优化：按属性名索引，实现 O(1) 查找（替代原来的 O(n) 线性搜索）
        std::unordered_map<std::string, const PropertyInfo*> nameIndex_;

        const SourceManager& SM_;
        const std::map<std::string, PropertyInfo>& properties_;
        std::vector<PropertyReplacementItem>& replacements_;
        const PropertyNameObfuscationStrategy* strategy_;
        ASTContext* context_;  // ASTContext，用于获取 LangOpts 等

        // 构建按属性名的索引
        void buildNameIndex() {
            for (const auto& [key, info] : properties_) {
                // 对于同名属性，保留第一个（通常是最早声明的）
                if (nameIndex_.find(info.originalName) == nameIndex_.end()) {
                    nameIndex_[info.originalName] = &info;
                }
            }
        }
    };

    // 创建匹配器
    MatchFinder finder;
    PropertyCollector collector(SM, propertiesToObfuscate_, replacements, this);

    // 添加匹配模式
    using namespace clang::ast_matchers;
    finder.addMatcher(objcPropertyDecl().bind("property"), &collector);

    // 【新增】匹配所有表达式，然后在 run 中过滤出 PseudoObjectExpr 和 ObjCPropertyRefExpr
    // ASTMatchers.h 没有直接提供 pseudoObjectExpr()，所以使用 expr() 并过滤
    finder.addMatcher(expr().bind("anyExpr"), &collector);

    finder.addMatcher(memberExpr().bind("memberRef"), &collector);
    finder.addMatcher(objcIvarRefExpr().bind("ivarExpr"), &collector);
    finder.addMatcher(objcIvarDecl().bind("ivarDecl"), &collector);
    // 【设计】ObjCMessageExpr（括号语法）由 MethodNameStrategy 处理
    // 属性混淆只处理点语法和属性声明，方法调用由方法混淆策略负责
    // finder.addMatcher(objcMessageExpr().bind("messageExpr"), &collector);
    // ASTMatchers.h 没有直接提供 objcPropertyImplDecl()，使用 decl() 通配符
    finder.addMatcher(decl().bind("anyDecl"), &collector);

    // 【重要】使用 AST matcher 替代 fallback 字符串替换
    // 这样可以避免与方法名混淆冲突

    // 执行匹配
    finder.matchAST(context);

    // 【新架构】将所有替换项添加到 ReplacementManager
    // ReplacementManager 将负责统一排序和应用
    for (const auto& item : replacements) {
        if (item.loc.isValid() && !SM.isInSystemHeader(item.loc)) {
            // 添加替换到管理器，优先级为 0（默认）
            manager.addReplacement(item.loc, item.original, item.replacement, 0, "PropertyNameObfuscation");
        }
    }

    LOG_INFO("Property name replacement collection: " + std::to_string(replacements.size()) + " replacements");
}

void PropertyNameObfuscationStrategy::collectKeyPathReplacements(ASTContext& context, ReplacementManager& manager) {
    const SourceManager& SM = context.getSourceManager();
    FileID mainFileID = SM.getMainFileID();
    bool invalid = false;
    llvm::StringRef fileContent = SM.getBufferData(mainFileID, &invalid);
    if (invalid) {
        return;
    }

    // 存储替换信息：位置、原始长度、替换字符串
    struct Replacement {
        size_t pos;
        size_t length;
        std::string replacement;
    };
    std::vector<Replacement> replacements;

    // 【性能优化】使用 unordered_set 存储 KVC/KVO 方法名，O(1) 查找
    static const std::unordered_set<std::string> kvcKvoMethods = {
        // KVO 相关
        "addObserver:forKeyPath:options:context:",
        "removeObserver:forKeyPath:",
        "removeObserver:forKeyPath:context:",
        "observeValueForKeyPath:ofObject:change:context:",
        // KVC 相关
        "valueForKey:",
        "setValue:forKey:",
        "valueForKeyPath:",
        "setValue:forKeyPath:",
        "mutableArrayValueForKey:",
        "mutableOrderedSetValueForKey:",
        "mutableSetValueForKey:",
        // 简短匹配版本（用于实际方法调用）
        "forKeyPath:",
        "forKey:"
    };

    // 【性能优化】使用 Aho-Corasick 匹配字符串字面量
    if (!patternMatcher_) {
        return;
    }

    // 获取包含匹配长度的结果
    auto matchResults = patternMatcher_->matchWithLength(fileContent.str());

    // 【性能优化】预处理：构建括号匹配缓存
    std::vector<bool> isBracketMatch(fileContent.size(), false);
    for (size_t i = 0; i < fileContent.size(); ++i) {
        if (fileContent[i] == ']') {
            // 向前查找匹配的 [
            int depth = 1;
            size_t j = i - 1;
            bool found = false;
            while (j < fileContent.size() && depth > 0) {
                if (fileContent[j] == ']') {
                    depth++;
                } else if (fileContent[j] == '[') {
                    depth--;
                    if (depth == 0) {
                        found = true;
                        break;
                    }
                }
                if (j == 0) break;
                j--;
            }
            isBracketMatch[i] = found;
        }
    }

    // 检查字符串是否在 KVC/KVO 方法调用的上下文中
    auto isInKvcKvoContext = [&](size_t strStart, bool isOCString) -> bool {
        // strStart 是模式匹配的起始位置
        // 对于 OC_STRING, strStart 指向 @
        // 对于 C_STRING, strStart 指向 "
        // 搜索区域应该从 strStart 向前回溯 maxSearchDistance 个字符
        size_t maxSearchDistance = 150;  // 限制回溯距离

        // 【优化】使用 StringRef::contains 快速查找
        size_t searchStartPos = (strStart >= maxSearchDistance) ? strStart - maxSearchDistance : 0;
        llvm::StringRef searchArea = fileContent.slice(searchStartPos, strStart);

        // 先快速排除：如果是字典下标访问，直接返回 false
        for (size_t i = 1; i < maxSearchDistance && strStart >= i; ++i) {
            size_t checkPos = strStart - i;
            if (fileContent[checkPos] == ']') {
                if (checkPos < isBracketMatch.size() && isBracketMatch[checkPos]) {
                    return false;  // 字典下标访问
                }
            }
            if (fileContent[checkPos] == '\n') {
                break;
            }
        }

        // 【优化】遍历检查是否包含 KVC/KVO 方法名
        for (const auto& method : kvcKvoMethods) {
            if (searchArea.contains(method)) {
                return true;
            }
        }

        return false;
    };

    // 检查是否是字典字面量: @"key": 或 "key":
    auto isDictionaryLiteral = [&](size_t strStart, bool isOCString) -> bool {
        size_t afterStrStart = strStart + (isOCString ? 2 : 1);  // @" 或 " 之后

        // 找到字符串结束位置
        size_t strEndPos = afterStrStart;
        while (strEndPos < fileContent.size() && fileContent[strEndPos] != '"') {
            strEndPos++;
        }

        if (strEndPos + 1 < fileContent.size()) {
            char afterQuote = fileContent[strEndPos + 1];
            if (afterQuote == ':') {
                return true;  // 字典字面量 key
            }
        }
        return false;
    };

    // 处理每个匹配结果
    // 收集所有 OC_STRING 匹配的位置，用于过滤 C_STRING
    std::unordered_set<size_t> ocStringPositions;
    for (const auto& [pos, length, patternInfo] : matchResults) {
        if (patternInfo && patternInfo->type == MultiPatternMatcher::PatternInfo::OC_STRING) {
            ocStringPositions.insert(pos);
        }
    }

    for (const auto& [pos, length, patternInfo] : matchResults) {
        if (!patternInfo) continue;

        const std::string& propName = patternInfo->originalName;
        const std::string& obfName = patternInfo->obfuscatedName;

        // 只处理字符串字面量
        if (patternInfo->type == MultiPatternMatcher::PatternInfo::C_STRING) {
            // 如果存在相邻的 OC_STRING 匹配（pos-1），跳过此 C_STRING
            // 因为 OC_STRING 会替换整个 @"string"，包括 C_STRING 的 "string" 部分
            if (ocStringPositions.find(pos - 1) != ocStringPositions.end()) {
                continue;  // OC_STRING 会处理这个
            }
            if (isDictionaryLiteral(pos, false)) {
                continue;  // 跳过字典字面量
            }
            if (isInKvcKvoContext(pos, false)) {
                // 使用 Aho-Corasick 返回的实际匹配长度
                replacements.push_back({pos, length, "\"" + obfName + "\""});
            }
        } else if (patternInfo->type == MultiPatternMatcher::PatternInfo::OC_STRING) {
            if (isDictionaryLiteral(pos, true)) {
                continue;  // 跳过字典字面量
            }
            if (isInKvcKvoContext(pos, true)) {
                // 使用 Aho-Corasick 返回的实际匹配长度
                replacements.push_back({pos, length, "@\"" + obfName + "\""});
            }
        }
    }

    // 【新架构】将所有替换项添加到 ReplacementManager
    // ReplacementManager 将负责统一排序和应用
    std::unordered_set<size_t> usedPositions;
    for (const auto& repl : replacements) {
        if (usedPositions.find(repl.pos) == usedPositions.end()) {
            // 【关键修复】计算字符串字面量的完整起始位置和长度
            // repl.pos 是属性名的起始位置，需要找到字符串字面量的起始位置
            size_t stringStartPos = repl.pos;
            size_t originalLength = repl.length;

            // 向前查找字符串字面量的起始位置（@" 或 "）
            std::string fileStr = fileContent.str();
            if (stringStartPos >= 2 && fileStr[stringStartPos - 1] == '"' &&
                fileStr[stringStartPos - 2] == '@') {
                // OC_STRING: @"property"
                stringStartPos -= 2;  // 指向 @
                originalLength += 3;   // 包含 @" 和末尾的 "
            } else if (stringStartPos >= 1 && fileStr[stringStartPos - 1] == '"') {
                // C_STRING: "property"
                stringStartPos -= 1;   // 指向 "
                originalLength += 2;   // 包含两端的 "
            }

            // 获取原始文本
            std::string originalText = fileStr.substr(stringStartPos, originalLength);

            SourceLocation loc = SM.getLocForStartOfFile(mainFileID);
            loc = loc.getLocWithOffset(stringStartPos);

            if (loc.isValid() && SM.isInMainFile(loc)) {
                // 添加替换到管理器，使用正确的原始文本和长度
                manager.addReplacement(loc, originalText, repl.replacement, 0, "PropertyNameObfuscation");
                usedPositions.insert(repl.pos);
            }
        }
    }

    if (!replacements.empty()) {
        LOG_INFO("KVO/KVC keyPath replacement collection: " + std::to_string(usedPositions.size()) + " replacements");
    }
}

bool PropertyNameObfuscationStrategy::validate(ASTContext& context) const {
    LOG_INFO("Validating property name obfuscation");

    if (propertiesToObfuscate_.empty()) {
        LOG_WARNING("No properties to obfuscate");
        return true;
    }

    // 验证每个属性都有混淆名称
    for (const auto& [key, info] : propertiesToObfuscate_) {
        if (info.obfuscatedName.empty()) {
            LOG_ERROR("Property " + info.originalName + " has no obfuscated name");
            return false;
        }
    }

    LOG_INFO("Property name obfuscation validation passed");
    return true;
}

std::string PropertyNameObfuscationStrategy::getPropertyKey(const std::string& className, const std::string& propName) const {
    return className + "::" + propName;
}

std::string PropertyNameObfuscationStrategy::getClassNameFromProperty(const ObjCPropertyDecl* propDecl) const {
    if (!propDecl) {
        return "";
    }

    // 使用 getDeclContext() 获取所属类
    if (const DeclContext* ctx = propDecl->getDeclContext()) {
        if (const auto* interface = dyn_cast<ObjCInterfaceDecl>(ctx)) {
            return interface->getNameAsString();
        }
        if (const auto* protocol = dyn_cast<ObjCProtocolDecl>(ctx)) {
            return protocol->getNameAsString();
        }
        if (const auto* category = dyn_cast<ObjCCategoryDecl>(ctx)) {
            if (const auto* interface = category->getClassInterface()) {
                return interface->getNameAsString();
            }
        }
    }

    return "";
}

// ============================================================================
// Aho-Corasick 多模式匹配器构建
// ============================================================================

void PropertyNameObfuscationStrategy::buildPatternMatcher() {
    // 创建新的匹配器
    patternMatcher_ = std::make_unique<MultiPatternMatcher>();

    // 收集所有属性（当前文件 + 全局缓存）
    std::map<std::string, std::string> allProperties;

    // 添加当前文件的属性
    for (const auto& [key, info] : propertiesToObfuscate_) {
        if (!info.originalName.empty() && !info.obfuscatedName.empty()) {
            allProperties[info.originalName] = info.obfuscatedName;
        }
    }

    // 添加全局缓存的属性
    for (const auto& [propName, obfName] : globalPropertyObfuscationCache_) {
        if (!propName.empty() && !obfName.empty()) {
            allProperties[propName] = obfName;
        }
    }

    // 为每个属性添加所有相关模式到匹配器
    for (const auto& [propName, obfName] : allProperties) {
        // 跳过系统属性
        if (SYSTEM_PROPERTY_NAMES.find(propName) != SYSTEM_PROPERTY_NAMES.end()) {
            continue;
        }
        patternMatcher_->addProperty(propName, obfName);
    }

    // 构建自动机
    patternMatcher_->build();

    LOG_INFO("Aho-Corasick matcher built with " + std::to_string(allProperties.size()) +
             " properties (" + std::to_string(patternMatcher_->getPatternCount()) + " patterns)");
}

} // namespace obfuscator
