#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

// 包含json头文件
// 注意：由于ConfigManager.h中使用了json类型作为函数参数，
// 必须包含完整定义，不能使用前向声明
#include <nlohmann/json.hpp>

namespace obfuscator {

struct NamingRule {
    enum Style {
        RANDOM,
        PREFIX_WORD,
        WORDS  // 使用外部单词库生成有意义的名称
    };

    enum WordCase {
        CAMEL_CASE,    // camelCase
        PASCAL_CASE,   // PascalCase
        SNAKE_CASE,    // snake_case
        KEBAB_CASE,    // kebab-case
        UPPER_CASE     // UPPER_CASE
    };

    // 单词数量配置（用于不同元素类型）
    struct WordCountConfig {
        int min = 1;
        int max = 2;
    };

    // 随机字母长度配置（用于不同元素类型）
    struct RandomLengthConfig {
        int min = 6;
        int max = 12;
    };

    Style style = RANDOM;
    std::string prefix = "OBF_";
    int length = 8;
    int wordCount = 2;
    int wordLength = 3;
    std::string charset = "alphanumeric";

    // 单词库配置
    std::string wordListPath = "./wordlist";  // 单词库目录路径
    WordCase wordCase = CAMEL_CASE;           // 单词大小写风格

    // 各元素类型的单词数量配置
    WordCountConfig classNameWordCount = {1, 2};      // 形容词+名词
    WordCountConfig methodNameWordCount = {1, 2};      // 动词+名词
    WordCountConfig propertyNameWordCount = {1, 2};    // 名词
    WordCountConfig fileNameWordCount = {2, 3};        // 文件名
    WordCountConfig folderNameWordCount = {2, 3};      // 文件夹名
    WordCountConfig parameterNameWordCount = {1, 1};   // 参数名

    // 各元素类型的随机字母长度配置
    RandomLengthConfig classNameRandomLength = {6, 12};
    RandomLengthConfig methodNameRandomLength = {6, 12};
    RandomLengthConfig propertyNameRandomLength = {6, 12};
    RandomLengthConfig fileNameRandomLength = {8, 16};
    RandomLengthConfig folderNameRandomLength = {8, 16};
    RandomLengthConfig parameterNameRandomLength = {4, 8};
};

struct SDKConfig {
    std::string name;
    std::string type = "framework";
    std::string inputPath;
    std::string outputPath;
};

struct WhitelistConfig {
    std::vector<std::string> classes;
    std::vector<std::string> methods;
    std::vector<std::string> properties;
    std::vector<std::string> thirdPartySDKs;
    std::vector<std::string> publicHeaders;
};

struct DeadCodeConfig {
    double density = 0.2;                           // 插入密度 (0.0-1.0)
    int maxStatementsPerMethod = 3;                 // 每个方法最多插入语句数
    std::vector<std::string> templateTypes;         // 启用的模板类型
};

struct ObfuscationConfig {
    std::vector<std::string> strategies;
    NamingRule namingRule;
    WhitelistConfig whitelist;
    DeadCodeConfig deadCodeInjection;
    bool generateMapping = true;
    std::string mappingOutputPath = "./mapping.json";
};

struct Config {
    SDKConfig sdk;
    ObfuscationConfig obfuscation;
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // 从JSON文件加载配置
    bool loadFromFile(const std::string& configPath);
    
    // 从JSON字符串加载配置
    bool loadFromString(const std::string& jsonString);
    
    // 保存配置到文件
    bool saveToFile(const std::string& configPath) const;
    
    // 获取配置
    const Config& getConfig() const { return config_; }
    Config& getConfig() { return config_; }
    
    // 检查策略是否启用
    bool isStrategyEnabled(const std::string& strategyName) const;
    
    // 检查是否在白名单中
    bool isWhitelisted(const std::string& name, const std::string& type) const;

    // 检查是否是保留注释的头文件
    bool isPublicHeader(const std::string& filePath) const;

    // 检查是否是第三方SDK
    bool isThirdPartySDK(const std::string& name) const;
    
    // 获取命名规则
    const NamingRule& getNamingRule() const { return config_.obfuscation.namingRule; }

private:
    Config config_;
    
    /**
     * @brief 解析JSON对象并填充配置结构
     * @param j JSON对象（nlohmann::json类型）
     * @return 成功返回true，失败返回false
     */
    bool parseJSON(const nlohmann::json& j);
    
    /**
     * @brief 设置默认配置值
     */
    void setDefaults();
};

} // namespace obfuscator

#endif // CONFIG_MANAGER_H

