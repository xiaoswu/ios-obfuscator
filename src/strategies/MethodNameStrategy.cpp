/**
 * @file MethodNameStrategy.cpp
 * @brief 方法名混淆策略实现
 *
 * 混淆：自定义方法名、方法参数名、Block变量名
 * 保留：系统方法、协议方法、属性名
 */

#include "strategies/MethodNameStrategy.h"
#include "core/ConfigManager.h"
#include "core/SymbolTable.h"
#include "core/NameGenerator.h"
#include "core/Logger.h"
#include "core/ReplacementManager.h"
#include "core/SystemProperties.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/AST/StmtObjC.h"
#include "clang/AST/ExprObjC.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>
#include <unordered_set>
#include <functional>  // for std::hash
#include <algorithm>   // for std::sort

using namespace clang;
using namespace clang::ast_matchers;

namespace obfuscator {

// 全局方法名混淆缓存（实现全局同名同混淆）
// 相同的 selector 使用相同的混淆名，保持多态性
std::unordered_map<std::string, std::string> MethodNameObfuscationStrategy::globalMethodObfuscationCache_;

// 【P0 性能优化】协议方法缓存（静态成员定义）
std::unordered_map<std::string, std::unordered_set<std::string>> MethodNameObfuscationStrategy::globalProtocolMethodCache_;
bool MethodNameObfuscationStrategy::protocolCacheInitialized_ = false;

// 系统方法前缀列表
static const std::vector<std::string> SYSTEM_METHOD_PREFIXES = {
    "init", "new", "alloc", "copy", "mutableCopy",
    "view", "viewDid", "viewWill", "viewShould",
    "did", "will", "should",
    "application", "applicationDid",
    "tableView", "collectionView",
    "scrollView", "textField", "textView",
    "button", "gesture"
};

// 系统生命周期方法
static const std::unordered_set<std::string> LIFECYCLE_METHODS = {
    // UIViewController
    "viewDidLoad", "viewWillAppear:", "viewDidAppear:",
    "viewWillDisappear:", "viewDidDisappear:",
    "viewDidLayoutSubviews", "viewLayoutSubviews",
    "loadView", "loadViewIfNeeded", "awakeFromNib",
    // UIView
    "layoutSubviews", "drawRect:", "updateConstraints",
    "didAddSubview:", "willRemoveSubview:", "didMoveToWindow",
    "didMoveToSuperview:", "willMoveToWindow:", "willMoveToSuperview:",
    "sizeThatFits:", "sizeToFit", "intrinsicContentSize",
    "layoutMargins", "safeAreaInsetsDidChange","hitTest:withEvent",
    // NSObject
    "dealloc", "description", "debugDescription", "isEqual:", "hash",
    "isKindOfClass:", "isMemberOfClass:", "respondsToSelector:",
    "conformsToProtocol:", "performSelector:", "performSelector:withObject:",
    "performSelector:withObject:withObject:", "valueForKey:", "setValue:forKey:",
    // UIApplication
    "applicationDidFinishLaunching:", "applicationDidEnterBackground:",
    "applicationWillEnterForeground:", "applicationWillResignActive:",
    "applicationWillTerminate:"
};

// UIDelegate 方法
static const std::unordered_set<std::string> UI_DELEGATE_METHODS = {
    "tableView:numberOfRowsInSection:",
    "tableView:cellForRowAtIndexPath:",
    "tableView:heightForRowAtIndexPath:",
    "tableView:didSelectRowAtIndexPath:",
    "collectionView:numberOfItemsInSection:",
    "collectionView:cellForItemAtIndexPath:",
    "scrollViewDidScroll:",
    "scrollViewDidEndDecelerating:",
    "textFieldDidBeginEditing:",
    "textFieldDidEndEditing:",
    "textViewDidBeginEditing:",
    "textViewDidEndEditing:",
    "userContentController:didReceiveScriptMessage:",
    "webView:decidePolicyForNavigationResponse:decisionHandler:",
    "webView:decidePolicyForNavigationAction:decisionHandler:",
    "paymentQueue:updatedTransactions:",
    "productsRequest:didReceiveResponse:"
};

// ============================================================================
// SelectorTraits 预分析实现（性能优化）
// ============================================================================

SelectorTraits SelectorTraits::analyze(const std::string& sel) {
    SelectorTraits traits;
    traits.selector = sel;
    traits.length = sel.length();
    traits.hash = std::hash<std::string>{}(sel);
    traits.prefixFlags = 0;
    traits.contentFlags = 0;

    // 空字符串快速处理
    if (sel.empty()) {
        traits.ext.isEmpty = true;
        return traits;
    }
    traits.ext.isEmpty = false;
    traits.ext.firstChar = sel[0];
    traits.ext.firstCharIsUpper = (sel[0] >= 'A' && sel[0] <= 'Z');

    // ========== 分析前缀 ==========
    if (sel.length() >= 3) {
        // "set" 前缀
        if (sel[0] == 's' && sel[1] == 'e' && sel[2] == 't') {
            traits.prefixFlags |= FLAG_SET;
        }
        // "is" 前缀
        if (sel[0] == 'i' && sel[1] == 's') {
            traits.prefixFlags |= FLAG_IS;
        }
        // "new" 前缀
        if (sel[0] == 'n' && sel[1] == 'e' && sel[2] == 'w') {
            traits.prefixFlags |= FLAG_NEW;
        }
    }

    if (sel.length() >= 4) {
        // "init" 前缀
        if (sel[0] == 'i' && sel[1] == 'n' && sel[2] == 'i' && sel[3] == 't') {
            traits.prefixFlags |= FLAG_INIT;
        }
        // "alloc" 前缀
        if (sel[0] == 'a' && sel[1] == 'l' && sel[2] == 'l' && sel[3] == 'o') {
            traits.prefixFlags |= FLAG_ALLOC;
        }
        // "copy" 前缀
        if (sel[0] == 'c' && sel[1] == 'o' && sel[2] == 'p' && sel[3] == 'y') {
            traits.prefixFlags |= FLAG_COPY;
        }
        // "view" 前缀
        if (sel[0] == 'v' && sel[1] == 'i' && sel[2] == 'e' && sel[3] == 'w') {
            traits.prefixFlags |= FLAG_VIEW;
        }
    }

    if (sel.length() >= 7) {
        // "initWith" 前缀
        if (sel[0] == 'i' && sel[1] == 'n' && sel[2] == 'i' &&
            sel[3] == 't' && sel[4] == 'W' && sel[5] == 'i' && sel[6] == 't') {
            traits.prefixFlags |= FLAG_INITWITH;
        }
    }

    // "mutableCopy" 前缀检查
    if (sel.length() >= 10 && sel.compare(0, 10, "mutableCopy") == 0) {
        traits.prefixFlags |= FLAG_COPY;
    }

    // ========== 分析内容 ==========
    size_t colonCount = 0;
    for (char c : sel) {
        if (c == ':') colonCount++;
    }

    if (colonCount > 0) {
        traits.contentFlags |= FLAG_HAS_COLON;
        traits.ext.paramCount = static_cast<uint8_t>(std::min(colonCount, size_t(255)));
        traits.ext.endsWithColon = (sel.back() == ':');
    }

    // 检查是否包含 Did/Will/Should
    if (sel.find("Did") != std::string::npos) {
        traits.contentFlags |= FLAG_HAS_DID;
    }
    if (sel.find("Will") != std::string::npos) {
        traits.contentFlags |= FLAG_HAS_WILL;
    }
    if (sel.find("Should") != std::string::npos) {
        traits.contentFlags |= FLAG_HAS_SHOULD;
    }

    // 检查单例方法模式
    if (sel.length() >= 6 && sel.compare(0, 6, "shared") == 0) {
        traits.contentFlags |= FLAG_IS_SINGLETON;
    }

    // ========== 检查是否是系统方法 ==========
    // 【优化】使用全局常量，避免重复定义
    // 检查生命周期方法（完全匹配）
    if (LIFECYCLE_METHODS.count(sel)) {
        traits.contentFlags |= FLAG_IS_LIFECYCLE;
    }

    // 检查 UI Delegate 方法（完全匹配）
    if (UI_DELEGATE_METHODS.count(sel)) {
        traits.contentFlags |= FLAG_IS_UI_DELEGATE;
    }

    // 【新增】检查系统方法前缀
    for (const auto& prefix : SYSTEM_METHOD_PREFIXES) {
        if (sel.length() >= prefix.length() &&
            sel.compare(0, prefix.length(), prefix) == 0) {
            traits.contentFlags |= FLAG_HAS_SYSTEM_PREFIX;
            break;
        }
    }

    return traits;
}

// ============================================================================
// 协议方法缓存初始化（P0 性能优化）
// ============================================================================

void MethodNameObfuscationStrategy::initializeProtocolMethodCache(ASTContext& context) {
    if (protocolCacheInitialized_) {
        return;  // 已初始化，直接返回
    }

    // 遍历 AST 中所有协议声明
    TranslationUnitDecl* tu = context.getTranslationUnitDecl();
    if (!tu) {
        protocolCacheInitialized_ = true;
        return;
    }

    // 本地辅助函数：判断是否是系统协议
    // 系统协议通常以 NS, UI, CG, CF, CA, WK, GK, SK, SC, MK, CI, ML 等前缀开头
    auto isSystemProtocol = [](const std::string& name) -> bool {
        if (name.empty()) return false;

        // 检查常见系统前缀
        static const std::vector<std::string> systemPrefixes = {
            "NS", "UI", "CG", "CF", "CA", "WK", "GK", "SK", "SC", "MK",
            "CI", "ML", "MT", "SE", "AV", "AU", "MP", "MC", "CK", "CT",
            "GC", "CB", "LN", "ND", "TI", "EK", "Event", "NSItem", "UIText"
        };

        for (const auto& prefix : systemPrefixes) {
            if (name.length() >= prefix.length() &&
                name.compare(0, prefix.length(), prefix) == 0) {
                return true;
            }
        }
        return false;
    };

    // 收集所有系统协议的方法
    for (auto* decl : tu->decls()) {
        auto* protocol = dyn_cast<ObjCProtocolDecl>(decl);
        if (!protocol) continue;

        std::string protocolName = protocol->getNameAsString();

        // 只缓存系统协议的方法
        if (!isSystemProtocol(protocolName)) {
            continue;
        }

        // 收集实例方法
        for (const auto* method : protocol->methods()) {
            if (!method) continue;
            std::string selector = method->getSelector().getAsString();
            globalProtocolMethodCache_[selector].insert(protocolName);
        }

        // 收集类方法
        for (const auto* method : protocol->class_methods()) {
            if (!method) continue;
            std::string selector = method->getSelector().getAsString();
            globalProtocolMethodCache_[selector].insert(protocolName);
        }

        // 收集类属性（如 @property(class, readonly) BOOL supportsSecureCoding）
        for (const auto* prop : protocol->class_properties()) {
            if (!prop) continue;
            std::string getter = prop->getGetterName().getAsString();
            globalProtocolMethodCache_[getter].insert(protocolName);
        }
    }

    protocolCacheInitialized_ = true;

    // 调试日志
    LOG_DEBUG("Protocol method cache initialized with " +
              std::to_string(globalProtocolMethodCache_.size()) + " entries");
}

// ============================================================================
// 已知系统方法列表（用于备用检查）
// 当 AST 中没有系统类的完整定义时使用
// ============================================================================
static const std::unordered_map<std::string, std::unordered_set<std::string>> KNOWN_SYSTEM_METHODS = {
    // UIView 常用方法（备用检查）
    {"UIView", {"layoutSubviews", "drawRect:", "updateConstraints", "sizeThatFits:",
                "didAddSubview:", "willRemoveSubview:", "didMoveToWindow",
                "didMoveToSuperview:", "willMoveToWindow:", "willMoveToSuperview:",
                "layoutMargins", "safeAreaInsetsDidChange"}},
    // 用户代码中可能重写的系统方法（备用检查）
    {"NSOperation", {"start", "main", "cancel", "resume", "suspend"}},
    {"NSObject", {"init", "dealloc", "copy", "mutableCopy","keyPathsForValuesAffectingValueForKey"}},
    // NSURLSessionTask 系列方法（备用检查）
    {"NSURLSessionTask", {"resume", "suspend", "cancel", "state"}},
    {"NSURLSessionDataTask", {"resume", "suspend", "cancel"}},
    {"NSURLSessionUploadTask", {"resume", "suspend", "cancel"}},
    {"NSURLSessionDownloadTask", {"resume", "suspend", "cancel"}},
    {"NSURLConnection", {"start", "cancel", "resume", "suspend"}},
    {"NSURLConnectionDelegate", {"connection:didFailWithError:", "connectionDidFinishLoading:"}},
    // NSURLRequest 系列方法（备用检查）
    {"NSURLRequest", {"valueForHTTPHeaderField:", "allHTTPHeaderFields", "HTTPMethod", "URL"}},
    {"NSMutableURLRequest", {"valueForHTTPHeaderField:", "allHTTPHeaderFields", "HTTPMethod", "URL", "setValue:forHTTPHeaderField:"}},
};

// ============================================================================
// 已知系统属性列表（用于备用检查）
// ============================================================================
static const std::unordered_map<std::string, std::unordered_set<std::string>> KNOWN_SYSTEM_PROPERTIES = {
    // UIKit 类
    {"UIView", {"alpha", "hidden", "opaque", "backgroundColor", "frame", "bounds", "center", "transform"}},
    {"UIViewController", {"title", "view", "navigationItem", "tabBarItem"}},
    {"UIControl", {"enabled", "selected", "highlighted"}},
    {"UILabel", {"text", "font", "textColor", "textAlignment"}},
    {"UIButton", {"title", "image", "titleLabel", "imageView"}},
    {"UIImageView", {"image", "highlightedImage"}},
    {"UITableView", {"dataSource", "delegate", "rowHeight"}},
    {"UICollectionView", {"dataSource", "delegate", "collectionViewLayout"}},
    // Foundation 类
    {"NSObject", {"description", "debugDescription"}},
    {"NSOperationQueue", {"suspended", "maxConcurrentOperationCount", "name"}},
    {"NSOperation", {"cancelled", "isCancelled", "executing", "isExecuting", "finished", "isFinished", "asynchronous", "isAsynchronous", "ready", "isReady"}},
    {"NSThread", {"threadDictionary", "name", "stackSize", "isMainThread", "isExecuting", "isFinished", "isCancelled"}},
    {"NSURLRequest", {"URL", "cachePolicy", "timeoutInterval"}},
    {"NSURLResponse", {"URL", "MIMEType", "expectedContentLength", "textEncodingName", "suggestedFilename"}},
    {"NSURLSession", {"configuration", "delegate", "delegateQueue", "sessionDescription"}},
    {"NSURLSessionTask", {"state", "originalRequest", "currentRequest", "response", "taskIdentifier","resume"}},
    {"NSMutableArray", {"array"}},
    {"NSMutableDictionary", {"dictionary"}},
    {"NSMutableString", {"string"}},
    {"NSMutableSet", {"set"}},
    {"NSMutableData", {"data", "length", "mutableBytes"}},
    {"dispatch_queue", {"label", "_qos"}},
    {"NSNotificationCenter", {"defaultCenter"}},
};

// ============================================================================
// 辅助函数
// ============================================================================

namespace {
    /**
     * 提取 selector 的基础名称（去掉冒号）
     * 例如: "setValue:" -> "setValue", "tableView:cellForRowAtIndexPath:" -> "tableView"
     */
    inline std::string extractBaseSelector(const std::string& selector) {
        size_t colonPos = selector.find(':');
        if (colonPos != std::string::npos) {
            return selector.substr(0, colonPos);
        }
        return selector;
    }

    /**
     * 从 setter 方法名提取属性名
     * 例如: "setPropertyValue:" -> "propertyValue"
     */
    inline std::string extractPropertyNameFromSetter(const std::string& setterName) {
        // 检查格式: setXxxx:
        if (setterName.length() < 5 || setterName.find("set") != 0 || setterName.back() != ':') {
            return "";
        }
        // 提取: setEdgy: → edgy
        std::string propName = setterName.substr(3, setterName.length() - 4);
        if (propName.empty()) {
            return "";
        }
        // 首字母小写: setEdgy: → edgy
        propName[0] = tolower(propName[0]);
        return propName;
    }

    /**
     * 检查是否是标准 setter: setXxx:
     * 格式: 以 "set" 开头，首字母大写，以 ":" 结尾
     */
    inline bool isStandardSetter(const std::string& selector) {
        if (selector.length() < 5) return false;
        if (selector.find("set") != 0) return false;
        if (selector.back() != ':') return false;

        // 【P0 优化】使用 SelectorTraits 获取冒号数量，避免手动循环
        SelectorTraits traits = SelectorTraits::analyze(selector);
        // 多参数方法如 setImageWithURL:forState:completed: 有多个冒号，不应被识别为 setter
        if (traits.ext.paramCount != 1) return false;

        std::string propPart = selector.substr(3, selector.length() - 4);
        if (propPart.empty()) return false;
        // 首字母必须大写
        return (propPart[0] >= 'A' && propPart[0] <= 'Z');
    }

    /**
     * 检查是否是标准 getter: xxx (无参数，非 void 返回)
     */
    inline bool isStandardGetter(const ObjCMethodDecl* methodDecl) {
        if (!methodDecl) return false;
        // 类方法不能是属性 getter
        if (methodDecl->isClassMethod()) return false;
        if (methodDecl->param_size() != 0) return false;
        if (methodDecl->getReturnType()->isVoidType()) return false;

        // 排除返回 block 或函数指针的方法
        QualType returnType = methodDecl->getReturnType();
        const Type* typePtr = returnType.getTypePtr();
        std::string selector = methodDecl->getSelector().getAsString();

        // 检查是否是 block 类型
        if (typePtr->isBlockPointerType()) {
            return false;
        }

        // 检查是否是函数指针类型
        if (typePtr->isFunctionPointerType() || typePtr->isFunctionType()) {
            return false;
        }

        // 额外检查：如果返回类型包含括号（可能是函数或 block）
        std::string canonicalTypeName = returnType.getCanonicalType().getAsString();
        if (canonicalTypeName.find(")") != std::string::npos &&
            (canonicalTypeName.find("(") == 0 || canonicalTypeName.find("^") != std::string::npos)) {
            return false;
        }

        if (selector.empty()) return false;
        // 首字母小写，或 is 前缀
        if (selector[0] >= 'A' && selector[0] <= 'Z' && selector.find("is") != 0) return false;
        return true;
    }

    /**
     * 【新增】从 setter 提取属性名: setQueen: -> queen
     */
    inline std::string extractFromSetter(const std::string& setter) {
        if (setter.length() < 5 || setter.find("set") != 0 || setter.back() != ':') {
            return "";
        }
        std::string prop = setter.substr(3, setter.length() - 4);
        if (!prop.empty() && prop[0] >= 'A' && prop[0] <= 'Z') {
            prop[0] = prop[0] + 32;  // 首字母小写
        }
        return prop;
    }

    /**
     * 【新增】从 getter 提取属性名: queen -> queen, isQueen -> queen
     * @param getter getter 方法名
     * @param isBoolean 输出参数，表示是否是布尔属性
     * @return 属性基础名
     */
    inline std::string extractFromGetter(const std::string& getter, bool& isBoolean) {
        isBoolean = false;
        if (getter.length() > 2 && getter.substr(0, 2) == "is") {
            isBoolean = true;
            std::string prop = getter.substr(2);
            if (!prop.empty() && prop[0] >= 'A' && prop[0] <= 'Z') {
                prop[0] = prop[0] + 32;
            }
            return prop;
        }
        return getter;
    }

    /**
     * 【新增】首字母大写
     */
    inline std::string capitalize(const std::string& str) {
        if (str.empty()) return str;
        std::string result = str;
        if (result[0] >= 'a' && result[0] <= 'z') {
            result[0] = result[0] - 32;
        }
        return result;
    }

    /**
     * 在类及其扩展中查找指定名称的属性
     * @param classDecl 要查找的类
     * @param propName 属性名
     * @param outFoundInExtension 输出参数，是否在扩展中找到
     * @return 是否找到属性
     */
    bool findPropertyInClass(const ObjCInterfaceDecl* classDecl,
                             const std::string& propName,
                             bool* outFoundInExtension = nullptr) {
        if (!classDecl) {
            return false;
        }

        if (outFoundInExtension) {
            *outFoundInExtension = false;
        }

        // 查找主类中的属性
        for (const auto* prop : classDecl->properties()) {
            if (prop && prop->getNameAsString() == propName) {
                return true;
            }
        }

        // 查找类扩展中的属性
        for (const auto* ext : classDecl->visible_extensions()) {
            for (const auto* prop : ext->properties()) {
                if (prop && prop->getNameAsString() == propName) {
                    if (outFoundInExtension) {
                        *outFoundInExtension = true;
                    }
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * 检查方法是否在已知系统方法列表中
     * @param className 父类名
     * @param selector 要检查的 selector
     * @return 是否是已知系统方法
     */
    bool isInKnownSystemMethods(const std::string& className, const std::string& selector) {
        auto it = KNOWN_SYSTEM_METHODS.find(className);
        if (it == KNOWN_SYSTEM_METHODS.end()) {
            return false;
        }

        std::string baseSelector = extractBaseSelector(selector);

        for (const auto& sysMethod : it->second) {
            std::string baseSysMethod = extractBaseSelector(sysMethod);
            if (baseSelector == baseSysMethod) {
                return true;
            }
        }
        return false;
    }

    /**
     * 检查属性是否在已知系统属性列表中
     * @param className 父类名
     * @param propName 属性名
     * @return 是否是已知系统属性
     */
    bool isInKnownSystemProperties(const std::string& className, const std::string& propName) {
        auto it = KNOWN_SYSTEM_PROPERTIES.find(className);
        return it != KNOWN_SYSTEM_PROPERTIES.end() && it->second.count(propName) > 0;
    }
}

void MethodNameObfuscationStrategy::analyze(ASTContext& context) {
    LOG_INFO("Starting method name analysis");

    // 【P0 性能优化】初始化协议方法缓存
    // 必须在处理方法之前完成，确保 isProtocolMethod() 可以使用缓存
    initializeProtocolMethodCache(context);

    // 【性能优化】读取 SymbolTable 中的属性方法映射
    // 确保 property 的 getter/setter 方法使用与属性一致的混淆名
    if (symbolTable_) {
        const auto& propMappings = symbolTable_->getPropertyMappings();
        LOG_INFO("Loading " + std::to_string(propMappings.size()) + " property method mappings");

        // 【优化】合并两遍遍历为一遍，减少迭代开销
        for (const auto& [propName, mapping] : propMappings) {
            // 预填充 getter 方法的混淆名
            if (!mapping.originalGetterName.empty() && !mapping.obfuscatedGetterName.empty()) {
                globalMethodObfuscationCache_[mapping.originalGetterName] = mapping.obfuscatedGetterName;

                // 同时添加到 methodsToObfuscate_
                if (methodsToObfuscate_.find(mapping.originalGetterName) == methodsToObfuscate_.end()) {
                    MethodInfo info;
                    info.originalName = mapping.originalGetterName;
                    info.obfuscatedName = mapping.obfuscatedGetterName;
                    info.selector = mapping.originalGetterName;
                    info.isInstanceMethod = true;
                    info.className = mapping.originalPropertyName;
                    methodsToObfuscate_[mapping.originalGetterName] = info;
                }
            }

            // 预填充 setter 方法的混淆名
            if (!mapping.originalSetterName.empty() && !mapping.obfuscatedSetterName.empty()) {
                globalMethodObfuscationCache_[mapping.originalSetterName] = mapping.obfuscatedSetterName;

                // 同时添加到 methodsToObfuscate_
                if (methodsToObfuscate_.find(mapping.originalSetterName) == methodsToObfuscate_.end()) {
                    MethodInfo info;
                    info.originalName = mapping.originalSetterName;
                    info.obfuscatedName = mapping.obfuscatedSetterName;
                    info.selector = mapping.originalSetterName;
                    info.isInstanceMethod = true;
                    info.className = mapping.originalPropertyName;
                    methodsToObfuscate_[mapping.originalSetterName] = info;
                }
            }
        }

        LOG_INFO("Loaded " + std::to_string(propMappings.size()) + " property method mappings");
    }

    // 匹配所有ObjC方法声明
    finder_.addMatcher(
        objcMethodDecl().bind("method"),
        this
    );

    // 【新增】收集自定义协议中的方法声明（用于后续混淆协议声明）
    finder_.addMatcher(
        objcProtocolDecl().bind("protocol"),
        this
    );

    finder_.matchAST(context);

    // 【新增】处理孤立的 setter/getter 方法
    // 在所有方法收集完成后，处理没有配对的方法
    finalizeStandaloneMethods();

    // 简化的汇总日志
    std::map<std::string, int> methodsPerClass;
    for (const auto& [selector, info] : methodsToObfuscate_) {
        methodsPerClass[info.className]++;
    }

    // 只输出汇总信息，不输出详细列表
    LOG_INFO("Method name analysis: " + std::to_string(methodsToObfuscate_.size()) +
             " methods in " + std::to_string(methodsPerClass.size()) + " classes");
}

void MethodNameObfuscationStrategy::run(const MatchFinder::MatchResult& result) {
    if (const auto* method = result.Nodes.getNodeAs<ObjCMethodDecl>("method")) {
        handleMethod(method);
    }
    // 处理协议声明（收集自定义协议的方法）
    else if (const auto* protocolDecl = result.Nodes.getNodeAs<ObjCProtocolDecl>("protocol")) {
        handleProtocolDecl(protocolDecl);
    }
}

void MethodNameObfuscationStrategy::handleMethod(const ObjCMethodDecl* methodDecl) {
    if (!methodDecl || !methodDecl->getClassInterface()) {
        return;
    }

    // 【重要】只处理主文件中的方法声明，不处理系统头文件中的方法
    // 系统头文件中的方法声明（如 NSData 的 dataWithContentsOfFile:）不应被混淆
    SourceLocation loc = methodDecl->getLocation();
    if (loc.isValid()) {
        const SourceManager& SM = methodDecl->getASTContext().getSourceManager();
        // 跳过系统头文件
        if (SM.isInSystemHeader(loc)) {
            return;
        }
        // 跳过不在主文件中的声明（避免重复处理）
        // 注意：这会导致 PCH 中导入的方法声明不被处理
        // 这些方法的混淆映射需要通过 globalMethodObfuscationCache_ 来传递
        if (!SM.isInMainFile(loc)) {
            return;
        }
    }

    // 【重要】跳过隐式方法声明（属性的 getter/setter 方法）
    // 属性会自动生成隐式的 getter/setter 方法，这些方法不应该被混淆
    // 混淆属性相关的方法应该由 PropertyNameStrategy 处理
    if (methodDecl->isImplicit()) {
        return;
    }

    std::string selector = methodDecl->getSelector().getAsString();
    std::string className = getClassNameFromMethod(methodDecl);

    // 【P0 性能优化】使用 SelectorTraits 一次性分析所有 selector 特征
    // 避免在后续流程中重复进行字符串操作（find()、substr()、length() 等）
    SelectorTraits traits = SelectorTraits::analyze(selector);

    // 检查是否应该跳过此方法
    if (shouldSkipMethod(methodDecl)) {
        return;
    }

    // 【P0 优化】检查是否是标准 setter，尝试与 getter 配对
    // 使用 SelectorTraits 的位标记判断（O(1)），避免重复字符串操作
    if (traits.isSetterPattern()) {
        // 【P0 优化】直接使用 SelectorTraits 的预分析结果
        // setter 格式: setXxx:，Xxx 部分从 selector[3] 到倒数第二个字符
        std::string baseName;
        if (traits.length >= 5) {
            baseName = selector.substr(3, traits.length - 4);
            if (!baseName.empty() && baseName[0] >= 'A' && baseName[0] <= 'Z') {
                baseName[0] = baseName[0] + 32;  // 首字母小写
            }
        }
        if (baseName.empty()) return;

        std::string expectedGetter = baseName;

        // 在 unpairedGetters_ 中查找配对
        auto it = unpairedGetters_.find(expectedGetter);
        if (it != unpairedGetters_.end()) {
            // 找到配对！创建 pair
            SetterGetterPair pair;
            pair.baseName = baseName;
            pair.setterSelector = selector;
            pair.getterSelector = expectedGetter;
            pair.className = className;
            pair.isBoolean = false;
            pair.hasSetter = true;
            pair.hasGetter = true;

            // 生成混淆后的基础名
            auto cacheIt = globalMethodObfuscationCache_.find(baseName);
            if (cacheIt != globalMethodObfuscationCache_.end()) {
                // 已有同名属性的混淆名，复用
                pair.obfuscatedBaseName = cacheIt->second;
            } else {
                // 首次遇到此属性名，生成新混淆名并缓存
                pair.obfuscatedBaseName = nameGenerator_->generate(baseName, "propertyName");
                globalMethodObfuscationCache_[baseName] = pair.obfuscatedBaseName;
            }

            // 添加到全局缓存（确保同名方法使用相同混淆名）
            globalMethodObfuscationCache_[selector] = pair.getObfuscatedSetter();
            globalMethodObfuscationCache_[expectedGetter] = pair.getObfuscatedGetter();

            // 添加到待混淆列表
            MethodInfo setterInfo;
            setterInfo.originalName = selector;
            setterInfo.obfuscatedName = pair.getObfuscatedSetter();
            setterInfo.selector = selector;
            setterInfo.isInstanceMethod = methodDecl->isInstanceMethod();
            setterInfo.className = className;
            methodsToObfuscate_[selector] = setterInfo;

            MethodInfo getterInfo = it->second;
            getterInfo.obfuscatedName = pair.getObfuscatedGetter();
            methodsToObfuscate_[expectedGetter] = getterInfo;

            // 【关键】同步给 PropertyNameStrategy
            if (symbolTable_) {
                PropertyMethodMapping mapping;
                mapping.originalPropertyName = baseName;
                mapping.obfuscatedPropertyName = pair.obfuscatedBaseName;
                mapping.originalSetterName = selector;
                mapping.obfuscatedSetterName = pair.getObfuscatedSetter();
                mapping.originalGetterName = expectedGetter;
                mapping.obfuscatedGetterName = pair.getObfuscatedGetter();
                mapping.isBoolean = false;
                symbolTable_->addPropertyMapping(mapping);

                // 【新增】将 getter 方法添加到 methodSymbols，供 PropertyNameStrategy 查找
                SymbolTable::MethodSymbolInfo getterMethodInfo;
                getterMethodInfo.originalName = expectedGetter;
                getterMethodInfo.obfuscatedName = pair.getObfuscatedGetter();
                getterMethodInfo.isInstanceMethod = it->second.isInstanceMethod;
                getterMethodInfo.className = it->second.className;
                getterMethodInfo.isGetter = true;
                symbolTable_->addMethodSymbol(expectedGetter, getterMethodInfo);
            }

            setterGetterPairs_[selector] = pair;
            unpairedGetters_.erase(it);

            LOG_INFO("Paired setter/getter: " + selector + " <-> " + expectedGetter +
                     " -> " + pair.getObfuscatedSetter() + " / " + pair.getObfuscatedGetter());
        } else {
            // 暂存，等待可能的 getter
            MethodInfo info;
            info.originalName = selector;
            info.selector = selector;
            info.isInstanceMethod = methodDecl->isInstanceMethod();
            info.className = className;
            unpairedSetters_[selector] = info;
        }
        return;
    }

    // 【P0 优化】检查是否是标准 getter，尝试与 setter 配对
    if (isStandardGetter(methodDecl)) {
        // 【修复】检查是否是系统属性的 getter
        // 系统属性的 getter 不应该被混淆（如 UIView.center 的 getter）
        if (isGetterOfSystemProperty(methodDecl)) {
            LOG_INFO("Skipping system property getter: " + selector);
            return;
        }

        // 【P0 优化】使用 SelectorTraits 预分析结果提取 baseName
        std::string baseName;
        bool isBoolean = traits.hasIsPrefix();  // O(1) 位标记判断

        if (isBoolean && traits.length > 2) {
            // isXxx 格式，提取 Xxx 部分
            baseName = selector.substr(2);
            if (!baseName.empty() && baseName[0] >= 'A' && baseName[0] <= 'Z') {
                baseName[0] = baseName[0] + 32;  // 首字母小写
            }
        } else {
            // 直接 getter（普通属性）
            baseName = selector;
        }

        // 构造预期的 setter 名称: setXxx:
        std::string expectedSetter = "set" + capitalize(baseName) + ":";

        // 在 unpairedSetters_ 中查找配对
        auto it = unpairedSetters_.find(expectedSetter);
        if (it != unpairedSetters_.end()) {
            // 找到配对！创建 pair
            SetterGetterPair pair;
            pair.baseName = baseName;
            pair.setterSelector = expectedSetter;
            pair.getterSelector = selector;  // 使用原始 selector
            pair.className = className;
            pair.isBoolean = isBoolean;
            pair.hasSetter = true;
            pair.hasGetter = true;

            // 生成混淆后的基础名
            auto cacheIt = globalMethodObfuscationCache_.find(baseName);
            if (cacheIt != globalMethodObfuscationCache_.end()) {
                // 已有同名属性的混淆名，复用
                pair.obfuscatedBaseName = cacheIt->second;
            } else {
                // 首次遇到此属性名，生成新混淆名并缓存
                pair.obfuscatedBaseName = nameGenerator_->generate(baseName, "propertyName");
                globalMethodObfuscationCache_[baseName] = pair.obfuscatedBaseName;
            }

            // 添加到全局缓存
            globalMethodObfuscationCache_[expectedSetter] = pair.getObfuscatedSetter();
            globalMethodObfuscationCache_[selector] = pair.getObfuscatedGetter();

            // 添加到待混淆列表
            MethodInfo setterInfo = it->second;
            setterInfo.obfuscatedName = pair.getObfuscatedSetter();
            methodsToObfuscate_[expectedSetter] = setterInfo;

            MethodInfo getterInfo;
            getterInfo.originalName = selector;
            getterInfo.obfuscatedName = pair.getObfuscatedGetter();
            getterInfo.selector = selector;
            getterInfo.isInstanceMethod = methodDecl->isInstanceMethod();
            getterInfo.className = className;
            methodsToObfuscate_[selector] = getterInfo;

            // 【关键】同步给 PropertyNameStrategy
            if (symbolTable_) {
                PropertyMethodMapping mapping;
                mapping.originalPropertyName = baseName;
                mapping.obfuscatedPropertyName = pair.obfuscatedBaseName;
                mapping.originalSetterName = expectedSetter;
                mapping.obfuscatedSetterName = pair.getObfuscatedSetter();
                mapping.originalGetterName = selector;
                mapping.obfuscatedGetterName = pair.getObfuscatedGetter();
                mapping.isBoolean = isBoolean;
                symbolTable_->addPropertyMapping(mapping);

                // 【新增】将 getter 方法添加到 methodSymbols，供 PropertyNameStrategy 查找
                SymbolTable::MethodSymbolInfo getterMethodInfo;
                getterMethodInfo.originalName = selector;
                getterMethodInfo.obfuscatedName = pair.getObfuscatedGetter();
                getterMethodInfo.isInstanceMethod = methodDecl->isInstanceMethod();
                getterMethodInfo.className = className;
                getterMethodInfo.isGetter = true;
                symbolTable_->addMethodSymbol(selector, getterMethodInfo);
            }

            setterGetterPairs_[expectedSetter] = pair;
            unpairedSetters_.erase(it);

            LOG_INFO("Paired getter/setter: " + selector + " <-> " + expectedSetter +
                     " -> " + pair.getObfuscatedGetter() + " / " + pair.getObfuscatedSetter());
        } else {
            // 暂存，等待可能的 setter
            MethodInfo info;
            info.originalName = selector;
            info.selector = selector;
            info.isInstanceMethod = methodDecl->isInstanceMethod();
            info.className = className;
            unpairedGetters_[selector] = info;
        }
        return;
    }

    currentClass_ = className;
    currentMethod_ = selector;

    // 生成混淆后的方法名（使用全局缓存实现同名同混淆）
    std::string obfuscatedName;
    auto it = globalMethodObfuscationCache_.find(selector);
    if (it != globalMethodObfuscationCache_.end()) {
        // 已有同名方法的混淆名，复用
        obfuscatedName = it->second;
    } else {
        // 首次遇到此 selector，生成新混淆名并缓存
        obfuscatedName = nameGenerator_->generate(selector, "methodName");
        globalMethodObfuscationCache_[selector] = obfuscatedName;
    }

    // 【协作】将方法符号信息填充到 SymbolTable
    // 这样 PropertyNameStrategy 可以通过 SymbolTable 查找方法 getter
    if (symbolTable_) {
        SymbolTable::MethodSymbolInfo methodInfo;
        methodInfo.originalName = selector;
        methodInfo.obfuscatedName = obfuscatedName;
        methodInfo.isInstanceMethod = methodDecl->isInstanceMethod();
        methodInfo.className = className;
        // 判断是否是 getter 风格的方法（无参数）
        methodInfo.isGetter = (methodDecl->param_size() == 0);
        symbolTable_->addMethodSymbol(selector, methodInfo);
    }

    MethodInfo info;
    info.originalName = selector;
    info.obfuscatedName = obfuscatedName;
    info.selector = getFullSelector(methodDecl);
    info.isInstanceMethod = methodDecl->isInstanceMethod();
    info.className = className;

    // 【新增】为多参数方法的每个selector片段单独生成混淆名
    // 计算selector片段数量（通过统计冒号数量）
    unsigned numColons = 0;
    for (char c : selector) {
        if (c == ':') numColons++;
    }
    unsigned numPieces = (numColons > 0) ? numColons : 1;

    if (numPieces > 1) {
        // 多参数方法：为每个片段单独生成混淆名
        // 使用 "selector_片段索引" 作为唯一标识来确保同名同混淆
        for (unsigned i = 0; i < numPieces; ++i) {
            std::string pieceKey = selector + "_piece_" + std::to_string(i);

            std::string pieceObfuscated;
            auto pieceIt = globalMethodObfuscationCache_.find(pieceKey);
            if (pieceIt != globalMethodObfuscationCache_.end()) {
                // 已有同名片段的混淆名，复用
                pieceObfuscated = pieceIt->second;
            } else {
                // 首次遇到此片段，生成新混淆名并缓存
                pieceObfuscated = nameGenerator_->generate(pieceKey, "methodName");
                globalMethodObfuscationCache_[pieceKey] = pieceObfuscated;
            }

            info.pieceObfuscatedNames.push_back(pieceObfuscated);
        }
    }
    // 单参数方法直接使用 obfuscatedName，不需要 pieceObfuscatedNames

    // 处理参数名
    processMethodParameters(methodDecl);

    methodsToObfuscate_[selector] = info;

    // 处理方法体内的Block变量
    if (methodDecl->hasBody()) {
        const Stmt* body = methodDecl->getBody();
        if (body) {
            processBlocksInMethod(body);
        }
    }
}

void MethodNameObfuscationStrategy::handleProtocolDecl(const ObjCProtocolDecl* protocolDecl) {
    if (!protocolDecl) {
        return;
    }

    std::string protocolName = protocolDecl->getNameAsString();

    // 检查是否为自定义协议（非系统协议）
    if (isSystemSymbol(protocolName)) {
        return;  // 跳过系统协议
    }

    // 遍历协议中的所有方法
    for (const auto* method : protocolDecl->methods()) {
        if (!method || method->isImplicit()) {
            continue;
        }

        std::string selector = method->getSelector().getAsString();

        // 检查是否应该跳过此方法（系统方法、白名单等）
        if (isSystemMethod(method) || isInitMethod(selector) ||
            isLifecycleMethod(selector) || isWhitelisted(selector, "method")) {
            continue;
        }

        // 生成混淆后的方法名（使用全局缓存实现同名同混淆）
        std::string obfuscatedName;
        auto it = globalMethodObfuscationCache_.find(selector);
        if (it != globalMethodObfuscationCache_.end()) {
            // 已有同名方法的混淆名，复用
            obfuscatedName = it->second;
        } else {
            // 首次遇到此 selector，生成新混淆名并缓存
            obfuscatedName = nameGenerator_->generate(selector, "methodName");
            globalMethodObfuscationCache_[selector] = obfuscatedName;
        }

        // 存储协议方法信息
        MethodInfo info;
        info.originalName = selector;
        info.obfuscatedName = obfuscatedName;
        info.selector = getFullSelector(method);
        info.isInstanceMethod = method->isInstanceMethod();
        info.className = "@" + protocolName;  // 使用 @ 前缀标记为协议方法

        // 为多参数方法的每个 selector 片段单独生成混淆名
        unsigned numColons = 0;
        for (char c : selector) {
            if (c == ':') numColons++;
        }
        unsigned numPieces = (numColons > 0) ? numColons : 1;

        if (numPieces > 1) {
            for (unsigned i = 0; i < numPieces; ++i) {
                std::string pieceKey = selector + "_piece_" + std::to_string(i);

                std::string pieceObfuscated;
                auto pieceIt = globalMethodObfuscationCache_.find(pieceKey);
                if (pieceIt != globalMethodObfuscationCache_.end()) {
                    pieceObfuscated = pieceIt->second;
                } else {
                    pieceObfuscated = nameGenerator_->generate(pieceKey, "methodName");
                    globalMethodObfuscationCache_[pieceKey] = pieceObfuscated;
                }

                info.pieceObfuscatedNames.push_back(pieceObfuscated);
            }
        }

        // 将协议方法添加到混淆列表
        methodsToObfuscate_[selector] = info;
        protocolMethodsToObfuscate_[protocolName].push_back({selector, info});
    }
}

bool MethodNameObfuscationStrategy::shouldSkipMethod(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return true;
    }

    std::string methodName = methodDecl->getNameAsString();
    std::string selector = methodDecl->getSelector().getAsString();

    // 【新增】系统方法黑名单（协议和分类方法不应被混淆）
    // 当 AST 中协议/分类信息不完整时的备用检查
    static const std::unordered_set<std::string> SYSTEM_PROTOCOL_METHODS_BLACKLIST = {
        // ===== NSCoding 协议 =====
        "supportsSecureCoding",                    // NSSecureCoding
        "encodeWithCoder:",                        // NSCoding
        "initWithCoder:",                          // NSCoding

        // ===== NSCopying 协议 =====
        "copyWithZone:",                           // NSCopying

        // ===== NSMutableCopying 协议 =====
        "mutableCopyWithZone:",                    // NSMutableCopying

        // ===== NSKeyValueObserving 分类 =====
        "observeValueForKeyPath:ofObject:change:context:",  // 实例方法
        "automaticallyNotifiesObserversForKey:",           // 类方法
        "keyPathsForValuesAffectingValueForKey:",           // 类方法
    };

    if (SYSTEM_PROTOCOL_METHODS_BLACKLIST.find(selector) != SYSTEM_PROTOCOL_METHODS_BLACKLIST.end()) {
        return true;
    }

    // 检查白名单（使用 selector 而不是 methodName，以支持多参数方法的完整 selector）
    if (isWhitelisted(selector, "method")) {
        return true;
    }

    // 【P0 性能优化】合并属性相关检查：同时检查白名单属性和已处理的属性
    // 白名单属性的 getter/setter 不应被混淆；已由 PropertyNameStrategy 处理的方法也应跳过
    if (symbolTable_) {
        const auto& propMappings = symbolTable_->getPropertyMappings();
        for (const auto& [propName, mapping] : propMappings) {
            // 检查1：如果是白名单属性（原始名等于混淆名），跳过
            if (mapping.originalPropertyName == mapping.obfuscatedPropertyName) {
                // 检查是否是该属性的 getter
                if (!mapping.originalGetterName.empty() &&
                    methodName == mapping.originalGetterName) {
                    LOG_INFO("Skipping getter of whitelisted property: " + methodName +
                            " (property: " + propName + ")");
                    return true;
                }
                // 检查是否是该属性的 setter
                if (!mapping.originalSetterName.empty() &&
                    methodName == mapping.originalSetterName) {
                    LOG_INFO("Skipping setter of whitelisted property: " + methodName +
                            " (property: " + propName + ")");
                    return true;
                }
            }

            // 检查2：如果方法已在 PropertyNameStrategy 中处理过，跳过混淆
            if (methodName == mapping.originalSetterName ||
                methodName == mapping.originalGetterName) {
                // 已由属性策略处理，跳过
                return true;
            }
        }
    }

    // 检查系统方法
    if (isSystemMethod(methodDecl)) {
        return true;
    }

    // 检查协议方法（实现协议的方法不混淆）
    if (isProtocolMethod(methodDecl)) {
        return true;
    }

    // 【新增】检查是否重写系统类的方法
    if (isOverridingSystemMethod(methodDecl)) {
        return true;
    }

    // 【新增】检查是否是系统属性的 setter
    if (isSetterOfSystemProperty(methodDecl)) {
        return true;
    }

    return false;
}

bool MethodNameObfuscationStrategy::isSystemMethod(const ObjCMethodDecl* methodDecl) const {
    std::string methodName = methodDecl->getNameAsString();
    std::string selector = methodDecl->getSelector().getAsString();

    // 检查是否为init方法
    if (isInitMethod(selector)) {
        return true;
    }

    // 注意：getter/setter 方法的混淆由 PropertyNameStrategy 处理
    // 通过 SymbolTable 进行同步，这里不跳过自定义的 getter/setter
    // 只有系统类的 getter/setter 才需要跳过（通过下面的系统类检查处理）

    // P1 性能优化：使用 unordered_set 的 find() 实现 O(1) 查找
    // 检查生命周期方法（完全匹配）
    if (LIFECYCLE_METHODS.find(selector) != LIFECYCLE_METHODS.end()) {
        return true;
    }

    // 检查UIDelegate方法（完全匹配）
    if (UI_DELEGATE_METHODS.find(selector) != UI_DELEGATE_METHODS.end()) {
        return true;
    }

    // 检查系统方法前缀（需要遍历，无法使用哈希）
    for (const auto& prefix : SYSTEM_METHOD_PREFIXES) {
        if (selector.find(prefix) == 0) {
            return true;
        }
    }

    // 【修复】检查是否真的是属性的 getter
    // 不能简单地判断以 "is" 开头，因为可能是独立的查询方法（如 isRunning）
    if (selector.find("is") == 0 && methodDecl->isInstanceMethod() && methodDecl->param_size() == 0) {
        // 检查该方法是否确实是某个属性的 getter
        // 通过查找同名的属性来判断
        std::string propName = selector.substr(2);  // 去掉 "is" 前缀
        if (methodDecl->getClassInterface()) {
            for (const auto* prop : methodDecl->getClassInterface()->properties()) {
                std::string propGetterName = prop->getGetterName().getAsString();
                if (propGetterName == selector) {
                    // 这确实是某个属性的 getter，跳过混淆
                    return true;
                }
            }
        }
        // 不是属性的 getter，是独立的查询方法，应该混淆
    }

    // 检查是否是系统类原本定义的方法（不包括 category 方法）
    // Category 添加到系统类的方法应该被混淆（如 sd_imageURL）
    if (methodDecl->getClassInterface() &&
        isSystemSymbol(methodDecl->getClassInterface()->getNameAsString())) {
        // 只有非 category 方法才跳过（即系统类原本的方法）
        // getCategory() 返回 null 表示是类主接口的方法
        // getCategory() 返回非 null 表示是 category 方法
        if (methodDecl->getCategory() == nullptr) {
            return true;
        }
    }

    return false;
}

bool MethodNameObfuscationStrategy::isInitMethod(const std::string& methodName) const {
    if (methodName == "init") {
        return true;
    }
    if (methodName.find("initWith") == 0) {
        return true;
    }
    if (methodName.find("init") == 0) {
        return true;
    }
    return false;
}

bool MethodNameObfuscationStrategy::isGetterMethod(const ObjCMethodDecl* methodDecl) const {
    // getter方法：无参数，返回类型不是void
    if (methodDecl->param_size() != 0) {
        return false;
    }

    // 获取返回类型
    QualType returnType = methodDecl->getReturnType();
    if (returnType->isVoidType()) {
        return false;
    }

    std::string methodName = methodDecl->getNameAsString();

    // 检查是否以非标准前缀开头（通常是getter）
    // 标准 setter 以 "set" 开头，对应 getter 不带 "set"
    // 这里简单判断：如果不是 setter 且无参数，可能是 getter
    return true;
}

bool MethodNameObfuscationStrategy::isSetterMethod(const ObjCMethodDecl* methodDecl) const {
    std::string methodName = methodDecl->getNameAsString();

    // setter方法通常以 "set" 开头，且有一个参数
    if (methodName.find("set") == 0 && methodDecl->param_size() == 1) {
        return true;
    }

    return false;
}

bool MethodNameObfuscationStrategy::isDelegateMethod(const ObjCMethodDecl* methodDecl) const {
    std::string selector = methodDecl->getSelector().getAsString();

    // P1 性能优化：使用 unordered_set 的 find() 实现 O(1) 查找
    // delegate方法通常包含特定的关键字
    if (UI_DELEGATE_METHODS.find(selector) != UI_DELEGATE_METHODS.end()) {
        return true;
    }

    // 检查方法名中是否包含 "delegate" 相关关键词
    if (selector.find("Did") != std::string::npos ||
        selector.find("Will") != std::string::npos ||
        selector.find("Should") != std::string::npos) {
        return true;
    }

    return false;
}

bool MethodNameObfuscationStrategy::isLifecycleMethod(const std::string& methodName) const {
    // P1 性能优化：使用 unordered_set 的 find() 实现 O(1) 查找
    return LIFECYCLE_METHODS.find(methodName) != LIFECYCLE_METHODS.end();
}

bool MethodNameObfuscationStrategy::isProtocolMethod(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return false;
    }

    std::string selector = methodDecl->getSelector().getAsString();

    // 【P0 性能优化】直接查缓存，避免嵌套循环
    // 从 O(P × M) 降低到 O(1)
    return globalProtocolMethodCache_.find(selector) != globalProtocolMethodCache_.end();
}

// ============================================================================
// 检查是否重写系统方法的辅助函数
// ============================================================================

namespace {
    /**
     * 通过 AST 遍历检查方法是否重写系统类方法
     * @param classDecl 当前类
     * @param selector 方法选择器
     * @param paramCount 参数数量
     * @param isSystemSymbol 判断是否为系统符号的函数
     * @return 是否重写系统方法
     */
    bool checkOverrideViaAST(const ObjCInterfaceDecl* classDecl,
                             const std::string& selector,
                             unsigned paramCount,
                             const std::function<bool(const std::string&)>& isSystemSymbol) {
        if (!classDecl) {
            return false;
        }

        std::string className = classDecl->getNameAsString();

        // 遍历继承链
        const ObjCInterfaceDecl* superDecl = classDecl->getSuperClass();
        while (superDecl) {
            std::string superClassName = superDecl->getNameAsString();

            // 检查父类是否是系统类
            if (isSystemSymbol(superClassName)) {
                // 在系统父类中查找同名方法
                for (const auto* method : superDecl->methods()) {
                    if (!method) {
                        continue;
                    }

                    // 比较 selector 和参数数量
                    if (method->getSelector().getAsString() == selector &&
                        method->param_size() == paramCount) {
                        // 找到了！这是重写系统类的方法
                        LOG_INFO("Skipping override of system method: " + selector +
                                " in class " + className +
                                " (overrides " + superClassName + ")");
                        return true;
                    }
                }
            }
            superDecl = superDecl->getSuperClass();
        }

        return false;
    }

    /**
     * 通过已知系统方法列表检查是否重写系统方法
     * 当 AST 中没有系统类的完整定义时使用
     * @param classDecl 当前类
     * @param selector 方法选择器
     * @return 是否重写已知系统方法
     */
    bool checkOverrideViaKnownMethods(const ObjCInterfaceDecl* classDecl,
                                      const std::string& selector) {
        if (!classDecl) {
            return false;
        }

        std::string className = classDecl->getNameAsString();

        // 遍历继承链
        const ObjCInterfaceDecl* superDecl = classDecl->getSuperClass();
        while (superDecl) {
            std::string superClassName = superDecl->getNameAsString();

            if (isInKnownSystemMethods(superClassName, selector)) {
                LOG_INFO("Skipping override of known system method: " + selector +
                        " in class " + className +
                        " (overrides " + superClassName + ")");
                return true;
            }

            superDecl = superDecl->getSuperClass();
        }

        return false;
    }
}

bool MethodNameObfuscationStrategy::isOverridingSystemMethod(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return false;
    }

    const ObjCInterfaceDecl* classDecl = methodDecl->getClassInterface();
    if (!classDecl) {
        return false;
    }

    std::string selector = methodDecl->getSelector().getAsString();
    unsigned paramCount = methodDecl->param_size();

    // 首先通过 AST 遍历检查
    if (checkOverrideViaAST(classDecl, selector, paramCount,
            [this](const std::string& name) { return isSystemSymbol(name); })) {
        return true;
    }

    // 备用检查：如果 AST 中没有系统类的完整定义，使用已知系统方法列表
    if (checkOverrideViaKnownMethods(classDecl, selector)) {
        return true;
    }

    return false;
}

// ============================================================================
// 检查是否是系统属性 setter 的辅助函数
// ============================================================================

namespace {
    /**
     * 通过 AST 遍历检查是否是系统属性的 setter
     * @param classDecl 当前类
     * @param propName 属性名
     * @param methodName 方法名（用于日志）
     * @param isSystemSymbol 判断是否为系统符号的函数
     * @return 是否是系统属性 setter
     */
    bool checkSystemPropertyViaAST(const ObjCInterfaceDecl* classDecl,
                                   const std::string& propName,
                                   const std::string& methodName,
                                   const std::function<bool(const std::string&)>& isSystemSymbol) {
        if (!classDecl) {
            return false;
        }

        // 遍历继承链，查找属性
        const ObjCInterfaceDecl* currentDecl = classDecl;
        while (currentDecl) {
            bool foundInExtension = false;

            if (findPropertyInClass(currentDecl, propName, &foundInExtension)) {
                // 找到了属性，检查是否是系统类
                if (isSystemSymbol(currentDecl->getNameAsString())) {
                    std::string logMsg = foundInExtension ?
                        "Skipping setter of system property in extension: " :
                        "Skipping setter of system property: ";
                    LOG_INFO(logMsg + methodName +
                            " (property in " + currentDecl->getNameAsString() + ")");
                    return true;
                }
            }

            currentDecl = currentDecl->getSuperClass();
        }

        return false;
    }

    /**
     * 通过已知系统属性列表检查是否是系统属性的 setter
     * @param classDecl 当前类
     * @param propName 属性名
     * @param methodName 方法名（用于日志）
     * @return 是否是已知系统属性 setter
     */
    bool checkSystemPropertyViaKnownList(const ObjCInterfaceDecl* classDecl,
                                        const std::string& propName,
                                        const std::string& methodName) {
        if (!classDecl) {
            return false;
        }

        // 检查当前类是否有该属性
        bool classHasThisProperty = false;
        const ObjCInterfaceDecl* currentDecl = classDecl;
        while (currentDecl) {
            if (findPropertyInClass(currentDecl, propName)) {
                classHasThisProperty = true;
                break;
            }
            currentDecl = currentDecl->getSuperClass();
        }

        // 如果当前类没有该属性，检查是否匹配任何系统类的属性
        if (!classHasThisProperty) {
            for (const auto& [sysClass, props] : KNOWN_SYSTEM_PROPERTIES) {
                if (props.count(propName)) {
                    LOG_INFO("Skipping setter matching system property: " + methodName +
                            " (property " + propName + " in " + sysClass + ")");
                    return true;
                }
            }
        }

        // 检查父类的已知属性
        if (const ObjCInterfaceDecl* superClass = classDecl->getSuperClass()) {
            std::string superClassName = superClass->getNameAsString();

            if (isInKnownSystemProperties(superClassName, propName)) {
                LOG_INFO("Skipping setter of known system property: " + methodName +
                        " (property " + propName + " in " + superClassName + ")");
                return true;
            }
        }

        return false;
    }
}

bool MethodNameObfuscationStrategy::isSetterOfSystemProperty(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return false;
    }

    std::string methodName = methodDecl->getSelector().getAsString();

    // 提取属性名（使用辅助函数）
    std::string propName = extractPropertyNameFromSetter(methodName);
    if (propName.empty()) {
        return false;
    }

    // 【修复】直接检查属性名是否在全局系统属性名集合中
    // 即使是自定义类的方法，如果属性名恰好是系统属性名（如 center, width, margin），
    // 也不应该混淆，因为这会与系统属性混淆产生冲突
    if (SYSTEM_PROPERTY_NAMES.count(propName)) {
        LOG_INFO("Skipping setter with system property name: " + methodName);
        return true;
    }

    const ObjCInterfaceDecl* classDecl = methodDecl->getClassInterface();
    if (!classDecl) {
        return false;
    }

    // 首先通过 AST 遍历检查
    if (checkSystemPropertyViaAST(classDecl, propName, methodName,
            [this](const std::string& name) { return isSystemSymbol(name); })) {
        return true;
    }

    // 备用检查：如果 AST 中没有系统类的完整定义，使用已知系统属性列表
    if (checkSystemPropertyViaKnownList(classDecl, propName, methodName)) {
        return true;
    }

    return false;
}

bool MethodNameObfuscationStrategy::isGetterOfSystemProperty(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return false;
    }

    std::string methodName = methodDecl->getSelector().getAsString();

    // 对于 getter，方法名就是属性名
    std::string propName = methodName;

    // 【修复】直接检查方法名是否在全局系统属性名集合中
    // 即使是自定义类的方法，如果方法名恰好是系统属性名（如 center, width），
    // 也不应该混淆，因为这会与系统属性混淆产生冲突
    if (SYSTEM_PROPERTY_NAMES.count(propName)) {
        LOG_INFO("Skipping getter with system property name: " + methodName);
        return true;
    }

    const ObjCInterfaceDecl* classDecl = methodDecl->getClassInterface();
    if (!classDecl) {
        return false;
    }

    // 首先通过 AST 遍历检查
    if (checkSystemPropertyViaAST(classDecl, propName, methodName,
            [this](const std::string& name) { return isSystemSymbol(name); })) {
        return true;
    }

    // 备用检查：如果 AST 中没有系统类的完整定义，使用已知系统属性列表
    if (checkSystemPropertyViaKnownList(classDecl, propName, methodName)) {
        return true;
    }

    return false;
}

void MethodNameObfuscationStrategy::processMethodParameters(const ObjCMethodDecl* methodDecl) {
    if (!methodDecl) {
        return;
    }

    std::map<std::string, std::string> paramMapping;

    for (unsigned i = 0; i < methodDecl->param_size(); ++i) {
        const ParmVarDecl* param = methodDecl->getParamDecl(i);
        if (!param) {
            continue;
        }

        std::string paramName = param->getNameAsString();
        if (paramName.empty()) {
            continue;
        }

        // 生成混淆后的参数名
        std::string obfuscatedParamName = generateParameterName(paramName);
        paramMapping[paramName] = obfuscatedParamName;
    }

    // 存储到本地映射
    std::string selector = methodDecl->getSelector().getAsString();
    methodParameters_[selector] = paramMapping;

    // 同步注册到 SymbolTable，供 PropertyNameStrategy 查询
    if (symbolTable_) {
        symbolTable_->addMethodParameterMapping(selector, paramMapping);
    }
}

std::string MethodNameObfuscationStrategy::generateParameterName(const std::string& originalName) {
    auto it = methodParameters_.find(currentMethod_);
    if (it != methodParameters_.end()) {
        auto it2 = it->second.find(originalName);
        if (it2 != it->second.end()) {
            return it2->second;
        }
    }

    return getObfuscatedParameterName(originalName);
}

std::string MethodNameObfuscationStrategy::getObfuscatedParameterName(const std::string& originalName) {
    return nameGenerator_->generate(originalName, "parameterName");
}

void MethodNameObfuscationStrategy::processBlocksInMethod(const Stmt* stmt) {
    if (!stmt) {
        return;
    }

    findAndProcessBlocks(stmt);
}

void MethodNameObfuscationStrategy::findAndProcessBlocks(const Stmt* stmt) {
    if (!stmt) {
        return;
    }

    // 处理不同类型的语句
    switch (stmt->getStmtClass()) {
        case Stmt::DeclStmtClass: {
            const DeclStmt* declStmt = cast<DeclStmt>(stmt);
            if (declStmt) {
                handleBlockVariable(declStmt);
            }
            break;
        }
        case Stmt::CompoundStmtClass: {
            const CompoundStmt* compound = cast<CompoundStmt>(stmt);
            if (compound) {
                for (const auto* child : compound->body()) {
                    findAndProcessBlocks(child);
                }
            }
            break;
        }
        case Stmt::ForStmtClass:
        case Stmt::WhileStmtClass:
        case Stmt::DoStmtClass:
        case Stmt::IfStmtClass:
        case Stmt::SwitchStmtClass: {
            // 遍历子语句
            for (const auto* child : stmt->children()) {
                findAndProcessBlocks(child);
            }
            break;
        }
        case Stmt::ObjCForCollectionStmtClass: {
            const ObjCForCollectionStmt* forStmt = cast<ObjCForCollectionStmt>(stmt);
            if (forStmt) {
                findAndProcessBlocks(forStmt->getBody());
            }
            break;
        }
        case Stmt::ObjCAtTryStmtClass: {
            const ObjCAtTryStmt* tryStmt = cast<ObjCAtTryStmt>(stmt);
            if (tryStmt) {
                findAndProcessBlocks(tryStmt->getTryBody());
                // 处理所有catch语句
                for (unsigned i = 0; i < tryStmt->getNumCatchStmts(); ++i) {
                    findAndProcessBlocks(tryStmt->getCatchStmt(i));
                }
                // 处理finally语句
                if (const ObjCAtFinallyStmt* finallyStmt = tryStmt->getFinallyStmt()) {
                    findAndProcessBlocks(finallyStmt->getFinallyBody());
                }
            }
            break;
        }
        default:
            break;
    }
}

void MethodNameObfuscationStrategy::handleBlockVariable(const DeclStmt* declStmt) {
    // Block 变量现在由 VariableNameStrategy 处理，不再在此处理
    // 这样可以避免两个策略对同一个 block 变量产生不同的混淆名
    return;

    if (!declStmt) {
        return;
    }

    for (const auto* decl : declStmt->getDeclGroup()) {
        if (!decl) {
            continue;
        }

        // 检查是否为 Block 变量
        const auto* varDecl = dyn_cast<VarDecl>(decl);
        if (!varDecl) {
            continue;
        }

        QualType varType = varDecl->getType();
        const auto* blockType = varType->getAs<BlockPointerType>();
        if (!blockType) {
            continue;
        }

        std::string varName = varDecl->getNameAsString();
        if (varName.empty()) {
            continue;
        }

        // 生成混淆后的变量名
        std::string obfuscatedVarName = nameGenerator_->generate(varName, "parameterName");

        BlockInfo info;
        info.originalName = varName;
        info.obfuscatedName = obfuscatedVarName;
        info.varType = varType.getAsString();
        info.isParameter = false;

        blockVariables_[currentMethod_][varName] = info;

        // 性能优化：移除热点日志
        // LOG_INFO("Will obfuscate block variable: " + varName + " -> " + obfuscatedVarName);
    }
}

// 【新架构】收集所有替换项
void MethodNameObfuscationStrategy::collectReplacements(ASTContext& context, ReplacementManager& manager) {
    LOG_INFO("Collecting method name replacements");

    // 【重要】在收集阶段开始时，再次从 SymbolTable 读取最新的属性映射
    // 这是因为文件是按顺序处理的，某些文件可能在之前被处理时属性映射还不存在
    // 通过在收集阶段更新缓存，确保能够访问到所有属性的 getter/setter 映射
    if (symbolTable_) {
        const auto& propMappings = symbolTable_->getPropertyMappings();
        int newMappings = 0;
        for (const auto& [propName, mapping] : propMappings) {
            // 预填充 getter 方法的混淆名
            if (!mapping.originalGetterName.empty() && !mapping.obfuscatedGetterName.empty()) {
                auto it = globalMethodObfuscationCache_.find(mapping.originalGetterName);
                if (it == globalMethodObfuscationCache_.end()) {
                    globalMethodObfuscationCache_[mapping.originalGetterName] = mapping.obfuscatedGetterName;
                    newMappings++;
                }
            }
            // 预填充 setter 方法的混淆名
            if (!mapping.originalSetterName.empty() && !mapping.obfuscatedSetterName.empty()) {
                auto it = globalMethodObfuscationCache_.find(mapping.originalSetterName);
                if (it == globalMethodObfuscationCache_.end()) {
                    globalMethodObfuscationCache_[mapping.originalSetterName] = mapping.obfuscatedSetterName;
                    newMappings++;
                }
            }
        }
        if (newMappings > 0) {
            LOG_INFO("Updated method obfuscation cache with " + std::to_string(newMappings) + " new property mappings");
        }
    }

    replaceAllInOnePass(context, manager);

    LOG_INFO("Method name replacement collection completed");
}

namespace {
    // 统一的替换项类型
    enum class ReplacementType {
        MethodName,      // 方法名
        ParameterName,   // 参数名
        BlockVariable,   // Block变量
        MessageExpr,     // 消息表达式
        SelectorExpr     // @selector表达式
    };

    // 统一的替换项结构
    struct ReplacementItem {
        clang::SourceLocation loc;
        std::string original;
        std::string replacement;
        ReplacementType type;
        unsigned length;  // 原始文本长度
    };
}

// ============================================================================
// 辅助函数和缓存：检查符号名是否是系统类
// ============================================================================
namespace {
    // 系统类前缀集合（O(1) 查找）
    static const std::unordered_set<std::string> SYSTEM_PREFIXES = {
        "NS",  // NSObject, NSString, NSURLConnection, NSOperation 等
        "UI",  // UIViewController, UIView 等
        "CG",  // CGPoint, CGRect 等
        "CA",  // CALayer 等
        "CF",  // CFTypeRef 等
        "DI",  // dispatch_ 相关
        "CI",  // Core Image
        "GL",  // OpenGL
        "GK",  // GameKit
        "GC",  // GameController
        "SC",  // SpriteKit
        "SK",  // StoreKit
        "MK",  // MapKit
        "AV",  // AVFoundation
        "WK",  // WebKit
    };

    // 内联函数：快速检查是否是系统类
    inline bool isSystemSymbolName(const std::string& name) {
        if (name.length() < 2) return false;
        std::string prefix = name.substr(0, 2);
        return SYSTEM_PREFIXES.count(prefix) > 0;
    }

    // 缓存结构：用于避免重复检查
    struct CheckCache {
        std::unordered_set<std::string> systemClassCache;      // 已确认为系统类的类名
        std::unordered_set<std::string> nonSystemClassCache;   // 已确认为非系统类的类名
        std::unordered_map<std::string, bool> overrideCache;   // 方法重写检查缓存 key: "class::selector"

        // 检查类名是否是系统类（带缓存）
        inline bool isSystemClass(const std::string& className) {
            // 先查缓存
            if (systemClassCache.count(className)) return true;
            if (nonSystemClassCache.count(className)) return false;

            // 未命中缓存，执行检查
            bool result = isSystemSymbolName(className);
            if (result) {
                systemClassCache.insert(className);
            } else {
                nonSystemClassCache.insert(className);
            }
            return result;
        }

        // 检查方法是否重写系统方法（带缓存）
        inline bool isOverridingSystemMethod(
            const std::string& className,
            const std::string& selector,
            unsigned paramCount,
            const ObjCInterfaceDecl* classDecl)
        {
            // 构建缓存键
            std::string cacheKey = className + "::" + selector;
            auto it = overrideCache.find(cacheKey);
            if (it != overrideCache.end()) {
                return it->second;
            }

            // 未命中缓存，执行检查（带提前终止优化）
            bool result = false;
            bool foundSystemParent = false;
            const ObjCInterfaceDecl* superDecl = classDecl->getSuperClass();

            while (superDecl) {
                std::string superClassName = superDecl->getNameAsString();

                if (isSystemClass(superClassName)) {
                    foundSystemParent = true;

                    // 在系统父类中查找同名方法
                    for (const auto* superMethod : superDecl->methods()) {
                        if (!superMethod) continue;
                        if (superMethod->getSelector().getAsString() == selector &&
                            superMethod->param_size() == paramCount) {
                            result = true;
                            break;
                        }
                    }

                    if (result) break;  // 找到了，无需继续
                } else if (foundSystemParent) {
                    // 已经从系统类回到了自定义类，无需继续
                    break;
                }

                superDecl = superDecl->getSuperClass();
            }

            // 缓存结果
            overrideCache[cacheKey] = result;
            return result;
        }
    };
}

void MethodNameObfuscationStrategy::replaceAllInOnePass(ASTContext& context, ReplacementManager& manager) {
    const SourceManager& SM = context.getSourceManager();

    // 收集所有替换项
    std::vector<ReplacementItem> replacements;

    // 创建缓存实例（在整个 transform 过程中复用）
    CheckCache cache;

    // ============================================================================
    // 统一的 AST 匹配收集器
    // 单次遍历收集所有需要替换的位置
    // ============================================================================
    class UnifiedCollector : public MatchFinder::MatchCallback {
    public:
        // 【重构】函数指针类型：用于检查是否是已知系统方法
        using IsKnownSystemMethodFn = std::function<bool(const std::string&, const std::string&)>;
        using IsSystemSymbolNameFn = std::function<bool(const std::string&)>;

        UnifiedCollector(
            const SourceManager& sm,
            const std::map<std::string, MethodInfo>& methodsToObfuscate,
            const std::unordered_map<std::string, std::string>& globalMethodObfuscationCache,
            const std::map<std::string, std::map<std::string, std::string>>& methodParameters,
            const std::map<std::string, std::map<std::string, BlockInfo>>& blockVariables,
            std::vector<ReplacementItem>& replacements,
            CheckCache& cache,
            IsKnownSystemMethodFn isKnownSystemMethodFn,
            IsSystemSymbolNameFn isSystemSymbolNameFn)
            : SM_(sm)
            , methodsToObfuscate_(methodsToObfuscate)
            , globalMethodObfuscationCache_(globalMethodObfuscationCache)
            , methodParameters_(methodParameters)
            , blockVariables_(blockVariables)
            , replacements_(replacements)
            , cache_(cache)
            , isKnownSystemMethodFn_(isKnownSystemMethodFn)
            , isSystemSymbolNameFn_(isSystemSymbolNameFn) {}

    private:
        const SourceManager& SM_;
        const std::map<std::string, MethodInfo>& methodsToObfuscate_;
        const std::unordered_map<std::string, std::string>& globalMethodObfuscationCache_;
        const std::map<std::string, std::map<std::string, std::string>>& methodParameters_;
        const std::map<std::string, std::map<std::string, BlockInfo>>& blockVariables_;
        std::vector<ReplacementItem>& replacements_;
        CheckCache& cache_;  // 缓存引用
        std::string currentMethodSelector_;  // 当前正在处理的方法 selector
        IsKnownSystemMethodFn isKnownSystemMethodFn_;  // 【重构】已知系统方法检查函数
        IsSystemSymbolNameFn isSystemSymbolNameFn_;    // 【重构】系统符号名称检查函数

        // 【重构】辅助方法：检查接口中是否有某个方法
        bool hasMethodInInterface(const ObjCInterfaceDecl* interfaceDecl,
                                  const std::string& selector) const {
            if (!interfaceDecl) return false;

            for (const auto* method : interfaceDecl->methods()) {
                if (method && method->getSelector().getAsString() == selector) {
                    return true;
                }
            }

            for (const auto* method : interfaceDecl->class_methods()) {
                if (method && method->getSelector().getAsString() == selector) {
                    return true;
                }
            }

            return false;
        }

        // 检查是否应该跳过系统方法调用
        bool shouldSkipSystemMethodCall(const ObjCMessageExpr* msgExpr,
                                       const std::string& selector,
                                       const QualType& receiverType) const {
            if (receiverType.isNull()) return false;

            // 处理链式属性访问（如 obj.prop.method）
            QualType actualReceiverType = receiverType;
            const Expr* receiverExpr = msgExpr->getInstanceReceiver();
            if (receiverExpr) {
                if (auto memberExpr = dyn_cast<ObjCIvarRefExpr>(receiverExpr)) {
                    actualReceiverType = memberExpr->getDecl()->getType();
                } else if (auto propExpr = dyn_cast<ObjCPropertyRefExpr>(receiverExpr)) {
                    actualReceiverType = propExpr->getType();
                }
            }

            // 检查接收者类型
            if (const auto* ptrType = actualReceiverType->getAs<ObjCObjectPointerType>()) {
                if (const ObjCInterfaceDecl* interfaceDecl = ptrType->getInterfaceDecl()) {
                    std::string receiverClassName = interfaceDecl->getNameAsString();

                    // 系统类检查
                    if (cache_.isSystemClass(receiverClassName)) {
                        if (hasMethodInInterface(interfaceDecl, selector)) {
                            return true;
                        }
                        if (isKnownSystemMethodFn_ && isKnownSystemMethodFn_(receiverClassName, selector)) {
                            return true;
                        }
                        return false;
                    }

                    // 检查继承链
                    const ObjCInterfaceDecl* currentDecl = interfaceDecl;
                    while (currentDecl) {
                        const ObjCInterfaceDecl* superDecl = currentDecl->getSuperClass();
                        if (!superDecl) break;

                        std::string superClassName = superDecl->getNameAsString();
                        if (cache_.isSystemClass(superClassName)) {
                            if (hasMethodInInterface(superDecl, selector)) {
                                return true;
                            }
                            if (isKnownSystemMethodFn_ && isKnownSystemMethodFn_(superClassName, selector)) {
                                return true;
                            }
                            break;
                        }

                        currentDecl = superDecl;
                    }
                } else {
                    // interfaceDecl 为 null 时（如 id 类型或协议类型）
                    std::string typeStr = actualReceiverType.getAsString();
                    if (!typeStr.empty() && typeStr[typeStr.length() - 1] == '*') {
                        typeStr = typeStr.substr(0, typeStr.length() - 1);
                        typeStr.erase(typeStr.find_last_not_of(" \t") + 1);
                    }

                    if (cache_.isSystemClass(typeStr)) {
                        if (isKnownSystemMethodFn_ && isKnownSystemMethodFn_(typeStr, selector)) {
                            return true;
                        }
                        if (isSystemSymbolNameFn_ && isSystemSymbolNameFn_(typeStr)) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

    public:
        // 处理方法声明（收集方法名和参数名替换）
        void handleMethodDecl(const ObjCMethodDecl* method) {
            if (!method) return;

            // 【重要修复】跳过隐式方法声明（属性的 getter/setter 方法）
            // 但如果方法在主文件中有实现，则需要处理（@property 的 getter 可能在 .m 中实现）
            if (method->isImplicit()) {
                // 检查是否有方法体在主文件中
                if (!method->hasBody() || !SM_.isInMainFile(method->getBody()->getBeginLoc())) {
                    return;  // 没有方法体或方法体不在主文件，跳过
                }
                // 有方法体在主文件中，继续处理
            }

            std::string selector = method->getSelector().getAsString();

            // 【优化】检查是否重写了系统类的方法（使用缓存）
            const ObjCInterfaceDecl* classDecl = method->getClassInterface();
            if (classDecl) {
                std::string className = classDecl->getNameAsString();
                unsigned paramCount = method->param_size();

                if (cache_.isOverridingSystemMethod(className, selector, paramCount, classDecl)) {
                    return;  // 重写了系统方法，不混淆
                }
            }

            // 1. 收集方法名替换
            auto it = methodsToObfuscate_.find(selector);
            if (it != methodsToObfuscate_.end() && !it->second.obfuscatedName.empty()) {
                unsigned numSelectorPieces = method->getNumSelectorLocs();

                // 检查是否有多参数方法的片段混淆名
                const auto& pieceObfuscatedNames = it->second.pieceObfuscatedNames;

                for (unsigned i = 0; i < numSelectorPieces; ++i) {
                    SourceLocation pieceLoc = method->getSelectorLoc(i);
                    if (!pieceLoc.isValid()) {
                        continue;
                    }

                    // 【重要修复】区分方法声明和方法定义
                    // 对于在头文件中声明但在主文件中实现的方法：
                    // - getSelectorLoc() 返回的是头文件中声明的位置
                    // - 我们需要检查方法体是否在主文件中，如果是则应该混淆
                    bool isInMainFile = SM_.isInMainFile(pieceLoc);

                    // 如果 selector 位置不在主文件，检查是否为方法定义（有方法体）
                    if (!isInMainFile && method->hasBody()) {
                        const Stmt* body = method->getBody();
                        if (body && SM_.isInMainFile(body->getBeginLoc())) {
                            // 方法体在主文件中，这是方法定义而非仅仅是声明
                            // 需要在主文件中找到方法名的位置
                            // 尝试从方法定义获取位置
                            SourceLocation defLoc = method->getSelectorStartLoc();
                            if (defLoc.isValid() && SM_.isInMainFile(defLoc)) {
                                pieceLoc = defLoc;
                                isInMainFile = true;
                            } else {
                                // 备选方案：使用方法体位置并向前搜索方法名
                                // 这种情况下，我们仍需处理，但需要特殊处理位置
                                isInMainFile = true;
                                // 保留原始 pieceLoc，但在后面使用时特殊处理
                            }
                        }
                    }

                    if (!isInMainFile) {
                        continue;
                    }

                    // 【重要】当方法体在主文件但 selector 位置在头文件时
                    // 需要在主文件中搜索方法名的实际位置
                    bool needsSearchInMainFile = false;
                    if (!SM_.isInMainFile(pieceLoc) && method->hasBody()) {
                        const Stmt* body = method->getBody();
                        if (body && SM_.isInMainFile(body->getBeginLoc())) {
                            needsSearchInMainFile = true;
                        }
                    }

                    if (needsSearchInMainFile) {
                        // 【重要修复】使用更简单和可靠的方法来找到方法实现中的方法名
                        // 直接从方法体向前搜索，避免复杂的位置计算
                        const Stmt* body = method->getBody();
                        if (!body) {
                            continue;  // 没有方法体，跳过
                        }

                        SourceLocation bodyStart = body->getBeginLoc();
                        if (!bodyStart.isValid()) {
                            continue;
                        }

                        // 获取方法名（第一个 selector 片段）
                        std::string methodName;
                        if (i == 0) {
                            methodName = selector;
                            size_t colonPos = methodName.find(':');
                            if (colonPos != std::string::npos) {
                                methodName = methodName.substr(0, colonPos);
                            }
                        } else {
                            // 后续片段使用原来的逻辑
                            continue;
                        }

                        if (methodName.empty()) {
                            continue;
                        }

                        // 向前搜索方法名：从方法体前最多 200 个字符中搜索
                        bool invalid = false;
                        const char* searchStart = SM_.getCharacterData(bodyStart.getLocWithOffset(-200), &invalid);
                        if (invalid) {
                            continue;  // 无法获取搜索范围，跳过
                        }
                        const char* searchEnd = SM_.getCharacterData(bodyStart, &invalid);
                        if (invalid || !searchStart || !searchEnd || searchStart >= searchEnd) {
                            continue;
                        }

                        std::string searchContext(searchStart, searchEnd - searchStart);
                        size_t foundPos = searchContext.find(methodName);
                        if (foundPos == std::string::npos) {
                            continue;  // 未找到方法名，跳过
                        }

                        // 计算方法名的实际位置
                        pieceLoc = bodyStart.getLocWithOffset(-200 + static_cast<int>(foundPos));
                        if (!pieceLoc.isValid() || !SM_.isInMainFile(pieceLoc)) {
                            continue;  // 位置无效，跳过
                        }
                    }

                    // 获取源代码中的selector文本（包括冒号）
                    // 【重要修复】首先尝试 getCharRange，如果返回空则使用 getTokenRange
                    // getCharRange 返回精确的字符范围，但可能在某些情况下返回空
                    // getTokenRange 返回完整的token，可能包含额外字符需要过滤
                    StringRef pieceText = Lexer::getSourceText(
                        CharSourceRange::getCharRange(pieceLoc),
                        SM_, LangOptions()
                    );
                    std::string pieceStr = pieceText.str();

                    // 如果 getCharRange 返回空，尝试使用 getTokenRange
                    bool usedTokenRange = false;
                    if (pieceStr.empty()) {
                        pieceText = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(pieceLoc),
                            SM_, LangOptions()
                        );
                        pieceStr = pieceText.str();
                        usedTokenRange = true;
                    }

                    // 【重要】手动提取有效的标识符部分
                    // 过滤掉非标识符字符（如空格、{、;等）
                    // 注意：我们需要保留原始文本长度用于替换，但只使用有效标识符部分作为originalPiece
                    size_t validEnd = 0;
                    for (size_t j = 0; j < pieceStr.size(); ++j) {
                        char c = pieceStr[j];
                        if (c == ':' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') || c == '_') {
                            validEnd = j + 1;
                        } else {
                            break;  // 遇到无效字符就停止
                        }
                    }

                    // 提取有效的标识符部分作为filteredPiece
                    std::string filteredPiece = pieceStr.substr(0, validEnd);
                    if (filteredPiece.empty()) continue;

                    // 【修复】使用原始文本长度作为替换长度，而不是过滤后的长度
                    // 如果使用了getTokenRange，需要检查 filteredPiece 是否包含冒号
                    // 如果 filteredPiece 不包含冒号但 pieceStr 包含冒号，说明冒号被过滤掉了
                    // 这种情况下应该使用 validEnd 作为替换长度
                    unsigned replaceLength;
                    if (usedTokenRange) {
                        // 检查 pieceStr 是否包含冒号但 filteredPiece 不包含
                        bool pieceStrHasColon = (pieceStr.find(':') != std::string::npos);
                        bool filteredHasColon = (filteredPiece.find(':') != std::string::npos);
                        if (pieceStrHasColon && !filteredHasColon) {
                            // 冒号被过滤掉了，使用 filteredPiece 的长度（validEnd）
                            replaceLength = static_cast<unsigned>(validEnd);
                        } else {
                            // 使用 pieceStr 的长度
                            replaceLength = static_cast<unsigned>(pieceStr.length());
                        }
                    } else {
                        replaceLength = static_cast<unsigned>(validEnd);
                    }

                    // 如果获取的文本不包含冒号，检查下一个字符是否是冒号
                    bool hasColon = !filteredPiece.empty() && filteredPiece.back() == ':';
                    if (!hasColon && !filteredPiece.empty()) {
                        bool invalid = false;
                        const char* charData = SM_.getCharacterData(pieceLoc.getLocWithOffset(filteredPiece.length()), &invalid);
                        if (!invalid && charData && *charData == ':') {
                            hasColon = true;
                            filteredPiece += ':';
                            // 【重要修复】当检测到下一个字符是冒号时，需要更新 replaceLength
                            // 否则会保留原始冒号，导致双冒号
                            replaceLength++;
                        }
                    }

                    std::string originalPiece = filteredPiece;

                    // 获取该片段的混淆名
                    std::string pieceObfuscated;
                    if (!pieceObfuscatedNames.empty() && i < pieceObfuscatedNames.size()) {
                        // 多参数方法：使用预先生成的片段混淆名
                        pieceObfuscated = pieceObfuscatedNames[i];
                    } else {
                        // 单参数方法：直接使用 obfuscatedName
                        pieceObfuscated = it->second.obfuscatedName;
                    }

                    // 如果源代码包含冒号，添加冒号到混淆名
                    // 【修复】检查混淆名是否已经包含冒号（避免重复添加）
                    if (hasColon && (pieceObfuscated.empty() || pieceObfuscated.back() != ':')) {
                        pieceObfuscated += ":";
                    }

                    // 【重要修复】检查方法名后是否紧跟 {（方法实现）
                    // 如果是，确保替换长度正确，不包含 { 字符
                    if (!needsSearchInMainFile && i == 0 && !pieceObfuscated.empty()) {
                        bool invalid = false;
                        const char* nextChar = SM_.getCharacterData(pieceLoc.getLocWithOffset(replaceLength), &invalid);
                        if (!invalid && nextChar && *nextChar == ' ') {
                            // 检查空格后是否是 {
                            const char* afterSpace = SM_.getCharacterData(pieceLoc.getLocWithOffset(replaceLength + 1), &invalid);
                            if (!invalid && afterSpace && *afterSpace == '{') {
                                // 方法实现，方法名后是空格 + {
                                // 确保 replaceLength 只包括方法名，不包括空格和 {
                                // 这里不需要做任何修改，因为 replaceLength 应该已经正确
                            }
                        }
                    }

                    replacements_.push_back({
                        pieceLoc, originalPiece, pieceObfuscated,
                        ReplacementType::MethodName, replaceLength
                    });
                }
            }

            // 2. 收集参数名替换
            auto paramIt = methodParameters_.find(selector);
            if (paramIt != methodParameters_.end() && !paramIt->second.empty()) {
                for (unsigned i = 0; i < method->param_size(); ++i) {
                    const ParmVarDecl* param = method->getParamDecl(i);
                    if (!param) continue;

                    std::string paramName = param->getNameAsString();
                    if (paramName.empty()) continue;

                    auto nameIt = paramIt->second.find(paramName);
                    if (nameIt != paramIt->second.end() && !nameIt->second.empty()) {
                        SourceLocation loc = param->getLocation();
                        if (loc.isValid() && SM_.isInMainFile(loc)) {
                            replacements_.push_back({
                                loc, paramName, nameIt->second,
                                ReplacementType::ParameterName, static_cast<unsigned>(paramName.length())
                            });
                        }
                    }
                }
            }
        }

        // 处理 Block 变量声明
        void handleBlockVariable(const VarDecl* varDecl) {
            if (!varDecl) return;

            std::string varName = varDecl->getNameAsString();
            if (varName.empty()) return;

            // 检查是否是 Block 类型
            QualType varType = varDecl->getType();
            if (!varType->getAs<BlockPointerType>()) return;

            // 在所有方法的 Block 映射中查找
            for (const auto& [methodSelector, blockMap] : blockVariables_) {
                auto it = blockMap.find(varName);
                if (it != blockMap.end() && !it->second.obfuscatedName.empty()) {
                    SourceLocation loc = varDecl->getLocation();
                    if (loc.isValid() && SM_.isInMainFile(loc)) {
                        replacements_.push_back({
                            loc, varName, it->second.obfuscatedName,
                            ReplacementType::BlockVariable, static_cast<unsigned>(varName.length())
                        });
                    }
                    break;
                }
            }
        }

        // 处理消息表达式
        void handleMessageExpr(const ObjCMessageExpr* msgExpr) {
            if (!msgExpr) return;

            std::string selector = msgExpr->getSelector().getAsString();

            // 首先在 methodsToObfuscate_ 中查找
            auto it = methodsToObfuscate_.find(selector);
            bool found = false;
            std::string obfuscatedName;
            const std::vector<std::string>* pieceObfuscatedNamesPtr = nullptr;

            if (it != methodsToObfuscate_.end() && !it->second.obfuscatedName.empty()) {
                found = true;
                obfuscatedName = it->second.obfuscatedName;
                pieceObfuscatedNamesPtr = &it->second.pieceObfuscatedNames;
            } else {
                // 检查 globalMethodObfuscationCache_（处理属性 getter/setter 方法调用）
                auto cacheIt = globalMethodObfuscationCache_.find(selector);
                if (cacheIt != globalMethodObfuscationCache_.end() && !cacheIt->second.empty()) {
                    found = true;
                    obfuscatedName = cacheIt->second;
                }
            }

            if (!found) return;

            // 使用辅助方法检查是否应该跳过系统方法调用
            QualType receiverType = msgExpr->getReceiverType();
            if (shouldSkipSystemMethodCall(msgExpr, selector, receiverType)) {
                return;  // 系统类方法，跳过混淆
            }

            unsigned numSelectorPieces = msgExpr->getNumSelectorLocs();

            // 检查是否有多参数方法的片段混淆名
            std::vector<std::string> emptyPieces;
            const auto& pieceObfuscatedNames = pieceObfuscatedNamesPtr ? *pieceObfuscatedNamesPtr : emptyPieces;

            for (unsigned i = 0; i < numSelectorPieces; ++i) {
                SourceLocation pieceLoc = msgExpr->getSelectorLoc(i);
                if (!pieceLoc.isValid() || !SM_.isInMainFile(pieceLoc)) {
                    continue;
                }

                // 【关键修复】使用 getSpellingLoc 获取实际的源代码位置
                // 如果 selector 在宏中，getSelectorLoc 可能返回宏展开位置
                SourceLocation spellingLoc = SM_.getSpellingLoc(pieceLoc);
                if (spellingLoc.isValid() && !SM_.isInSystemHeader(spellingLoc)) {
                    pieceLoc = spellingLoc;
                }

                // 获取源代码中的selector文本（包括冒号）
                StringRef pieceText = Lexer::getSourceText(
                    CharSourceRange::getTokenRange(pieceLoc),
                    SM_, LangOptions()
                );
                std::string pieceStr = pieceText.str();

                // 如果获取的文本不包含冒号，检查下一个字符是否是冒号
                bool hasColon = !pieceStr.empty() && pieceStr.back() == ':';
                if (!hasColon && !pieceStr.empty()) {
                    bool invalid = false;
                    const char* charData = SM_.getCharacterData(pieceLoc.getLocWithOffset(pieceStr.length()), &invalid);
                    if (!invalid && charData && *charData == ':') {
                        hasColon = true;
                        pieceStr += ':';
                    }
                }

                std::string originalPiece = pieceStr;
                if (originalPiece.empty()) continue;

                // 获取该片段的混淆名
                std::string pieceObfuscated;
                if (!pieceObfuscatedNames.empty() && i < pieceObfuscatedNames.size()) {
                    // 多参数方法：使用预先生成的片段混淆名
                    pieceObfuscated = pieceObfuscatedNames[i];
                } else {
                    // 单参数方法：直接使用 obfuscatedName
                    pieceObfuscated = obfuscatedName;
                }

                // 如果源代码包含冒号，添加冒号到混淆名
                // 【修复】检查混淆名是否已经包含冒号（避免重复添加）
                if (hasColon && (pieceObfuscated.empty() || pieceObfuscated.back() != ':')) {
                    pieceObfuscated += ":";
                }

                replacements_.push_back({
                    pieceLoc, originalPiece, pieceObfuscated,
                    ReplacementType::MessageExpr, static_cast<unsigned>(originalPiece.length())
                });
            }

            // 【修复】处理 numSelectorPieces == 0 的情况（点语法调用）
            // 对于 operation.isCancelled 这种点语法，getNumSelectorLocs() 可能返回 0
            if (numSelectorPieces == 0 && !obfuscatedName.empty()) {
                // 尝试在源代码中搜索 selector 名称
                // 使用消息表达式的位置作为起点
                SourceLocation exprLoc = msgExpr->getExprLoc();
                if (exprLoc.isValid() && SM_.isInMainFile(exprLoc)) {
                    SourceLocation spellingLoc = SM_.getSpellingLoc(exprLoc);
                    if (spellingLoc.isValid() && !SM_.isInSystemHeader(spellingLoc)) {
                        // 获取源代码中的消息表达式文本
                        StringRef exprText = Lexer::getSourceText(
                            CharSourceRange::getTokenRange(spellingLoc),
                            SM_, LangOptions()
                        );
                        std::string exprStr = exprText.str();

                        // 在表达式文本中搜索 selector 名称
                        // 对于 "operation.isCancelled"，搜索 ".isCancelled"
                        // 对于 "[self isCancelled]"，搜索 "isCancelled"
                        size_t foundPos = std::string::npos;
                        std::string searchPattern = "." + selector;
                        foundPos = exprStr.find(searchPattern);

                        // 如果没找到点语法形式，尝试方括号语法
                        if (foundPos == std::string::npos) {
                            foundPos = exprStr.find(selector);
                        }

                        if (foundPos != std::string::npos) {
                            // 计算 selector 在源代码中的实际位置
                            SourceLocation selectorLoc = spellingLoc.getLocWithOffset(foundPos);
                            // 如果是点语法，跳过点号
                            if (exprStr[foundPos] == '.') {
                                selectorLoc = selectorLoc.getLocWithOffset(1);
                            }

                            // 添加替换
                            replacements_.push_back({
                                selectorLoc, selector, obfuscatedName,
                                ReplacementType::MessageExpr, static_cast<unsigned>(selector.length())
                            });
                        }
                    }
                }
            }
        }

        // 【修复】处理属性引用表达式（operation.prop 形式的调用）
        void handlePropertyRefExpr(const ObjCPropertyRefExpr* propRef) {
            if (!propRef) return;

            // 检查是否是显式属性引用
            if (!propRef->isExplicitProperty()) {
                // 隐式属性（没有 @property 声明），跳过
                return;
            }

            // 获取属性声明
            const ObjCPropertyDecl* prop = propRef->getExplicitProperty();
            if (!prop) return;

            std::string propName = prop->getNameAsString();
            std::string getterName = prop->getGetterName().getAsString();

            // 检查是否有自定义 getter（例如 isCancelled 而不是 cancelled）
            // 如果 getter 名称不是 "get" + 属性名（首字母大写），则认为是自定义 getter
            bool hasCustomGetter = (getterName != "get" + propName &&
                                    getterName != propName);

            // 使用 getter 名称作为要查找的选择器
            std::string selectorToLookup = getterName;

            // 在 globalMethodObfuscationCache_ 中查找 getter 的混淆名
            auto cacheIt = globalMethodObfuscationCache_.find(selectorToLookup);
            if (cacheIt == globalMethodObfuscationCache_.end() || cacheIt->second.empty()) {
                // 没有找到混淆映射，不需要替换
                return;
            }

            std::string obfuscatedName = cacheIt->second;

            // 获取属性引用的位置
            SourceLocation loc = propRef->getLocation();
            if (!loc.isValid() || !SM_.isInMainFile(loc)) {
                return;
            }

            // 使用拼写位置
            SourceLocation spellingLoc = SM_.getSpellingLoc(loc);
            if (!spellingLoc.isValid() || SM_.isInSystemHeader(spellingLoc)) {
                return;
            }

            // 获取源代码中的属性名称文本
            StringRef propText = Lexer::getSourceText(
                CharSourceRange::getTokenRange(spellingLoc),
                SM_, LangOptions()
            );
            std::string propStr = propText.str();

            // 检查是否是 getter 形式的调用（例如 isCancelled）
            // 如果源代码中的属性名与 getter 名称匹配，则替换
            if (propStr == selectorToLookup) {
                // 添加替换
                replacements_.push_back({
                    spellingLoc, propStr, obfuscatedName,
                    ReplacementType::MessageExpr, static_cast<unsigned>(propStr.length())
                });
            }
        }

        // 处理 @selector 表达式
        void handleSelectorExpr(const Stmt* stmt) {
            if (!stmt) return;

            // 尝试转换为 ObjCSelectorExpr
            const auto* selectorExpr = dyn_cast<ObjCSelectorExpr>(stmt);
            if (!selectorExpr) {
                // 如果不是直接的 selector 表达式，尝试遍历子节点
                for (const auto* child : stmt->children()) {
                    if (const auto* selExpr = dyn_cast_or_null<ObjCSelectorExpr>(child)) {
                        selectorExpr = selExpr;
                        break;
                    }
                }
            }

            if (!selectorExpr) return;

            std::string selector = selectorExpr->getSelector().getAsString();

            // 首先在 methodsToObfuscate_ 中查找
            auto it = methodsToObfuscate_.find(selector);
            bool found = false;
            std::string obfuscatedSelector;

            if (it != methodsToObfuscate_.end() && !it->second.obfuscatedName.empty()) {
                found = true;
                // 计算selector片段数量（通过统计冒号数量）
                unsigned numColons = 0;
                for (char c : selector) {
                    if (c == ':') numColons++;
                }
                unsigned numPieces = (numColons > 0) ? numColons : 1;

                // 检查是否有多参数方法的片段混淆名
                const auto& pieceObfuscatedNames = it->second.pieceObfuscatedNames;

                // 构建混淆后的selector字符串
                if (numColons == 0 || pieceObfuscatedNames.empty()) {
                    // 单参数方法，直接使用 obfuscatedName
                    obfuscatedSelector = it->second.obfuscatedName;
                    // 【修复】添加尾随冒号（防止空字符串崩溃）
                    obfuscatedSelector = ensureTrailingColon(obfuscatedSelector, numColons);
                } else {
                    // 多参数方法，使用预先生成的片段混淆名
                    for (unsigned i = 0; i < numPieces && i < pieceObfuscatedNames.size(); ++i) {
                        obfuscatedSelector += pieceObfuscatedNames[i] + ":";
                    }
                }
            } else {
                // 检查 globalMethodObfuscationCache_（处理属性 getter/setter 的 @selector）
                auto cacheIt = globalMethodObfuscationCache_.find(selector);
                if (cacheIt != globalMethodObfuscationCache_.end() && !cacheIt->second.empty()) {
                    found = true;
                    obfuscatedSelector = cacheIt->second;
                    // 【修复】计算原 selector 的冒号数量并添加尾随冒号
                    unsigned cacheNumColons = 0;
                    for (char c : selector) {
                        if (c == ':') cacheNumColons++;
                    }
                    obfuscatedSelector = ensureTrailingColon(obfuscatedSelector, cacheNumColons);
                }
            }

            if (!found) return;

            SourceLocation atLoc = selectorExpr->getAtLoc();
            if (atLoc.isValid() && SM_.isInMainFile(atLoc)) {
                // @selector(methodName) - 跳过 "@selector(" 前缀
                unsigned offset = 10;  // "@selector(" 的长度
                SourceLocation loc = atLoc.getLocWithOffset(offset);
                replacements_.push_back({
                    loc, selector, obfuscatedSelector,
                    ReplacementType::SelectorExpr, static_cast<unsigned>(selector.length())
                });
            }
        }

        void run(const MatchFinder::MatchResult& result) override {
            // 处理方法声明
            if (const auto* method = result.Nodes.getNodeAs<ObjCMethodDecl>("method")) {
                // 记录当前处理的方法 selector
                currentMethodSelector_ = method->getSelector().getAsString();
                handleMethodDecl(method);
                currentMethodSelector_.clear();
            }
            // 处理 Block 变量
            else if (const auto* var = result.Nodes.getNodeAs<VarDecl>("var")) {
                handleBlockVariable(var);
            }
            // 处理消息表达式
            else if (const auto* msg = result.Nodes.getNodeAs<ObjCMessageExpr>("message")) {
                handleMessageExpr(msg);
            }
            // 【修复】处理属性引用表达式（operation.prop 形式的调用）
            // 注意：PropertyNameStrategy 已经处理了属性引用，这里暂时禁用
            // else if (const auto* expr = result.Nodes.getNodeAs<Expr>("propertyRef")) {
            //     // 检查是否是 ObjCPropertyRefExpr
            //     if (const auto* propRef = dyn_cast<ObjCPropertyRefExpr>(expr)) {
            //         handlePropertyRefExpr(propRef);
            //     }
            // }
            // 处理 @selector 表达式
            else if (const auto* expr = result.Nodes.getNodeAs<Expr>("selector")) {
                // 尝试转换为 ObjCSelectorExpr
                if (const auto* selExpr = dyn_cast<ObjCSelectorExpr>(expr)) {
                    std::string selector = selExpr->getSelector().getAsString();

                    // 首先在 methodsToObfuscate_ 中查找
                    auto it = methodsToObfuscate_.find(selector);
                    bool found = false;
                    std::string obfuscatedSelector;

                    if (it != methodsToObfuscate_.end() && !it->second.obfuscatedName.empty()) {
                        found = true;
                        // 生成混淆后的selector字符串（包含冒号）

                        // 计算selector片段数量（通过统计冒号数量）
                        unsigned numColons = 0;
                        for (char c : selector) {
                            if (c == ':') numColons++;
                        }
                        unsigned numPieces = (numColons > 0) ? numColons : 1;

                        // 检查是否有多参数方法的片段混淆名
                        const auto& pieceObfuscatedNames = it->second.pieceObfuscatedNames;

                        // 构建混淆后的selector字符串
                        if (numColons == 0 || pieceObfuscatedNames.empty()) {
                            // 单参数方法，直接使用 obfuscatedName
                            obfuscatedSelector = it->second.obfuscatedName;
                            // 【修复】添加尾随冒号（防止空字符串崩溃）
                            obfuscatedSelector = ensureTrailingColon(obfuscatedSelector, numColons);
                        } else {
                            // 多参数方法，使用预先生成的片段混淆名
                            for (unsigned i = 0; i < numPieces && i < pieceObfuscatedNames.size(); ++i) {
                                obfuscatedSelector += pieceObfuscatedNames[i] + ":";
                            }
                        }
                    } else {
                        // 【新增】检查 globalMethodObfuscationCache_（处理属性 getter/setter 的 @selector）
                        auto cacheIt = globalMethodObfuscationCache_.find(selector);
                        if (cacheIt != globalMethodObfuscationCache_.end() && !cacheIt->second.empty()) {
                            found = true;
                            obfuscatedSelector = cacheIt->second;
                            // 【修复】计算原 selector 的冒号数量并添加尾随冒号
                            unsigned cacheNumColons = 0;
                            for (char c : selector) {
                                if (c == ':') cacheNumColons++;
                            }
                            obfuscatedSelector = ensureTrailingColon(obfuscatedSelector, cacheNumColons);
                        }
                    }

                    if (found) {
                        SourceLocation atLoc = selExpr->getAtLoc();
                        if (atLoc.isValid() && SM_.isInMainFile(atLoc)) {
                            // @selector(methodName) - 跳过 "@selector(" 前缀
                            unsigned offset = 10;  // "@selector(" 的长度
                            SourceLocation loc = atLoc.getLocWithOffset(offset);
                            replacements_.push_back({
                                loc, selector, obfuscatedSelector,
                                ReplacementType::SelectorExpr, static_cast<unsigned>(selector.length())
                            });
                        }
                    }
                }
            }
            // 处理参数引用（方法体内对参数的引用）
            else if (const auto* paramRef = result.Nodes.getNodeAs<DeclRefExpr>("paramRef")) {
                if (const auto* param = dyn_cast<ParmVarDecl>(paramRef->getDecl())) {
                    // 获取参数名
                    std::string paramName = param->getNameAsString();
                    if (paramName.empty()) return;

                    // 查找该参数所属的方法
                    const DeclContext* context = param->getParentFunctionOrMethod();
                    const ObjCMethodDecl* method = dyn_cast_or_null<ObjCMethodDecl>(context);

                    // 【修复】如果在 Block 内部，向上查找父级方法
                    // Block 内的参数引用（如 completedBlock）的 getParentFunctionOrMethod()
                    // 返回的是 Block 本身，需要向上遍历找到外层方法
                    if (!method) {
                        const DeclContext* parent = context;
                        int maxDepth = 10;  // 防止无限循环
                        while (parent && !method && maxDepth-- > 0) {
                            parent = parent->getParent();
                            method = dyn_cast_or_null<ObjCMethodDecl>(parent);
                        }
                    }

                    if (!method) return;

                    std::string selector = method->getSelector().getAsString();

                    // 【重要修复】只替换需要被混淆的方法的参数
                    // 首先检查该具体方法是否在被混淆的方法列表中
                    // 注意：methodsToObfuscate_ 只存储了 selector，不能区分不同类中的同名方法
                    // 所以需要检查该具体方法的方法声明是否会被混淆
                    auto it = methodsToObfuscate_.find(selector);
                    if (it == methodsToObfuscate_.end()) {
                        return;  // 没有任何同名方法被混淆，直接返回
                    }

                    // 【关键修复】检查该具体方法声明是否会被混淆
                    // 通过 handleMethodDecl 中的逻辑来判断：如果方法在主文件中有实现，
                    // 且通过了系统方法重写检查，才会被混淆
                    const ObjCInterfaceDecl* classDecl = method->getClassInterface();
                    if (classDecl) {
                        // 检查是否重写了系统方法
                        if (cache_.isOverridingSystemMethod(classDecl->getNameAsString(),
                                selector, method->param_size(), classDecl)) {
                            return;  // 该方法是系统方法重写，参数不混淆
                        }
                    }

                    // 【额外检查】确保该方法确实在主文件中且会被混淆
                    // 如果方法声明不在主文件，且没有在主文件中的实现，则跳过
                    bool methodWillBeObfuscated = false;
                    if (method->hasBody() && SM_.isInMainFile(method->getBody()->getBeginLoc())) {
                        // 方法在主文件中有实现，会被混淆（除非是系统方法重写，已在上面的检查中排除）
                        methodWillBeObfuscated = true;
                    } else {
                        // 检查方法声明是否在主文件中
                        for (unsigned i = 0; i < method->getNumSelectorLocs(); ++i) {
                            if (SM_.isInMainFile(method->getSelectorLoc(i))) {
                                methodWillBeObfuscated = true;
                                break;
                            }
                        }
                    }

                    if (!methodWillBeObfuscated) {
                        return;  // 该方法不会被混淆，参数也不混淆
                    }

                    // 查找该方法的参数映射
                    auto paramIt = methodParameters_.find(selector);
                    if (paramIt != methodParameters_.end()) {
                        auto nameIt = paramIt->second.find(paramName);
                        if (nameIt != paramIt->second.end() && !nameIt->second.empty()) {
                            SourceLocation loc = paramRef->getLocation();
                            // 【关键修复】使用 getSpellingLoc 处理宏展开中的参数引用
                            // 对于宏内的代码（如 dispatch_main_async_safe），getLocation() 返回宏展开位置
                            // 需要使用 getSpellingLoc() 获取实际源代码位置
                            SourceLocation spellingLoc = SM_.getSpellingLoc(loc);
                            if (spellingLoc.isValid() && SM_.isInMainFile(spellingLoc)) {
                                replacements_.push_back({
                                    spellingLoc, paramName, nameIt->second,
                                    ReplacementType::ParameterName, static_cast<unsigned>(paramName.length())
                                });
                            }
                        }
                    }
                }
            }
        }
    };

    // 创建统一的匹配器
    MatchFinder finder;
    // 创建 UnifiedCollector，传入函数指针用于外部方法检查
    UnifiedCollector collector(
        SM, methodsToObfuscate_, globalMethodObfuscationCache_, methodParameters_, blockVariables_, replacements, cache,
        // isKnownSystemMethodFn
        [](const std::string& className, const std::string& selector) -> bool {
            return isInKnownSystemMethods(className, selector);
        },
        // isSystemSymbolNameFn
        [](const std::string& name) -> bool {
            return isSystemSymbolName(name);
        }
    );

    // 添加所有需要匹配的模式
    finder.addMatcher(objcMethodDecl().bind("method"), &collector);
    finder.addMatcher(varDecl(hasType(blockPointerType())).bind("var"), &collector);
    finder.addMatcher(objcMessageExpr().bind("message"), &collector);
    // 【修复】添加对属性引用表达式的匹配，处理 operation.prop 形式的调用
    // 注意：objcPropertyRefExpr matcher 在某些 Clang 版本中不可用
    // PropertyNameStrategy 已经处理了属性引用，所以这里暂时禁用
    // finder.addMatcher(
    //     expr(hasType(objcObjectPointerType())).bind("propertyRef"),
    //     &collector
    // );
    // @selector 表达式匹配（使用 Expr 匹配）
    finder.addMatcher(
        expr(hasType(asString("SEL"))).bind("selector"),
        &collector
    );
    // 参数引用匹配：匹配方法体内对参数的引用
    finder.addMatcher(
        declRefExpr(to(parmVarDecl())).bind("paramRef"),
        &collector
    );

    // 单次遍历 AST，收集所有替换项
    finder.matchAST(context);

    // 【新架构】将所有替换项添加到 ReplacementManager
    // ReplacementManager 将负责统一排序和应用
    std::unordered_set<unsigned> processedOffsets;
    for (const auto& item : replacements) {
        if (item.loc.isValid() && SM.isInMainFile(item.loc)) {
            unsigned offset = SM.getFileOffset(item.loc);

            // 检查是否已处理过此位置
            if (processedOffsets.find(offset) == processedOffsets.end()) {
                // 添加替换到管理器
                manager.addReplacement(item.loc, item.original, item.replacement, 0, "MethodNameObfuscation");
                processedOffsets.insert(offset);
            }
        }
    }

    LOG_INFO("Method name replacement collection: " + std::to_string(processedOffsets.size()) + " replacements");

    // 处理宏定义值中的标识符替换
    processMacroValueIdentifiers(context, manager, methodsToObfuscate_, globalMethodObfuscationCache_);
}

// ============================================================================
// 处理宏定义值中的标识符替换
// 只替换宏值中的方法名/属性名，不改变宏名本身
// ============================================================================
void MethodNameObfuscationStrategy::processMacroValueIdentifiers(
    ASTContext& context,
    ReplacementManager& manager,
    const std::map<std::string, MethodInfo>& methodsToObfuscate,
    const std::unordered_map<std::string, std::string>& globalMethodObfuscationCache) {

    const SourceManager& SM = context.getSourceManager();
    FileID mainFileID = SM.getMainFileID();

    // 获取主文件内容
    bool invalid = false;
    llvm::StringRef fileContent = SM.getBufferData(mainFileID, &invalid);
    if (invalid) {
        return;
    }

    // 构建所有需要替换的标识符映射
    std::unordered_map<std::string, std::string> identifierMap;

    // 添加方法混淆映射
    for (const auto& [selector, info] : methodsToObfuscate) {
        identifierMap[selector] = info.obfuscatedName;

        // 添加 selector 的各个片段（不带冒号）
        std::string selectorCopy = selector;
        size_t pos = 0;
        while (pos < selectorCopy.length()) {
            size_t colonPos = selectorCopy.find(':', pos);
            if (colonPos == std::string::npos) {
                std::string lastPiece = selectorCopy.substr(pos);
                if (!lastPiece.empty()) {
                    if (pos == 0 && info.pieceObfuscatedNames.size() > 0) {
                        identifierMap[lastPiece] = info.pieceObfuscatedNames[0];
                    }
                }
                break;
            }
            std::string piece = selectorCopy.substr(pos, colonPos - pos);
            size_t pieceIndex = 0;
            size_t tempPos = 0;
            while (tempPos < selector.length()) {
                size_t c = selector.find(':', tempPos);
                if (c == std::string::npos) break;
                if (tempPos == pos) {
                    if (pieceIndex < info.pieceObfuscatedNames.size()) {
                        identifierMap[piece] = info.pieceObfuscatedNames[pieceIndex];
                    }
                    break;
                }
                pieceIndex++;
                tempPos = c + 1;
            }
            pos = colonPos + 1;
        }
    }

    // 添加全局缓存中的方法名
    for (const auto& [name, obfuscatedName] : globalMethodObfuscationCache) {
        identifierMap[name] = obfuscatedName;

        // 处理带冒号的 selector，也添加不带冒号的基础名
        if (!name.empty() && name.back() == ':') {
            std::string nameWithoutColon = name.substr(0, name.length() - 1);
            std::string obfuscatedWithoutColon = obfuscatedName;
            if (!obfuscatedName.empty() && obfuscatedName.back() == ':') {
                obfuscatedWithoutColon = obfuscatedName.substr(0, obfuscatedName.length() - 1);
            }
            identifierMap[nameWithoutColon] = obfuscatedWithoutColon;
        }
    }

    if (identifierMap.empty()) {
        return;
    }

    // 定义宏值替换项结构
    struct MacroValueReplacement {
        size_t pos;
        size_t length;
        std::string original;
        std::string replacement;
    };
    std::vector<MacroValueReplacement> macroReplacements;

    std::string content = fileContent.str();
    size_t pos = 0;

    // 从最长的标识符开始替换（避免部分匹配）
    std::vector<std::string> sortedIdentifiers;
    for (const auto& [id, obs] : identifierMap) {
        sortedIdentifiers.push_back(id);
    }
    std::sort(sortedIdentifiers.begin(), sortedIdentifiers.end(),
              [](const std::string& a, const std::string& b) {
                  return a.length() > b.length();
              });

    // 扫描文件中的 #define
    while (pos < content.length()) {
        size_t definePos = content.find("#define", pos);
        if (definePos == std::string::npos) {
            break;
        }

        // 检查 #define 前面是否有空白（确保是预处理指令）
        bool validDefine = true;
        for (size_t i = definePos; i > 0; --i) {
            char c = content[i - 1];
            if (c == '\n' || c == '\r') {
                break;
            }
            if (c != ' ' && c != '\t') {
                validDefine = false;
                break;
            }
        }

        if (!validDefine) {
            pos = definePos + 7;
            continue;
        }

        // 找到宏名称
        size_t nameStart = definePos + 7;
        while (nameStart < content.length() && (content[nameStart] == ' ' || content[nameStart] == '\t')) {
            nameStart++;
        }

        if (nameStart >= content.length()) {
            break;
        }

        size_t nameEnd = nameStart;
        while (nameEnd < content.length() &&
               (isalnum(content[nameEnd]) || content[nameEnd] == '_')) {
            nameEnd++;
        }

        // 找到宏值的开始位置
        size_t valueStart = nameEnd;
        while (valueStart < content.length() && (content[valueStart] == ' ' || content[valueStart] == '\t')) {
            valueStart++;
        }

        // 找到宏值的结束位置（行尾或反斜杠续行）
        size_t valueEnd = valueStart;
        while (valueEnd < content.length()) {
            if (content[valueEnd] == '\n') {
                if (valueEnd > 0 && content[valueEnd - 1] == '\\') {
                    valueEnd++;
                    continue;
                }
                break;
            }
            valueEnd++;
        }

        std::string macroValue = content.substr(valueStart, valueEnd - valueStart);

        // 在宏值中查找需要替换的标识符
        bool hasReplacement = false;
        std::string processedValue = macroValue;

        for (const auto& identifier : sortedIdentifiers) {
            size_t searchPos = 0;
            while ((searchPos = processedValue.find(identifier, searchPos)) != std::string::npos) {
                // 检查是否是完整的标识符
                bool validStart = (searchPos == 0 ||
                                  !(isalnum(processedValue[searchPos - 1]) ||
                                    processedValue[searchPos - 1] == '_'));
                bool validEnd = (searchPos + identifier.length() >= processedValue.length() ||
                                !(isalnum(processedValue[searchPos + identifier.length()]) ||
                                  processedValue[searchPos + identifier.length()] == '_'));

                if (validStart && validEnd) {
                    processedValue.replace(searchPos, identifier.length(),
                                         identifierMap[identifier]);
                    searchPos += identifierMap[identifier].length();
                    hasReplacement = true;
                } else {
                    searchPos += identifier.length();
                }
            }
        }

        // 如果有替换，添加到替换列表
        if (hasReplacement) {
            macroReplacements.push_back({
                valueStart,
                macroValue.length(),
                macroValue,
                processedValue
            });
        }

        pos = valueEnd;
    }

    // 应用所有替换（从后往前，避免位置偏移）
    for (auto it = macroReplacements.rbegin(); it != macroReplacements.rend(); ++it) {
        SourceLocation loc = SM.getLocForStartOfFile(mainFileID);
        loc = loc.getLocWithOffset(it->pos);

        if (loc.isValid() && SM.isInMainFile(loc)) {
            manager.addReplacement(loc, it->original, it->replacement, 0, "MethodNameObfuscation-MacroValue");
        }
    }

    if (!macroReplacements.empty()) {
        LOG_INFO("Macro value identifier replacement: " +
                 std::to_string(macroReplacements.size()) + " macro values updated");
    }
}


bool MethodNameObfuscationStrategy::validate(clang::ASTContext& context) const {
    // 验证混淆结果
    LOG_INFO("Validating method name obfuscation");
    return true;
}

std::string MethodNameObfuscationStrategy::getFullSelector(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return "";
    }

    Selector sel = methodDecl->getSelector();
    return sel.getAsString();
}

std::string MethodNameObfuscationStrategy::getMethodSignature(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl) {
        return "";
    }

    std::string signature;
    if (methodDecl->isInstanceMethod()) {
        signature += "-";
    } else {
        signature += "+";
    }
    signature += " ";
    signature += getFullSelector(methodDecl);

    return signature;
}

std::string MethodNameObfuscationStrategy::getClassNameFromMethod(const ObjCMethodDecl* methodDecl) const {
    if (!methodDecl || !methodDecl->getClassInterface()) {
        return "";
    }

    return methodDecl->getClassInterface()->getNameAsString();
}

// =============================================================================
// ensureTrailingColon - 确保 selector 有正确数量的尾随冒号
// =============================================================================
//
// 如果原 selector 有冒号但混淆名后没有冒号，则添加冒号
// 防止对空字符串调用 .back() 导致崩溃
//
std::string MethodNameObfuscationStrategy::ensureTrailingColon(
    const std::string& selector,
    unsigned expectedColons) {

    // 如果没有期望冒号数量，或 selector 为空，直接返回
    if (expectedColons == 0 || selector.empty()) {
        return selector;
    }

    // 如果已经以冒号结尾，直接返回
    if (selector.back() == ':') {
        return selector;
    }

    // 添加尾随冒号
    return selector + ":";
}

void MethodNameObfuscationStrategy::finalizeStandaloneMethods() {
    // 系统协议方法黑名单（这些方法不应被混淆）
    static const std::unordered_set<std::string> SYSTEM_PROTOCOL_METHODS_BLACKLIST = {
        "supportsSecureCoding",      // NSSecureCoding
        "encodeWithCoder:",          // NSCoding
        "initWithCoder:",            // NSCoding
        "copyWithZone:",             // NSCopying
        "mutableCopyWithZone:",      // NSMutableCopying
    };

    // 处理未配对的 setter（只有 setter，没有 getter）
    for (const auto& [selector, info] : unpairedSetters_) {
        // 跳过系统协议方法
        if (SYSTEM_PROTOCOL_METHODS_BLACKLIST.find(selector) != SYSTEM_PROTOCOL_METHODS_BLACKLIST.end()) {
            continue;
        }

        std::string baseName = extractFromSetter(selector);
        if (baseName.empty()) continue;

        // 生成混淆后的属性名
        std::string obfuscatedBase;
        auto cacheIt = globalMethodObfuscationCache_.find(baseName);
        if (cacheIt != globalMethodObfuscationCache_.end()) {
            // 已有同名属性的混淆名，复用
            obfuscatedBase = cacheIt->second;
        } else {
            // 首次遇到此属性名，生成新混淆名并缓存
            obfuscatedBase = nameGenerator_->generate(baseName, "propertyName");
            globalMethodObfuscationCache_[baseName] = obfuscatedBase;
        }

        // setter: setXxx: -> setObfuscated:
        std::string obfuscatedSetter = "set" + capitalize(obfuscatedBase) + ":";

        // 添加到全局缓存
        globalMethodObfuscationCache_[selector] = obfuscatedSetter;

        // 添加到待混淆列表
        MethodInfo setterInfo;
        setterInfo.originalName = selector;
        setterInfo.obfuscatedName = obfuscatedSetter;
        setterInfo.selector = selector;
        setterInfo.isInstanceMethod = info.isInstanceMethod;
        setterInfo.className = info.className;
        methodsToObfuscate_[selector] = setterInfo;

        // 【关键】同步给 PropertyNameStrategy 作为"虚拟属性"（只写属性）
        if (symbolTable_) {
            PropertyMethodMapping mapping;
            mapping.originalPropertyName = baseName;
            mapping.obfuscatedPropertyName = obfuscatedBase;
            mapping.originalSetterName = selector;
            mapping.obfuscatedSetterName = obfuscatedSetter;
            mapping.originalGetterName = "";  // 空，表示没有 getter
            mapping.obfuscatedGetterName = "";
            mapping.isBoolean = false;
            symbolTable_->addPropertyMapping(mapping);
        }

        LOG_INFO("Registered standalone setter: " + selector + " -> " + obfuscatedSetter);
    }

    // 处理未配对的 getter（只有 getter，没有 setter）
    for (const auto& [selector, info] : unpairedGetters_) {
        // 跳过系统协议方法
        if (SYSTEM_PROTOCOL_METHODS_BLACKLIST.find(selector) != SYSTEM_PROTOCOL_METHODS_BLACKLIST.end()) {
            continue;
        }

        bool isBoolean = false;
        std::string baseName = extractFromGetter(selector, isBoolean);
        if (baseName.empty()) continue;

        // 生成混淆后的属性名
        std::string obfuscatedBase;
        auto cacheIt = globalMethodObfuscationCache_.find(baseName);
        if (cacheIt != globalMethodObfuscationCache_.end()) {
            // 已有同名属性的混淆名，复用
            obfuscatedBase = cacheIt->second;
        } else {
            // 首次遇到此属性名，生成新混淆名并缓存
            obfuscatedBase = nameGenerator_->generate(baseName, "propertyName");
            globalMethodObfuscationCache_[baseName] = obfuscatedBase;
        }

        // getter: xxx -> obfuscated, isXxx -> isObfuscated
        std::string obfuscatedGetter;
        if (isBoolean) {
            obfuscatedGetter = "is" + capitalize(obfuscatedBase);
        } else {
            obfuscatedGetter = obfuscatedBase;
        }

        // 添加到全局缓存
        globalMethodObfuscationCache_[selector] = obfuscatedGetter;

        // 添加到待混淆列表
        MethodInfo getterInfo;
        getterInfo.originalName = selector;
        getterInfo.obfuscatedName = obfuscatedGetter;
        getterInfo.selector = selector;
        getterInfo.isInstanceMethod = info.isInstanceMethod;
        getterInfo.className = info.className;
        methodsToObfuscate_[selector] = getterInfo;

        // 【关键】同步给 PropertyNameStrategy 作为"虚拟属性"（只读属性）
        if (symbolTable_) {
                PropertyMethodMapping mapping;
            mapping.originalPropertyName = baseName;
            mapping.obfuscatedPropertyName = obfuscatedBase;
            mapping.originalSetterName = "";  // 空，表示没有 setter
            mapping.obfuscatedSetterName = "";
            mapping.originalGetterName = selector;
            mapping.obfuscatedGetterName = obfuscatedGetter;
            mapping.isBoolean = isBoolean;
            symbolTable_->addPropertyMapping(mapping);

            // 【新增】将 getter 方法添加到 methodSymbols，供 PropertyNameStrategy 查找
            SymbolTable::MethodSymbolInfo getterMethodInfo;
            getterMethodInfo.originalName = selector;
            getterMethodInfo.obfuscatedName = obfuscatedGetter;
            getterMethodInfo.isInstanceMethod = info.isInstanceMethod;
            getterMethodInfo.className = info.className;
            getterMethodInfo.isGetter = true;
            symbolTable_->addMethodSymbol(selector, getterMethodInfo);
        }

        LOG_INFO("Registered standalone getter: " + selector + " -> " + obfuscatedGetter);
    }

    // 清空临时缓存
    unpairedSetters_.clear();
    unpairedGetters_.clear();

    if (!setterGetterPairs_.empty()) {
        LOG_INFO("Total setter/getter pairs paired: " + std::to_string(setterGetterPairs_.size()));
    }
}

// ============================================================================
// 【重构】减少 handleMessageExpr 嵌套的辅助方法实现
// ============================================================================

bool MethodNameObfuscationStrategy::findObfuscatedNameForSelector(
    const std::string& selector,
    std::string& obfuscatedName,
    const std::vector<std::string>** pieceNamesPtr
) const {
    // 首先在 methodsToObfuscate_ 中查找
    auto it = methodsToObfuscate_.find(selector);
    if (it != methodsToObfuscate_.end() && !it->second.obfuscatedName.empty()) {
        obfuscatedName = it->second.obfuscatedName;
        *pieceNamesPtr = &it->second.pieceObfuscatedNames;
        return true;
    }

    // 检查 global方法混淆缓存（处理属性 getter/setter 方法调用）
    auto cacheIt = globalMethodObfuscationCache_.find(selector);
    if (cacheIt != globalMethodObfuscationCache_.end() && !cacheIt->second.empty()) {
        obfuscatedName = cacheIt->second;
        *pieceNamesPtr = nullptr;  // 全局缓存没有 piece 信息
        return true;
    }

    return false;
}

bool MethodNameObfuscationStrategy::hasMethodInInterface(
    const ObjCInterfaceDecl* interfaceDecl,
    const std::string& selector
) const {
    if (!interfaceDecl) return false;

    // 检查实例方法
    for (const auto* method : interfaceDecl->methods()) {
        if (method && method->getSelector().getAsString() == selector) {
            return true;
        }
    }

    // 检查类方法
    for (const auto* method : interfaceDecl->class_methods()) {
        if (method && method->getSelector().getAsString() == selector) {
            return true;
        }
    }

    return false;
}

bool MethodNameObfuscationStrategy::checkInheritanceChainForSystemMethod(
    const ObjCInterfaceDecl* classDecl,
    const std::string& selector
) const {
    if (!classDecl) return false;

    const ObjCInterfaceDecl* currentDecl = classDecl;
    while (currentDecl) {
        const ObjCInterfaceDecl* superDecl = currentDecl->getSuperClass();
        if (!superDecl) break;

        std::string superClassName = superDecl->getNameAsString();

        // 检查是否是系统类
        bool isSystemClass = false;
        // 简单检查：使用系统前缀
        for (const auto& prefix : SYSTEM_PREFIXES) {
            if (superClassName.find(prefix) == 0) {
                isSystemClass = true;
                break;
            }
        }

        if (isSystemClass) {
            // 检查父类是否有这个方法
            if (hasMethodInInterface(superDecl, selector)) {
                return true;  // 在父类（系统类）中找到了这个方法
            }

            // 备用检查：使用已知系统方法列表
            auto it = KNOWN_SYSTEM_METHODS.find(superClassName);
            if (it != KNOWN_SYSTEM_METHODS.end()) {
                const auto& methods = it->second;
                if (methods.find(selector) != methods.end()) {
                    return true;
                }
            }
        }

        currentDecl = superDecl;
    }

    return false;
}

} // namespace obfuscator
