/**
 * @file DeadCodeGenerator.h
 * @brief 垃圾代码生成器
 *
 * 根据配置生成假业务逻辑代码，用于混淆项目代码。
 * 生成的代码遵循三大原则：
 * 1. 编译器不会优化 - 使用运行时值
 * 2. Hopper 难判断 - 看起来像真实业务逻辑
 * 3. 无性能损耗 - 仅轻量级操作
 */

#ifndef DEAD_CODE_GENERATOR_H
#define DEAD_CODE_GENERATOR_H

#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <random>
#include <functional>

namespace obfuscator {

// 前向声明
class DeadCodeTemplate;
struct DeadCodeConfig;

/**
 * @brief 框架信息
 *
 * 表示一个已导入的框架及其可用的类。
 */
struct FrameworkInfo {
    std::string name;           // 框架名称，如 "Foundation", "UIKit"
    std::set<std::string> availableClasses;  // 可用的类名
};

/**
 * @brief 生成上下文
 *
 * 包含生成垃圾代码所需的上下文信息。
 */
struct GenerationContext {
    std::set<std::string> importedFrameworks;   // 已导入的框架
    std::string methodName;                     // 当前方法名
    std::string className;                      // 当前类名
    bool isInstanceMethod;                      // 是否实例方法
    std::vector<std::string> availableTemplates; // 可用的模板类型
};

/**
 * @class DeadCodeGenerator
 * @brief 垃圾代码生成器
 *
 * 根据上下文和配置生成假业务逻辑代码。
 */
class DeadCodeGenerator {
public:
    explicit DeadCodeGenerator(const DeadCodeConfig& config);
    ~DeadCodeGenerator();

    /**
     * @brief 生成一段垃圾代码
     * @param context 生成上下文
     * @return 生成的代码字符串
     */
    std::string generate(const GenerationContext& context);

    /**
     * @brief 设置启用的模板类型
     * @param types 模板类型列表
     */
    void setEnabledTypes(const std::vector<std::string>& types);

    /**
     * @brief 设置可用的框架
     * @param frameworks 框架信息列表
     */
    void setAvailableFrameworks(const std::vector<FrameworkInfo>& frameworks);

    /**
     * @brief 检查某个类是否可用
     * @param className 类名
     * @return 可用返回 true
     */
    bool isClassAvailable(const std::string& className) const;

    /**
     * @brief 获取随机变量名
     * @param prefix 变量名前缀
     * @return 随机变量名
     */
    std::string generateVarName(const std::string& prefix = "");

    /**
     * @brief 获取随机字符串常量
     * @param category 字符串类别（如 "token", "userId" 等）
     * @return 随机字符串
     */
    std::string generateStringConstant(const std::string& category = "");

    /**
     * @brief 生成随机整数
     * @param min 最小值
     * @param max 最大值
     * @return 随机整数
     */
    int generateRandomInt(int min, int max);

private:
    const DeadCodeConfig& config_;
    std::mt19937 rng_;  // 随机数生成器
    std::vector<std::unique_ptr<DeadCodeTemplate>> templates_;
    std::map<std::string, std::set<std::string>> frameworkClasses_;
    std::vector<std::string> enabledTypes_;
    unsigned int varCounter_ = 0;  // 变量计数器

    /**
     * @brief 初始化模板
     */
    void initializeTemplates();

    /**
     * @brief 初始化框架类映射
     */
    void initializeFrameworkMappings();

    /**
     * @brief 选择一个可用的模板
     * @param context 生成上下文
     * @return 选择的模板，如果没有可用模板返回 nullptr
     */
    DeadCodeTemplate* selectTemplate(const GenerationContext& context);

    /**
     * @brief 从字符串生成哈希值（用于确定性随机）
     * @param str 输入字符串
     * @return 哈希值
     */
    size_t stringHash(const std::string& str) const;
};

} // namespace obfuscator

#endif // DEAD_CODE_GENERATOR_H
