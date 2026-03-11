/**
 * @file ProtocolNameStrategy.h
 * @brief 协议名混淆策略头文件
 *
 * 定义ProtocolNameObfuscationStrategy类，用于混淆Objective-C协议名。
 */

#ifndef PROTOCOL_NAME_STRATEGY_H
#define PROTOCOL_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExprObjC.h"
#include <map>
#include <unordered_set>
#include <string>

namespace obfuscator {

// 前向声明
class ReplacementManager;

/**
 * @struct ProtocolInfo
 * @brief 协议信息结构体
 */
struct ProtocolInfo {
    std::string originalName;       // 原始协议名
    std::string obfuscatedName;     // 混淆后的协议名
    std::string filePath;           // 文件路径
};

/**
 * @class ProtocolNameObfuscationStrategy
 * @brief 协议名混淆策略
 *
 * 该策略负责混淆：
 * - @protocol 协议声明
 * - 类声明中的协议列表 <ProtocolName>
 * - @protocol(ProtocolName) 表达式
 * - 协议继承 <ParentProtocol>
 *
 * 保留不混淆：
 * - 系统协议（NS*, UI*, CG*, CF*, CA* 等前缀）
 * - 白名单中的协议
 * - NSObject 等核心协议
 */
class ProtocolNameObfuscationStrategy : public ObfuscationStrategy,
                                        public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    std::string getName() const override {
        return "ProtocolNameObfuscation";
    }

    std::string getDescription() const override {
        return "混淆Objective-C协议名";
    }

    void analyze(clang::ASTContext& context) override;
    void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) override;
    bool validate(clang::ASTContext& context) const override;
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    // 协议处理
    void handleProtocolDecl(const clang::ObjCProtocolDecl* protocolDecl);
    bool shouldSkipProtocol(const clang::ObjCProtocolDecl* protocolDecl) const;
    bool isSystemProtocol(const std::string& name) const;

    // 【新架构】收集协议相关的替换项
    void collectProtocolReplacements(clang::ASTContext& context,
                                    ReplacementManager& manager,
                                    const std::map<std::string, std::string>& currentFileProtocols,
                                    int& totalReplacements);

    clang::ast_matchers::MatchFinder finder_;

    // 存储需要混淆的协议（key: 协议名, value: 协议信息）
    std::map<std::string, ProtocolInfo> protocolsToObfuscate_;
};

} // namespace obfuscator

#endif // PROTOCOL_NAME_STRATEGY_H
