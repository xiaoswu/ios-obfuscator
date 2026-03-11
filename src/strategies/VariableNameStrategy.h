/**
 * @file VariableNameStrategy.h
 * @brief 变量名混淆策略头文件
 *
 * 定义VariableNameObfuscationStrategy类，用于混淆Objective-C局部变量、
 * 静态局部变量和全局变量名。
 */

#ifndef VARIABLE_NAME_STRATEGY_H
#define VARIABLE_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <map>
#include <set>
#include <vector>

namespace obfuscator {

// 前向声明
class ReplacementManager;

/**
 * @enum VariableScope
 * @brief 变量作用域枚举
 */
enum class VariableScope {
    Local,       // 局部变量
    StaticLocal, // 静态局部变量
    Global,      // 全局变量
    Loop         // 循环变量
};

/**
 * @struct VariableInfo
 * @brief 变量信息结构体
 */
struct VariableInfo {
    std::string originalName;      // 原始变量名
    std::string obfuscatedName;    // 混淆后的变量名
    VariableScope scope;           // 变量作用域
    std::string functionName;      // 所属函数名
    std::string filePath;          // 文件路径
};

/**
 * @class VariableNameObfuscationStrategy
 * @brief 变量名混淆策略
 *
 * 该策略负责混淆：
 * - 局部变量名
 * - 静态局部变量名
 * - 全局变量名（可选）
 *
 * 保留不混淆：
 * - 方法参数（由 MethodNameStrategy 处理）
 * - 成员变量/实例变量（由 PropertyNameStrategy 处理）
 * - 编译器关键字（self, super, _cmd）
 * - 短循环变量（i, j, k 等）
 * - 全大写常量（ALL_CAPS 命名约定）
 * - 白名单中的变量
 */
class VariableNameObfuscationStrategy : public ObfuscationStrategy,
                                        public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    std::string getName() const override {
        return "VariableNameObfuscation";
    }

    std::string getDescription() const override {
        return "混淆Objective-C局部变量、静态局部变量和全局变量名";
    }

    void analyze(clang::ASTContext& context) override;
    void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) override;
    bool validate(clang::ASTContext& context) const override;
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    // 变量处理
    void handleVariable(const clang::VarDecl* varDecl);
    bool shouldSkipVariable(const clang::VarDecl* varDecl) const;
    VariableScope classifyVariable(const clang::VarDecl* varDecl) const;

    // 辅助方法
    std::string getVariableKey(const clang::VarDecl* varDecl) const;
    std::string getCurrentFunctionName(const clang::VarDecl* varDecl) const;

    // ApplicationMode 处理
    void collectInApplicationMode(
        clang::ASTContext& context,
        ReplacementManager& manager,
        const std::map<std::string, std::string>& currentFileVariables,
        int& totalReplacements
    );

    // 过滤判断
    bool isCompilerKeyword(const std::string& name) const;
    bool isShortLoopVariable(const std::string& name) const;
    bool isAllCaps(const std::string& name) const;

    clang::ast_matchers::MatchFinder finder_;

    // 存储需要混淆的变量
    // key: "函数名::变量名", value: 变量信息
    std::map<std::string, VariableInfo> variablesToObfuscate_;

    // 记录已替换的位置（避免重复替换）
    std::set<unsigned> replacedLocations_;
};

} // namespace obfuscator

#endif // VARIABLE_NAME_STRATEGY_H
