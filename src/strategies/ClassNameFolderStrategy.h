#ifndef CLASS_NAME_FOLDER_STRATEGY_H
#define CLASS_NAME_FOLDER_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include <filesystem>
#include <map>
#include <string>

namespace obfuscator {

    /**
     * @brief 文件夹混淆策略
     * 
     * 负责重命名所有文件夹（不依赖类名映射）
     */
    class ClassNameFolderStrategy : public ObfuscationStrategy {
public:
    ClassNameFolderStrategy();
    virtual ~ClassNameFolderStrategy() = default;
    
    std::string getName() const override { return "FolderName"; }
    std::string getDescription() const override { return "Obfuscate all folder names"; }
    
    // 执行文件夹混淆
    // outputDir: 输出目录
    void execute(const std::filesystem::path& outputDir);

protected:
    void analyze(clang::ASTContext& context) override {}
    void transform(clang::ASTContext& context, clang::Rewriter& rewriter) override {}
    bool validate(clang::ASTContext& context) const override { return true; }

    // 检查路径是否在.framework目录中
    static bool isInFramework(const std::filesystem::path& path);

    // 检查路径是否在.xcodeproj目录中
    static bool isInXcodeproj(const std::filesystem::path& path);
};

} // namespace obfuscator

#endif // CLASS_NAME_FOLDER_STRATEGY_H

