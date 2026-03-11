/**
 * @file ProtocolNameStrategy.cpp
 * @brief 协议名混淆策略实现
 *
 * 实现ProtocolNameObfuscationStrategy类，用于混淆Objective-C协议名。
 */

#include "strategies/ProtocolNameStrategy.h"
#include "core/ConfigManager.h"
#include "core/ReplacementManager.h"
#include "core/SymbolTable.h"
#include "core/Logger.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExprObjC.h"
#include "clang/Lex/Lexer.h"
#include <functional>

namespace obfuscator {

// =============================================================================
// analyze: 分析阶段，设置 AST matchers
// =============================================================================

void ProtocolNameObfuscationStrategy::analyze(clang::ASTContext& context) {
    using namespace clang::ast_matchers;

    LOG_INFO("Starting protocol name analysis");

    // 匹配协议声明 @protocol Name <SuperProtocol>
    finder_.addMatcher(
        objcProtocolDecl().bind("protocol"),
        this
    );

    // 执行匹配
    finder_.matchAST(context);

    LOG_INFO("Protocol name analysis completed: " +
             std::to_string(protocolsToObfuscate_.size()) + " protocols to obfuscate");
}

// =============================================================================
// run: MatchFinder 回调，处理匹配到的 AST 节点
// =============================================================================

void ProtocolNameObfuscationStrategy::run(
    const clang::ast_matchers::MatchFinder::MatchResult& result) {

    if (const auto* protocolDecl = result.Nodes.getNodeAs<clang::ObjCProtocolDecl>("protocol")) {
        handleProtocolDecl(protocolDecl);
    }
}

// =============================================================================
// handleProtocolDecl: 处理协议声明
// =============================================================================

void ProtocolNameObfuscationStrategy::handleProtocolDecl(
    const clang::ObjCProtocolDecl* protocolDecl) {

    if (!protocolDecl) return;

    // 检查依赖是否已初始化
    if (!nameGenerator_) {
        LOG_ERROR("NameGenerator is null in ProtocolNameObfuscationStrategy::handleProtocolDecl");
        return;
    }

    // 检查是否应该跳过
    if (shouldSkipProtocol(protocolDecl)) {
        return;
    }

    std::string protocolName = protocolDecl->getNameAsString();
    if (protocolName.empty()) {
        return;
    }

    // 检查是否已处理
    if (protocolsToObfuscate_.find(protocolName) != protocolsToObfuscate_.end()) {
        return;
    }

    // 生成混淆名
    std::string obfuscatedName = nameGenerator_->generate(protocolName, "protocolName");

    // 获取文件路径
    const clang::SourceManager& SM = protocolDecl->getASTContext().getSourceManager();
    std::string filePath = SM.getFilename(protocolDecl->getLocation()).str();

    // 保存协议信息
    ProtocolInfo info;
    info.originalName = protocolName;
    info.obfuscatedName = obfuscatedName;
    info.filePath = filePath;

    protocolsToObfuscate_[protocolName] = info;

    // 添加到符号表
    if (symbolTable_) {
        symbolTable_->addSymbol(protocolName, SymbolType::PROTOCOL, false, "");
    }

    LOG_DEBUG("Protocol to obfuscate: " + protocolName + " -> " + obfuscatedName);
}

// =============================================================================
// shouldSkipProtocol: 判断是否跳过该协议
// =============================================================================

bool ProtocolNameObfuscationStrategy::shouldSkipProtocol(
    const clang::ObjCProtocolDecl* protocolDecl) const {

    if (!protocolDecl) return true;

    const clang::SourceManager& SM = protocolDecl->getASTContext().getSourceManager();

    // 1. 跳过系统头文件中的协议
    if (SM.isInSystemHeader(protocolDecl->getLocation())) {
        return true;
    }

    // 2. 跳过空名称
    std::string protocolName = protocolDecl->getNameAsString();
    if (protocolName.empty()) {
        return true;
    }

    // 3. 跳过系统协议
    if (isSystemProtocol(protocolName)) {
        return true;
    }

    // 4. 检查用户白名单
    if (isWhitelisted(protocolName, "protocol")) {
        return true;
    }

    return false;
}

// =============================================================================
// isSystemProtocol: 判断是否是系统协议
// =============================================================================

bool ProtocolNameObfuscationStrategy::isSystemProtocol(const std::string& name) const {
    if (name.empty()) return false;

    // 系统协议前缀
    static const std::vector<std::string> systemPrefixes = {
        "NS",      // Foundation: NSObject, NSCopying, NSCoding, etc.
        "UI",      // UIKit: UIApplicationDelegate, etc.
        "CG",      // CoreGraphics
        "CF",      // CoreFoundation
        "CA",      // CoreAnimation
        "WK",      // WebKit
        "GK",      // GameKit
        "SK",      // StoreKit
        "SC",      // SwiftUI
        "MK",      // MapKit
        "CI",      // CoreImage
        "ML",      // CoreML
        "MT",      // Metal
        "SE",      // SafariExtensions
        "AV",      // AVFoundation
        "AU",      // AudioUnit
        "MP",      // MediaPlayer
        "MC",      // MultipeerConnectivity
        "CK",      // CloudKit
        "CT",      // CoreTelephony
        "GC",      // GameController
        "CB",      // CoreBluetooth
        "LN",      // NaturalLanguage
        "ND",      // NodeDevice
        "TI",      // TextIdentification
        "EK",      // EventKit
        "Event",   // Event-related protocols
        "NSItem",  // NSItemProvider protocols
        "UIText"   // UITextInput protocols
    };

    // 检查前缀匹配
    for (const auto& prefix : systemPrefixes) {
        if (name.length() >= prefix.length() &&
            name.compare(0, prefix.length(), prefix) == 0) {
            return true;
        }
    }

    // 系统协议白名单（没有明显前缀的核心协议）
    static const std::unordered_set<std::string> coreProtocols = {
        "NSObject",
        "JSExport"
    };

    return coreProtocols.find(name) != coreProtocols.end();
}

// =============================================================================
// collectReplacements: 收集所有替换项
// =============================================================================

void ProtocolNameObfuscationStrategy::collectReplacements(
    clang::ASTContext& context, ReplacementManager& manager) {

    const clang::SourceManager& SM = context.getSourceManager();

    // 获取当前文件路径
    clang::FileID mainFileID = SM.getMainFileID();
    clang::SourceLocation mainFileLoc = SM.getLocForStartOfFile(mainFileID);
    std::string currentFilePath = SM.getFilename(mainFileLoc).str();

    // 构建需要混淆的协议集合（所有协议都可以在其他文件中被引用）
    std::map<std::string, std::string> currentFileProtocols;  // protocolName -> obfuscatedName
    for (const auto& pair : protocolsToObfuscate_) {
        const ProtocolInfo& info = pair.second;
        // 所有协议都可以在其他文件中被引用，不过滤文件路径
        currentFileProtocols[pair.first] = info.obfuscatedName;
    }

    if (currentFileProtocols.empty()) {
        return;
    }

    int totalReplacements = 0;
    collectProtocolReplacements(context, manager, currentFileProtocols, totalReplacements);

    LOG_INFO("Collecting protocol name replacements");
    LOG_INFO("Current file: " + currentFilePath);
    LOG_INFO("Protocols to obfuscate in this file: " + std::to_string(currentFileProtocols.size()));
    LOG_INFO("Replacement collection: " + std::to_string(totalReplacements) + " replacements");
}

// =============================================================================
// collectProtocolReplacements: 收集协议相关的替换项
// =============================================================================

namespace {
    // 协议声明收集器
    class ProtocolDeclCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
    public:
        ProtocolDeclCollector(const std::map<std::string, std::string>& protocols,
                             obfuscator::ReplacementManager& mgr, int& count)
            : protocols_(protocols), manager_(mgr), count_(count) {}

        void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
            if (const auto* protocolDecl = result.Nodes.getNodeAs<clang::ObjCProtocolDecl>("protocol")) {
                std::string protocolName = protocolDecl->getNameAsString();

                auto it = protocols_.find(protocolName);
                if (it != protocols_.end()) {
                    const clang::SourceManager& SM = result.Context->getSourceManager();
                    clang::SourceLocation loc = protocolDecl->getLocation();
                    if (loc.isValid() && !SM.isInSystemHeader(loc)) {
                        // 替换协议声明中的名称
                        manager_.addReplacement(loc, protocolName, it->second, 0, "ProtocolNameObfuscation");
                        count_++;
                    }
                }
            }
        }

    private:
        const std::map<std::string, std::string>& protocols_;
        obfuscator::ReplacementManager& manager_;
        int& count_;
    };

    // 类声明中的协议列表收集器（使用并行迭代器获取引用位置）
    class InterfaceProtocolCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
    public:
        InterfaceProtocolCollector(const std::map<std::string, std::string>& protocols,
                                   obfuscator::ReplacementManager& mgr, int& count)
            : protocols_(protocols), manager_(mgr), count_(count) {}

        void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
            if (const auto* interfaceDecl = result.Nodes.getNodeAs<clang::ObjCInterfaceDecl>("interface")) {
                // 使用 protocol_begin/end() 和 protocol_loc_begin/end() 获取协议及其引用位置
                auto protocolIt = interfaceDecl->protocol_begin();
                auto locationIt = interfaceDecl->protocol_loc_begin();

                while (protocolIt != interfaceDecl->protocol_end()) {
                    const clang::ObjCProtocolDecl* protocol = *protocolIt;
                    clang::SourceLocation refLoc = *locationIt;

                    if (!protocol) {
                        ++protocolIt;
                        ++locationIt;
                        continue;
                    }

                    std::string protocolName = protocol->getNameAsString();
                    auto it = protocols_.find(protocolName);
                    if (it != protocols_.end()) {
                        const clang::SourceManager& SM = result.Context->getSourceManager();
                        if (refLoc.isValid() && !SM.isInSystemHeader(refLoc)) {
                            manager_.addReplacement(refLoc, protocolName, it->second, 0, "ProtocolNameObfuscation");
                            count_++;
                        }
                    }
                    ++protocolIt;
                    ++locationIt;
                }
            }
        }

    private:
        const std::map<std::string, std::string>& protocols_;
        obfuscator::ReplacementManager& manager_;
        int& count_;
    };

    // 协议继承收集器（处理协议继承中的父协议引用）
    class ProtocolInheritanceCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
    public:
        ProtocolInheritanceCollector(const std::map<std::string, std::string>& protocols,
                                     obfuscator::ReplacementManager& mgr, int& count)
            : protocols_(protocols), manager_(mgr), count_(count) {}

        void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override {
            if (const auto* protocolDecl = result.Nodes.getNodeAs<clang::ObjCProtocolDecl>("protocol")) {
                // 使用并行迭代器获取父协议引用的位置
                const clang::ObjCProtocolList& protoList = protocolDecl->getReferencedProtocols();
                auto protocolIt = protoList.begin();
                auto locationIt = protoList.loc_begin();

                while (protocolIt != protoList.end()) {
                    const clang::ObjCProtocolDecl* parentProtocol = *protocolIt;
                    clang::SourceLocation refLoc = *locationIt;

                    if (!parentProtocol) {
                        ++protocolIt;
                        ++locationIt;
                        continue;
                    }

                    std::string parentName = parentProtocol->getNameAsString();
                    auto it = protocols_.find(parentName);
                    if (it != protocols_.end()) {
                        const clang::SourceManager& SM = result.Context->getSourceManager();
                        if (refLoc.isValid() && !SM.isInSystemHeader(refLoc)) {
                            manager_.addReplacement(refLoc, parentName, it->second, 0, "ProtocolNameObfuscation");
                            count_++;
                        }
                    }
                    ++protocolIt;
                    ++locationIt;
                }
            }
        }

    private:
        const std::map<std::string, std::string>& protocols_;
        obfuscator::ReplacementManager& manager_;
        int& count_;
    };
}

void ProtocolNameObfuscationStrategy::collectProtocolReplacements(
    clang::ASTContext& context,
    ReplacementManager& manager,
    const std::map<std::string, std::string>& currentFileProtocols,
    int& totalReplacements) {

    using namespace clang::ast_matchers;

    if (currentFileProtocols.empty()) {
        return;
    }

    try {
        MatchFinder localFinder;

        ProtocolDeclCollector declCollector(currentFileProtocols, manager, totalReplacements);
        InterfaceProtocolCollector interfaceCollector(currentFileProtocols, manager, totalReplacements);
        ProtocolInheritanceCollector inheritanceCollector(currentFileProtocols, manager, totalReplacements);

        // 匹配协议声明
        localFinder.addMatcher(objcProtocolDecl().bind("protocolDecl"), &declCollector);

        // 匹配类声明（处理协议列表）
        localFinder.addMatcher(objcInterfaceDecl().bind("interface"), &interfaceCollector);

        // 匹配协议声明（处理协议继承）- 使用不同的绑定名称
        localFinder.addMatcher(objcProtocolDecl().bind("protocolInherit"), &inheritanceCollector);

        // 执行匹配
        localFinder.matchAST(context);

        LOG_INFO("Protocol replacement collection: " + std::to_string(totalReplacements) + " replacements");
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in collectProtocolReplacements: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in collectProtocolReplacements");
    }
}

// =============================================================================
// validate: 验证混淆结果
// =============================================================================

bool ProtocolNameObfuscationStrategy::validate(clang::ASTContext& context) const {
    LOG_INFO("Protocol name obfuscation validation passed");
    return true;
}

} // namespace obfuscator
