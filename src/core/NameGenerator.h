#ifndef NAME_GENERATOR_H
#define NAME_GENERATOR_H

#include <string>
#include <set>
#include <vector>
#include <filesystem>

// 前向声明
namespace obfuscator {
    struct NamingRule;
}

#include "core/ConfigManager.h"

namespace obfuscator {

class NameGenerator {
public:
    NameGenerator(const NamingRule& rule);
    ~NameGenerator();

    // 生成混淆名称
    // type: "className", "methodName", "propertyName", "fileName", "folderName", "parameterName"
    std::string generate(const std::string& originalName, const std::string& type = "");

    // 检查名称是否已使用
    bool isUsed(const std::string& name) const;

    // 重置（清空已使用的名称）
    void reset();

private:
    NamingRule rule_;
    std::set<std::string> usedNames_;

    // 单词库存储
    std::vector<std::string> adjectives_;
    std::vector<std::string> nouns_;
    std::vector<std::string> verbs_;
    std::vector<std::string> parameters_;

    // 生成随机字符串名称
    std::string generateRandomName(const std::string& type = "");

    // 生成前缀+单词组名称
    std::string generatePrefixWordName();

    // 生成使用单词库的名称
    std::string generateWordsName(const std::string& type);

    // 判断是否应该添加更多单词
    static bool shouldAddWord(const NamingRule::WordCountConfig& config, int currentCount);

    // 加载单词库
    void loadWordList();

    // 加载单个单词文件
    void loadWordFile(const std::filesystem::path& filePath, std::vector<std::string>& wordList);

    // 获取各类随机单词
    std::string getRandomAdjective();
    std::string getRandomNoun();
    std::string getRandomVerb();
    std::string getRandomParameter();
    std::string getRandomFallbackWord();

    // 生成随机字符
    char randomChar();

    // 获取随机单词
    std::string randomWord();

    // 确保名称唯一
    std::string ensureUnique(const std::string& baseName);

    // 字符串大小写转换辅助函数
    static std::string toLower(const std::string& str);
    static std::string toUpper(const std::string& str);
    static std::string toCapitalized(const std::string& str);
};

} // namespace obfuscator

#endif // NAME_GENERATOR_H
