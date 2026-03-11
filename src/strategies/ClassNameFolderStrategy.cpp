#include "strategies/ClassNameFolderStrategy.h"
#include "core/Logger.h"
#include "core/NameGenerator.h"
#include <filesystem>
#include <algorithm>
#include <vector>

namespace obfuscator {

ClassNameFolderStrategy::ClassNameFolderStrategy() {
}

bool ClassNameFolderStrategy::isInFramework(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    fs::path checkPath = path;

    while (!checkPath.empty() && checkPath != checkPath.root_path()) {
        std::string dirName = checkPath.filename().string();

        // 使用 rfind 检查目录名是否以 .framework 结尾（更健壮）
        // ".framework" 长度为 10
        size_t pos = dirName.rfind(".framework");
        if (pos != std::string::npos && pos == dirName.length() - 10) {
            return true;
        }

        checkPath = checkPath.parent_path();
    }
    return false;
}

bool ClassNameFolderStrategy::isInXcodeproj(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    fs::path checkPath = path;

    while (!checkPath.empty() && checkPath != checkPath.root_path()) {
        std::string dirName = checkPath.filename().string();

        // 使用 rfind 检查目录名是否以 .xcodeproj 结尾
        // ".xcodeproj" 长度为 10
        size_t pos = dirName.rfind(".xcodeproj");
        if (pos != std::string::npos && pos == dirName.length() - 10) {
            return true;
        }

        checkPath = checkPath.parent_path();
    }
    return false;
}

void ClassNameFolderStrategy::execute(const std::filesystem::path& outputDir) {
    namespace fs = std::filesystem;
    
    LOG_INFO("Renaming all folders");
    try {
        // 收集需要重命名的文件夹（从后往前处理，避免路径问题）
        std::vector<std::pair<fs::path, fs::path>> foldersToRename;
        
        for (const auto& entry : fs::recursive_directory_iterator(outputDir)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();
                fs::path entryPath = entry.path();
                
                // 首先检查路径是否在.framework目录中（包括.framework本身及其内部所有目录）
                // 这个检查必须在最前面，确保.framework及其内部所有目录都被跳过
                if (isInFramework(entryPath)) {
                    // 减少日志输出，提高性能
                    continue;
                }

                // 检查路径是否在.xcodeproj目录中（包括.xcodeproj本身及其内部所有目录）
                // .xcodeproj 内部结构不能被修改，否则 Xcode 无法打开项目
                if (isInXcodeproj(entryPath)) {
                    // 减少日志输出，提高性能
                    continue;
                }
                
                // 跳过隐藏目录（以.开头的目录，除了.git）
                if (dirName[0] == '.' && dirName != ".git") {
                    continue;
                }
                
                // 跳过根目录本身
                if (entryPath == outputDir) {
                    continue;
                }
                
                // 为所有符合条件的文件夹生成混淆名称
                fs::path oldPath = entryPath;
                fs::path parentPath = oldPath.parent_path();
                std::string obfuscatedName = nameGenerator_->generate(dirName, "folder");
                fs::path newPath = parentPath / obfuscatedName;
                
                foldersToRename.push_back({oldPath, newPath});
                // 减少日志输出，提高性能（只在DEBUG模式下输出详细信息）
            }
        }
        
        // 从后往前排序（按路径深度），避免重命名父目录时影响子目录
        std::sort(foldersToRename.begin(), foldersToRename.end(),
                 [](const std::pair<fs::path, fs::path>& a, const std::pair<fs::path, fs::path>& b) {
                     return a.first.string().length() > b.first.string().length();
                 });
        
        // 批量重命名文件夹（减少日志输出，提高性能）
        size_t renamedCount = 0;
        for (const auto& [oldPath, newPath] : foldersToRename) {
            if (oldPath != newPath && fs::exists(oldPath) && !fs::exists(newPath)) {
                fs::rename(oldPath, newPath);
                renamedCount++;
            }
        }
        LOG_INFO("Renamed " + std::to_string(renamedCount) + " folders");
    } catch (const std::exception& e) {
        LOG_ERROR("Error renaming folders: " + std::string(e.what()));
    }
}

} // namespace obfuscator

