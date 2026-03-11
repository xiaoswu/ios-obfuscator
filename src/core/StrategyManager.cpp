#include "core/StrategyManager.h"
#include "strategies/ClassNameStrategy.h"
#include "strategies/MethodNameStrategy.h"
#include "strategies/PropertyNameStrategy.h"
#include "strategies/VariableNameStrategy.h"
#include "strategies/ProtocolNameStrategy.h"
#include "strategies/CommentRemovalStrategy.h"
#include "strategies/DeadCodeInjectionStrategy.h"
#include "core/ReplacementManager.h"
#include "core/Logger.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <algorithm>

namespace obfuscator {

StrategyManager::StrategyManager(ConfigManager* config,
                                SymbolTable* symbolTable,
                                NameGenerator* nameGenerator)
    : config_(config), symbolTable_(symbolTable), nameGenerator_(nameGenerator),
      replacementManager_(std::make_unique<ReplacementManager>()) {
}

StrategyManager::~StrategyManager() {
}

void StrategyManager::registerStrategy(std::unique_ptr<ObfuscationStrategy> strategy) {
    std::string name = strategy->getName();
    strategy->initialize(config_, symbolTable_, nameGenerator_);
    strategies_[name] = std::move(strategy);
}

void StrategyManager::loadStrategies() {
    if (!config_) {
        LOG_ERROR("ConfigManager is null");
        return;
    }

    auto strategyNames = config_->getConfig().obfuscation.strategies;

    // 解析依赖关系
    executionOrder_ = resolveDependencies(strategyNames);

    // 按顺序创建策略实例
    for (const auto& name : executionOrder_) {
        if (config_->isStrategyEnabled(name)) {
            auto strategy = createStrategy(name);
            if (strategy) {
                strategy->initialize(config_, symbolTable_, nameGenerator_);
                strategies_[name] = std::move(strategy);
                LOG_INFO("Loaded strategy: " + name);
            } else {
                LOG_WARNING("Failed to create strategy: " + name);
            }
        }
    }
}

std::unique_ptr<ObfuscationStrategy> StrategyManager::createStrategy(const std::string& name) {
    if (name == "ClassNameObfuscation") {
        return std::make_unique<ClassNameObfuscationStrategy>();
    } else if (name == "PropertyNameObfuscation") {
        return std::make_unique<PropertyNameObfuscationStrategy>();
    } else if (name == "MethodNameObfuscation") {
        return std::make_unique<MethodNameObfuscationStrategy>();
    } else if (name == "VariableNameObfuscation") {
        return std::make_unique<VariableNameObfuscationStrategy>();
    } else if (name == "ProtocolNameObfuscation") {
        return std::make_unique<ProtocolNameObfuscationStrategy>();
    } else if (name == "CommentRemoval") {
        return std::make_unique<CommentRemovalStrategy>();
    } else if (name == "DeadCodeInjection") {
        return std::make_unique<DeadCodeInjectionStrategy>();
    }

    return nullptr;
}

std::vector<std::string> StrategyManager::resolveDependencies(
    const std::vector<std::string>& strategyNames) const {

    // 简单的依赖解析：按固定顺序执行
    // TODO: 实现更复杂的依赖解析算法
    // 【重要】策略执行顺序
    // PropertyNameObfuscation 在 MethodNameObfuscation 之前执行（分析阶段）
    // 但 transform 阶段使用 ReplacementManager 统一管理，执行顺序不影响结果
    // CommentRemoval 放在最后，因为它是在文本处理阶段执行的
    std::vector<std::string> order = {
        "ClassNameObfuscation",
        "PropertyNameObfuscation",
        "MethodNameObfuscation",
        "VariableNameObfuscation",
        "ProtocolNameObfuscation",
        "CategoryNameObfuscation",
        "FileNameObfuscation",
        "StringObfuscation",
        "ResourceObfuscation",
        "MetadataObfuscation",
        "ControlFlowObfuscation",
        "DeadCodeInjection",
        "CommentRemoval"
    };

    // 只返回配置中启用的策略
    std::vector<std::string> result;
    for (const auto& strategy : order) {
        if (std::find(strategyNames.begin(), strategyNames.end(), strategy) != strategyNames.end()) {
            result.push_back(strategy);
        }
    }

    return result;
}

void StrategyManager::analyzeAll(clang::ASTContext& context) {
    for (const auto& name : executionOrder_) {
        auto it = strategies_.find(name);
        if (it != strategies_.end()) {
            LOG_INFO("Analyzing with strategy: " + name);
            it->second->analyze(context);
        }
    }
}

void StrategyManager::collectReplacementsAll(clang::ASTContext& context, clang::Rewriter& rewriter) {
    // 【新架构】使用 ReplacementManager 统一管理所有替换

    // 清空之前的替换（如果有）
    replacementManager_->clear();

    // 第一阶段：所有策略收集替换项
    LOG_INFO("Collecting replacements from all strategies");
    for (const auto& name : executionOrder_) {
        auto it = strategies_.find(name);
        if (it != strategies_.end()) {
            LOG_INFO("Collecting replacements from strategy: " + name);
            it->second->collectReplacements(context, *replacementManager_);
        }
    }

    // 第二阶段：统一应用所有替换
    LOG_INFO("Applying all replacements");
    size_t applied = replacementManager_->applyAll(context, rewriter);

    LOG_INFO("Transformation complete: " + std::to_string(applied) + " replacements applied");
}

bool StrategyManager::validateAll(clang::ASTContext& context) {
    bool allValid = true;
    for (const auto& pair : strategies_) {
        if (!pair.second->validate(context)) {
            LOG_ERROR("Validation failed for strategy: " + pair.first);
            allValid = false;
        }
    }
    return allValid;
}

ObfuscationStrategy* StrategyManager::getStrategy(const std::string& name) {
    auto it = strategies_.find(name);
    if (it != strategies_.end()) {
        return it->second.get();
    }
    return nullptr;
}

} // namespace obfuscator

