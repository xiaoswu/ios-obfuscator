/**
 * @file ClassNameStrategy.cpp
 * @brief 类名混淆策略实现
 * 
 * 该策略负责混淆Objective-C类名，包括：
 * 1. @interface 声明中的类名
 * 2. @implementation 声明中的类名
 * 3. 所有使用该类名的地方（类型声明、方法返回类型、变量类型等）
 * 
 * 工作原理：
 * - analyze阶段：扫描AST，识别所有需要混淆的类，记录其位置
 * - transform阶段：使用Rewriter替换所有类名出现的位置
 * - validate阶段：验证混淆是否成功
 */

#include "strategies/ClassNameStrategy.h"
#include "strategies/ObfuscationStrategy.h"
#include "core/ConfigManager.h"
#include "core/SymbolTable.h"
#include "core/NameGenerator.h"
#include "core/Logger.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>
#include <algorithm>

using namespace clang;
using namespace clang::ast_matchers;

namespace obfuscator {

/**
 * @brief 分析阶段：扫描AST，识别需要混淆的类
 * 
 * 使用AST Matcher匹配所有ObjC类声明，然后调用handleInterface处理每个类。
 * 
 * @param context AST上下文，包含整个编译单元的AST信息
 */
void ClassNameObfuscationStrategy::analyze(ASTContext& context) {
    LOG_INFO("Starting class name analysis");
    
    // 添加匹配器：匹配所有ObjC接口声明（除了NSObject）
    // objcInterfaceDecl: 匹配 @interface 声明
    // unless(hasName("NSObject")): 排除NSObject基类
    // bind("interface"): 将匹配结果绑定到"interface"节点
    finder_.addMatcher(
        objcInterfaceDecl(unless(hasName("NSObject"))).bind("interface"),
        this
    );
    
    // 添加匹配器：匹配所有ObjC实现声明
    // objcImplementationDecl: 匹配 @implementation 声明
    finder_.addMatcher(
        objcImplementationDecl().bind("implementation"),
        this
    );
    
    // 执行匹配，对每个匹配结果调用run()方法
    finder_.matchAST(context);
    
    LOG_INFO("Class name analysis completed. Found " + 
             std::to_string(classesToObfuscate_.size()) + " classes to obfuscate");
}

/**
 * @brief MatchFinder回调：处理匹配到的AST节点
 * 
 * 当AST Matcher找到匹配的节点时，会调用此方法。
 * 根据节点类型，调用相应的处理方法。
 * 
 * @param result 匹配结果，包含匹配到的AST节点和源文件信息
 */
void ClassNameObfuscationStrategy::run(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    // 尝试获取接口声明节点
    if (const auto* interface = result.Nodes.getNodeAs<ObjCInterfaceDecl>("interface")) {
        handleInterface(interface);
    } 
    // 尝试获取实现声明节点
    else if (const auto* impl = result.Nodes.getNodeAs<ObjCImplementationDecl>("implementation")) {
        handleImplementation(impl);
    }
}

/**
 * @brief 处理接口声明（@interface）
 * 
 * 检查类是否需要混淆，如果需要则添加到混淆列表。
 * 
 * @param decl ObjC接口声明节点
 */
void ClassNameObfuscationStrategy::handleInterface(const ObjCInterfaceDecl* decl) {
    std::string className = decl->getNameAsString();
    
    // 检查白名单：如果类在白名单中，跳过混淆
    if (isWhitelisted(className, "class")) {
        LOG_INFO("Class " + className + " is whitelisted, skipping");
        return;
    }
    
    // 检查系统类：系统框架的类不混淆
    if (isSystemSymbol(className)) {
        LOG_INFO("Class " + className + " is system class, skipping");
        return;
    }
    
    // 检查第三方SDK：第三方SDK的类不混淆
    if (isThirdPartySDK(className)) {
        LOG_INFO("Class " + className + " is third-party SDK, skipping");
        return;
    }
    
    // 添加到混淆列表
    if (classesToObfuscate_.find(className) == classesToObfuscate_.end()) {
        // 判断是否是公开API（用于映射表生成）
        // 这里简单判断：包含X_SDKManager或methods_X_的类视为公开API
        bool isPublic = className.find("X_SDKManager") != std::string::npos ||
                       className.find("methods_X_") != std::string::npos;
        
        // 添加到符号表，生成混淆名称
        symbolTable_->addSymbol(className, SymbolType::CLASS, isPublic);
        std::string obfuscatedName = symbolTable_->getObfuscatedName(className);
        classesToObfuscate_[className] = obfuscatedName;
        
        LOG_INFO("Will obfuscate class: " + className + " -> " + obfuscatedName);
    }
}

/**
 * @brief 处理实现声明（@implementation）
 * 
 * 实现类名与接口类名相同，通常已在handleInterface中处理。
 * 如果没有接口声明（只有实现），则单独处理。
 * 
 * @param decl ObjC实现声明节点
 */
void ClassNameObfuscationStrategy::handleImplementation(const ObjCImplementationDecl* decl) {
    std::string className = decl->getNameAsString();
    
    // 如果已经在接口中处理过，直接返回
    if (classesToObfuscate_.find(className) != classesToObfuscate_.end()) {
        return;
    }
    
    // 如果没有接口声明，单独处理实现
    // 这种情况很少见，但需要处理
    if (const ObjCInterfaceDecl* interface = decl->getClassInterface()) {
        handleInterface(interface);
    }
}

/**
 * @brief 转换阶段：执行实际的代码替换
 * 
 * 遍历所有需要混淆的类，使用Rewriter替换源代码中的类名。
 * 
 * @param context AST上下文
 * @param rewriter 代码重写器，用于修改源代码
 */
void ClassNameObfuscationStrategy::transform(ASTContext& context, Rewriter& rewriter) {
    LOG_INFO("Starting class name transformation");
    
    // 遍历所有需要混淆的类
    for (const auto& pair : classesToObfuscate_) {
        const std::string& originalName = pair.first;
        const std::string& obfuscatedName = pair.second;
        
        LOG_INFO("Transforming class: " + originalName + " -> " + obfuscatedName);
        
        // 替换类名（包括类型引用）
        replaceClassName(context, rewriter, originalName, obfuscatedName);
        
        // 更新import语句
        updateImports(rewriter, originalName, obfuscatedName);
    }
    
    LOG_INFO("Class name transformation completed");
}

/**
 * @brief 替换源代码中的类名
 * 
 * 这是核心替换逻辑，需要替换所有出现类名的地方：
 * 1. @interface 声明中的类名
 * 2. @implementation 声明中的类名
 * 3. 类型声明（如：MyClass *obj）
 * 4. 方法返回类型
 * 5. 方法参数类型
 * 6. 变量类型
 * 
 * 实现策略：
 * 使用AST遍历，找到所有使用该类名的Type节点，然后替换。
 * 这样可以避免替换字符串字面量中的类名。
 * 
 * @param context AST上下文
 * @param rewriter 代码重写器
 * @param original 原始类名
 * @param obfuscated 混淆后的类名
 */
void ClassNameObfuscationStrategy::replaceClassName(ASTContext& context,
                                                   Rewriter& rewriter,
                                                   const std::string& original,
                                                   const std::string& obfuscated) {
    const SourceManager& SM = rewriter.getSourceMgr();
    
    // 策略1：替换@interface和@implementation声明中的类名
    // 使用AST Matcher找到所有使用该类名的声明
    MatchFinder finder;
    std::vector<SourceLocation> locationsToReplace;
    
    // 定义回调类来收集需要替换的位置
    class ReplacementCollector : public MatchFinder::MatchCallback {
    public:
        ReplacementCollector(const SourceManager& SM, 
                            const std::string& original,
                            std::vector<SourceLocation>& locations)
            : SM_(SM), original_(original), locations_(locations) {}
        
        void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
            // 处理接口声明
            if (const auto* decl = result.Nodes.getNodeAs<ObjCInterfaceDecl>("interface")) {
                SourceLocation loc = decl->getLocation();
                if (loc.isValid() && SM_.isInMainFile(loc)) {
                    if (decl->getNameAsString() == original_) {
                        locations_.push_back(loc);
                    }
                }
            }
            // 处理实现声明
            else if (const auto* decl = result.Nodes.getNodeAs<ObjCImplementationDecl>("implementation")) {
                SourceLocation loc = decl->getLocation();
                if (loc.isValid() && SM_.isInMainFile(loc)) {
                    if (decl->getNameAsString() == original_) {
                        locations_.push_back(loc);
                    }
                }
            }
        }
        
    private:
        const SourceManager& SM_;
        const std::string& original_;
        std::vector<SourceLocation>& locations_;
    };
    
    ReplacementCollector collector(SM, original, locationsToReplace);
    
    // 匹配接口声明
    finder.addMatcher(
        objcInterfaceDecl(hasName(original)).bind("interface"),
        &collector
    );
    
    // 匹配实现声明
    finder.addMatcher(
        objcImplementationDecl(hasName(original)).bind("implementation"),
        &collector
    );
    
    // 执行匹配
    finder.matchAST(context);
    
    // 执行替换（从后往前替换，避免位置偏移问题）
    // 按文件偏移量从大到小排序
    std::sort(locationsToReplace.begin(), locationsToReplace.end(),
              [&SM](const SourceLocation& a, const SourceLocation& b) {
                  return SM.getFileOffset(a) > SM.getFileOffset(b);
              });
    
    // 使用set记录已替换的位置，避免重复替换
    std::set<unsigned> replacedOffsets;
    
    for (const auto& loc : locationsToReplace) {
        if (loc.isValid() && SM.isInMainFile(loc)) {
            unsigned offset = SM.getFileOffset(loc);
            // 检查是否已经替换过这个位置
            if (replacedOffsets.find(offset) == replacedOffsets.end()) {
                rewriter.ReplaceText(loc, original.length(), obfuscated);
                replacedOffsets.insert(offset);
            }
        }
    }
    
    // 策略2：替换类型引用（属性类型、方法参数类型、变量类型等）
    // 使用字符串搜索方法，更简单可靠
    std::vector<std::pair<SourceLocation, unsigned>> typeLocations;
    
    // 获取文件内容进行字符串搜索
    FileID mainFileID = SM.getMainFileID();
    bool invalid = false;
    llvm::StringRef fileContent = SM.getBufferData(mainFileID, &invalid);
    if (!invalid) {
        std::string content = fileContent.str();
        size_t pos = 0;
        
        // 搜索模式：类名后跟特定字符（类型声明相关）
        // 只匹配明确的类型使用场景，避免误匹配
        std::string searchPattern = original;
        std::vector<std::string> patterns = {
            searchPattern + " *",      // TestClass * (属性类型)
            searchPattern + "*",        // TestClass* (属性类型，无空格)
            searchPattern + ":",        // TestClass: (继承)
            searchPattern + " ",        // TestClass  (后面跟空格，如 [[TestClass alloc]])
            searchPattern + "[",        // TestClass[ (数组类型，虽然ObjC不常用)
        };
        
        // 使用set来避免重复添加相同位置
        std::set<unsigned> addedOffsets;
        
        for (const auto& pattern : patterns) {
            pos = 0;
            while ((pos = content.find(pattern, pos)) != std::string::npos) {
                // 检查前面是否是有效的标识符边界
                if (pos > 0) {
                    char prev = content[pos - 1];
                    // 确保前面不是字母、数字或下划线（避免匹配部分单词）
                    if (std::isalnum(prev) || prev == '_') {
                        pos += pattern.length();
                        continue;
                    }
                }
                
                // 检查是否在字符串字面量中（避免替换字符串内容）
                // 简单检查：如果前面有引号，可能是字符串
                bool inString = false;
                for (size_t i = pos; i > 0 && i > pos - 100; --i) {
                    if (content[i] == '"' && (i == 0 || content[i-1] != '\\')) {
                        // 检查引号对
                        size_t quoteCount = 0;
                        for (size_t j = i; j < content.length() && j < pos + 100; ++j) {
                            if (content[j] == '"' && (j == 0 || content[j-1] != '\\')) {
                                quoteCount++;
                                if (quoteCount == 2 && j >= pos) {
                                    inString = true;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
                
                if (inString) {
                    pos += pattern.length();
                    continue;
                }
                
                // 检查是否已经添加过这个位置（避免重复替换）
                unsigned offset = pos;
                if (addedOffsets.find(offset) == addedOffsets.end()) {
                    // 检查是否已经在之前的替换中处理过（避免重复替换已混淆的类名）
                    // 简单检查：如果这个位置已经被替换过，跳过
                    SourceLocation loc = SM.getLocForStartOfFile(mainFileID);
                    loc = loc.getLocWithOffset(pos);
                    if (loc.isValid() && SM.isInMainFile(loc)) {
                        // 检查这个位置是否已经在locationsToReplace中（避免重复）
                        bool alreadyReplaced = false;
                        for (const auto& replacedLoc : locationsToReplace) {
                            if (SM.getFileOffset(replacedLoc) == offset) {
                                alreadyReplaced = true;
                                break;
                            }
                        }
                        if (!alreadyReplaced) {
                            typeLocations.push_back({loc, original.length()});
                            addedOffsets.insert(offset);
                        }
                    }
                }
                pos += pattern.length();
            }
        }
    }
    
    // 执行类型引用替换（从后往前替换）
    std::sort(typeLocations.begin(), typeLocations.end(),
              [&SM](const std::pair<SourceLocation, unsigned>& a,
                    const std::pair<SourceLocation, unsigned>& b) {
                  return SM.getFileOffset(a.first) > SM.getFileOffset(b.first);
              });
    
    for (const auto& [loc, len] : typeLocations) {
        if (loc.isValid() && SM.isInMainFile(loc)) {
            rewriter.ReplaceText(loc, len, obfuscated);
        }
    }
    
    int totalReplacements = locationsToReplace.size() + typeLocations.size();
    LOG_INFO("Replaced class name: " + original + " -> " + obfuscated + 
             " (" + std::to_string(totalReplacements) + " occurrences)");
}

/**
 * @brief 更新import语句
 * 
 * 当类名改变后，需要更新所有引用该类的import语句。
 * 例如：#import "MyClass.h" 需要改为 #import "OBF_A1B2C3.h"
 * 
 * @param rewriter 代码重写器
 * @param original 原始类名
 * @param obfuscated 混淆后的类名
 */
void ClassNameObfuscationStrategy::updateImports(Rewriter& rewriter,
                                                const std::string& original,
                                                const std::string& obfuscated) {
    const SourceManager& SM = rewriter.getSourceMgr();
    FileID mainFileID = SM.getMainFileID();
    
    // 获取主文件的完整内容
    bool invalid = false;
    llvm::StringRef fileContent = SM.getBufferData(mainFileID, &invalid);
    if (invalid) {
        return;
    }
    
    // 查找所有 #import "OriginalClass.h" 或 #import <Framework/OriginalClass.h> 语句
    std::vector<std::pair<SourceLocation, unsigned>> importLocations;
    
    // 使用正则表达式或字符串搜索来查找import语句
    // 格式1: #import "OriginalClass.h"
    std::string pattern1 = "#import \"" + original + ".h\"";
    std::string replacement1 = "#import \"" + obfuscated + ".h\"";
    
    // 格式2: #import <Framework/OriginalClass.h>
    std::string pattern2 = "#import <" + original + ".h>";
    std::string replacement2 = "#import <" + obfuscated + ".h>";
    
    // 格式3: #import "OriginalClass.h" (可能有空格)
    std::string pattern3 = "#import  \"" + original + ".h\"";
    std::string replacement3 = "#import  \"" + obfuscated + ".h\"";
    
    // 在文件内容中搜索
    std::string content = fileContent.str();
    size_t pos = 0;
    
    // 搜索格式1: #import "OriginalClass.h"
    while ((pos = content.find(pattern1, pos)) != std::string::npos) {
        SourceLocation loc = SM.getLocForStartOfFile(mainFileID);
        // 找到类名的位置（在引号之后）
        size_t classNamePos = pos + 9; // "#import \"" 的长度
        loc = loc.getLocWithOffset(classNamePos);
        if (loc.isValid()) {
            importLocations.push_back({loc, original.length()});
        }
        pos += pattern1.length();
    }
    
    // 搜索格式2: #import <OriginalClass.h>
    pos = 0;
    while ((pos = content.find(pattern2, pos)) != std::string::npos) {
        SourceLocation loc = SM.getLocForStartOfFile(mainFileID);
        // 找到类名的位置（在<之后，可能还有Framework/）
        size_t classNamePos = pos + 8; // "#import <" 的长度
        // 检查是否有Framework/
        size_t slashPos = content.find('/', pos + 8);
        if (slashPos != std::string::npos && slashPos < pos + pattern2.length()) {
            classNamePos = slashPos + 1; // 跳过 "Framework/"
        }
        loc = loc.getLocWithOffset(classNamePos);
        if (loc.isValid()) {
            importLocations.push_back({loc, original.length()});
        }
        pos += pattern2.length();
    }
    
    // 搜索格式3: #import  "OriginalClass.h" (双空格)
    pos = 0;
    while ((pos = content.find(pattern3, pos)) != std::string::npos) {
        SourceLocation loc = SM.getLocForStartOfFile(mainFileID);
        // 找到类名的位置（在引号之后）
        size_t classNamePos = pos + 10; // "#import  \"" 的长度
        loc = loc.getLocWithOffset(classNamePos);
        if (loc.isValid()) {
            importLocations.push_back({loc, original.length()});
        }
        pos += pattern3.length();
    }
    
    // 执行替换（从后往前替换，避免位置偏移问题）
    std::sort(importLocations.begin(), importLocations.end(),
              [&SM](const std::pair<SourceLocation, unsigned>& a,
                    const std::pair<SourceLocation, unsigned>& b) {
                  return SM.getFileOffset(a.first) > SM.getFileOffset(b.first);
              });
    
    for (const auto& [loc, len] : importLocations) {
        if (loc.isValid() && SM.isInMainFile(loc)) {
            rewriter.ReplaceText(loc, len, obfuscated);
        }
    }
    
    if (!importLocations.empty()) {
        LOG_INFO("Updated " + std::to_string(importLocations.size()) + 
                 " import statements for class: " + original + " -> " + obfuscated);
    }
}

/**
 * @brief 验证阶段：检查混淆结果
 * 
 * 验证所有需要混淆的类名是否都已成功混淆。
 * 
 * @param context AST上下文
 * @return 验证通过返回true，否则返回false
 */
bool ClassNameObfuscationStrategy::validate(ASTContext& context) const {
    // TODO: 实现验证逻辑
    // 1. 检查所有类名是否都已混淆
    // 2. 检查是否有命名冲突
    // 3. 检查是否有遗漏的类名
    
    LOG_INFO("Validating class name obfuscation");
    
    // 简单验证：检查混淆列表是否为空
    if (classesToObfuscate_.empty()) {
        LOG_WARNING("No classes to obfuscate");
        return true;
    }
    
    // 验证每个类都有混淆名称
    for (const auto& pair : classesToObfuscate_) {
        if (pair.second.empty()) {
            LOG_ERROR("Class " + pair.first + " has no obfuscated name");
            return false;
        }
    }
    
    LOG_INFO("Class name obfuscation validation passed");
    return true;
}

} // namespace obfuscator
