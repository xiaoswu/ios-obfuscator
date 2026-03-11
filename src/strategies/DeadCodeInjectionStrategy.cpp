/**
 * @file DeadCodeInjectionStrategy.cpp
 * @brief 垃圾代码插入策略实现
 *
 * 在方法体内随机位置插入假业务逻辑代码。
 */

#include "strategies/DeadCodeInjectionStrategy.h"
#include "core/ConfigManager.h"
#include "core/SymbolTable.h"
#include "core/NameGenerator.h"
#include "core/Logger.h"
#include "core/ReplacementManager.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtObjC.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <random>
#include <sstream>

using namespace clang;
using namespace clang::ast_matchers;

namespace obfuscator {

// =============================================================================
// initialize
// =============================================================================

void DeadCodeInjectionStrategy::initialize(
    ConfigManager* config,
    SymbolTable* symbolTable,
    NameGenerator* nameGenerator) {

    ObfuscationStrategy::initialize(config, symbolTable, nameGenerator);

    // 获取配置
    const auto& obfConfig = config_->getConfig().obfuscation;
    density_ = obfConfig.deadCodeInjection.density;
    maxStatementsPerMethod_ = obfConfig.deadCodeInjection.maxStatementsPerMethod;

    // 创建代码生成器
    generator_ = new DeadCodeGenerator(obfConfig.deadCodeInjection);
}

// =============================================================================
// analyze - 分析阶段
// =============================================================================

void DeadCodeInjectionStrategy::analyze(ASTContext& context) {
    // 扫描导入的头文件
    std::set<std::string> importedFrameworks = scanImportedHeaders(context);
    std::string filename = context.getSourceManager().getFilename(
        context.getSourceManager().getLocForStartOfFile(
            context.getSourceManager().getMainFileID()
        )
    ).str();

    fileFrameworks_[filename] = importedFrameworks;

    // 设置 AST Matcher - 匹配所有有方法体的 Objective-C 方法定义
    auto methodMatcher = objcMethodDecl(
        isDefinition()
    ).bind("method");

    finder_.addMatcher(methodMatcher, this);

    // 执行匹配！这是关键步骤
    finder_.matchAST(context);
}

// =============================================================================
// run - 处理匹配的方法
// =============================================================================

void DeadCodeInjectionStrategy::run(const MatchFinder::MatchResult& result) {
    const auto* methodDecl = result.Nodes.getNodeAs<ObjCMethodDecl>("method");
    if (!methodDecl) return;

    // 获取文件名
    SourceManager& sm = result.Context->getSourceManager();
    std::string filename = sm.getFilename(methodDecl->getLocation()).str();

    // 获取类名和方法名（用于调试）
    std::string className = methodDecl->getClassInterface() ?
        methodDecl->getClassInterface()->getName().str() : "unknown";
    std::string methodName = methodDecl->getSelector().getAsString();

    LOG_INFO("DeadCodeInjection::run - Found method: " + className + "::" + methodName + " in file: " + filename);

    // 跳过系统头文件和第三方库
    if (filename.empty() || filename.find("/usr/") == 0 ||
        isThirdPartySDK(filename)) {
        return;
    }

    // 跳过白名单中的类
    if (isWhitelisted(className, "class")) {
        return;
    }

    // 收集插入点
    auto points = collectInsertionPoints(methodDecl, *result.Context);

    // 【修复】收集所有插入点，密度控制在 collectReplacements() 中按插入点级别进行
    if (!points.empty()) {
        insertionPoints_[filename].insert(
            insertionPoints_[filename].end(),
            points.begin(), points.end()
        );
    }
}

// =============================================================================
// scanImportedHeaders - 扫描导入的头文件
// =============================================================================

std::set<std::string> DeadCodeInjectionStrategy::scanImportedHeaders(ASTContext& context) {
    std::set<std::string> frameworks;

    SourceManager& sm = context.getSourceManager();
    FileID mainFileID = sm.getMainFileID();

    // 获取源文件的词法序列
    auto bufferOpt = sm.getBufferOrNone(mainFileID);
    if (!bufferOpt) {
        return frameworks;
    }

    llvm::MemoryBufferRef buffer = *bufferOpt;

    const char* start = buffer.getBufferStart();
    const char* end = buffer.getBufferEnd();
    const char* pos = start;

    while (pos < end) {
        // 查找 #import 或 #include
        if (*pos == '#' && pos + 1 < end) {
            const char* directiveStart = pos;
            pos++;

            // 跳过空白
            while (pos < end && (*pos == ' ' || *pos == '\t')) pos++;

            // 检查是否是 import 或 include
            bool isImport = false;
            bool isInclude = false;

            if (pos + 5 < end && strncmp(pos, "import", 5) == 0) {
                isImport = true;
                pos += 5;
            } else if (pos + 7 < end && strncmp(pos, "include", 7) == 0) {
                isInclude = true;
                pos += 7;
            } else {
                continue;
            }

            // 跳过空白
            while (pos < end && (*pos == ' ' || *pos == '\t')) pos++;

            // 提取路径
            if (pos < end && (*pos == '"' || *pos == '<')) {
                char quote = *pos;
                pos++;  // 跳过引号
                const char* pathStart = pos;
                while (pos < end && *pos != quote && *pos != '\n') {
                    pos++;
                }
                std::string importPath(pathStart, pos);

                // 解析框架名称
                std::string framework = parseFrameworkName(importPath);
                if (!framework.empty()) {
                    frameworks.insert(framework);
                }
            }
        } else {
            pos++;
        }
    }

    return frameworks;
}

std::string DeadCodeInjectionStrategy::parseFrameworkName(const std::string& importPath) {
    // 解析框架名称
    // 例如: <Foundation/Foundation.h> -> Foundation
    //       <UIKit/UIKit.h> -> UIKit
    //       "AVFoundation/AVAsset.h" -> AVFoundation

    // 查找第一个 /
    size_t slashPos = importPath.find('/');
    if (slashPos != std::string::npos) {
        return importPath.substr(0, slashPos);
    }

    // 没有 / 的情况，可能是 <Foundation.h>
    size_t dotPos = importPath.find('.');
    if (dotPos != std::string::npos) {
        return importPath.substr(0, dotPos);
    }

    return "";
}

// =============================================================================
// collectInsertionPoints - 收集插入点
// =============================================================================

std::vector<InsertionPoint> DeadCodeInjectionStrategy::collectInsertionPoints(
    const ObjCMethodDecl* methodDecl,
    ASTContext& context) {

    std::vector<InsertionPoint> points;

    const Stmt* bodyStmt = methodDecl->getBody();
    if (!bodyStmt) return points;

    // 尝试转换为 CompoundStmt
    const CompoundStmt* body = dyn_cast<CompoundStmt>(bodyStmt);
    if (!body) return points;

    std::string className = methodDecl->getClassInterface()->getName().str();
    std::string methodName = methodDecl->getSelector().getAsString();
    bool isInstanceMethod = !methodDecl->isClassMethod();

    SourceManager& sm = context.getSourceManager();

    // 【修复】获取方法体的开始和结束位置（花括号位置）
    SourceLocation methodStartLoc = body->getLBracLoc();
    SourceLocation methodEndLoc = body->getRBracLoc();

    // 验证方法体边界是否有效
    if (!methodStartLoc.isValid() || !methodEndLoc.isValid()) {
        LOG_WARNING("DeadCodeInjection: Invalid method body boundaries for " + className + "::" + methodName);
        return points;
    }

    // 获取方法体边界的文件偏移量
    unsigned int startOffset = sm.getFileOffset(methodStartLoc);
    unsigned int endOffset = sm.getFileOffset(methodEndLoc);

    LOG_DEBUG("DeadCodeInjection: Method " + className + "::" + methodName +
              " body range: " + std::to_string(startOffset) + " - " + std::to_string(endOffset));

    // 【修复】标志：是否遇到 return 语句
    // 一旦遇到 return，后续代码永远不会执行，不应该作为插入点
    bool seenReturnStmt = false;

    // 遍历方法体中的语句
    for (const Stmt* stmt : body->body()) {
        if (!stmt) continue;

        // 【修复】检查是否是 ReturnStmt
        // return 语句后的代码永远不会执行，不应作为插入点
        if (isa<ReturnStmt>(stmt)) {
            seenReturnStmt = true;
            LOG_DEBUG("DeadCodeInjection: Found ReturnStmt in " + className + "::" + methodName +
                      ", skipping this and all subsequent statements");
            continue;
        }

        // 【修复】如果已经遇到 return，跳过后续所有语句
        if (seenReturnStmt) {
            LOG_DEBUG("DeadCodeInjection: Skipping statement after ReturnStmt (unreachable code)");
            continue;
        }

        // 【修复】跳过宏展开的语句
        // 宏展开的语句其 SourceLocation 可能指向宏定义位置（可能在文件开头）
        // 这会导致死代码被插入到错误的位置（全局作用域）
        SourceLocation stmtStartLoc = stmt->getBeginLoc();
        if (stmtStartLoc.isMacroID()) {
            // 对于宏展开的语句，获取拼写位置（宏调用的位置）
            stmtStartLoc = sm.getSpellingLoc(stmtStartLoc);
            // 如果拼写位置无效，跳过此语句
            if (!stmtStartLoc.isValid()) {
                LOG_DEBUG("DeadCodeInjection: Skipping macro statement with invalid spelling loc");
                continue;
            }
        }

        // 【修复】验证语句是否在方法体内
        // 检查语句的开始位置是否在方法的 { 和 } 之间
        unsigned int stmtOffset = sm.getFileOffset(stmtStartLoc);

        if (stmtOffset <= startOffset || stmtOffset >= endOffset) {
            LOG_WARNING("DeadCodeInjection: Skipping statement outside method body (offset: " +
                       std::to_string(stmtOffset) + ", method range: " +
                       std::to_string(startOffset) + " - " + std::to_string(endOffset) + ")");
            continue;  // 跳过不在方法体内的语句
        }

        // 获取语句的结束位置
        SourceLocation endLoc = stmt->getEndLoc();
        if (!endLoc.isValid()) continue;

        // 跳过宏定义和系统头文件中的代码
        if (endLoc.isMacroID()) {
            endLoc = sm.getSpellingLoc(endLoc);
        }

        // 【修复】找到分号之后的位置
        // 在 Clang AST 中，分号不是语句的一部分，需要单独查找
        // 使用 Lexer 获取 endLoc 之后的 token（应该是分号）
        SourceLocation insertLoc = endLoc;
        bool foundSemi = false;

        std::optional<Token> nextTok = Lexer::findNextToken(
            endLoc, sm, context.getLangOpts()
        );

        if (nextTok.has_value()) {
            const Token& tok = *nextTok;
            if (tok.is(tok::semi)) {
                // 找到分号，使用分号的位置
                // InsertTextAfterToken 会在分号之后插入
                insertLoc = tok.getLocation();
                foundSemi = true;
            } else if (tok.is(tok::r_brace)) {
                // 如果是右花括号，说明是块语句的最后，不需要分号
                insertLoc = endLoc;
            }
            // 其他情况（如逗号），使用 endLoc
        }

        InsertionPoint point;
        point.location = insertLoc;
        point.statement = stmt;
        point.methodName = methodName;
        point.className = className;
        point.isInstanceMethod = isInstanceMethod;

        points.push_back(point);
    }

    return points;
}

// =============================================================================
// shouldInsert - 根据密度决定是否插入
// =============================================================================

bool DeadCodeInjectionStrategy::shouldInsert() const {
    if (density_ <= 0.0) return false;
    if (density_ >= 1.0) return true;

    // 【修复】每次调用创建新的随机数生成器
    // 确保方法级别的密度控制真正随机
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < density_;
}

// =============================================================================
// generateContextualCode - 生成上下文相关的垃圾代码
// =============================================================================

std::string DeadCodeInjectionStrategy::generateContextualCode(
    const std::string& methodName,
    const std::string& className,
    bool isInstanceMethod,
    const std::set<std::string>& frameworks) {

    if (!generator_) return "";

    GenerationContext context;
    context.methodName = methodName;
    context.className = className;
    context.isInstanceMethod = isInstanceMethod;
    context.importedFrameworks = frameworks;  // 设置可用框架

    return generator_->generate(context);
}

// =============================================================================
// insertCodeAtLocation - 在指定位置插入代码
// =============================================================================

bool DeadCodeInjectionStrategy::insertCodeAtLocation(
    SourceLocation location,
    const std::string& code,
    Rewriter& rewriter) {

    if (!location.isValid()) return false;

    // 在位置后插入代码
    bool success = rewriter.InsertTextAfterToken(location, "\n" + code);
    return success;
}

// =============================================================================
// transform - 转换阶段
// =============================================================================

void DeadCodeInjectionStrategy::transform(ASTContext& context, Rewriter& rewriter) {
    SourceManager& sm = context.getSourceManager();
    std::string filename = sm.getFilename(
        sm.getLocForStartOfFile(sm.getMainFileID())
    ).str();

    // 获取该文件的插入点
    auto it = insertionPoints_.find(filename);
    if (it == insertionPoints_.end()) {
        return;
    }

    // 获取该文件导入的框架
    auto fwIt = fileFrameworks_.find(filename);
    std::set<std::string> frameworks;
    if (fwIt != fileFrameworks_.end()) {
        frameworks = fwIt->second;
    }

    // iOS 项目默认添加 Foundation 和 UIKit（通常通过 .pch 预编译头导入）
    // 这是必须的，因为大部分 iOS 文件不会显式 #import <Foundation/Foundation.h>
    frameworks.insert("Foundation");
    frameworks.insert("UIKit");

    // 更新生成器的可用框架
    if (generator_) {
        std::vector<FrameworkInfo> frameworkInfos;
        for (const auto& fw : frameworks) {
            FrameworkInfo info;
            info.name = fw;
            frameworkInfos.push_back(info);
        }
        generator_->setAvailableFrameworks(frameworkInfos);
    }

    // 限制每个方法的插入数量
    std::map<std::string, int> methodInsertionCount;

    // 随机打乱插入点
    std::vector<InsertionPoint> points = it->second;
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(points.begin(), points.end(), g);

    // 插入代码
    for (const auto& point : points) {
        std::string methodKey = point.className + "::" + point.methodName;

        if (methodInsertionCount[methodKey] >= maxStatementsPerMethod_) {
            continue;
        }

        // 生成垃圾代码（传递可用框架）
        std::string deadCode = generateContextualCode(
            point.methodName,
            point.className,
            point.isInstanceMethod,
            frameworks  // 传递当前文件的可用框架
        );

        if (!deadCode.empty()) {
            if (insertCodeAtLocation(point.location, deadCode, rewriter)) {
                methodInsertionCount[methodKey]++;
            }
        }
    }
}

// =============================================================================
// collectReplacements - 收集插入操作
// =============================================================================

void DeadCodeInjectionStrategy::collectReplacements(
    clang::ASTContext& context,
    ReplacementManager& manager) {

    SourceManager& sm = context.getSourceManager();
    std::string filename = sm.getFilename(
        sm.getLocForStartOfFile(sm.getMainFileID())
    ).str();

    LOG_INFO("DeadCodeInjection::collectReplacements - Looking for insertion points for file: " + filename);
    LOG_INFO("  Total insertion points in map: " + std::to_string(insertionPoints_.size()));

    // 调试：打印所有已存储的文件名
    if (!insertionPoints_.empty()) {
        for (const auto& entry : insertionPoints_) {
            LOG_INFO("  Stored file: " + entry.first + " with " + std::to_string(entry.second.size()) + " points");
        }
    }

    // 获取该文件的插入点
    auto it = insertionPoints_.find(filename);
    if (it == insertionPoints_.end()) {
        LOG_WARNING("DeadCodeInjection: No insertion points found for file: " + filename);
        return;
    }

    // 获取该文件导入的框架
    auto fwIt = fileFrameworks_.find(filename);
    std::set<std::string> frameworks;
    if (fwIt != fileFrameworks_.end()) {
        frameworks = fwIt->second;
    }

    // iOS 项目默认添加 Foundation 和 UIKit（通常通过 .pch 预编译头导入）
    // 这是必须的，因为大部分 iOS 文件不会显式 #import <Foundation/Foundation.h>
    frameworks.insert("Foundation");
    frameworks.insert("UIKit");

    // 更新生成器的可用框架
    if (generator_) {
        std::vector<FrameworkInfo> frameworkInfos;
        for (const auto& fw : frameworks) {
            FrameworkInfo info;
            info.name = fw;
            frameworkInfos.push_back(info);
        }
        generator_->setAvailableFrameworks(frameworkInfos);
    }

    // 限制每个方法的插入数量
    std::map<std::string, int> methodInsertionCount;

    // 随机打乱插入点
    std::vector<InsertionPoint> points = it->second;
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(points.begin(), points.end(), g);

    // 【修复】按方法分组插入点，实现方法级别的密度控制
    // density: 0.2 = 20% 的方法会被插入死代码，而不是 20% 的插入点
    std::map<std::string, std::vector<InsertionPoint>> pointsByMethod;
    for (const auto& point : points) {
        std::string methodKey = point.className + "::" + point.methodName;
        pointsByMethod[methodKey].push_back(point);
    }

    // 对每个方法决定是否插入（只检查一次密度）
    for (auto& entry : pointsByMethod) {
        const std::string& methodKey = entry.first;

        // 【关键】方法级别密度检查 - 每个方法只检查一次
        // density: 0.2 表示 20% 的方法会被插入死代码
        if (!shouldInsert()) {
            continue;  // 跳过整个方法
        }

        // 【修复】随机决定插入条数，而不是插入到上限
        // 计算实际可插入的最大条数
        size_t availablePoints = entry.second.size();
        int actualMax = std::min(availablePoints, (size_t)maxStatementsPerMethod_);

        if (actualMax < 1) {
            continue;  // 没有可用插入点
        }

        // 随机决定插入条数（1 到 actualMax，均匀分布）
        // 例如：max=3，可用点≥3 → 插入 1/2/3 条的概率各为 1/3
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> dist(1, actualMax);
        int targetCount = dist(rng);  // 随机选择插入条数

        // 插入指定条数的死代码
        int currentCount = 0;
        for (const auto& point : entry.second) {
            if (currentCount >= targetCount) {
                break;  // 达到目标条数，停止插入
            }

            // 生成垃圾代码（传递可用框架）
            std::string deadCode = generateContextualCode(
                point.methodName,
                point.className,
                point.isInstanceMethod,
                frameworks
            );

            if (!deadCode.empty()) {
                std::string codeWithNewline = "\n" + deadCode;
                manager.addInsertion(point.location, codeWithNewline, true, 0, "DeadCodeInjection");
                currentCount++;
            }
        }

        // 记录该方法插入了死代码（用于日志统计）
        if (currentCount > 0) {
            methodInsertionCount[methodKey] = currentCount;
        }
    }

    LOG_INFO("DeadCodeInjection: Added " + std::to_string(methodInsertionCount.size()) +
             " method(s) with dead code insertions");
}

// =============================================================================
// validate - 验证阶段
// =============================================================================

bool DeadCodeInjectionStrategy::validate(ASTContext& context) const {
    // 基本验证：检查是否有插入点
    return !insertionPoints_.empty();
}

} // namespace obfuscator
