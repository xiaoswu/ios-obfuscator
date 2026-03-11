/**
 * @file DeadCodeInjectionStrategy.h
 * @brief 垃圾代码插入策略
 *
 * 在方法体内随机位置插入假业务逻辑代码。
 * 遵循三大原则：
 * 1. 编译器不会优化 - 使用运行时值
 * 2. Hopper 难判断 - 看起来像真实业务逻辑
 * 3. 无性能损耗 - 仅轻量级操作
 */

#ifndef DEAD_CODE_INJECTION_STRATEGY_H
#define DEAD_CODE_INJECTION_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include "core/DeadCodeGenerator.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <map>
#include <vector>
#include <set>

namespace clang {
    class ASTContext;
    class Rewriter;
    namespace ast_matchers {
        class MatchFinder;
        struct MatchResult;
    }
}

namespace obfuscator {

// 前向声明
class ReplacementManager;

/**
 * @struct InsertionPoint
 * @brief 代码插入点
 *
 * 表示一个可以插入垃圾代码的位置。
 */
struct InsertionPoint {
    clang::SourceLocation location;       // 插入位置
    const clang::Stmt* statement;         // 对应的语句
    std::string methodName;               // 方法名
    std::string className;                // 类名
    bool isInstanceMethod;                // 是否实例方法
};

/**
 * @class DeadCodeInjectionStrategy
 * @brief 垃圾代码插入策略
 *
 * 在方法体内随机位置插入假业务逻辑代码，增加逆向分析难度。
 */
class DeadCodeInjectionStrategy : public ObfuscationStrategy,
                                  public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    std::string getName() const override {
        return "DeadCodeInjection";
    }

    std::string getDescription() const override {
        return "在方法体内插入假业务逻辑代码";
    }

    void initialize(ConfigManager* config, SymbolTable* symbolTable, NameGenerator* nameGenerator) override;

    void analyze(clang::ASTContext& context) override;
    void transform(clang::ASTContext& context, clang::Rewriter& rewriter) override;
    void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) override;
    bool validate(clang::ASTContext& context) const override;
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    /**
     * @brief 扫描文件导入的头文件
     * @param context AST上下文
     * @return 已导入的框架名称集合
     */
    std::set<std::string> scanImportedHeaders(clang::ASTContext& context);

    /**
     * @brief 解析框架名称从导入语句
     * @param importStmt 导入语句
     * @return 框架名称，如果无法解析返回空字符串
     */
    std::string parseFrameworkName(const std::string& importPath);

    /**
     * @brief 收集方法的插入点
     * @param methodDecl 方法声明
     * @return 插入点列表
     */
    std::vector<InsertionPoint> collectInsertionPoints(
        const clang::ObjCMethodDecl* methodDecl,
        clang::ASTContext& context
    );

    /**
     * @brief 根据密度决定是否插入垃圾代码
     * @return true 表示应该插入
     */
    bool shouldInsert() const;

    /**
     * @brief 生成与上下文相关的垃圾代码
     * @param methodName 方法名
     * @param className 类名
     * @param isInstanceMethod 是否实例方法
     * @param frameworks 可用的框架集合
     * @return 生成的代码字符串
     */
    std::string generateContextualCode(
        const std::string& methodName,
        const std::string& className,
        bool isInstanceMethod,
        const std::set<std::string>& frameworks
    );

    /**
     * @brief 在指定位置插入代码
     * @param location 插入位置
     * @param code 要插入的代码
     * @param rewriter 重写器
     * @return 成功返回 true
     */
    bool insertCodeAtLocation(
        clang::SourceLocation location,
        const std::string& code,
        clang::Rewriter& rewriter
    );

    clang::ast_matchers::MatchFinder finder_;

    DeadCodeGenerator* generator_ = nullptr;
    std::map<std::string, std::vector<InsertionPoint>> insertionPoints_;  // 按文件分组
    std::map<std::string, std::set<std::string>> fileFrameworks_;        // 每个文件导入的框架

    std::set<std::string> processedFiles_;      // 已处理的文件

    // 配置
    double density_ = 0.2;                      // 插入密度
    int maxStatementsPerMethod_ = 3;            // 每个方法最多插入的语句数
};

} // namespace obfuscator

#endif // DEAD_CODE_INJECTION_STRATEGY_H
