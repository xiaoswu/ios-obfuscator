#ifndef SDK_NAME_STRATEGY_H
#define SDK_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include <filesystem>
#include <string>

namespace obfuscator {

/**
 * @brief SDK名称混淆策略
 *
 * 负责重命名.xcodeproj文件并更新project.pbxproj中的路径引用
 * 同时更新源代码文件中的SDK名称引用
 * 重命名Public Headers目录为与主目录相同的名称
 */
class SDKNameStrategy : public ObfuscationStrategy {
public:
    SDKNameStrategy();
    virtual ~SDKNameStrategy() = default;

    std::string getName() const override { return "SDKName"; }
    std::string getDescription() const override { return "Obfuscate SDK name (rename .xcodeproj)"; }

    // 执行SDK名称混淆
    // outputDir: 输出目录
    // originalSdkName: 原始SDK名称，用于在project.pbxproj中查找和替换
    void execute(const std::filesystem::path& outputDir, const std::string& originalSdkName);

protected:
    void analyze(clang::ASTContext& context) override {}
    void transform(clang::ASTContext& context, clang::Rewriter& rewriter) override {}
    bool validate(clang::ASTContext& context) const override { return true; }

    // 查找混淆后的项目目录名称
    static std::string findObfuscatedProjectName(const std::filesystem::path& outputDir);

    // 更新project.pbxproj文件中的路径引用
    static void updateProjectFile(const std::filesystem::path& projectFile,
                                  const std::filesystem::path& outputDir,
                                  const std::string& oldName,
                                  const std::string& newName);

    // 重命名Public Headers目录为与主目录相同的名称
    static void renamePublicHeadersDir(const std::filesystem::path& outputDir,
                                       const std::string& obfuscatedName);

    // 更新源代码文件中的SDK名称引用
    static void updateSourceFiles(const std::filesystem::path& outputDir,
                                  const std::string& oldName,
                                  const std::string& newName);
};

} // namespace obfuscator

#endif // SDK_NAME_STRATEGY_H

