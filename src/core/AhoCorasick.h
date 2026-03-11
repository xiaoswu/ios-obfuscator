/**
 * @file AhoCorasick.h
 * @brief Aho-Corasick 多模式字符串匹配算法
 *
 * 时间复杂度：
 * - 构建自动机: O(总模式长度)
 * - 匹配: O(文本长度 + 匹配数量)
 *
 * 相比逐个模式搜索的 O(n×m)，提升 50-500 倍
 */

#ifndef AHO_CORASICK_H
#define AHO_CORASICK_H

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>

// 前向声明 llvm::StringRef
namespace llvm {
    class StringRef;
}

namespace obfuscator {

/**
 * @struct MatchResult
 * @brief 匹配结果
 */
struct MatchResult {
    size_t start;           // 匹配起始位置
    size_t end;             // 匹配结束位置（不包含）
    size_t patternIndex;    // 模式索引

    MatchResult(size_t s, size_t e, size_t idx)
        : start(s), end(e), patternIndex(idx) {}
};

/**
 * @class AhoCorasick
 * @brief Aho-Corasick 自动机
 *
 * 使用示例：
 * @code
 * AhoCorasick ac;
 * ac.addPattern("hello", 0);
 * ac.addPattern("world", 1);
 * ac.build();
 *
 * std::vector<MatchResult> results = ac.match("hello world");
 * @endcode
 */
class AhoCorasick {
public:
    AhoCorasick();
    ~AhoCorasick();

    /**
     * 添加模式字符串
     * @param pattern 模式字符串
     * @param patternIndex 模式索引（用于标识匹配的是哪个模式）
     */
    void addPattern(const std::string& pattern, size_t patternIndex);

    /**
     * 构建自动机（添加所有模式后调用一次）
     */
    void build();

    /**
     * 匹配文本，返回所有匹配结果
     * @param text 待匹配的文本
     * @return 匹配结果列表（按位置排序）
     */
    std::vector<MatchResult> match(const std::string& text) const;

    /**
     * 匹配文本（使用 StringRef 避免复制）
     */
    std::vector<MatchResult> match(llvm::StringRef text) const;

    /**
     * 清空所有模式
     */
    void clear();

    /**
     * 获取模式数量
     */
    size_t getPatternCount() const { return patternCount_; }

    /**
     * 获取节点数量（用于调试）
     */
    size_t getNodeCount() const { return nodes_.size(); }

private:
    /**
     * @struct Node
     * @brief Trie 节点
     */
    struct Node {
        std::unordered_map<char, size_t> next;    // 子节点
        size_t fail;                              // 失败指针
        std::vector<std::pair<size_t, size_t>> output;  // 输出 (模式索引, 模式长度)

        Node() : fail(0) {}
    };

    std::vector<Node> nodes_;      // 节点池（nodes_[0] 是根节点）
    std::vector<size_t> patternLengths_;  // 每个模式的长度
    size_t patternCount_;          // 模式数量

    /**
     * 获取子节点，不存在则创建
     */
    size_t getOrCreateChild(size_t nodeIndex, char c);

    /**
     * 获取子节点，不存在返回 -1
     */
    int getChild(size_t nodeIndex, char c) const;
};

/**
 * @class MultiPatternMatcher
 * @brief 多模式匹配器（针对属性名混淆优化）
 *
 * 支持：
 * - ".propertyName" 模式匹配（点语法）
 * - "_propertyName" 模式匹配（成员变量）
 * - "@\"propertyName\"" 模式匹配（字符串字面量）
 */
class MultiPatternMatcher {
public:
    /**
     * @struct PatternInfo
     * @brief 模式信息
     */
    struct PatternInfo {
        std::string originalName;       // 原始属性名
        std::string obfuscatedName;     // 混淆后的名称
        enum PatternType {
            DOT_ACCESS,     // .property
            IVAR_ACCESS,    // _property
            C_STRING,       // "property"
            OC_STRING,      // @"property"
            COMMA_PROPERTY, // , property (for @synthesize)
            DYN_PROPERTY,   // @dynamic property / @synthesize property
            EQUAL_IVAR      // = _property / = property
        } type;

        PatternInfo(const std::string& orig, const std::string& obf, PatternType t)
            : originalName(orig), obfuscatedName(obf), type(t) {}
    };

    MultiPatternMatcher();
    ~MultiPatternMatcher();

    /**
     * 添加属性模式（自动生成所有相关模式）
     * @param propName 原始属性名
     * @param obfName 混淆后的名称
     */
    void addProperty(const std::string& propName, const std::string& obfName);

    /**
     * 构建自动机
     */
    void build();

    /**
     * 匹配文本，返回所有匹配
     * @param text 待匹配文本
     * @return 匹配结果（位置、模式信息）
     */
    std::vector<std::pair<size_t, const PatternInfo*>> match(const std::string& text) const;

    std::vector<std::pair<size_t, const PatternInfo*>> match(llvm::StringRef text) const;

    /**
     * 匹配文本，返回所有匹配（包含匹配长度）
     * @param text 待匹配文本
     * @return 匹配结果（位置、长度、模式信息）
     */
    std::vector<std::tuple<size_t, size_t, const PatternInfo*>> matchWithLength(const std::string& text) const;

    /**
     * 清空
     */
    void clear();

    /**
     * 获取模式数量
     */
    size_t getPatternCount() const { return patterns_.size(); }

private:
    AhoCorasick automaton_;                           // AC 自动机
    std::vector<PatternInfo> patterns_;              // 模式信息列表
    std::unordered_map<size_t, size_t> indexMap_;    // AC 索引 -> 模式索引
};

} // namespace obfuscator

#endif // AHO_CORASICK_H
