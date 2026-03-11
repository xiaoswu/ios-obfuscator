#include "core/NameGenerator.h"
#include "core/ConfigManager.h"
#include "core/Logger.h"
#include <random>
#include <algorithm>
#include <set>
#include <fstream>
#include <filesystem>
#include <sstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

namespace obfuscator {

// 预定义单词库（作为备用）
static const std::vector<std::string> FALLBACK_WORD_LIST = {
    "cat", "dog", "run", "jump", "fly", "bird", "fish", "tree",
    "sun", "moon", "star", "sky", "sea", "land", "hill", "lake",
    "book", "pen", "desk", "chair", "table", "door", "window", "wall",
    "red", "blue", "green", "yellow", "black", "white", "gray", "brown"
};

NameGenerator::NameGenerator(const NamingRule& rule) : rule_(rule) {
    // 如果使用单词库风格，加载单词文件
    if (rule_.style == NamingRule::WORDS) {
        loadWordList();
    }
}

NameGenerator::~NameGenerator() {
}

std::string NameGenerator::generate(const std::string& originalName, const std::string& type) {
    std::string obfuscatedName;

    if (rule_.style == NamingRule::RANDOM) {
        obfuscatedName = generateRandomName(type);
    } else if (rule_.style == NamingRule::PREFIX_WORD) {
        obfuscatedName = generatePrefixWordName();
    } else if (rule_.style == NamingRule::WORDS) {
        obfuscatedName = generateWordsName(type);
    } else {
        obfuscatedName = generateRandomName(type);
    }

    return ensureUnique(obfuscatedName);
}

bool NameGenerator::isUsed(const std::string& name) const {
    return usedNames_.find(name) != usedNames_.end();
}

void NameGenerator::reset() {
    usedNames_.clear();
}

std::string NameGenerator::generateRandomName(const std::string& type) {
    std::string name = rule_.prefix;

    std::random_device rd;
    std::mt19937 gen(rd());

    // 根据类型选择随机长度配置
    int minLength = rule_.length;
    int maxLength = rule_.length;

    if (type == "className" || type == "class") {
        minLength = rule_.classNameRandomLength.min;
        maxLength = rule_.classNameRandomLength.max;
    } else if (type == "methodName" || type == "method") {
        minLength = rule_.methodNameRandomLength.min;
        maxLength = rule_.methodNameRandomLength.max;
    } else if (type == "propertyName" || type == "property") {
        minLength = rule_.propertyNameRandomLength.min;
        maxLength = rule_.propertyNameRandomLength.max;
    } else if (type == "fileName" || type == "file") {
        minLength = rule_.fileNameRandomLength.min;
        maxLength = rule_.fileNameRandomLength.max;
    } else if (type == "folderName" || type == "folder") {
        minLength = rule_.folderNameRandomLength.min;
        maxLength = rule_.folderNameRandomLength.max;
    } else if (type == "parameterName" || type == "parameter") {
        minLength = rule_.parameterNameRandomLength.min;
        maxLength = rule_.parameterNameRandomLength.max;
    }

    // 在范围内随机选择长度
    std::uniform_int_distribution<> lengthDis(minLength, maxLength);
    int randomLength = lengthDis(gen);

    if (rule_.charset == "alphanumeric") {
        std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string allChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::uniform_int_distribution<> letterDis(0, letters.size() - 1);
        std::uniform_int_distribution<> charDis(0, allChars.size() - 1);

        for (int i = 0; i < randomLength; ++i) {
            if (i == 0) {
                // 第一个字符必须是字母（Objective-C 标识符不能以数字开头）
                name += letters[letterDis(gen)];
            } else {
                name += allChars[charDis(gen)];
            }
        }
    } else {
        std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::uniform_int_distribution<> dis(0, chars.size() - 1);

        for (int i = 0; i < randomLength; ++i) {
            name += chars[dis(gen)];
        }
    }

    return name;
}

std::string NameGenerator::generatePrefixWordName() {
    std::string name = rule_.prefix;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, FALLBACK_WORD_LIST.size() - 1);

    for (int i = 0; i < rule_.wordCount; ++i) {
        if (i > 0) {
            name += "_";
        }
        name += FALLBACK_WORD_LIST[dis(gen)];
    }

    return name;
}

std::string NameGenerator::generateWordsName(const std::string& type) {
    std::vector<std::string> wordsToUse;

    // 根据类型选择单词来源和数量
    const NamingRule::WordCountConfig* wordCountConfig = &rule_.classNameWordCount;

    if (type == "className") {
        wordCountConfig = &rule_.classNameWordCount;
        // 类名: 形容词 + 名词组合
        if (!adjectives_.empty() && !nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                if (i == 0) {
                    wordsToUse.push_back(getRandomAdjective());
                } else {
                    wordsToUse.push_back(getRandomNoun());
                }
            }
        }
    } else if (type == "methodName") {
        wordCountConfig = &rule_.methodNameWordCount;
        // 方法名: 动词 + 名词组合
        if (!verbs_.empty() && !nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                if (i == 0) {
                    wordsToUse.push_back(getRandomVerb());
                } else {
                    wordsToUse.push_back(getRandomNoun());
                }
            }
        }
    } else if (type == "propertyName") {
        wordCountConfig = &rule_.propertyNameWordCount;
        // 属性名: 名词（可能带形容词）
        if (!nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                if (i == 0) {
                    wordsToUse.push_back(getRandomNoun());
                } else if (!adjectives_.empty()) {
                    wordsToUse.insert(wordsToUse.begin(), getRandomAdjective());
                }
            }
        }
    } else if (type == "fileName" || type == "file") {
        // 文件名: 形容词 + 名词组合
        wordCountConfig = &rule_.fileNameWordCount;
        if (!adjectives_.empty() && !nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                if (i == 0) {
                    wordsToUse.push_back(getRandomAdjective());
                } else {
                    wordsToUse.push_back(getRandomNoun());
                }
            }
        }
    } else if (type == "folderName" || type == "folder") {
        // 文件夹名: 形容词 + 名词组合
        wordCountConfig = &rule_.folderNameWordCount;
        if (!adjectives_.empty() && !nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                if (i == 0) {
                    wordsToUse.push_back(getRandomAdjective());
                } else {
                    wordsToUse.push_back(getRandomNoun());
                }
            }
        }
    } else if (type == "parameterName") {
        wordCountConfig = &rule_.parameterNameWordCount;
        // 参数名: 从 parameters.txt 或名词组合
        if (!parameters_.empty() || !nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                if (!parameters_.empty()) {
                    wordsToUse.push_back(getRandomParameter());
                } else {
                    wordsToUse.push_back(getRandomNoun());
                }
            }
        }
    } else {
        // 默认使用名词（均匀分布）
        wordCountConfig = &rule_.classNameWordCount;
        if (!nouns_.empty()) {
            // 先决定总的单词数量（均匀分布）
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(wordCountConfig->min, wordCountConfig->max);
            int targetCount = dis(gen);

            for (int i = 0; i < targetCount; ++i) {
                wordsToUse.push_back(getRandomNoun());
            }
        }
    }

    // 如果没有加载单词库，使用备用单词
    if (wordsToUse.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        int count = std::uniform_int_distribution<>(wordCountConfig->min, wordCountConfig->max)(gen);
        for (int i = 0; i < count; ++i) {
            std::uniform_int_distribution<> dis(0, FALLBACK_WORD_LIST.size() - 1);
            wordsToUse.push_back(FALLBACK_WORD_LIST[dis(gen)]);
        }
    }

    // 根据大小写规则组合单词
    std::string name = rule_.prefix;
    std::string combinedWords;

    switch (rule_.wordCase) {
        case NamingRule::CAMEL_CASE:
            for (size_t i = 0; i < wordsToUse.size(); ++i) {
                if (i == 0) {
                    combinedWords += toLower(wordsToUse[i]);
                } else {
                    combinedWords += toCapitalized(wordsToUse[i]);
                }
            }
            break;
        case NamingRule::PASCAL_CASE:
            for (const auto& word : wordsToUse) {
                combinedWords += toCapitalized(word);
            }
            break;
        case NamingRule::SNAKE_CASE:
            for (size_t i = 0; i < wordsToUse.size(); ++i) {
                if (i > 0) {
                    combinedWords += "_";
                }
                combinedWords += toLower(wordsToUse[i]);
            }
            break;
        case NamingRule::KEBAB_CASE:
            for (size_t i = 0; i < wordsToUse.size(); ++i) {
                if (i > 0) {
                    combinedWords += "-";
                }
                combinedWords += toLower(wordsToUse[i]);
            }
            break;
        case NamingRule::UPPER_CASE:
            for (size_t i = 0; i < wordsToUse.size(); ++i) {
                if (i > 0) {
                    combinedWords += "_";
                }
                combinedWords += toUpper(wordsToUse[i]);
            }
            break;
    }

    name += combinedWords;
    return name;
}

bool NameGenerator::shouldAddWord(const NamingRule::WordCountConfig& config, int currentCount) {
    if (currentCount < config.min) return true;
    if (currentCount >= config.max) return false;
    // 在范围内随机决定是否添加
    std::random_device rd;
    std::mt19937 gen(rd());
    return std::uniform_int_distribution<>(0, 1)(gen) == 1;
}

void NameGenerator::loadWordList() {
    namespace fs = std::filesystem;

    fs::path wordListDir(rule_.wordListPath);

    // 如果是相对路径，尝试相对于当前工作目录或可执行文件目录
    if (wordListDir.is_relative()) {
        // 先尝试当前工作目录
        if (!fs::exists(wordListDir)) {
            // 尝试相对于可执行文件所在目录的上级目录（开发环境）
            char exePath[PATH_MAX];
            uint32_t size = sizeof(exePath);
            if (_NSGetExecutablePath(exePath, &size) == 0) {
                fs::path exeDir(exePath);
                fs::path projectDir = exeDir.parent_path().parent_path();
                fs::path altPath = projectDir / "wordlist";
                if (fs::exists(altPath)) {
                    wordListDir = altPath;
                }
            }
        }
    }

    if (!fs::exists(wordListDir) || !fs::is_directory(wordListDir)) {
        LOG_WARNING("Word list directory not found: " + wordListDir.string() + ", using fallback word list");
        return;
    }

    // 加载各类单词文件
    loadWordFile(wordListDir / "adjectives.txt", adjectives_);
    loadWordFile(wordListDir / "nouns.txt", nouns_);
    loadWordFile(wordListDir / "verbs.txt", verbs_);
    loadWordFile(wordListDir / "parameters.txt", parameters_);

    LOG_INFO("Word list loaded: " + std::to_string(adjectives_.size()) + " adjectives, " +
             std::to_string(nouns_.size()) + " nouns, " +
             std::to_string(verbs_.size()) + " verbs, " +
             std::to_string(parameters_.size()) + " parameters");
}

void NameGenerator::loadWordFile(const std::filesystem::path& filePath, std::vector<std::string>& wordList) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 去除首尾空白字符
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");

        if (start != std::string::npos) {
            std::string word = line.substr(start, end - start + 1);
            if (!word.empty()) {
                wordList.push_back(word);
            }
        }
    }
}

std::string NameGenerator::getRandomAdjective() {
    if (adjectives_.empty()) return getRandomFallbackWord();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, adjectives_.size() - 1);
    return adjectives_[dis(gen)];
}

std::string NameGenerator::getRandomNoun() {
    if (nouns_.empty()) return getRandomFallbackWord();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, nouns_.size() - 1);
    return nouns_[dis(gen)];
}

std::string NameGenerator::getRandomVerb() {
    if (verbs_.empty()) return getRandomFallbackWord();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, verbs_.size() - 1);
    return verbs_[dis(gen)];
}

std::string NameGenerator::getRandomParameter() {
    if (parameters_.empty()) return getRandomFallbackWord();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, parameters_.size() - 1);
    return parameters_[dis(gen)];
}

std::string NameGenerator::getRandomFallbackWord() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, FALLBACK_WORD_LIST.size() - 1);
    return FALLBACK_WORD_LIST[dis(gen)];
}

std::string NameGenerator::toLower(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

std::string NameGenerator::toUpper(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }
    return result;
}

std::string NameGenerator::toCapitalized(const std::string& str) {
    std::string result = toLower(str);
    if (!result.empty()) {
        result[0] = result[0] - 'a' + 'A';
    }
    return result;
}

char NameGenerator::randomChar() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis('A', 'Z');
    return static_cast<char>(dis(gen));
}

std::string NameGenerator::randomWord() {
    std::random_device rd;
    std::mt19937 gen(rd());

    if (!adjectives_.empty() && !nouns_.empty()) {
        std::uniform_int_distribution<> dis(0, adjectives_.size() + nouns_.size() - 1);
        int idx = dis(gen);
        if (idx < static_cast<int>(adjectives_.size())) {
            return adjectives_[idx];
        } else {
            return nouns_[idx - adjectives_.size()];
        }
    }

    std::uniform_int_distribution<> dis(0, FALLBACK_WORD_LIST.size() - 1);
    return FALLBACK_WORD_LIST[dis(gen)];
}

std::string NameGenerator::ensureUnique(const std::string& baseName) {
    std::string name = baseName;
    int counter = 1;

    while (isUsed(name)) {
        name = baseName + std::to_string(counter);
        counter++;
    }

    usedNames_.insert(name);
    return name;
}

} // namespace obfuscator
