#include "core/CompileOptions.h"
#include "core/Logger.h"
#include <filesystem>
#include <algorithm>
#include <vector>

namespace obfuscator {

std::string CompileOptions::detectIOSSDKPath() {
    namespace fs = std::filesystem;
    
    std::string iosSDKPath;
    
    // 尝试查找iPhoneOS SDK
    std::string iphoneOSPath = "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs";
    if (fs::exists(iphoneOSPath)) {
        for (const auto& entry : fs::directory_iterator(iphoneOSPath)) {
            if (entry.is_directory()) {
                std::string sdkName = entry.path().filename().string();
                if (sdkName.find("iPhoneOS") == 0) {
                    iosSDKPath = entry.path().string();
                    break;
                }
            }
        }
    }
    
    // 如果找不到iPhoneOS SDK，尝试iPhoneSimulator SDK
    if (iosSDKPath.empty()) {
        std::string simulatorPath = "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs";
        if (fs::exists(simulatorPath)) {
            for (const auto& entry : fs::directory_iterator(simulatorPath)) {
                if (entry.is_directory()) {
                    std::string sdkName = entry.path().filename().string();
                    if (sdkName.find("iPhoneSimulator") == 0) {
                        iosSDKPath = entry.path().string();
                        break;
                    }
                }
            }
        }
    }
    
    // 如果都找不到，使用macOS SDK作为后备（仅用于测试）
    if (iosSDKPath.empty()) {
        iosSDKPath = "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
        LOG_WARNING("iOS SDK not found, using macOS SDK for testing only");
    } else {
        LOG_INFO("Using iOS SDK: " + iosSDKPath);
    }
    
    return iosSDKPath;
}

std::vector<std::string> CompileOptions::buildCompileArgs(const std::string& iosSDKPath) {
    return buildCompileArgs(iosSDKPath, "");
}

std::vector<std::string> CompileOptions::buildCompileArgs(const std::string& iosSDKPath,
                                                           const std::string& projectPath) {
    namespace fs = std::filesystem;

    std::vector<std::string> compileArgs = {
        "-x", "objective-c",  // 指定语言为Objective-C
        "-fobjc-arc",         // 启用ARC
        "-fobjc-weak",        // 启用弱引用
        "-isysroot", iosSDKPath,
        "-miphoneos-version-min=9.0",  // 设置最低iOS版本
        "-arch", "arm64",     // 指定架构
        // 添加常用框架（在指定语言之后立即添加）
        "-framework", "UIKit",
        "-framework", "Foundation",
        "-framework", "CoreGraphics",
        "-framework", "QuartzCore",
        "-framework", "CoreFoundation"
    };

    // 添加预处理器定义
    compileArgs.push_back("-D");
    compileArgs.push_back("DEBUG=1");

    compileArgs.push_back("-D");
    compileArgs.push_back("COCOAPODS=1");

    // 定义 DLog 宏（NSLog 替代品）
    compileArgs.push_back("-D");
    compileArgs.push_back("DLog(...)=NSLog(__VA_ARGS__)");

    compileArgs.push_back("-D");
    compileArgs.push_back("DLogError(...)=NSLog(__VA_ARGS__)");

    // 添加Clang内置头文件路径（包含stdarg.h等标准C头文件）
    // 这些头文件位于工具链的clang include目录中，优先级最高
    std::string toolchainBase = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain";
    std::string clangIncludeBase = toolchainBase + "/usr/lib/clang";

    if (fs::exists(clangIncludeBase)) {
        // 查找clang版本目录（如 16.0.0, 17.0.0等）
        // 按版本号降序排序，使用最新版本
        std::vector<std::string> clangVersions;
        for (const auto& entry : fs::directory_iterator(clangIncludeBase)) {
            if (entry.is_directory()) {
                std::string versionPath = entry.path().string() + "/include";
                if (fs::exists(versionPath)) {
                    clangVersions.push_back(versionPath);
                }
            }
        }

        // 排序并选择最新的版本
        if (!clangVersions.empty()) {
            std::sort(clangVersions.begin(), clangVersions.end(), std::greater<std::string>());
            compileArgs.push_back("-I");
            compileArgs.push_back(clangVersions[0]);
            // 减少日志输出，提高性能
        }
    }

    // 添加系统头文件搜索路径
    // iOS SDK的usr/include目录包含stdarg.h等标准C头文件
    std::string usrIncludePath = iosSDKPath + "/usr/include";
    if (fs::exists(usrIncludePath)) {
        compileArgs.push_back("-I");
        compileArgs.push_back(usrIncludePath);
        // 减少日志输出，提高性能
    } else {
        LOG_WARNING("System include path not found: " + usrIncludePath);
    }

    // 添加工具链头文件路径（如果存在）
    std::string toolchainIncludePath = toolchainBase + "/usr/include";
    if (fs::exists(toolchainIncludePath)) {
        compileArgs.push_back("-I");
        compileArgs.push_back(toolchainIncludePath);
        // 减少日志输出，提高性能
    }

    // 添加C++标准库头文件路径（如果存在）
    std::string cxxIncludePath = iosSDKPath + "/usr/include/c++/v1";
    if (fs::exists(cxxIncludePath)) {
        compileArgs.push_back("-I");
        compileArgs.push_back(cxxIncludePath);
        // 减少日志输出，提高性能
    }

    // 添加系统框架路径
    std::string frameworksPath = iosSDKPath + "/System/Library/Frameworks";
    if (fs::exists(frameworksPath)) {
        compileArgs.push_back("-F");
        compileArgs.push_back(frameworksPath);
        // 减少日志输出，提高性能
    }

    // 添加私有框架路径（如果需要）
    std::string privateFrameworksPath = iosSDKPath + "/System/Library/PrivateFrameworks";
    if (fs::exists(privateFrameworksPath)) {
        compileArgs.push_back("-F");
        compileArgs.push_back(privateFrameworksPath);
        // 减少日志输出，提高性能
    }

    // 如果提供了项目路径，添加项目内部头文件搜索路径
    if (!projectPath.empty() && fs::exists(projectPath)) {
        // 添加项目根目录作为头文件搜索路径
        compileArgs.push_back("-I");
        compileArgs.push_back(projectPath);

        // 递归添加所有子目录作为头文件搜索路径
        // 这样可以处理项目中不同目录下的头文件引用
        // 注意：这个操作可能耗时，特别是在大型项目中
        size_t includeCount = 0;
        for (const auto& entry : fs::recursive_directory_iterator(projectPath)) {
            if (entry.is_directory()) {
                std::string dirPath = entry.path().string();
                // 跳过隐藏目录和 .framework 目录
                std::string dirName = entry.path().filename().string();
                if (!dirName.empty() && dirName[0] != '.') {
                    size_t frameworkPos = dirName.rfind(".framework");
                    if (frameworkPos == std::string::npos || frameworkPos != dirName.length() - 10) {
                        compileArgs.push_back("-I");
                        compileArgs.push_back(dirPath);
                        includeCount++;
                    }
                }
            }
        }
        // 只在添加了大量路径时才输出日志
        if (includeCount > 100) {
            LOG_INFO("Added " + std::to_string(includeCount) + " project include paths");
        }
    }

    return compileArgs;
}

} // namespace obfuscator


