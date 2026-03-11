#ifndef OBFUSCATION_STRATEGY_H
#define OBFUSCATION_STRATEGY_H

#include <string>
#include <vector>

// 前向声明Clang类型，避免头文件依赖
// 实际使用时在.cpp文件中包含完整的Clang头文件
namespace clang {
    class ASTContext;
    namespace ast_matchers {
        class MatchFinder;
        struct MatchResult;
    }
    class Rewriter;
}

// 前向声明内部类型
namespace obfuscator {
    class ConfigManager;
    class SymbolTable;
    class NameGenerator;
    class ReplacementManager;
}

namespace obfuscator {

class ObfuscationStrategy {
public:
    virtual ~ObfuscationStrategy() = default;

    // 策略名称
    virtual std::string getName() const = 0;

    // 策略描述
    virtual std::string getDescription() const = 0;

    // 初始化
    virtual void initialize(ConfigManager* config, SymbolTable* symbolTable, NameGenerator* nameGenerator) {
        config_ = config;
        symbolTable_ = symbolTable;
        nameGenerator_ = nameGenerator;
    }

    // 分析阶段：收集需要混淆的符号
    virtual void analyze(clang::ASTContext& context) = 0;

    // 转换阶段：执行混淆
    // 【新架构】默认实现调用 collectReplacements，子类可以重写以直接使用 rewriter（向后兼容）
    virtual void transform(clang::ASTContext& context, clang::Rewriter& rewriter);

    // 【新方法】收集替换项（推荐子类实现此方法）
    // 子类应该实现此方法，向 ReplacementManager 注册所有需要替换的位置
    // 默认实现为空，保持向后兼容
    virtual void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) {
        // 默认为空，子类可以选择实现
    }

    // 验证阶段：检查混淆结果
    virtual bool validate(clang::ASTContext& context) const = 0;

    // 获取依赖的其他策略
    virtual std::vector<std::string> getDependencies() const { return {}; }

    // 获取符号表（供策略间协作使用）
    SymbolTable* getSymbolTable() const { return symbolTable_; }

protected:
    ConfigManager* config_ = nullptr;
    SymbolTable* symbolTable_ = nullptr;
    NameGenerator* nameGenerator_ = nullptr;

    // 检查是否在白名单中
    bool isWhitelisted(const std::string& name, const std::string& type) const;

    // 检查是否是系统符号
    bool isSystemSymbol(const std::string& name) const;

    // 检查是否是第三方SDK
    bool isThirdPartySDK(const std::string& name) const;
};

} // namespace obfuscator

#endif // OBFUSCATION_STRATEGY_H

