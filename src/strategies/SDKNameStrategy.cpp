#include "strategies/SDKNameStrategy.h"
#include "core/Logger.h"
#include "core/NameGenerator.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <vector>

namespace obfuscator {

SDKNameStrategy::SDKNameStrategy() {
}

std::string SDKNameStrategy::findObfuscatedProjectName(const std::filesystem::path& outputDir) {
    namespace fs = std::filesystem;

    // 首先尝试找到与原始SDK名称对应的混淆名称
    // 通过查找包含 .m 或 .h 文件的目录来确定主项目目录
    for (const auto& entry : fs::directory_iterator(outputDir)) {
        if (entry.is_directory()) {
            std::string dirName = entry.path().filename().string();

            // 跳过 .xcodeproj 目录
            if (dirName.find(".xcodeproj") != std::string::npos) {
                continue;
            }

            // 递归检查是否包含源文件（这是主项目目录的标志）
            bool hasSourceFiles = false;
            try {
                // 使用递归目录迭代器检查所有子目录
                for (const auto& subEntry : fs::recursive_directory_iterator(entry.path())) {
                    if (subEntry.is_regular_file()) {
                        std::string ext = subEntry.path().extension().string();
                        if (ext == ".m" || ext == ".h" || ext == ".swift") {
                            hasSourceFiles = true;
                            break;
                        }
                    }
                }
            } catch (...) {
                // 忽略无法访问的目录
            }

            if (hasSourceFiles) {
                LOG_INFO("Found obfuscated project directory: " + dirName);
                return dirName;
            }
        }
    }

    // 如果找不到，返回空字符串
    LOG_WARNING("Could not find obfuscated project directory");
    return "";
}

void SDKNameStrategy::updateProjectFile(const std::filesystem::path& projectFile,
                                       const std::filesystem::path& outputDir,
                                       const std::string& oldName,
                                       const std::string& newName) {
    namespace fs = std::filesystem;

    LOG_INFO("Updating project file: " + projectFile.string());
    LOG_INFO("Replacing '" + oldName + "' with '" + newName + "'");

    try {
        // 读取原文件内容
        std::ifstream inFile(projectFile);
        if (!inFile.is_open()) {
            LOG_ERROR("Failed to open project file for reading: " + projectFile.string());
            return;
        }

        std::stringstream buffer;
        buffer << inFile.rdbuf();
        std::string content = buffer.str();
        inFile.close();

        int replaceCount = 0;

        // 收集所有需要替换的变体
        std::vector<std::string> variantsToReplace;

        // 1. 添加原始名称
        variantsToReplace.push_back(oldName);

        // 2. 查找 PBXNativeTarget 区域，找到 target 名称
        // 格式: 413308CB24B2FBF700E828E4 /* HYSDK */ = {
        size_t targetStartPos = content.find("/* Begin PBXNativeTarget section */");
        if (targetStartPos != std::string::npos) {
            size_t targetEndPos = content.find("/* End PBXNativeTarget section */", targetStartPos);
            if (targetEndPos != std::string::npos) {
                std::string targetSection = content.substr(targetStartPos, targetEndPos - targetStartPos);

                // 查找所有 /* XXXX */ = { 模式，其中 XXXX 包含 SDK
                size_t pos = 0;
                while (pos < targetSection.length()) {
                    size_t commentStart = targetSection.find("/* ", pos);
                    if (commentStart == std::string::npos) break;

                    size_t commentEnd = targetSection.find(" */", commentStart);
                    if (commentEnd == std::string::npos) break;

                    std::string targetName = targetSection.substr(commentStart + 3, commentEnd - commentStart - 3);

                    // 如果包含 SDK，长度合理，且不是 newName
                    if (targetName.find("SDK") != std::string::npos &&
                        targetName.length() >= 4 && targetName.length() <= oldName.length() + 2 &&
                        targetName != newName && targetName.find(newName) == std::string::npos) {
                        // 检查是否已存在
                        bool exists = false;
                        for (const auto& v : variantsToReplace) {
                            if (v == targetName) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists) {
                            variantsToReplace.push_back(targetName);
                            LOG_INFO("Found target name in PBXNativeTarget: " + targetName);
                        }
                    }

                    pos = commentEnd + 3;
                }
            }
        }

        // 执行替换
        for (const auto& variant : variantsToReplace) {
            size_t pos = 0;
            while ((pos = content.find(variant, pos)) != std::string::npos) {
                content.replace(pos, variant.length(), newName);
                pos += newName.length();
                replaceCount++;
            }
        }

        // 3. 更新 Prefix Header 路径
        // 查找包含 .pch 文件的目录，并更新路径
        fs::path mainProjectDir = outputDir / newName;

        if (fs::exists(mainProjectDir) && fs::is_directory(mainProjectDir)) {
            // 在主项目目录下查找包含 .pch 文件的目录
            for (const auto& subEntry : fs::directory_iterator(mainProjectDir)) {
                if (subEntry.is_directory()) {
                    // 查找该目录下的任何 .pch 文件
                    for (const auto& fileEntry : fs::directory_iterator(subEntry.path())) {
                        if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".pch") {
                            std::string otherFolderObfuscatedName = subEntry.path().filename().string();
                            std::string pchFileName = fileEntry.path().filename().string();
                            LOG_INFO("Found pch folder: " + otherFolderObfuscatedName);
                            LOG_INFO("Found pch file: " + pchFileName);

                            // 尝试多种可能的旧路径格式进行替换
                            std::vector<std::string> oldPathsToReplace = {
                                "$(PROJECT_DIR)/" + oldName + "/Other/sdkpch.pch",
                                "$(PROJECT_DIR)/" + newName + "/Other/sdkpch.pch",
                                "$(PROJECT_DIR)/" + oldName + "/Other/" + oldName + "-Prefix.pch",
                                "$(PROJECT_DIR)/" + newName + "/Other/" + oldName + "-Prefix.pch"
                            };

                            std::string newPrefixPath = "$(PROJECT_DIR)/" + newName + "/" + otherFolderObfuscatedName + "/" + pchFileName;

                            for (const auto& oldPrefixPath : oldPathsToReplace) {
                                size_t pos = 0;
                                int prefixReplaceCount = 0;
                                while ((pos = content.find(oldPrefixPath, pos)) != std::string::npos) {
                                    content.replace(pos, oldPrefixPath.length(), newPrefixPath);
                                    pos += newPrefixPath.length();
                                    prefixReplaceCount++;
                                }

                                if (prefixReplaceCount > 0) {
                                    LOG_INFO("Updated " + std::to_string(prefixReplaceCount) + " Prefix Header paths: " +
                                            oldPrefixPath + " -> " + newPrefixPath);
                                }
                            }
                            break;
                        }
                    }
                    // 找到 .pch 文件后跳出外层循环
                    break;
                }
            }
        }

        // 写回文件
        std::ofstream outFile(projectFile);
        if (!outFile.is_open()) {
            LOG_ERROR("Failed to open project file for writing: " + projectFile.string());
            return;
        }

        outFile << content;
        outFile.close();

        LOG_INFO("Updated " + std::to_string(replaceCount) + " occurrences in project file");
    } catch (const std::exception& e) {
        LOG_ERROR("Error updating project file: " + std::string(e.what()));
    }
}

void SDKNameStrategy::execute(const std::filesystem::path& outputDir, const std::string& originalSdkName) {
    namespace fs = std::filesystem;

    // 第一步：找到混淆后的项目目录名称
    std::string obfuscatedName = findObfuscatedProjectName(outputDir);

    if (obfuscatedName.empty()) {
        LOG_WARNING("No obfuscated project name found, skipping .xcodeproj renaming");
        return;
    }

    // 第二步：重命名 .xcodeproj 文件
    LOG_INFO("Renaming .xcodeproj files");
    fs::path xcodeprojPath;
    try {
        for (const auto& entry : fs::directory_iterator(outputDir)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();

                // 检查是否是 .xcodeproj 目录
                if (dirName.size() >= 10 && dirName.substr(dirName.size() - 10) == ".xcodeproj") {
                    fs::path oldPath = entry.path();
                    fs::path parentPath = oldPath.parent_path();
                    fs::path newPath = parentPath / (obfuscatedName + ".xcodeproj");

                    if (oldPath != newPath) {
                        // 先更新 project.pbxproj 文件内容
                        fs::path projectFile = oldPath / "project.pbxproj";
                        if (fs::exists(projectFile)) {
                            updateProjectFile(projectFile, outputDir, originalSdkName, obfuscatedName);
                        }

                        // 重命名 .xcodeproj 目录
                        fs::rename(oldPath, newPath);
                        xcodeprojPath = newPath;
                        LOG_INFO("Renamed .xcodeproj: " + oldPath.filename().string() + " -> " + newPath.filename().string());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error renaming .xcodeproj files: " + std::string(e.what()));
    }

    // 第三步：重命名 Public Headers 目录为与主目录相同的名称
    LOG_INFO("Renaming public headers directory");
    renamePublicHeadersDir(outputDir, obfuscatedName);

    // 第四步：更新源代码文件中的 SDK 名称引用
    LOG_INFO("Updating source files with new SDK name");
    updateSourceFiles(outputDir, originalSdkName, obfuscatedName);
}

void SDKNameStrategy::renamePublicHeadersDir(const std::filesystem::path& outputDir,
                                              const std::string& obfuscatedName) {
    namespace fs = std::filesystem;

    try {
        // 首先找到混淆后的主项目目录（使用 obfuscatedName 直接定位）
        fs::path mainProjectDir = outputDir / obfuscatedName;

        if (!fs::exists(mainProjectDir) || !fs::is_directory(mainProjectDir)) {
            LOG_WARNING("Main project directory not found at: " + mainProjectDir.string());
            return;
        }

        LOG_INFO("Main project directory: " + mainProjectDir.string());

        // 在主项目目录下查找 Public Headers 目录（通过内容识别）
        // Public Headers 目录通常包含 .h 文件，但没有对应的 .m 文件
        for (const auto& subEntry : fs::directory_iterator(mainProjectDir)) {
            if (subEntry.is_directory()) {
                fs::path subDirPath = subEntry.path();
                std::string subDirName = subDirPath.filename().string();

                // 跳过已经与主目录同名的目录
                if (subDirName == obfuscatedName) {
                    continue;
                }

                // 检查这个目录是否只包含 .h 文件（没有 .m 文件）
                // 这是 Public Headers 目录的典型特征
                bool hasHeaderFiles = false;
                bool hasImplementationFiles = false;
                int headerCount = 0;

                try {
                    for (const auto& fileEntry : fs::directory_iterator(subDirPath)) {
                        if (fileEntry.is_regular_file()) {
                            std::string ext = fileEntry.path().extension().string();
                            if (ext == ".h") {
                                hasHeaderFiles = true;
                                headerCount++;
                            } else if (ext == ".m" || ext == ".mm" || ext == ".swift") {
                                hasImplementationFiles = true;
                                break;
                            }
                        }
                    }
                } catch (...) {
                    // 忽略无法访问的目录
                }

                // 如果有 .h 文件但没有 .m 文件，且文件数量合理，则认为是 Public Headers 目录
                if (hasHeaderFiles && !hasImplementationFiles && headerCount > 0 && headerCount < 50) {
                    fs::path newPath = mainProjectDir / obfuscatedName;

                    LOG_INFO("Found public headers directory: " + subDirName + " (" + std::to_string(headerCount) + " files)");

                    if (subDirPath != newPath) {
                        if (fs::exists(newPath)) {
                            // 目标目录已存在，需要合并内容
                            LOG_INFO("Merging public headers into: " + newPath.string());
                            for (const auto& fileEntry : fs::directory_iterator(subDirPath)) {
                                if (fileEntry.is_regular_file()) {
                                    fs::path destFile = newPath / fileEntry.path().filename();
                                    fs::copy_file(fileEntry.path(), destFile, fs::copy_options::overwrite_existing);
                                }
                            }
                            fs::remove_all(subDirPath);
                        } else {
                            // 重命名目录
                            fs::rename(subDirPath, newPath);
                            LOG_INFO("Renamed public headers directory: " + subDirName + " -> " + obfuscatedName);
                        }
                    }
                    break;  // 只处理第一个匹配的目录
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error renaming public headers directory: " + std::string(e.what()));
    }
}

void SDKNameStrategy::updateSourceFiles(const std::filesystem::path& outputDir,
                                        const std::string& oldName,
                                        const std::string& newName) {
    namespace fs = std::filesystem;

    // 需要替换的变体
    std::vector<std::string> variantsToReplace = {oldName};

    // 添加常见的 target 名称变体
    if (!oldName.empty() && oldName.length() >= 4) {
        // 提取前缀，生成可能的变体
        std::string prefix = oldName.substr(0, oldName.length() - 3);  // 去掉 "SDK"
        variantsToReplace.push_back(prefix + "SDK");
        variantsToReplace.push_back(prefix + "Sdk");
        variantsToReplace.push_back(prefix + "sdk");

        // 如果 oldName 以 H 开头，尝试 HYSDK 等
        if (oldName[0] == 'H') {
            variantsToReplace.push_back("HYSDK");
            variantsToReplace.push_back("HYSdk");
            variantsToReplace.push_back("HYsdk");
        }
    }

    int filesUpdated = 0;
    int totalReplacements = 0;

    try {
        // 递归遍历所有文件
        for (const auto& entry : fs::recursive_directory_iterator(outputDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();

                // 只处理源代码文件
                if (ext == ".h" || ext == ".m" || ext == ".mm" || ext == ".swift" ||
                    ext == ".cpp" || ext == ".hpp" || ext == ".c") {
                    fs::path filePath = entry.path();

                    // 读取文件内容
                    std::ifstream inFile(filePath);
                    if (!inFile.is_open()) continue;

                    std::stringstream buffer;
                    buffer << inFile.rdbuf();
                    std::string content = buffer.str();
                    inFile.close();

                    // 检查是否需要替换
                    bool needsUpdate = false;
                    for (const auto& variant : variantsToReplace) {
                        if (content.find(variant) != std::string::npos) {
                            needsUpdate = true;
                            break;
                        }
                    }

                    if (needsUpdate) {
                        // 执行替换
                        int fileReplaceCount = 0;
                        for (const auto& variant : variantsToReplace) {
                            size_t pos = 0;
                            while ((pos = content.find(variant, pos)) != std::string::npos) {
                                content.replace(pos, variant.length(), newName);
                                pos += newName.length();
                                fileReplaceCount++;
                            }
                        }

                        // 写回文件
                        if (fileReplaceCount > 0) {
                            std::ofstream outFile(filePath);
                            if (outFile.is_open()) {
                                outFile << content;
                                outFile.close();
                                filesUpdated++;
                                totalReplacements += fileReplaceCount;
                                LOG_INFO("Updated: " + filePath.string() + " (" + std::to_string(fileReplaceCount) + " replacements)");
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error updating source files: " + std::string(e.what()));
    }

    LOG_INFO("Updated " + std::to_string(filesUpdated) + " source files with " + std::to_string(totalReplacements) + " total replacements");
}

} // namespace obfuscator

