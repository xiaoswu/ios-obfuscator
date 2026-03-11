#ifndef FILE_NAME_STRATEGY_H
#define FILE_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include <filesystem>
#include <map>
#include <string>

namespace obfuscator {

/**
 * @brief 文件名混淆策略
 *
 * 负责混淆源文件的文件名，但跳过.framework目录中的文件
 * 只处理 .h, .m, .pch 文件，并更新 import 引用
 * 同时更新文件中的类名，保持 .h 和 .m 文件名一致
 */
class FileNameStrategy : public ObfuscationStrategy {
public:
    FileNameStrategy();
    virtual ~FileNameStrategy() = default;

    std::string getName() const override { return "FileName"; }
    std::string getDescription() const override { return "Obfuscate file names"; }

    // 执行文件名混淆
    // obfuscatedFiles: 混淆后的文件内容映射（文件路径 -> 混淆后的代码）
    // inputDir: 输入目录
    // outputDir: 输出目录
    // 返回：写入的文件数量
    int execute(const std::map<std::string, std::string>& obfuscatedFiles,
                const std::filesystem::path& inputDir,
                const std::filesystem::path& outputDir);

    // 只写出混淆后的代码内容，不重命名文件
    // 用于类名混淆但不需要文件名混淆的场景
    int writeObfuscatedContent(const std::map<std::string, std::string>& obfuscatedFiles,
                               const std::filesystem::path& inputDir,
                               const std::filesystem::path& outputDir);

    // 检查路径是否在.framework目录中
    static bool isInFramework(const std::filesystem::path& path);

    // 检查文件是否需要处理（只处理 .h, .m, .pch 文件）
    static bool shouldProcessFile(const std::filesystem::path& filePath);

    // 检查是否是系统类（用于区分分类的命名规则）
    static bool isSystemClass(const std::string& className);

    // 复制工程结构（包含.framework目录）
    static void copyProjectStructure(const std::filesystem::path& inputDir,
                                     const std::filesystem::path& outputDir);

    // 更新 pbxproj 文件中的 GCC_PREFIX_HEADER 设置
    // 在所有目录重命名后调用
    static void updatePbxprojPrefixHeader(const std::filesystem::path& inputDir,
                                          const std::filesystem::path& outputDir);

protected:
    void analyze(clang::ASTContext& context) override {}
    void transform(clang::ASTContext& context, clang::Rewriter& rewriter) override {}
    bool validate(clang::ASTContext& context) const override { return true; }

    // 更新代码中的 import 引用
    static std::string updateImportsInCode(const std::string& code,
                                          const std::map<std::string, std::string>& fileNameMap);

    // 更新代码中的类名
    static std::string updateClassNameInCode(const std::string& code,
                                             const std::string& originalClassName,
                                             const std::string& obfuscatedClassName);

    // 保存文件名映射到文件
    static void saveFileNameMapping(const std::map<std::string, std::string>& fileNameMap,
                                    const std::filesystem::path& outputPath);
};

} // namespace obfuscator

#endif // FILE_NAME_STRATEGY_H

