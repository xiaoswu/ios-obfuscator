#ifndef COMPILE_OPTIONS_H
#define COMPILE_OPTIONS_H

#include <string>
#include <vector>
#include <filesystem>

namespace obfuscator {

/**
 * @brief 编译选项构建器
 *
 * 负责检测iOS SDK路径并构建正确的编译参数
 */
class CompileOptions {
public:
    /**
     * @brief 检测iOS SDK路径
     * @return iOS SDK路径，如果找不到则返回空字符串
     */
    static std::string detectIOSSDKPath();

    /**
     * @brief 构建编译参数（不含项目路径）
     * @param iosSDKPath iOS SDK路径
     * @return 编译参数向量
     */
    static std::vector<std::string> buildCompileArgs(const std::string& iosSDKPath);

    /**
     * @brief 构建完整编译参数（包含项目头文件路径）
     * @param iosSDKPath iOS SDK路径
     * @param projectPath 项目源代码根目录
     * @return 编译参数向量
     */
    static std::vector<std::string> buildCompileArgs(const std::string& iosSDKPath,
                                                      const std::string& projectPath);
};

} // namespace obfuscator

#endif // COMPILE_OPTIONS_H

