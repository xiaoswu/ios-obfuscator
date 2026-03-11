#ifndef STRATEGY_MANAGER_H
#define STRATEGY_MANAGER_H

#include <vector>
#include <memory>
#include <map>
#include <string>
#include "strategies/ObfuscationStrategy.h"
#include "core/ConfigManager.h"
#include "core/SymbolTable.h"
#include "core/NameGenerator.h"

// 前向声明Clang类型
namespace clang {
    class ASTContext;
    class Rewriter;
}

namespace obfuscator {

class ReplacementManager;

class StrategyManager {
public:
    StrategyManager(ConfigManager* config, SymbolTable* symbolTable, NameGenerator* nameGenerator);
    ~StrategyManager();

    // 注册策略
    void registerStrategy(std::unique_ptr<ObfuscationStrategy> strategy);

    // 根据配置加载策略
    void loadStrategies();

    // 执行所有策略的分析阶段
    void analyzeAll(clang::ASTContext& context);

    // 执行所有策略的收集替换阶段（应用阶段）
    // 【新架构】两阶段处理：第一遍analyze收集符号，第二遍collectReplacements应用混淆
    void collectReplacementsAll(clang::ASTContext& context, clang::Rewriter& rewriter);

    // 执行所有策略的验证阶段
    bool validateAll(clang::ASTContext& context);

    // 获取策略
    ObfuscationStrategy* getStrategy(const std::string& name);

private:
    // 创建策略实例
    std::unique_ptr<ObfuscationStrategy> createStrategy(const std::string& name);

    // 解析策略依赖关系并排序
    std::vector<std::string> resolveDependencies(
        const std::vector<std::string>& strategyNames) const;

    ConfigManager* config_;
    SymbolTable* symbolTable_;
    NameGenerator* nameGenerator_;

    std::map<std::string, std::unique_ptr<ObfuscationStrategy>> strategies_;
    std::vector<std::string> executionOrder_;

    // 【新】替换管理器（拥有所有权）
    std::unique_ptr<ReplacementManager> replacementManager_;
};

} // namespace obfuscator

#endif // STRATEGY_MANAGER_H

