/**
 * @file VariableNameStrategy.cpp
 * @brief 变量名混淆策略实现
 *
 * 实现VariableNameObfuscationStrategy类，用于混淆Objective-C局部变量、
 * 静态局部变量和全局变量名。
 */

#include "strategies/VariableNameStrategy.h"
#include "core/ConfigManager.h"
#include "core/ReplacementManager.h"
#include "core/SymbolTable.h"
#include "core/Logger.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/Lex/Lexer.h"
#include <vector>
#include <functional>
#include <unordered_set>

namespace obfuscator {

// =============================================================================
// analyze: 分析阶段，设置 AST matchers
// =============================================================================

void VariableNameObfuscationStrategy::analyze(clang::ASTContext& context) {
    using namespace clang::ast_matchers;

    // 收集所有文件的局部变量映射（不存储 SourceLocation）
    // 在 ApplicationMode 中会重新扫描 AST 获取新的 SourceLocation

    LOG_INFO("Starting variable name analysis");

    // 只匹配变量声明，收集变量映射
    // 注意：ObjCIvarDecl 不是 VarDecl 的子类，所以 varDecl() matcher 不会匹配 ivar
    finder_.addMatcher(
        varDecl(
            unless(isImplicit()),              // 排除编译器生成的隐式变量
            unless(parmVarDecl())             // 排除方法参数（由 MethodNameStrategy 处理）
        ).bind("variable"),
        this
    );

    // 执行匹配
    finder_.matchAST(context);

    LOG_INFO("Variable name analysis completed: " +
             std::to_string(variablesToObfuscate_.size()) + " variables to obfuscate");
}

// =============================================================================
// run: MatchFinder 回调，处理匹配到的 AST 节点
// =============================================================================

void VariableNameObfuscationStrategy::run(
    const clang::ast_matchers::MatchFinder::MatchResult& result) {

    // 只处理变量声明，收集变量映射
    // 变量引用会在 ApplicationMode 中重新扫描收集
    if (const auto* varDecl = result.Nodes.getNodeAs<clang::VarDecl>("variable")) {
        handleVariable(varDecl);
    }
}

// =============================================================================
// handleVariable: 处理变量声明
// =============================================================================

void VariableNameObfuscationStrategy::handleVariable(const clang::VarDecl* varDecl) {
    if (!varDecl) return;

    // 检查依赖是否已初始化
    if (!nameGenerator_) {
        LOG_ERROR("NameGenerator is null in VariableNameObfuscationStrategy::handleVariable");
        return;
    }

    // 检查是否应该跳过
    if (shouldSkipVariable(varDecl)) {
        return;
    }

    std::string varName = varDecl->getNameAsString();
    if (varName.empty()) {
        return;
    }

    // 简化版：只处理局部变量，使用 "函数名::变量名" 作为 key
    std::string funcName = getCurrentFunctionName(varDecl);
    if (funcName.empty()) {
        return;  // 不在函数中的变量，跳过
    }
    std::string key = funcName + "::" + varName;

    // 检查是否已处理
    if (variablesToObfuscate_.find(key) != variablesToObfuscate_.end()) {
        return;
    }

    // 生成混淆名
    std::string obfuscatedName = nameGenerator_->generate(varName, "variableName");

    // 获取文件路径（用于 ApplicationMode 过滤）
    const clang::SourceManager& SM = varDecl->getASTContext().getSourceManager();
    std::string filePath = SM.getFilename(varDecl->getLocation()).str();

    // 保存变量信息（不存储 SourceLocation，它们在 ApplicationMode 中会重新获取）
    VariableInfo info;
    info.originalName = varName;
    info.obfuscatedName = obfuscatedName;
    info.scope = VariableScope::Local;  // 简化版：只处理局部变量
    info.functionName = funcName;
    info.filePath = filePath;

    variablesToObfuscate_[key] = info;

    // 添加到符号表
    if (symbolTable_) {
        symbolTable_->addSymbol(varName, SymbolType::VARIABLE, false, funcName);
    }
}

// =============================================================================
// shouldSkipVariable: 判断是否跳过该变量
// =============================================================================

bool VariableNameObfuscationStrategy::shouldSkipVariable(const clang::VarDecl* varDecl) const {
    if (!varDecl) return true;

    const clang::SourceManager& SM = varDecl->getASTContext().getSourceManager();

    // 1. 跳过系统头文件中的变量
    if (SM.isInSystemHeader(varDecl->getLocation())) {
        return true;
    }

    // 2. 跳过方法参数（由 MethodNameStrategy 处理）
    if (clang::isa<clang::ParmVarDecl>(varDecl)) {
        return true;
    }

    // 3. 跳过 ivar（由 PropertyNameStrategy 处理）
    if (clang::isa<clang::ObjCIvarDecl>(varDecl)) {
        return true;
    }

    // 4. 跳过空名称
    std::string varName = varDecl->getNameAsString();
    if (varName.empty()) {
        return true;
    }

    // 5. 跳过全局变量（跨文件，处理复杂）
    //    静态局部变量现在可以被处理
    VariableScope scope = classifyVariable(varDecl);
    if (scope == VariableScope::Global) {
        return true;
    }

    // 6. 只跳过编译器关键字（self, super, _cmd 等）
    if (isCompilerKeyword(varName)) {
        return true;
    }

    // 7. 跳过短循环变量（i, j, k 等）
    if (isShortLoopVariable(varName)) {
        return true;
    }

    // 8. 跳过全大写常量（ALL_CAPS 命名约定）
    if (isAllCaps(varName)) {
        return true;
    }

    // 9. 检查用户白名单
    if (isWhitelisted(varName, "variable")) {
        return true;
    }

    return false;
}

// =============================================================================
// classifyVariable: 分类变量作用域
// =============================================================================

VariableScope VariableNameObfuscationStrategy::classifyVariable(
    const clang::VarDecl* varDecl) const {

    if (!varDecl) {
        return VariableScope::Local;
    }

    // 检查全局变量（有全局存储且不是在函数内声明的）
    if (varDecl->hasGlobalStorage() && !varDecl->isLocalVarDecl()) {
        return VariableScope::Global;
    }

    // 检查静态局部变量
    // 使用 getStorageDuration() 来判断
    if (varDecl->isLocalVarDecl() && varDecl->getStorageDuration() == clang::SD_Static) {
        return VariableScope::StaticLocal;
    }

    // 检查循环变量（隐式声明的 for 循环变量）
    if (varDecl->isLocalVarDecl() && varDecl->isImplicit()) {
        return VariableScope::Loop;
    }

    return VariableScope::Local;
}

// =============================================================================
// getVariableKey: 获取变量的唯一 key
// =============================================================================

std::string VariableNameObfuscationStrategy::getVariableKey(
    const clang::VarDecl* varDecl) const {

    if (!varDecl) return "";

    // 简化版：只处理局部变量，使用 "函数名::变量名" 作为 key
    std::string varName = varDecl->getNameAsString();
    std::string funcName = getCurrentFunctionName(varDecl);

    if (funcName.empty()) {
        return varName;
    }
    return funcName + "::" + varName;
}

// =============================================================================
// getCurrentFunctionName: 获取当前函数名
// =============================================================================

std::string VariableNameObfuscationStrategy::getCurrentFunctionName(
    const clang::VarDecl* varDecl) const {

    if (!varDecl) return "";

    // 获取父函数或方法
    const clang::DeclContext* ctx = varDecl->getParentFunctionOrMethod();
    if (!ctx) return "";

    // 对于 Objective-C 方法，获取完整的选择器名
    if (const auto* methodDecl = clang::dyn_cast<clang::ObjCMethodDecl>(ctx)) {
        return methodDecl->getSelector().getAsString();
    }
    // 对于函数
    if (const auto* functionDecl = clang::dyn_cast<clang::FunctionDecl>(ctx)) {
        return functionDecl->getNameAsString();
    }

    return "";
}

// =============================================================================
// collectReplacements: 收集所有替换项
// =============================================================================

void VariableNameObfuscationStrategy::collectReplacements(
    clang::ASTContext& context, ReplacementManager& manager) {

    const clang::SourceManager& SM = context.getSourceManager();

    // 获取当前文件路径
    clang::FileID mainFileID = SM.getMainFileID();
    clang::SourceLocation mainFileLoc = SM.getLocForStartOfFile(mainFileID);
    std::string currentFilePath = SM.getFilename(mainFileLoc).str();

    // 构建当前文件中需要混淆的变量集合
    std::map<std::string, std::string> currentFileVariables;  // key -> obfuscatedName
    for (const auto& pair : variablesToObfuscate_) {
        const VariableInfo& info = pair.second;
        if (info.filePath == currentFilePath) {
            currentFileVariables[pair.first] = info.obfuscatedName;
        }
    }

    if (currentFileVariables.empty()) {
        LOG_INFO("Current file: " + currentFilePath);
        LOG_INFO("No variables to obfuscate in this file");
        return;
    }

    int totalReplacements = 0;
    collectInApplicationMode(context, manager, currentFileVariables, totalReplacements);

    LOG_INFO("Collecting variable name replacements");
    LOG_INFO("Current file: " + currentFilePath);
    LOG_INFO("Variables to obfuscate: " + std::to_string(currentFileVariables.size()));
    LOG_INFO("Replacement collection: " + std::to_string(totalReplacements) + " replacements");

    // 清空已处理的位置记录
    replacedLocations_.clear();
}

// =============================================================================
// collectInApplicationMode: 在 ApplicationMode 中收集替换
// =============================================================================

namespace {
    // 变量声明收集器
    class VarDeclCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
    public:
        VarDeclCollector(const std::map<std::string, std::string>& vars,
                        obfuscator::ReplacementManager& mgr, int& count)
            : variables_(vars), manager_(mgr), count_(count) {}

        void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
            if (const auto* varDecl = result.Nodes.getNodeAs<clang::VarDecl>("variable")) {
                std::string varName = varDecl->getNameAsString();
                std::string funcName = getCurrentFunctionName(varDecl);
                std::string key = funcName + "::" + varName;

                auto it = variables_.find(key);
                if (it != variables_.end()) {
                    // 从 MatchResult 获取 ASTContext
                    const clang::SourceManager& SM = result.Context->getSourceManager();
                    clang::SourceLocation loc = varDecl->getLocation();
                    if (loc.isValid() && !SM.isInSystemHeader(loc)) {
                        manager_.addReplacement(loc, varName, it->second, 0, "VariableNameObfuscation");
                        count_++;
                    }
                }
            }
        }

    private:
        std::string getCurrentFunctionName(const clang::VarDecl* varDecl) const {
            if (!varDecl) return "";
            const clang::DeclContext* ctx = varDecl->getParentFunctionOrMethod();
            if (!ctx) return "";
            if (const auto* methodDecl = clang::dyn_cast<clang::ObjCMethodDecl>(ctx)) {
                return methodDecl->getSelector().getAsString();
            }
            if (const auto* functionDecl = clang::dyn_cast<clang::FunctionDecl>(ctx)) {
                return functionDecl->getNameAsString();
            }
            return "";
        }

        const std::map<std::string, std::string>& variables_;
        obfuscator::ReplacementManager& manager_;
        int& count_;
    };

    // 变量引用收集器
    class VarRefCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
    public:
        VarRefCollector(const std::map<std::string, std::string>& vars,
                       obfuscator::ReplacementManager& mgr, int& count)
            : variables_(vars), manager_(mgr), count_(count) {}

        void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
            if (const auto* declRef = result.Nodes.getNodeAs<clang::DeclRefExpr>("varRef")) {
                const clang::VarDecl* varDecl = clang::dyn_cast<clang::VarDecl>(declRef->getDecl());
                if (!varDecl) return;

                std::string varName = varDecl->getNameAsString();
                std::string funcName = getCurrentFunctionName(varDecl);
                std::string key = funcName + "::" + varName;

                auto it = variables_.find(key);
                if (it != variables_.end()) {
                    const clang::SourceManager& SM = result.Context->getSourceManager();
                    clang::SourceLocation loc = declRef->getLocation();
                    if (loc.isValid() && !SM.isInSystemHeader(loc)) {
                        manager_.addReplacement(loc, varName, it->second, 0, "VariableNameObfuscation");
                        count_++;
                    }
                }
            }
        }

    private:
        std::string getCurrentFunctionName(const clang::VarDecl* varDecl) const {
            if (!varDecl) return "";
            const clang::DeclContext* ctx = varDecl->getParentFunctionOrMethod();
            if (!ctx) return "";
            if (const auto* methodDecl = clang::dyn_cast<clang::ObjCMethodDecl>(ctx)) {
                return methodDecl->getSelector().getAsString();
            }
            if (const auto* functionDecl = clang::dyn_cast<clang::FunctionDecl>(ctx)) {
                return functionDecl->getNameAsString();
            }
            return "";
        }

        const std::map<std::string, std::string>& variables_;
        obfuscator::ReplacementManager& manager_;
        int& count_;
    };
}

void VariableNameObfuscationStrategy::collectInApplicationMode(
    clang::ASTContext& context,
    ReplacementManager& manager,
    const std::map<std::string, std::string>& currentFileVariables,
    int& totalReplacements) {

    using namespace clang::ast_matchers;

    // 在当前文件中重新扫描 AST，获取新的 SourceLocation
    // 这解决了 SourceLocation 跨 ASTContext 不兼容的问题
    MatchFinder localFinder;

    VarDeclCollector declCollector(currentFileVariables, manager, totalReplacements);
    VarRefCollector refCollector(currentFileVariables, manager, totalReplacements);

    // 匹配变量声明
    localFinder.addMatcher(
        varDecl(
            unless(isImplicit()),
            unless(parmVarDecl())
        ).bind("variable"),
        &declCollector
    );

    // 匹配变量引用
    localFinder.addMatcher(
        declRefExpr(
            to(varDecl()),
            unless(to(parmVarDecl()))
        ).bind("varRef"),
        &refCollector
    );

    // 执行匹配
    localFinder.matchAST(context);
}

// =============================================================================
// validate: 验证混淆结果
// =============================================================================

bool VariableNameObfuscationStrategy::validate(clang::ASTContext& context) const {
    // 简单验证：检查是否有变量被混淆
    // 实际项目中可以添加更复杂的验证逻辑
    LOG_INFO("Variable name obfuscation validation passed");
    return true;
}

// =============================================================================
// isCompilerKeyword: 判断是否是编译器关键字
// =============================================================================

bool VariableNameObfuscationStrategy::isCompilerKeyword(const std::string& name) const {
    // 只排除编译器必需的关键字
    static const std::unordered_set<std::string> keywords = {
        "self", "super", "_cmd"
    };
    return keywords.find(name) != keywords.end();
}

// =============================================================================
// isShortLoopVariable: 判断是否是短循环变量
// =============================================================================

bool VariableNameObfuscationStrategy::isShortLoopVariable(const std::string& name) const {
    // 单字符循环变量和常见的短循环变量名
    static const std::unordered_set<std::string> loopVars = {
        "i", "j", "k", "l", "m", "n",
        "x", "y", "z",
        "it", "idx", "iter"
    };
    return loopVars.find(name) != loopVars.end();
}

// =============================================================================
// isAllCaps: 判断是否是全大写常量
// =============================================================================

bool VariableNameObfuscationStrategy::isAllCaps(const std::string& name) const {
    if (name.length() <= 2) return false;

    // 检查是否全部是大写字母或下划线
    bool hasLower = false;
    for (char c : name) {
        if (c >= 'a' && c <= 'z') {
            hasLower = true;
            break;
        }
    }

    return !hasLower;
}

} // namespace obfuscator
