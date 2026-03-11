/**
 * @file AhoCorasick.cpp
 * @brief Aho-Corasick 多模式字符串匹配算法实现
 */

#include "core/AhoCorasick.h"
#include "llvm/ADT/StringRef.h"
#include <algorithm>

namespace obfuscator {

// ============================================================================
// AhoCorasick 实现
// ============================================================================

AhoCorasick::AhoCorasick() : patternCount_(0) {
    // 创建根节点
    nodes_.emplace_back();
}

AhoCorasick::~AhoCorasick() {
}

void AhoCorasick::addPattern(const std::string& pattern, size_t patternIndex) {
    size_t current = 0;  // 从根节点开始

    for (char c : pattern) {
        size_t next = getOrCreateChild(current, c);
        current = next;
    }

    // 在终端节点记录模式索引和长度
    nodes_[current].output.push_back({patternIndex, pattern.length()});
    patternCount_++;

    // 确保patternLengths_足够大
    if (patternIndex >= patternLengths_.size()) {
        patternLengths_.resize(patternIndex + 1);
    }
    patternLengths_[patternIndex] = pattern.length();
}

void AhoCorasick::build() {
    std::queue<size_t> q;

    // 第一层节点的 fail 指针都指向根节点
    for (const auto& [c, next] : nodes_[0].next) {
        nodes_[next].fail = 0;
        q.push(next);
    }

    // BFS 构建 fail 指针
    while (!q.empty()) {
        size_t current = q.front();
        q.pop();

        for (const auto& [c, next] : nodes_[current].next) {
            q.push(next);

            // 计算 fail 指针
            size_t fail = nodes_[current].fail;
            while (fail != 0 && getChild(fail, c) == -1) {
                fail = nodes_[fail].fail;
            }

            size_t failChild = getChild(fail, c);
            if (failChild == static_cast<size_t>(-1) || failChild == 0) {
                nodes_[next].fail = 0;
            } else {
                nodes_[next].fail = failChild;
                // 合并输出
                nodes_[next].output.insert(nodes_[next].output.end(),
                                           nodes_[failChild].output.begin(),
                                           nodes_[failChild].output.end());
            }
        }
    }
}

std::vector<MatchResult> AhoCorasick::match(const std::string& text) const {
    std::vector<MatchResult> results;
    if (nodes_.empty()) {
        return results;
    }

    size_t current = 0;

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];

        // 跟随 fail 指针直到找到匹配或到达根节点
        while (current != 0 && getChild(current, c) == -1) {
            current = nodes_[current].fail;
        }

        int child = getChild(current, c);
        if (child != -1) {
            current = static_cast<size_t>(child);
        } else {
            current = 0;
        }

        // 收集所有匹配
        for (const auto& [patternIndex, patternLen] : nodes_[current].output) {
            // 计算匹配的起始位置: 当前位置 i 是匹配的最后一个字符位置
            size_t matchStart = i - patternLen + 1;
            size_t matchEnd = i + 1;
            results.push_back(MatchResult(matchStart, matchEnd, patternIndex));
        }
    }

    return results;
}

std::vector<MatchResult> AhoCorasick::match(llvm::StringRef text) const {
    std::vector<MatchResult> results;
    if (nodes_.empty()) {
        return results;
    }

    size_t current = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        while (current != 0 && getChild(current, c) == -1) {
            current = nodes_[current].fail;
        }

        int child = getChild(current, c);
        if (child != -1) {
            current = static_cast<size_t>(child);
        } else {
            current = 0;
        }

        for (const auto& [patternIndex, patternLen] : nodes_[current].output) {
            size_t matchStart = i - patternLen + 1;
            size_t matchEnd = i + 1;
            results.push_back(MatchResult(matchStart, matchEnd, patternIndex));
        }
    }

    return results;
}

void AhoCorasick::clear() {
    nodes_.clear();
    patternLengths_.clear();
    patternCount_ = 0;
    nodes_.emplace_back();  // 重新创建根节点
}

size_t AhoCorasick::getOrCreateChild(size_t nodeIndex, char c) {
    auto it = nodes_[nodeIndex].next.find(c);
    if (it != nodes_[nodeIndex].next.end()) {
        return it->second;
    }

    // 创建新节点
    size_t newIndex = nodes_.size();
    nodes_[nodeIndex].next[c] = newIndex;
    nodes_.emplace_back();
    return newIndex;
}

int AhoCorasick::getChild(size_t nodeIndex, char c) const {
    auto it = nodes_[nodeIndex].next.find(c);
    if (it != nodes_[nodeIndex].next.end()) {
        return static_cast<int>(it->second);
    }
    return -1;
}

// ============================================================================
// MultiPatternMatcher 实现
// ============================================================================

MultiPatternMatcher::MultiPatternMatcher() {
}

MultiPatternMatcher::~MultiPatternMatcher() {
}

void MultiPatternMatcher::addProperty(const std::string& propName, const std::string& obfName) {
    if (propName.empty() || obfName.empty()) {
        return;
    }

    size_t baseIndex = patterns_.size();

    // 添加各种模式
    // 1. .propertyName (点语法)
    patterns_.emplace_back(propName, obfName, PatternInfo::DOT_ACCESS);
    std::string dotPattern = "." + propName;
    automaton_.addPattern(dotPattern, baseIndex);

    // 2. _propertyName (成员变量)
    patterns_.emplace_back(propName, obfName, PatternInfo::IVAR_ACCESS);
    std::string ivarPattern = "_" + propName;
    automaton_.addPattern(ivarPattern, baseIndex + 1);

    // 3. "propertyName" (C 字符串)
    patterns_.emplace_back(propName, obfName, PatternInfo::C_STRING);
    std::string cStringPattern = "\"" + propName + "\"";
    automaton_.addPattern(cStringPattern, baseIndex + 2);

    // 4. @"propertyName" (OC 字符串)
    patterns_.emplace_back(propName, obfName, PatternInfo::OC_STRING);
    std::string ocStringPattern = "@\"" + propName + "\"";
    automaton_.addPattern(ocStringPattern, baseIndex + 3);

    // 5. , propertyName (逗号分隔，用于 @synthesize)
    patterns_.emplace_back(propName, obfName, PatternInfo::COMMA_PROPERTY);
    std::string commaPattern = ", " + propName;
    automaton_.addPattern(commaPattern, baseIndex + 4);

    // 6. = _propertyName (等号后成员变量)
    patterns_.emplace_back(propName, obfName, PatternInfo::EQUAL_IVAR);
    std::string equalIvarPattern = "= _" + propName;
    automaton_.addPattern(equalIvarPattern, baseIndex + 5);

    // 7. = propertyName (等号后属性名)
    patterns_.emplace_back(propName, obfName, PatternInfo::EQUAL_IVAR);
    std::string equalPropPattern = "= " + propName;
    automaton_.addPattern(equalPropPattern, baseIndex + 6);

    // 建立索引映射：每个模式索引映射到对应的 PatternInfo
    // baseIndex + 0 -> DOT_ACCESS
    // baseIndex + 1 -> IVAR_ACCESS
    // baseIndex + 2 -> C_STRING
    // baseIndex + 3 -> OC_STRING
    // baseIndex + 4 -> COMMA_PROPERTY
    // baseIndex + 5 -> EQUAL_IVAR (第一种)
    // baseIndex + 6 -> EQUAL_IVAR (第二种)
    for (size_t i = 0; i < 7; ++i) {
        indexMap_[baseIndex + i] = baseIndex + i;
    }
}

void MultiPatternMatcher::build() {
    automaton_.build();
}

std::vector<std::pair<size_t, const MultiPatternMatcher::PatternInfo*>> MultiPatternMatcher::match(const std::string& text) const {
    std::vector<std::pair<size_t, const MultiPatternMatcher::PatternInfo*>> results;

    if (patterns_.empty()) {
        return results;
    }

    // 使用 Aho-Corasick 匹配
    auto acResults = automaton_.match(text);

    for (const auto& acResult : acResults) {
        size_t patternIdx = indexMap_.at(acResult.patternIndex);
        const MultiPatternMatcher::PatternInfo* info = &patterns_[patternIdx];

        // acResult.start 已经是正确的起始位置
        // acResult.end 是匹配结束位置（不包含）
        results.push_back({acResult.start, info});
    }

    // 按位置排序
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    return results;
}

std::vector<std::pair<size_t, const MultiPatternMatcher::PatternInfo*>> MultiPatternMatcher::match(llvm::StringRef text) const {
    // 转换为 std::string 复用现有实现
    // 后续可以优化为直接使用 StringRef
    return match(text.str());
}

std::vector<std::tuple<size_t, size_t, const MultiPatternMatcher::PatternInfo*>> MultiPatternMatcher::matchWithLength(const std::string& text) const {
    std::vector<std::tuple<size_t, size_t, const MultiPatternMatcher::PatternInfo*>> results;

    if (patterns_.empty()) {
        return results;
    }

    // 使用 Aho-Corasick 匹配
    auto acResults = automaton_.match(text);

    for (const auto& acResult : acResults) {
        size_t patternIdx = indexMap_.at(acResult.patternIndex);
        const MultiPatternMatcher::PatternInfo* info = &patterns_[patternIdx];

        // acResult.start 是匹配起始位置
        // acResult.end 是匹配结束位置（不包含）
        // 长度 = end - start
        size_t matchLength = acResult.end - acResult.start;
        results.push_back(std::make_tuple(acResult.start, matchLength, info));
    }

    // 按位置排序
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) {
                  return std::get<0>(a) < std::get<0>(b);
              });

    return results;
}

void MultiPatternMatcher::clear() {
    automaton_.clear();
    patterns_.clear();
    indexMap_.clear();
}

} // namespace obfuscator
