/**
 * @file CommentRemovalStrategy.cpp
 * @brief 注释移除策略实现
 *
 * 实现CommentRemovalStrategy类，用于移除源代码中的所有注释。
 */

#include "strategies/CommentRemovalStrategy.h"
#include "core/Logger.h"
#include <regex>
#include <sstream>

namespace obfuscator {

// =============================================================================
// generatePlaceholder: 生成唯一的占位符
// =============================================================================

std::string CommentRemovalStrategy::generatePlaceholder(size_t index) {
    // 使用安全的占位符格式，避免与代码中的字符冲突
    // 格式: ___STR_索引_NUM___
    return "___STR_" + std::to_string(index) + "_NUM___";
}

// =============================================================================
// protectStrings: 保护字符串字面量
// =============================================================================

std::string CommentRemovalStrategy::protectStrings(
    const std::string& source,
    std::map<std::string, std::string>& stringMap) {

    std::string result;
    size_t pos = 0;
    size_t length = source.length();
    size_t stringIndex = 0;

    while (pos < length) {
        // 查找 @" 字符串（Objective-C 字符串）
        if (pos + 1 < length && source[pos] == '@' && source[pos + 1] == '"') {
            size_t endPos = pos + 2;
            while (endPos < length) {
                if (source[endPos] == '\\' && endPos + 1 < length) {
                    // 转义字符，跳过下一个字符
                    endPos += 2;
                } else if (source[endPos] == '"') {
                    // 字符串结束
                    endPos++;
                    break;
                } else {
                    endPos++;
                }
            }
            std::string str = source.substr(pos, endPos - pos);
            std::string placeholder = generatePlaceholder(stringIndex++);
            stringMap[placeholder] = str;
            result += placeholder;
            pos = endPos;
            continue;
        }

        // 查找 " 字符串（C 字符串）
        if (source[pos] == '"') {
            size_t endPos = pos + 1;
            while (endPos < length) {
                if (source[endPos] == '\\' && endPos + 1 < length) {
                    // 转义字符，跳过下一个字符
                    endPos += 2;
                } else if (source[endPos] == '"') {
                    // 字符串结束
                    endPos++;
                    break;
                } else {
                    endPos++;
                }
            }
            std::string str = source.substr(pos, endPos - pos);
            std::string placeholder = generatePlaceholder(stringIndex++);
            stringMap[placeholder] = str;
            result += placeholder;
            pos = endPos;
            continue;
        }

        // 查找 ' 字符（字符字面量）
        if (source[pos] == '\'') {
            size_t endPos = pos + 1;
            while (endPos < length) {
                if (source[endPos] == '\\' && endPos + 1 < length) {
                    // 转义字符
                    endPos += 2;
                } else if (source[endPos] == '\'') {
                    endPos++;
                    break;
                } else {
                    endPos++;
                }
            }
            std::string str = source.substr(pos, endPos - pos);
            std::string placeholder = generatePlaceholder(stringIndex++);
            stringMap[placeholder] = str;
            result += placeholder;
            pos = endPos;
            continue;
        }

        result += source[pos];
        pos++;
    }

    return result;
}

// =============================================================================
// restoreStrings: 恢复字符串字面量
// =============================================================================

std::string CommentRemovalStrategy::restoreStrings(
    const std::string& source,
    const std::map<std::string, std::string>& stringMap) {

    std::string result = source;
    for (const auto& pair : stringMap) {
        size_t pos = 0;
        while ((pos = result.find(pair.first, pos)) != std::string::npos) {
            result.replace(pos, pair.first.length(), pair.second);
            pos += pair.second.length();
        }
    }
    return result;
}

// =============================================================================
// removeBlockComments: 移除多行注释
// =============================================================================

std::string CommentRemovalStrategy::removeBlockComments(const std::string& source) {
    std::string result;
    size_t pos = 0;
    size_t length = source.length();

    while (pos < length) {
        // 查找 /* 注释开始
        if (pos + 1 < length && source[pos] == '/' && source[pos + 1] == '*') {
            size_t endPos = pos + 2;
            // 查找 */ 注释结束
            // 修复：允许检查到文件末尾的最后两个字符
            while (endPos + 1 <= length) {
                if (endPos + 1 < length && source[endPos] == '*' && source[endPos + 1] == '/') {
                    endPos += 2;
                    break;
                }
                endPos++;
            }
            // 跳过注释，但保留换行符
            for (size_t i = pos; i < endPos; i++) {
                if (source[i] == '\n') {
                    result += '\n';
                }
            }
            pos = endPos;
            continue;
        }
        result += source[pos];
        pos++;
    }

    return result;
}

// =============================================================================
// removeLineComments: 移除单行注释
// =============================================================================

std::string CommentRemovalStrategy::removeLineComments(const std::string& source) {
    std::string result;
    size_t pos = 0;
    size_t length = source.length();

    while (pos < length) {
        // 查找 // 注释开始
        if (pos + 1 < length && source[pos] == '/' && source[pos + 1] == '/') {
            // 跳过注释内容直到行尾
            while (pos < length && source[pos] != '\n') {
                pos++;
            }
            // 保留换行符
            if (pos < length && source[pos] == '\n') {
                result += '\n';
                pos++;
            }
            continue;
        }
        result += source[pos];
        pos++;
    }

    return result;
}

// =============================================================================
// removeComments: 移除所有注释（主入口）
// =============================================================================

std::string CommentRemovalStrategy::removeComments(const std::string& source) {
    // 重新实现：直接移除注释，不使用占位符
    // 这样可以避免字符串中的引号被误处理

    std::string result;
    size_t pos = 0;
    size_t length = source.length();

    while (pos < length) {
        // 检查是否在字符串中（跳过字符串字面量）
        bool inString = false;
        char stringDelimiter = '\0';

        if (pos + 1 < length && source[pos] == '@' && source[pos + 1] == '"') {
            // Objective-C 字符串 @"..."
            inString = true;
            stringDelimiter = '@';
            result += source[pos++];  // Add '@'
            result += source[pos++];  // Add '"'
        } else if (source[pos] == '"') {
            // C 字符串 "..."
            inString = true;
            stringDelimiter = '"';
            result += source[pos++];
        } else if (source[pos] == '\'') {
            // 字符字面量 '...'
            inString = true;
            stringDelimiter = '\'';
            result += source[pos++];
        }

        // 如果在字符串中，找到字符串结束
        if (inString) {
            while (pos < length) {
                result += source[pos];
                if (source[pos] == '\\') {
                    // 转义字符，跳过下一个字符
                    pos++;
                    if (pos < length) {
                        result += source[pos];
                        pos++;
                    }
                } else if (source[pos] == '"') {
                    pos++;
                    if (stringDelimiter == '@' || stringDelimiter == '"') {
                        break;
                    }
                } else if (source[pos] == '\'') {
                    pos++;
                    if (stringDelimiter == '\'') {
                        break;
                    }
                } else {
                    pos++;
                }
            }
            continue;
        }

        // 不在字符串中，检查注释
        // 检查块注释 /* */
        if (pos + 1 < length && source[pos] == '/' && source[pos + 1] == '*') {
            size_t endPos = pos + 2;
            while (endPos + 1 <= length) {
                if (endPos + 1 < length && source[endPos] == '*' && source[endPos + 1] == '/') {
                    endPos += 2;
                    break;
                }
                endPos++;
            }
            // 保留换行符
            for (size_t i = pos; i < endPos; i++) {
                if (source[i] == '\n') {
                    result += '\n';
                }
            }
            pos = endPos;
            continue;
        }

        // 【新增】检查并删除 #pragma mark 行
        // 只检查 "#pragma mark"，后面可以跟任意内容（如 "- Class methods"）
        if (pos + 12 < length &&
            source[pos] == '#' &&
            source[pos + 1] == 'p' &&
            source[pos + 2] == 'r' &&
            source[pos + 3] == 'a' &&
            source[pos + 4] == 'g' &&
            source[pos + 5] == 'm' &&
            source[pos + 6] == 'a' &&
            source[pos + 7] == ' ' &&
            source[pos + 8] == 'm' &&
            source[pos + 9] == 'a' &&
            source[pos + 10] == 'r' &&
            source[pos + 11] == 'k') {
            // 找到 #pragma mark，跳过整行（包括后面的内容）
            while (pos < length && source[pos] != '\n') {
                pos++;
            }
            // 可选：保留换行符（删除后保持代码结构）
            // if (pos < length && source[pos] == '\n') {
            //     result += '\n';
            //     pos++;
            // }
            continue;
        }

        // 检查行注释 //
        if (pos + 1 < length && source[pos] == '/' && source[pos + 1] == '/') {
            while (pos < length && source[pos] != '\n') {
                pos++;
            }
            if (pos < length && source[pos] == '\n') {
                result += '\n';
                pos++;
            }
            continue;
        }

        // 普通字符
        result += source[pos];
        pos++;
    }

    return result;
}

} // namespace obfuscator
