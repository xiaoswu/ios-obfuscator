#include "strategies/FileNameStrategy.h"
#include "strategies/CommentRemovalStrategy.h"
#include "core/Logger.h"
#include "core/NameGenerator.h"
#include "core/SymbolTable.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <regex>
#include <functional>

namespace obfuscator {

FileNameStrategy::FileNameStrategy() {
}

bool FileNameStrategy::isInFramework(const std::filesystem::path& path) {
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

bool FileNameStrategy::shouldProcessFile(const std::filesystem::path& filePath) {
    namespace fs = std::filesystem;
    std::string extension = filePath.extension().string();

    // 只处理 .h, .m, .pch 文件
    if (extension != ".h" && extension != ".m" && extension != ".pch") {
        return false;
    }

    // 检查文件名是否是第三方 SDK 的头文件（不处理）
    std::string fileName = filePath.filename().string();
    std::string fileNameLower = fileName;
    std::transform(fileNameLower.begin(), fileNameLower.end(), fileNameLower.begin(), ::tolower);

    // 第三方 SDK 白名单（这些文件不被混淆）
    std::vector<std::string> thirdPartyPrefixes = {
        "firebase",
        "firebaseapp",
        "firebasedatabase",
        "firebaseauth",
        "google",
        "gtm",
        "ggl",
        "fb",
        "fbsdk",
        "facebook"
    };

    for (const auto& prefix : thirdPartyPrefixes) {
        if (fileNameLower.find(prefix) == 0) {
            return false;  // 跳过第三方 SDK 文件
        }
    }

    return true;
}

bool FileNameStrategy::isSystemClass(const std::string& className) {
    // 常见的系统类前缀
    static const std::vector<std::string> systemPrefixes = {
        "UI", "CG", "CF", "NS", "CA", "CI", "CL",
        "MK", "SC", "SK", "GK", "GL", "GK",
        "AV", "MP", "CM", "Sec", "CT", "AT",
        "WK", "PK", "VN", "AR", "Reality",
        "Speech", "SFSpeech", "SF", "SKAd",
        "IN", "GC", "MC", "EK", "EventKit",
        "MapKit", "SceneKit", "SpriteKit"
    };

    for (const auto& prefix : systemPrefixes) {
        if (className.find(prefix) == 0) {
            return true;
        }
    }

    return false;
}

void FileNameStrategy::copyProjectStructure(const std::filesystem::path& inputDir,
                                           const std::filesystem::path& outputDir) {
    namespace fs = std::filesystem;

    // 检查目录或文件是否应该被跳过（构建产物、临时文件等）
    auto shouldSkip = [](const std::string& name, const fs::path& fullPath) -> bool {
        // 跳过隐藏文件和系统文件（保留.git）
        if (!name.empty() && name[0] == '.' && name != ".git") {
            return true;
        }

        // 跳过构建目录
        if (name == "build" || name.find("cmake-build-") == 0 ||
            name == "CMakeFiles" || name == "CMakeCache.txt" ||
            name == "cmake_install.cmake" || name == "Makefile" ||
            name == "compile_commands.json") {
            return true;
        }

        // 跳过 Xcode 构建产物
        if (name == "DerivedData" || name == "ModuleName.noindex") {
            return true;
        }

        // 跳过 .xcodeproj 内部构建数据（保留项目结构文件）
        std::string pathStr = fullPath.string();
        if (pathStr.find(".xcodeproj/") != std::string::npos) {
            // 只保留 project.pbxproj 和 project.xcworkspace
            std::string leaf = fullPath.filename().string();
            if (leaf != "project.pbxproj" &&
                leaf != "project.xcworkspace" &&
                leaf != "xcshareddata") {
                // 允许 xcuserdata 但跳过其内部
                if (leaf != "xcuserdata") {
                    return true;
                }
            }
        }

        // 跳过常见的临时和产物文件
        if (name.find(".o") == name.length() - 2 ||
            name.find(".a") == name.length() - 2 ||
            name.find(".dylib") == name.length() - 5 ||
            name.find(".framework") == name.length() - 9) {
            // 检查是否是用户自己的framework（在源码目录中）
            // 暂时不跳过，后续由shouldProcessFile判断
        }

        // 跳过 iOS 构建产物目录
        if (name.find("_deps") != std::string::npos) {
            return true;
        }

        return false;
    };

    try {
        if (fs::exists(outputDir)) {
            fs::remove_all(outputDir);
        }
        fs::create_directories(outputDir);

        // 递归复制所有文件和目录（包含.framework目录）
        std::function<void(const fs::path&, const fs::path&)> copyRecursive =
            [&](const fs::path& src, const fs::path& dst) {
                if (fs::is_directory(src)) {
                    // 检查是否应该跳过整个目录
                    std::string dirName = src.filename().string();
                    if (shouldSkip(dirName, src)) {
                        return;
                    }

                    fs::create_directories(dst);
                    for (const auto& entry : fs::directory_iterator(src)) {
                        std::string fileName = entry.path().filename().string();
                        if (shouldSkip(fileName, entry.path())) {
                            continue;
                        }
                        copyRecursive(entry.path(), dst / fileName);
                    }
                } else {
                    // 检查是否应该跳过文件
                    std::string fileName = src.filename().string();
                    if (shouldSkip(fileName, src)) {
                        return;
                    }
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
                }
            };

        for (const auto& entry : fs::directory_iterator(inputDir)) {
            std::string fileName = entry.path().filename().string();
            if (shouldSkip(fileName, entry.path())) {
                continue;
            }
            copyRecursive(entry.path(), outputDir / fileName);
        }

        LOG_INFO("Project structure copied successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Error copying project structure: " + std::string(e.what()));
    }
}

std::string FileNameStrategy::updateImportsInCode(const std::string& code,
                                                   const std::map<std::string, std::string>& fileNameMap) {
    std::string result = code;

    // 更新 #import "xxx.h" 和 #include "xxx.h" 形式的引用
    // 不更新 <xxx.h> 形式的引用（系统库和框架）

    for (const auto& [originalName, obfuscatedName] : fileNameMap) {
        // originalName 和 obfuscatedName 都包含扩展名
        // 例如: "MyClass.h" -> "OBF_abc123.h"

        // 替换 #import "xxx" 形式
        std::string importPattern = "#import \"" + originalName + "\"";
        std::string importReplacement = "#import \"" + obfuscatedName + "\"";

        size_t pos = 0;
        while ((pos = result.find(importPattern, pos)) != std::string::npos) {
            result.replace(pos, importPattern.length(), importReplacement);
            pos += importReplacement.length();
        }

        // 替换 #include "xxx" 形式
        std::string includePattern = "#include \"" + originalName + "\"";
        std::string includeReplacement = "#include \"" + obfuscatedName + "\"";

        pos = 0;
        while ((pos = result.find(includePattern, pos)) != std::string::npos) {
            result.replace(pos, includePattern.length(), includeReplacement);
            pos += includeReplacement.length();
        }

        // 同时处理只有基础名的情况（不带扩展名）
        std::string originalBase = originalName;
        size_t dotPos = originalName.find_last_of('.');
        if (dotPos != std::string::npos) {
            originalBase = originalName.substr(0, dotPos);
        }

        std::string obfuscatedBase = obfuscatedName;
        dotPos = obfuscatedName.find_last_of('.');
        if (dotPos != std::string::npos) {
            obfuscatedBase = obfuscatedName.substr(0, dotPos);
        }

        // 替换 #import "xxx" （不带扩展名，可能有人在代码里这样写）
        importPattern = "#import \"" + originalBase + "\"";
        importReplacement = "#import \"" + obfuscatedBase + "\"";

        pos = 0;
        while ((pos = result.find(importPattern, pos)) != std::string::npos) {
            // 确保后面不是 .h 或其他字符（避免重复替换）
            size_t endPos = pos + importPattern.length();
            if (endPos < result.length() && result[endPos] != '"') {
                continue;
            }
            result.replace(pos, importPattern.length(), importReplacement);
            pos += importReplacement.length();
        }
    }

    return result;
}

void FileNameStrategy::saveFileNameMapping(const std::map<std::string, std::string>& fileNameMap,
                                           const std::filesystem::path& outputPath) {
    namespace fs = std::filesystem;

    try {
        fs::path mappingFile = outputPath / "filename_mapping.json";
        std::ofstream outFile(mappingFile);

        if (!outFile.is_open()) {
            LOG_ERROR("Failed to open filename mapping file for writing: " + mappingFile.string());
            return;
        }

        outFile << "{\n";
        bool first = true;
        for (const auto& [original, obfuscated] : fileNameMap) {
            if (!first) {
                outFile << ",\n";
            }
            outFile << "  \"" << original << "\": \"" << obfuscated << "\"";
            first = false;
        }
        outFile << "\n}\n";
        outFile.close();

        LOG_INFO("Filename mapping saved to: " + mappingFile.string());
    } catch (const std::exception& e) {
        LOG_ERROR("Error saving filename mapping: " + std::string(e.what()));
    }
}

void FileNameStrategy::updatePbxprojPrefixHeader(const std::filesystem::path& inputDir,
                                                  const std::filesystem::path& outputDir) {
    namespace fs = std::filesystem;

    // 查找所有 .xcodeproj 目录
    std::vector<fs::path> pbxprojFiles;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(outputDir)) {
            if (entry.is_regular_file() && entry.path().filename() == "project.pbxproj") {
                pbxprojFiles.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error searching for pbxproj files: " + std::string(e.what()));
        return;
    }

    if (pbxprojFiles.empty()) {
        LOG_INFO("No pbxproj files found, skipping GCC_PREFIX_HEADER update");
        return;
    }

    // 查找混淆后的 pch 文件
    fs::path obfuscatedPchPath;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(outputDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pch") {
                obfuscatedPchPath = entry.path();
                break;
            }
        }
    } catch (...) {}

    if (obfuscatedPchPath.empty()) {
        LOG_INFO("No pch file found in output directory, skipping GCC_PREFIX_HEADER update");
        return;
    }

    // 处理每个 pbxproj 文件
    for (const auto& pbxprojPath : pbxprojFiles) {
        try {
            // 读取 pbxproj 文件内容
            std::ifstream inFile(pbxprojPath);
            if (!inFile.is_open()) {
                LOG_ERROR("Failed to open pbxproj file: " + pbxprojPath.string());
                continue;
            }

            std::stringstream buffer;
            buffer << inFile.rdbuf();
            inFile.close();

            std::string content = buffer.str();

            // 使用正则表达式匹配 GCC_PREFIX_HEADER 行
            std::regex prefixHeaderRegex("GCC_PREFIX_HEADER\\s*=\\s*\"([^\"]+)\"\\s*;");

            // 计算 pch 文件相对于 pbxproj 父目录的路径
            fs::path pbxprojParent = pbxprojPath.parent_path().parent_path();
            fs::path pchRelativeToPbxproj = fs::relative(obfuscatedPchPath, pbxprojParent);
            std::string newPath = pchRelativeToPbxproj.string();

            LOG_INFO("Updating GCC_PREFIX_HEADER:");
            LOG_INFO("  pch file: " + obfuscatedPchPath.string());
            LOG_INFO("  pbxproj parent: " + pbxprojParent.string());
            LOG_INFO("  new path: " + newPath);

            // 收集所有匹配项
            std::vector<std::pair<size_t, std::string>> replacements;
            std::sregex_iterator iter(content.begin(), content.end(), prefixHeaderRegex);
            std::sregex_iterator end;

            for (std::sregex_iterator i = iter; i != end; ++i) {
                std::smatch match = *i;
                std::string originalPath = match[1].str();
                std::string replaceStr = "GCC_PREFIX_HEADER = \"" + newPath + "\";";
                replacements.push_back({match.position(0), replaceStr});
            }

            // 从后向前替换（避免位置偏移问题）
            for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
                size_t matchPos = it->first;
                const std::string& replaceStr = it->second;

                // 找到原始匹配的结束位置
                size_t matchEnd = matchPos;
                while (matchEnd < content.length() && content[matchEnd] != ';') {
                    matchEnd++;
                }
                if (matchEnd < content.length()) {
                    matchEnd++; // 包含分号
                }

                content.replace(matchPos, matchEnd - matchPos, replaceStr);
            }

            // 写回文件
            std::ofstream outFile(pbxprojPath);
            if (outFile.is_open()) {
                outFile << content;
                outFile.close();
                LOG_INFO("Successfully updated GCC_PREFIX_HEADER in: " + pbxprojPath.string());
            } else {
                LOG_ERROR("Failed to write pbxproj file: " + pbxprojPath.string());
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing pbxproj file: " + pbxprojPath.string() + " - " + e.what());
        }
    }
}

std::string FileNameStrategy::updateClassNameInCode(const std::string& code,
                                                     const std::string& originalClassName,
                                                     const std::string& obfuscatedClassName) {
    std::string result = code;

    // 检查是否是分类文件（文件名包含 +）
    size_t plusPos = originalClassName.find('+');
    bool isCategory = (plusPos != std::string::npos);

    if (isCategory) {
        // 分类文件：格式为 "ClassName+CategoryName"
        // 保持 ClassName 不变，只混淆 CategoryName
        std::string baseClassName = originalClassName.substr(0, plusPos);
        std::string categoryName = originalClassName.substr(plusPos + 1);

        // 在混淆后的名称中也查找 +
        size_t obfPlusPos = obfuscatedClassName.find('+');
        std::string obfCategoryName = (obfPlusPos != std::string::npos) ?
                                     obfuscatedClassName.substr(obfPlusPos + 1) : obfuscatedClassName;

        // 替换 @interface ClassName(CategoryName) 和 @implementation ClassName(CategoryName)
        std::string interfacePattern = "@interface " + baseClassName + " (" + categoryName + ")";
        std::string interfaceReplacement = "@interface " + baseClassName + " (" + obfCategoryName + ")";

        size_t pos = 0;
        while ((pos = result.find(interfacePattern, pos)) != std::string::npos) {
            result.replace(pos, interfacePattern.length(), interfaceReplacement);
            pos += interfaceReplacement.length();
        }

        // 也处理没有空格的情况: @interface ClassName(CategoryName)
        interfacePattern = "@interface " + baseClassName + "(" + categoryName + ")";
        interfaceReplacement = "@interface " + baseClassName + "(" + obfCategoryName + ")";

        pos = 0;
        while ((pos = result.find(interfacePattern, pos)) != std::string::npos) {
            result.replace(pos, interfacePattern.length(), interfaceReplacement);
            pos += interfaceReplacement.length();
        }

        // 处理 @implementation
        std::string implPattern = "@implementation " + baseClassName + " (" + categoryName + ")";
        std::string implReplacement = "@implementation " + baseClassName + " (" + obfCategoryName + ")";

        pos = 0;
        while ((pos = result.find(implPattern, pos)) != std::string::npos) {
            result.replace(pos, implPattern.length(), implReplacement);
            pos += implReplacement.length();
        }

        implPattern = "@implementation " + baseClassName + "(" + categoryName + ")";
        implReplacement = "@implementation " + baseClassName + "(" + obfCategoryName + ")";

        pos = 0;
        while ((pos = result.find(implPattern, pos)) != std::string::npos) {
            result.replace(pos, implPattern.length(), implReplacement);
            pos += implReplacement.length();
        }

        LOG_INFO("Updated category: " + originalClassName + " -> " + obfuscatedClassName);
    } else {
        // 普通类：替换 @interface ClassName 和 @implementation ClassName
        // 以及 @interface ClassName() 和 @implementation ClassName
        // 同时替换代码中所有使用该类名的地方

        // 首先处理 @interface ClassName
        std::string interfacePattern = "@interface " + originalClassName;
        std::string interfaceReplacement = "@interface " + obfuscatedClassName;

        size_t pos = 0;
        while ((pos = result.find(interfacePattern, pos)) != std::string::npos) {
            // 确保后面不是字母或数字（避免误匹配如 MyClassTest）
            size_t endPos = pos + interfacePattern.length();
            if (endPos < result.length()) {
                char nextChar = result[endPos];
                if (isalnum(nextChar) || nextChar == '_') {
                    pos = endPos;
                    continue;
                }
            }
            result.replace(pos, interfacePattern.length(), interfaceReplacement);
            pos += interfaceReplacement.length();
        }

        // 处理 @implementation ClassName
        std::string implPattern = "@implementation " + originalClassName;
        std::string implReplacement = "@implementation " + obfuscatedClassName;

        pos = 0;
        while ((pos = result.find(implPattern, pos)) != std::string::npos) {
            size_t endPos = pos + implPattern.length();
            if (endPos < result.length()) {
                char nextChar = result[endPos];
                if (isalnum(nextChar) || nextChar == '_') {
                    pos = endPos;
                    continue;
                }
            }
            result.replace(pos, implPattern.length(), implReplacement);
            pos += implReplacement.length();
        }

        // 处理带括号的类别扩展 @interface ClassName()
        std::string interfaceExtPattern = "@interface " + originalClassName + "()";
        std::string interfaceExtReplacement = "@interface " + obfuscatedClassName + "()";

        pos = 0;
        while ((pos = result.find(interfaceExtPattern, pos)) != std::string::npos) {
            result.replace(pos, interfaceExtPattern.length(), interfaceExtReplacement);
            pos += interfaceExtReplacement.length();
        }

        // 处理带括号的类别扩展 @implementation ClassName()
        std::string implExtPattern = "@implementation " + originalClassName + "()";
        std::string implExtReplacement = "@implementation " + obfuscatedClassName + "()";

        pos = 0;
        while ((pos = result.find(implExtPattern, pos)) != std::string::npos) {
            result.replace(pos, implExtPattern.length(), implExtReplacement);
            pos += implExtReplacement.length();
        }

        // 处理代码中所有使用该类名的地方
        // 包括类型声明、变量声明、方法调用等
        // 例如：OtherClass *obj, [[OtherClass alloc] init], (OtherClass *)等

        pos = 0;
        std::string className = originalClassName;
        std::string obfName = obfuscatedClassName;

        while ((pos = result.find(className, pos)) != std::string::npos) {
            // 检查是否是一个独立的标识符
            size_t startPos = pos;
            size_t endPos = pos + className.length();

            // 检查前面的字符（应该是空白、标点符号或行首）
            bool validStart = false;
            if (startPos == 0) {
                validStart = true;
            } else {
                char prevChar = result[startPos - 1];
                if (isspace(prevChar) || prevChar == '*' || prevChar == '(' ||
                    prevChar == ')' || prevChar == ',' || prevChar == ';' ||
                    prevChar == '=' || prevChar == ':' || prevChar == '[' ||
                    prevChar == ']' || prevChar == '{' || prevChar == '}' ||
                    prevChar == '<' || prevChar == '>' || prevChar == '&' ||
                    prevChar == '+' || prevChar == '-' || prevChar == '!' ||
                    prevChar == '?' || prevChar == '|' || prevChar == '^') {
                    validStart = true;
                }
            }

            // 检查后面的字符（应该是空白、标点符号或行尾）
            bool validEnd = false;
            if (endPos >= result.length()) {
                validEnd = true;
            } else {
                char nextChar = result[endPos];
                if (isspace(nextChar) || nextChar == '*' || nextChar == '(' ||
                    nextChar == ')' || nextChar == ',' || nextChar == ';' ||
                    nextChar == '=' || nextChar == ':' || nextChar == '[' ||
                    nextChar == ']' || nextChar == '{' || nextChar == '}' ||
                    nextChar == '<' || nextChar == '>' || nextChar == '&' ||
                    nextChar == '+' || nextChar == '-' || nextChar == '!' ||
                    nextChar == '?' || nextChar == '|' || nextChar == '^' ||
                    nextChar == '.') {  // 添加点号，用于处理 ClassName.propertyName
                    validEnd = true;
                }
            }

            if (validStart && validEnd) {
                result.replace(pos, className.length(), obfName);
                pos += obfName.length();
            } else {
                pos++;
            }
        }
    }

    return result;
}

int FileNameStrategy::writeObfuscatedContent(const std::map<std::string, std::string>& obfuscatedFiles,
                                             const std::filesystem::path& inputDir,
                                             const std::filesystem::path& outputDir) {
    namespace fs = std::filesystem;

    // 这个方法只写出混淆后的代码内容，不进行文件重命名
    // 用于类名混淆但不需要文件名混淆的场景

    int filesWritten = 0;

    for (const auto& [filePath, obfuscatedCode] : obfuscatedFiles) {
        fs::path inputFile(filePath);

        // 计算相对于输入目录的相对路径
        fs::path relativePath = fs::relative(inputFile, inputDir);
        if (relativePath.empty() || relativePath.string() == ".") {
            relativePath = inputFile.filename();
        }

        // 检查是否在.framework目录中
        if (isInFramework(relativePath)) {
            continue;
        }

        // 检查是否是需要处理的文件类型
        if (!shouldProcessFile(inputFile)) {
            continue;
        }

        // 构建输出路径（保持原文件名）
        fs::path outputFile = outputDir / relativePath;

        // 确保目录存在
        fs::create_directories(outputFile.parent_path());

        // 修复 #import 语句
        std::string codeToWrite = obfuscatedCode;
        if (inputFile.extension() == ".m") {
            // 对于 .m 文件，检查是否需要添加对应的头文件
            fs::path headerFile = inputFile;
            headerFile.replace_extension(".h");

            // 检查头文件是否存在
            if (fs::exists(headerFile)) {
                std::string headerFileName = headerFile.filename().string();

                // 检查代码中是否已经包含了这个头文件
                if (codeToWrite.find("#import \"" + headerFileName + "\"") == std::string::npos &&
                    codeToWrite.find("#import <" + headerFileName + ">") == std::string::npos) {

                    // 找到第一个 #import 语句的位置，在它后面插入头文件
                    size_t importPos = codeToWrite.find("#import");
                    if (importPos != std::string::npos) {
                        // 找到这一行的结尾
                        size_t lineEnd = codeToWrite.find("\n", importPos);
                        if (lineEnd != std::string::npos) {
                            // 检查下一个字符是否是换行符（避免重复插入）
                            size_t nextLineStart = lineEnd + 1;
                            while (nextLineStart < codeToWrite.length() && codeToWrite[nextLineStart] == '\n') {
                                nextLineStart++;
                            }
                            // 在这一行后面插入新的 #import（带换行）
                            std::string newImport = "\n#import \"" + headerFileName + "\"\n";
                            codeToWrite.insert(lineEnd + 1, newImport);
                            LOG_INFO("Added #import \"" + headerFileName + "\" to " + inputFile.filename().string());
                        }
                    }
                }
            }
        }

        // 写出混淆后的代码
        std::ofstream outFile(outputFile);
        if (outFile.is_open()) {
            outFile << codeToWrite;
            outFile.close();
            filesWritten++;
        } else {
            LOG_ERROR("Failed to write file: " + outputFile.string());
        }
    }

    LOG_INFO("Written " + std::to_string(filesWritten) + " obfuscated files (without renaming)");
    return filesWritten;
}

int FileNameStrategy::execute(const std::map<std::string, std::string>& obfuscatedFiles,
                              const std::filesystem::path& inputDir,
                              const std::filesystem::path& outputDir) {
    namespace fs = std::filesystem;

    // 第一步：收集所有需要处理的文件，并生成文件名映射
    // key: 原文件名（带路径）, value: 混淆后的文件名
    std::map<std::string, std::string> fileNameMap;
    // key: 原基础名（不含扩展名）, value: 混淆后的基础名
    std::map<std::string, std::string> baseNameMap;
    // key: 原完整路径, value: 混淆后的完整路径
    std::map<std::string, std::string> filePathMap;
    // key: 原完整路径, value: 文件内容（从 obfuscatedFiles 或原始文件）
    std::map<std::string, std::string> fileContents;

    // 如果 obfuscatedFiles 不为空，使用其中的内容
    bool useObfuscatedFiles = !obfuscatedFiles.empty();

    // 收集文件名映射
    if (useObfuscatedFiles) {
        // 使用 obfuscatedFiles 中的文件
        for (const auto& [filePath, obfuscatedCode] : obfuscatedFiles) {
            fs::path inputFile(filePath);

            // 计算相对于输入目录的相对路径
            fs::path relativePath = fs::relative(inputFile, inputDir);
            if (relativePath.empty() || relativePath.string() == ".") {
                relativePath = inputFile.filename();
            }

            // 检查是否在.framework目录中
            if (isInFramework(relativePath)) {
                continue;
            }

            // 检查是否是需要处理的文件类型
            if (!shouldProcessFile(inputFile)) {
                continue;
            }

            std::string originalFileName = relativePath.filename().string();
            std::string baseName = relativePath.stem().string();
            std::string extension = relativePath.extension().string();

            // 生成或获取混淆后的基础名称（确保 .h 和 .m 使用相同的基础名）
            std::string obfuscatedBaseName;
            auto it = baseNameMap.find(baseName);
            if (it != baseNameMap.end()) {
                obfuscatedBaseName = it->second;
            } else {
                // 检查是否是分类文件（文件名包含 +）
                size_t plusPos = baseName.find('+');
                if (plusPos != std::string::npos) {
                    // 分类文件：格式为 "ClassName+CategoryName"
                    std::string className = baseName.substr(0, plusPos);
                    std::string categoryName = baseName.substr(plusPos + 1);
                    std::string obfCategoryName = nameGenerator_->generate(categoryName, "category");

                    // 检查基础类名是否是系统类
                    if (isSystemClass(className)) {
                        // 系统类的分类：保持类名不变，只混淆分类名
                        // 例如: UIView+MyCategory -> UIView+OBF_xyz
                        obfuscatedBaseName = className + "+" + obfCategoryName;
                    } else {
                        // 自定义类的分类：类名和分类名都混淆
                        // 需要先获取或生成混淆后的类名
                        std::string obfClassName;
                        auto classIt = baseNameMap.find(className);
                        if (classIt != baseNameMap.end()) {
                            // 从已有映射中获取混淆后的类名（去掉扩展名）
                            std::string existing = classIt->second;
                            // 检查是否也包含 + （可能是嵌套分类）
                            size_t existingPlus = existing.find('+');
                            if (existingPlus != std::string::npos) {
                                obfClassName = existing.substr(0, existingPlus);
                            } else {
                                obfClassName = existing;
                            }
                        } else {
                            // 检查类名是否在 SymbolTable 中
                            if (symbolTable_ && symbolTable_->hasSymbol(className)) {
                                obfClassName = symbolTable_->getObfuscatedName(className);
                            } else {
                                obfClassName = nameGenerator_->generate(className, "file");
                            }
                        }
                        // 例如: MyClass+MyCategory -> OBF_abc+OBF_xyz
                        obfuscatedBaseName = obfClassName + "+" + obfCategoryName;
                    }
                } else {
                    // 普通文件：检查基础名是否在 SymbolTable 中（作为类名）
                    // 如果存在，使用已混淆的类名；否则生成新的混淆名称
                    if (symbolTable_ && symbolTable_->hasSymbol(baseName)) {
                        obfuscatedBaseName = symbolTable_->getObfuscatedName(baseName);
                    } else {
                        obfuscatedBaseName = nameGenerator_->generate(baseName, "file");
                    }
                }
                baseNameMap[baseName] = obfuscatedBaseName;
            }

            std::string outputFileName = obfuscatedBaseName + extension;

            // 存储文件名映射（只存储文件名，不含路径）
            fileNameMap[originalFileName] = outputFileName;

            // 存储完整路径映射
            fs::path outputPath = relativePath.parent_path() / outputFileName;
            filePathMap[filePath] = outputPath.string();

            // 存储文件内容
            fileContents[filePath] = obfuscatedCode;
        }

        // 扫描输入目录，查找与 obfuscatedFiles 中文件配对的文件（如 .h/.m 配对）
        // 这些配对文件也需要被重命名，但它们的内容不需要修改（因为没有被 AST 处理）
        LOG_INFO("Scanning for paired files to rename");
        try {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                if (entry.is_regular_file()) {
                    fs::path filePath = entry.path();

                    // 计算相对于输入目录的相对路径
                    fs::path relativePath = fs::relative(filePath, inputDir);
                    if (relativePath.empty() || relativePath.string() == ".") {
                        relativePath = filePath.filename();
                    }

                    // 检查是否在.framework目录中
                    if (isInFramework(relativePath)) {
                        continue;
                    }

                    // 检查是否是需要处理的文件类型
                    if (!shouldProcessFile(filePath)) {
                        continue;
                    }

                    // 如果这个文件已经在 obfuscatedFiles 中，跳过（已经处理过了）
                    if (obfuscatedFiles.find(filePath.string()) != obfuscatedFiles.end()) {
                        continue;
                    }

                    // 检查这个文件的基础名是否在 baseNameMap 中
                    // 如果在，说明它是某个已处理文件的配对文件，需要重命名
                    std::string baseName = relativePath.stem().string();
                    auto it = baseNameMap.find(baseName);

                    if (it != baseNameMap.end()) {
                        std::string originalFileName = relativePath.filename().string();
                        std::string extension = relativePath.extension().string();
                        std::string obfuscatedBaseName = it->second;
                        std::string outputFileName = obfuscatedBaseName + extension;

                        // 存储文件名映射
                        fileNameMap[originalFileName] = outputFileName;

                        // 存储完整路径映射
                        fs::path outputPath = relativePath.parent_path() / outputFileName;
                        filePathMap[filePath.string()] = outputPath.string();

                        // 读取并存储文件内容
                        try {
                            std::ifstream inFile(filePath);
                            if (inFile.is_open()) {
                                std::stringstream buffer;
                                buffer << inFile.rdbuf();
                                fileContents[filePath.string()] = buffer.str();
                                inFile.close();
                            }
                        } catch (...) {
                            LOG_ERROR("Failed to read paired file: " + filePath.string());
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error scanning for paired files: " + std::string(e.what()));
        }

        // 扫描系统分类文件（如 UIView+Category）
        // 系统类名保持不变，只混淆分类名
        LOG_INFO("Scanning for system category files");

        // 第一步：收集所有系统分类的基础名
        std::map<std::string, std::string> systemCategoryMap; // className+categoryName -> obfuscated
        try {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                if (entry.is_regular_file()) {
                    fs::path filePath = entry.path();
                    fs::path relativePath = fs::relative(filePath, inputDir);
                    if (relativePath.empty() || relativePath.string() == ".") {
                        relativePath = filePath.filename();
                    }

                    if (isInFramework(relativePath)) continue;
                    if (!shouldProcessFile(filePath)) continue;
                    if (obfuscatedFiles.find(filePath.string()) != obfuscatedFiles.end()) continue;

                    std::string originalFileName = relativePath.filename().string();
                    if (fileNameMap.find(originalFileName) != fileNameMap.end()) continue;

                    std::string baseName = relativePath.stem().string();
                    size_t plusPos = baseName.find('+');
                    if (plusPos != std::string::npos) {
                        std::string className = baseName.substr(0, plusPos);
                        std::string categoryName = baseName.substr(plusPos + 1);

                        if (isSystemClass(className)) {
                            std::string key = className + "+" + categoryName;
                            if (systemCategoryMap.find(key) == systemCategoryMap.end()) {
                                std::string obfCategoryName = nameGenerator_->generate(categoryName, "category");
                                systemCategoryMap[key] = className + "+" + obfCategoryName;
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error collecting system category files: " + std::string(e.what()));
        }

        // 第二步：处理系统分类文件
        try {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                if (entry.is_regular_file()) {
                    fs::path filePath = entry.path();

                    // 计算相对于输入目录的相对路径
                    fs::path relativePath = fs::relative(filePath, inputDir);
                    if (relativePath.empty() || relativePath.string() == ".") {
                        relativePath = filePath.filename();
                    }

                    // 检查是否在.framework目录中
                    if (isInFramework(relativePath)) {
                        continue;
                    }

                    // 检查是否是需要处理的文件类型
                    if (!shouldProcessFile(filePath)) {
                        continue;
                    }

                    // 如果这个文件已经在 obfuscatedFiles 中，跳过
                    if (obfuscatedFiles.find(filePath.string()) != obfuscatedFiles.end()) {
                        continue;
                    }

                    // 如果这个文件已经在 fileNameMap 中，跳过（已经被处理过了）
                    std::string originalFileName = relativePath.filename().string();
                    if (fileNameMap.find(originalFileName) != fileNameMap.end()) {
                        continue;
                    }

                    // 检查是否是分类文件（文件名包含 +）
                    std::string baseName = relativePath.stem().string();
                    size_t plusPos = baseName.find('+');
                    if (plusPos != std::string::npos) {
                        std::string className = baseName.substr(0, plusPos);
                        std::string categoryName = baseName.substr(plusPos + 1);

                        // 检查是否是系统类的分类
                        if (isSystemClass(className)) {
                            std::string key = className + "+" + categoryName;
                            auto it = systemCategoryMap.find(key);
                            if (it == systemCategoryMap.end()) continue;

                            std::string obfuscatedBaseName = it->second;
                            std::string extension = relativePath.extension().string();
                            std::string outputFileName = obfuscatedBaseName + extension;

                            // 存储文件名映射
                            fileNameMap[originalFileName] = outputFileName;

                            // 存储完整路径映射
                            fs::path outputPath = relativePath.parent_path() / outputFileName;
                            filePathMap[filePath.string()] = outputPath.string();

                            // 读取并存储文件内容
                            try {
                                std::ifstream inFile(filePath);
                                if (inFile.is_open()) {
                                    std::stringstream buffer;
                                    buffer << inFile.rdbuf();
                                    std::string content = buffer.str();
                                    inFile.close();

                                    // 从 obfuscatedBaseName 中提取混淆后的分类名
                                    size_t obfPlusPos = obfuscatedBaseName.find('+');
                                    std::string obfCategoryName = (obfPlusPos != std::string::npos) ?
                                                                 obfuscatedBaseName.substr(obfPlusPos + 1) : categoryName;

                                    // 更新分类名（使用正则表达式匹配）
                                    std::string oldPattern = "@interface " + className + " \\(" + categoryName + "\\)";
                                    std::string newPattern = "@interface " + className + " (" + obfCategoryName + ")";
                                    try {
                                        std::regex pattern(oldPattern);
                                        content = std::regex_replace(content, pattern, newPattern);
                                    } catch (const std::regex_error& e) {
                                        // 如果正则表达式失败，使用简单的字符串替换
                                        size_t pos = 0;
                                        std::string searchFor = "@interface " + className + " (" + categoryName + ")";
                                        std::string replaceWith = "@interface " + className + " (" + obfCategoryName + ")";
                                        while ((pos = content.find(searchFor, pos)) != std::string::npos) {
                                            content.replace(pos, searchFor.length(), replaceWith);
                                            pos += replaceWith.length();
                                        }
                                    }

                                    oldPattern = "@implementation " + className + " \\(" + categoryName + "\\)";
                                    newPattern = "@implementation " + className + " (" + obfCategoryName + ")";
                                    try {
                                        std::regex pattern(oldPattern);
                                        content = std::regex_replace(content, pattern, newPattern);
                                    } catch (const std::regex_error& e) {
                                        // 如果正则表达式失败，使用简单的字符串替换
                                        size_t pos = 0;
                                        std::string searchFor = "@implementation " + className + " (" + categoryName + ")";
                                        std::string replaceWith = "@implementation " + className + " (" + obfCategoryName + ")";
                                        while ((pos = content.find(searchFor, pos)) != std::string::npos) {
                                            content.replace(pos, searchFor.length(), replaceWith);
                                            pos += replaceWith.length();
                                        }
                                    }

                                    // 同时更新文件名中的 import 引用（如果有对应的分类头文件）
                                    std::string headerImport = "\"" + originalFileName + "\"";
                                    std::string headerReplacement = "\"" + outputFileName + "\"";
                                    size_t pos = 0;
                                    while ((pos = content.find(headerImport, pos)) != std::string::npos) {
                                        content.replace(pos, headerImport.length(), headerReplacement);
                                        pos += headerReplacement.length();
                                    }

                                    fileContents[filePath.string()] = content;
                                }
                            } catch (...) {
                                LOG_ERROR("Failed to read system category file: " + filePath.string());
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error scanning for system category files: " + std::string(e.what()));
        }

        // 扫描 .pch 文件（这些文件不在 obfuscatedFiles 中，需要单独处理）
        LOG_INFO("Scanning for .pch files");
        try {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                if (entry.is_regular_file()) {
                    fs::path filePath = entry.path();

                    // 计算相对于输入目录的相对路径
                    fs::path relativePath = fs::relative(filePath, inputDir);
                    if (relativePath.empty() || relativePath.string() == ".") {
                        relativePath = filePath.filename();
                    }

                    // 只处理 .pch 文件
                    if (filePath.extension().string() != ".pch") {
                        continue;
                    }

                    // 检查是否在.framework目录中
                    if (isInFramework(relativePath)) {
                        continue;
                    }

                    std::string originalFileName = relativePath.filename().string();
                    std::string baseName = relativePath.stem().string();
                    std::string extension = relativePath.extension().string();

                    // 生成混淆后的文件名
                    std::string obfuscatedBaseName;
                    auto it = baseNameMap.find(baseName);
                    if (it != baseNameMap.end()) {
                        obfuscatedBaseName = it->second;
                    } else {
                        // 为 pch 文件生成混淆名称
                        obfuscatedBaseName = nameGenerator_->generate(baseName, "file");
                        baseNameMap[baseName] = obfuscatedBaseName;
                    }

                    std::string outputFileName = obfuscatedBaseName + extension;

                    // 存储文件名映射
                    fileNameMap[originalFileName] = outputFileName;

                    // 存储完整路径映射
                    fs::path outputPath = relativePath.parent_path() / outputFileName;
                    filePathMap[filePath.string()] = outputPath.string();

                    // 读取并存储文件内容
                    try {
                        std::ifstream inFile(filePath);
                        if (inFile.is_open()) {
                            std::stringstream buffer;
                            buffer << inFile.rdbuf();
                            fileContents[filePath.string()] = buffer.str();
                            inFile.close();
                            LOG_INFO("Found .pch file: " + originalFileName + " -> " + outputFileName);
                        }
                    } catch (...) {
                        LOG_ERROR("Failed to read .pch file: " + filePath.string());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error scanning for .pch files: " + std::string(e.what()));
        }
    } else {
        // 直接扫描输入目录中的文件
        LOG_INFO("Scanning input directory for files to obfuscate");
        try {
            for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
                if (entry.is_regular_file()) {
                    fs::path filePath = entry.path();

                    // 计算相对于输入目录的相对路径
                    fs::path relativePath = fs::relative(filePath, inputDir);
                    if (relativePath.empty() || relativePath.string() == ".") {
                        relativePath = filePath.filename();
                    }

                    // 检查是否在.framework目录中
                    if (isInFramework(relativePath)) {
                        continue;
                    }

                    // 检查是否是需要处理的文件类型
                    if (!shouldProcessFile(filePath)) {
                        continue;
                    }

                    std::string originalFileName = relativePath.filename().string();
                    std::string baseName = relativePath.stem().string();
                    std::string extension = relativePath.extension().string();

                    // 生成或获取混淆后的基础名称（确保 .h 和 .m 使用相同的基础名）
                    std::string obfuscatedBaseName;
                    auto it = baseNameMap.find(baseName);
                    if (it != baseNameMap.end()) {
                        obfuscatedBaseName = it->second;
                    } else {
                        // 检查是否是分类文件（文件名包含 +）
                        size_t plusPos = baseName.find('+');
                        if (plusPos != std::string::npos) {
                            // 分类文件：格式为 "ClassName+CategoryName"
                            std::string className = baseName.substr(0, plusPos);
                            std::string categoryName = baseName.substr(plusPos + 1);
                            std::string obfCategoryName = nameGenerator_->generate(categoryName, "category");

                            // 检查基础类名是否是系统类
                            if (isSystemClass(className)) {
                                // 系统类的分类：保持类名不变，只混淆分类名
                                // 例如: UIView+MyCategory -> UIView+OBF_xyz
                                obfuscatedBaseName = className + "+" + obfCategoryName;
                            } else {
                                // 自定义类的分类：类名和分类名都混淆
                                // 需要先获取或生成混淆后的类名
                                std::string obfClassName;
                                auto classIt = baseNameMap.find(className);
                                if (classIt != baseNameMap.end()) {
                                    // 从已有映射中获取混淆后的类名（去掉扩展名）
                                    std::string existing = classIt->second;
                                    // 检查是否也包含 + （可能是嵌套分类）
                                    size_t existingPlus = existing.find('+');
                                    if (existingPlus != std::string::npos) {
                                        obfClassName = existing.substr(0, existingPlus);
                                    } else {
                                        obfClassName = existing;
                                    }
                                } else {
                                    // 检查类名是否在 SymbolTable 中
                                    if (symbolTable_ && symbolTable_->hasSymbol(className)) {
                                        obfClassName = symbolTable_->getObfuscatedName(className);
                                    } else {
                                        obfClassName = nameGenerator_->generate(className, "file");
                                    }
                                }
                                // 例如: MyClass+MyCategory -> OBF_abc+OBF_xyz
                                obfuscatedBaseName = obfClassName + "+" + obfCategoryName;
                            }
                        } else {
                            // 普通文件：检查基础名是否在 SymbolTable 中（作为类名）
                            // 如果存在，使用已混淆的类名；否则生成新的混淆名称
                            if (symbolTable_ && symbolTable_->hasSymbol(baseName)) {
                                obfuscatedBaseName = symbolTable_->getObfuscatedName(baseName);
                            } else {
                                obfuscatedBaseName = nameGenerator_->generate(baseName, "file");
                            }
                        }
                        baseNameMap[baseName] = obfuscatedBaseName;
                    }

                    std::string outputFileName = obfuscatedBaseName + extension;

                    // 存储文件名映射
                    fileNameMap[originalFileName] = outputFileName;

                    // 存储完整路径映射
                    fs::path outputPath = relativePath.parent_path() / outputFileName;
                    filePathMap[filePath.string()] = outputPath.string();

                    // 读取并存储文件内容
                    try {
                        std::ifstream inFile(filePath);
                        if (inFile.is_open()) {
                            std::stringstream buffer;
                            buffer << inFile.rdbuf();
                            fileContents[filePath.string()] = buffer.str();
                            inFile.close();
                        }
                    } catch (...) {
                        LOG_ERROR("Failed to read file: " + filePath.string());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error scanning input directory: " + std::string(e.what()));
        }
    }

    LOG_INFO("Generated " + std::to_string(fileNameMap.size()) + " filename mappings");

    // 第二步：写入混淆后的文件，并更新 import 引用和类名
    int filesWritten = 0;

    for (const auto& [filePath, fileContent] : fileContents) {
        fs::path inputFile(filePath);

        // 计算相对于输入目录的相对路径
        fs::path relativePath = fs::relative(inputFile, inputDir);
        if (relativePath.empty() || relativePath.string() == ".") {
            relativePath = inputFile.filename();
        }

        // 检查是否在.framework目录中
        if (isInFramework(relativePath)) {
            continue;
        }

        std::string originalFileName = relativePath.filename().string();
        std::string baseName = relativePath.stem().string();
        std::string extension = relativePath.extension().string();

        // 确定输出文件名
        std::string outputFileName;
        bool shouldRename = false;

        if (shouldProcessFile(inputFile)) {
            // 需要重命名的文件
            auto it = fileNameMap.find(originalFileName);
            if (it != fileNameMap.end()) {
                outputFileName = it->second;
                shouldRename = (originalFileName != outputFileName);
            } else {
                outputFileName = originalFileName;
            }
        } else {
            // 不需要处理的文件，保持原名
            outputFileName = originalFileName;
        }

        if (shouldRename) {
            LOG_INFO("Renaming file: " + originalFileName + " -> " + outputFileName);
        }

        // 构建输出路径
        fs::path outputFile = outputDir / relativePath.parent_path() / outputFileName;

        // 确保目录存在
        fs::create_directories(outputFile.parent_path());

        // 如果是需要处理的文件，更新其中的 import 引用和所有类名引用
        std::string codeToWrite = fileContent;
        if (extension == ".m" || extension == ".h" || extension == ".pch") {
            // 更新 import 引用
            codeToWrite = updateImportsInCode(fileContent, fileNameMap);

            // 更新所有类名的引用（遍历 baseNameMap）
            for (const auto& [originalBaseName, obfuscatedBaseName] : baseNameMap) {
                // 只处理普通类（不包含 + 的分类），分类只在文件内部更新
                if (originalBaseName.find('+') == std::string::npos &&
                    obfuscatedBaseName.find('+') == std::string::npos &&
                    originalBaseName != obfuscatedBaseName) {
                    codeToWrite = updateClassNameInCode(codeToWrite, originalBaseName, obfuscatedBaseName);
                }
            }

            // 如果当前文件本身是一个类文件，也更新它自己的 @interface/@implementation
            auto baseIt = baseNameMap.find(baseName);
            if (baseIt != baseNameMap.end()) {
                std::string obfuscatedBaseName = baseIt->second;
                if (baseName != obfuscatedBaseName) {
                    codeToWrite = updateClassNameInCode(codeToWrite, baseName, obfuscatedBaseName);
                }
            }
        }

        std::ofstream outFile(outputFile);
        if (outFile.is_open()) {
            outFile << codeToWrite;
            outFile.close();
            filesWritten++;
            LOG_INFO("Written obfuscated file: " + outputFile.string());

            // 如果文件被重命名了，删除原始文件
            if (shouldRename) {
                fs::path originalFile = outputDir / relativePath.parent_path() / originalFileName;
                try {
                    if (fs::exists(originalFile) && fs::exists(outputFile)) {
                        fs::remove(originalFile);
                        LOG_INFO("Removed original file: " + originalFile.string());
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to remove original file: " + originalFile.string() + " - " + e.what());
                }
            }
        } else {
            LOG_ERROR("Failed to write file: " + outputFile.string());
        }
    }

    // 第三步：保存文件名映射到文件
    if (!fileNameMap.empty()) {
        saveFileNameMapping(fileNameMap, outputDir);
    }

    return filesWritten;
}

} // namespace obfuscator

