/**
 * @file main.cpp
 * @brief iOS混淆工具主程序（重构版）
 * 
 * 命令行工具入口，负责：
 * 1. 解析命令行参数
 * 2. 加载配置文件
 * 3. 初始化混淆工具
 * 4. 执行混淆操作
 */

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <map>
#include "core/ConfigManager.h"
#include "core/Logger.h"
#include "core/SymbolTable.h"
#include "core/NameGenerator.h"
#include "core/StrategyManager.h"
#include "core/CompileOptions.h"
#include "strategies/FileNameStrategy.h"
#include "strategies/SDKNameStrategy.h"
#include "strategies/ClassNameFolderStrategy.h"
#include "strategies/CommentRemovalStrategy.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"
#include <memory>

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory ObfuscatorCategory("iOS Obfuscator Options");

static llvm::cl::opt<std::string> ConfigFile(
    "config",
    llvm::cl::desc("Path to configuration file (JSON format)"),
    llvm::cl::value_desc("file"),
    llvm::cl::init("config.json"),
    llvm::cl::cat(ObfuscatorCategory));

static llvm::cl::opt<std::string> InputPath(
    "input",
    llvm::cl::desc("Input SDK source path"),
    llvm::cl::value_desc("path"),
    llvm::cl::cat(ObfuscatorCategory));

static llvm::cl::opt<std::string> OutputPath(
    "output",
    llvm::cl::desc("Output path for obfuscated code"),
    llvm::cl::value_desc("path"),
    llvm::cl::cat(ObfuscatorCategory));

static llvm::cl::opt<bool> Verbose(
    "verbose",
    llvm::cl::desc("Enable verbose output (debug level logging)"),
    llvm::cl::init(false),
    llvm::cl::cat(ObfuscatorCategory));

/**
 * @brief AST消费者 - 负责在AST分析完成后执行混淆策略
 * 【两阶段处理】支持收集模式和应用模式
 */
class ObfuscatorASTConsumer : public ASTConsumer {
public:
    enum Mode {
        CollectionMode,   // 收集阶段：只执行 analyze，收集符号
        ApplicationMode   // 应用阶段：执行 collectReplacements，应用混淆
    };

    ObfuscatorASTConsumer(obfuscator::StrategyManager* strategyManager,
                         clang::Rewriter& rewriter,
                         const std::string& currentFile,
                         Mode mode = CollectionMode)
        : strategyManager_(strategyManager), rewriter_(rewriter),
          currentFile_(currentFile), mode_(mode) {}

    void HandleTranslationUnit(ASTContext& context) override {
        if (!strategyManager_) {
            return;
        }

        if (mode_ == CollectionMode) {
            // 收集阶段：只执行 analyze，不修改代码
            obfuscator::LOG_INFO("Collection phase - Analyzing: " + currentFile_);
            strategyManager_->analyzeAll(context);
        } else {
            // 应用阶段：执行 collectReplacements，应用混淆
            obfuscator::LOG_INFO("Application phase - Processing: " + currentFile_);
            strategyManager_->collectReplacementsAll(context, rewriter_);
            strategyManager_->validateAll(context);
        }
    }

private:
    obfuscator::StrategyManager* strategyManager_;
    clang::Rewriter& rewriter_;
    std::string currentFile_;
    Mode mode_;
};

/**
 * @brief 混淆工具FrontendAction
 * 【两阶段处理】支持收集模式和应用模式
 */
class ObfuscatorFrontendAction : public ASTFrontendAction {
public:
    ObfuscatorFrontendAction(obfuscator::ConfigManager* config,
                            obfuscator::SymbolTable* symbolTable,
                            obfuscator::NameGenerator* nameGenerator,
                            std::map<std::string, std::string>* outputFiles = nullptr,
                            obfuscator::StrategyManager* strategyManager = nullptr,
                            ObfuscatorASTConsumer::Mode mode = ObfuscatorASTConsumer::CollectionMode)
        : config_(config), symbolTable_(symbolTable), nameGenerator_(nameGenerator),
          outputFiles_(outputFiles), mode_(mode) {
        if (strategyManager) {
            strategyManager_ = nullptr;
            externalStrategyManager_ = strategyManager;
        } else {
            strategyManager_ = std::make_unique<obfuscator::StrategyManager>(
                config, symbolTable, nameGenerator);
            externalStrategyManager_ = strategyManager_.get();
        }
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override {
        currentFile_ = file.str();
        rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<ObfuscatorASTConsumer>(externalStrategyManager_, rewriter_, currentFile_, mode_);
    }

    void EndSourceFileAction() override {
        // 只有在应用阶段才输出混淆后的代码
        if (outputFiles_ && !currentFile_.empty() && mode_ == ObfuscatorASTConsumer::ApplicationMode) {
            const llvm::RewriteBuffer* buffer = rewriter_.getRewriteBufferFor(rewriter_.getSourceMgr().getMainFileID());
            if (buffer) {
                // 有替换，使用混淆后的代码
                std::string rewrittenCode;
                llvm::raw_string_ostream stream(rewrittenCode);
                buffer->write(stream);
                stream.flush();
                (*outputFiles_)[currentFile_] = rewrittenCode;
            } else {
                // 没有替换，读取原始文件内容（用于 CommentRemoval 等策略）
                auto fileContent = rewriter_.getSourceMgr().getBufferData(rewriter_.getSourceMgr().getMainFileID());
                std::string originalCode(fileContent.begin(), fileContent.end());
                (*outputFiles_)[currentFile_] = originalCode;
            }
        }
    }

    bool BeginSourceFileAction(CompilerInstance& CI) override {
        return true;
    }

protected:
    obfuscator::ConfigManager* config_;
    obfuscator::SymbolTable* symbolTable_;
    obfuscator::NameGenerator* nameGenerator_;
    std::map<std::string, std::string>* outputFiles_;
    ObfuscatorASTConsumer::Mode mode_;
    std::unique_ptr<obfuscator::StrategyManager> strategyManager_;
    obfuscator::StrategyManager* externalStrategyManager_;
    clang::Rewriter rewriter_;
    std::string currentFile_;
};

class ObfuscatorActionFactory : public FrontendActionFactory {
public:
    ObfuscatorActionFactory(obfuscator::ConfigManager* config,
                           obfuscator::SymbolTable* symbolTable,
                           obfuscator::NameGenerator* nameGenerator,
                           std::map<std::string, std::string>* outputFiles,
                           obfuscator::StrategyManager* strategyManager = nullptr,
                           ObfuscatorASTConsumer::Mode mode = ObfuscatorASTConsumer::CollectionMode)
        : config_(config), symbolTable_(symbolTable), nameGenerator_(nameGenerator),
          outputFiles_(outputFiles), strategyManager_(strategyManager), mode_(mode) {}

    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<ObfuscatorFrontendAction>(
            config_, symbolTable_, nameGenerator_, outputFiles_, strategyManager_, mode_);
    }

private:
    obfuscator::ConfigManager* config_;
    obfuscator::SymbolTable* symbolTable_;
    obfuscator::NameGenerator* nameGenerator_;
    std::map<std::string, std::string>* outputFiles_;
    obfuscator::StrategyManager* strategyManager_;
    ObfuscatorASTConsumer::Mode mode_;
};

int main(int argc, const char** argv) {
    // 解析命令行参数
    llvm::cl::ParseCommandLineOptions(argc, argv, "iOS Obfuscator Tool\n");
    
    // 加载配置
    obfuscator::ConfigManager configManager;
    if (!configManager.loadFromFile(ConfigFile)) {
        obfuscator::LOG_WARNING("Failed to load config file: " + ConfigFile + ", using default configuration");
        // 继续使用默认配置运行，不退出
    } else {
        obfuscator::LOG_INFO("Config file loaded: " + ConfigFile);
    }
    
    // 设置日志级别
    if (Verbose) {
        obfuscator::Logger::getInstance().setLogLevel(obfuscator::LogLevel::DEBUG);
    }
    
    // 命令行参数覆盖配置文件
    if (!InputPath.empty()) {
        configManager.getConfig().sdk.inputPath = InputPath;
        obfuscator::LOG_INFO("Input path (from command line): " + InputPath);
    }
    if (!OutputPath.empty()) {
        configManager.getConfig().sdk.outputPath = OutputPath;
        obfuscator::LOG_INFO("Output path (from command line): " + OutputPath);
    }
    
    // 初始化核心组件
    obfuscator::NameGenerator nameGenerator(configManager.getConfig().obfuscation.namingRule);
    obfuscator::SymbolTable symbolTable(&nameGenerator);
    
    // 检测iOS SDK并构建编译参数
    std::string iosSDKPath = obfuscator::CompileOptions::detectIOSSDKPath();
    std::string inputPath = configManager.getConfig().sdk.inputPath;
    std::vector<std::string> compileArgs = obfuscator::CompileOptions::buildCompileArgs(iosSDKPath, inputPath);
    
    std::unique_ptr<CompilationDatabase> compilations = 
        std::make_unique<FixedCompilationDatabase>(".", compileArgs);
    
    // 获取源文件列表
    std::vector<std::string> sourcePaths;
    namespace fs = std::filesystem;
    
    if (!configManager.getConfig().sdk.inputPath.empty()) {
        obfuscator::LOG_INFO("Will scan source files from: " + configManager.getConfig().sdk.inputPath);
        
        std::string inputPath = configManager.getConfig().sdk.inputPath;
        try {
            fs::path inputDir(inputPath);
            if (inputDir.is_relative()) {
                inputDir = fs::absolute(inputDir);
            }
            
            if (fs::exists(inputDir) && fs::is_directory(inputDir)) {
                for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        if (ext == ".m" || ext == ".h" || ext == ".pch") {
                            std::string filePath = entry.path().string();
                            std::filesystem::path filePathObj(filePath);

                            // 跳过.framework目录中的文件（ThirdPart/ThirdChannel目录不再跳过）
                            if (!obfuscator::FileNameStrategy::isInFramework(filePathObj)) {
                                sourcePaths.push_back(filePath);
                            }
                        }
                    }
                }
                obfuscator::LOG_INFO("Found " + std::to_string(sourcePaths.size()) + " source files");
            } else {
                obfuscator::LOG_WARNING("Input path does not exist or is not a directory: " + inputPath);
            }
        } catch (const std::exception& e) {
            obfuscator::LOG_ERROR("Error scanning source files: " + std::string(e.what()));
        }
    }
    
    if (sourcePaths.empty()) {
        obfuscator::LOG_WARNING("No source files found, nothing to obfuscate");
    }
    
    // 用于存储每个文件的混淆后内容
    std::map<std::string, std::string> obfuscatedFiles;

    // 创建策略管理器并加载策略
    obfuscator::StrategyManager strategyManager(&configManager, &symbolTable, &nameGenerator);
    strategyManager.loadStrategies();
    obfuscator::LOG_INFO("Loaded obfuscation strategies");

    // 【两阶段处理】解决跨文件符号引用问题
    // 第一遍：收集阶段 - 分析所有文件，收集符号
    obfuscator::LOG_INFO("===== Phase 1: Collection (analyzing all files) =====");
    ClangTool collectionTool(*compilations, sourcePaths);
    ObfuscatorActionFactory collectionFactory(
        &configManager, &symbolTable, &nameGenerator,
        nullptr, &strategyManager,
        ObfuscatorASTConsumer::CollectionMode);  // 收集模式
    int collectionResult = collectionTool.run(&collectionFactory);

    // 第二遍：应用阶段 - 对所有文件应用混淆
    obfuscator::LOG_INFO("===== Phase 2: Application (applying obfuscation) =====");
    ClangTool applicationTool(*compilations, sourcePaths);
    ObfuscatorActionFactory applicationFactory(
        &configManager, &symbolTable, &nameGenerator,
        &obfuscatedFiles, &strategyManager,
        ObfuscatorASTConsumer::ApplicationMode);  // 应用模式
    int result = applicationTool.run(&applicationFactory);
    
    // 执行混淆策略
    if (!obfuscatedFiles.empty() || !sourcePaths.empty()) {
        std::string outputPath = configManager.getConfig().sdk.outputPath;
        if (outputPath.empty()) {
            outputPath = "./obfuscated";
        }
        
        fs::path outputDir(outputPath);
        if (outputDir.is_relative()) {
            outputDir = fs::absolute(outputDir);
        }
        
        std::string inputPath = configManager.getConfig().sdk.inputPath;
        fs::path inputDir(inputPath);
        if (inputDir.is_relative()) {
            inputDir = fs::absolute(inputDir);
        }
        
        // 创建策略实例
        obfuscator::FileNameStrategy fileNameStrategy;
        fileNameStrategy.initialize(&configManager, &symbolTable, &nameGenerator);

        obfuscator::SDKNameStrategy sdkNameStrategy;
        sdkNameStrategy.initialize(&configManager, &symbolTable, &nameGenerator);

        obfuscator::ClassNameFolderStrategy classNameFolderStrategy;
        classNameFolderStrategy.initialize(&configManager, &symbolTable, &nameGenerator);

        // 第一步：复制工程结构（包含.framework）
        obfuscator::LOG_INFO("Copying project structure from " + inputDir.string() + " to " + outputDir.string());
        obfuscator::FileNameStrategy::copyProjectStructure(inputDir, outputDir);

        // 第二步：执行文件名混淆
        // 文件名混淆在以下情况执行：
        // 1. 用户明确启用了 FileNameObfuscation 策略
        // 2. 用户启用了 ClassNameObfuscation（自动重命名匹配的文件）
        bool shouldObfuscateFileNames = configManager.isStrategyEnabled("FileNameObfuscation") ||
                                        configManager.isStrategyEnabled("ClassNameObfuscation");
        int filesWritten = 0;

        // 【新】注释移除：在写入文件之前移除所有注释
        // publicHeaders 中的文件保留注释
        if (configManager.isStrategyEnabled("CommentRemoval")) {
            obfuscator::LOG_INFO("Removing comments from source files");
            int removedCount = 0;
            int skippedCount = 0;
            for (auto& [filePath, code] : obfuscatedFiles) {
                if (configManager.isPublicHeader(filePath)) {
                    skippedCount++;
                    continue;  // 跳过 public header，保留注释
                }
                code = obfuscator::CommentRemovalStrategy::removeComments(code);
                removedCount++;
            }
            obfuscator::LOG_INFO("Comments removed from " + std::to_string(removedCount) + " files");
            if (skippedCount > 0) {
                obfuscator::LOG_INFO("Preserved comments in " + std::to_string(skippedCount) + " public header files");
            }
        }

        if (shouldObfuscateFileNames) {
            obfuscator::LOG_INFO("Executing file name obfuscation");
            filesWritten = fileNameStrategy.execute(obfuscatedFiles, inputDir, outputDir);
        } else {
            // 即使不执行文件名混淆，也需要写出混淆后的代码内容
            obfuscator::LOG_INFO("Writing obfuscated source files (without renaming)");
            filesWritten = fileNameStrategy.writeObfuscatedContent(obfuscatedFiles, inputDir, outputDir);
        }

        // 第三步：执行文件夹混淆（仅在用户明确启用时执行）
        if (configManager.isStrategyEnabled("FolderNameObfuscation")) {
            obfuscator::LOG_INFO("Executing folder name obfuscation");
            classNameFolderStrategy.execute(outputDir);
        } else {
            obfuscator::LOG_INFO("Folder name obfuscation skipped (not enabled)");
        }

        // 第四步：执行SDK名称混淆（仅在用户明确启用时执行）
        if (configManager.isStrategyEnabled("SDKNameObfuscation")) {
            std::string sdkName = configManager.getConfig().sdk.name;
            obfuscator::LOG_INFO("Executing SDK name obfuscation");
            sdkNameStrategy.execute(outputDir, sdkName);
        } else {
            obfuscator::LOG_INFO("SDK name obfuscation skipped (not enabled)");
        }

        // 第五步：更新 pbxproj 中的 GCC_PREFIX_HEADER（在所有目录重命名后）
        obfuscator::LOG_INFO("Updating GCC_PREFIX_HEADER in pbxproj files");
        fileNameStrategy.updatePbxprojPrefixHeader(inputDir, outputDir);

        obfuscator::LOG_INFO("Written " + std::to_string(filesWritten) + " obfuscated files to: " + outputDir.string());
        obfuscator::LOG_INFO("Symbol count: " + std::to_string(symbolTable.getSymbolCount()));
        
        if (result == 0) {
            obfuscator::LOG_INFO("Obfuscation completed successfully");
        } else {
            obfuscator::LOG_WARNING("Obfuscation completed with errors, but " + 
                                   std::to_string(filesWritten) + " files were written");
        }
    } else {
        if (result == 0) {
            obfuscator::LOG_INFO("Obfuscation completed successfully (no files to write)");
        } else {
            obfuscator::LOG_ERROR("Obfuscation failed - no files processed");
        }
    }
    
    return result;
}

