#ifndef REPLACEMENT_MANAGER_H
#define REPLACEMENT_MANAGER_H

#include <clang/Basic/SourceLocation.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace clang {
    class SourceManager;
    class ASTContext;
}

namespace obfuscator {

/**
 * @brief 单个替换项
 *
 * 描述一个源代码位置的替换操作
 */
struct Replacement {
    clang::SourceLocation loc;       // 原始位置
    std::string original;            // 原始文本
    std::string replacement;         // 替换文本
    unsigned originalLength;         // 原始长度
    int priority;                    // 优先级（用于处理冲突，值越大优先级越高）
    std::string strategyName;        // 策略名称（用于调试）

    // 构造函数
    Replacement(clang::SourceLocation l, const std::string& orig,
              const std::string& repl, unsigned len, int pri = 0,
              const std::string& strategy = "")
        : loc(l), original(orig), replacement(repl), originalLength(len),
          priority(pri), strategyName(strategy) {}

    // 默认构造函数
    Replacement() : loc(), original(), replacement(), originalLength(0),
                    priority(0), strategyName() {}
};

/**
 * @brief 单个插入项
 *
 * 描述在源代码位置插入新文本的操作（不是替换）
 */
struct Insertion {
    clang::SourceLocation loc;       // 插入位置
    std::string text;                // 要插入的文本
    bool afterToken;                 // true: InsertTextAfterToken, false: InsertTextBefore
    int priority;                    // 优先级（用于处理冲突，值越大优先级越高）
    std::string strategyName;        // 策略名称（用于调试）

    // 构造函数
    Insertion(clang::SourceLocation l, const std::string& t,
              bool after = true, int pri = 0,
              const std::string& strategy = "")
        : loc(l), text(t), afterToken(after), priority(pri),
          strategyName(strategy) {}

    // 默认构造函数
    Insertion() : loc(), text(), afterToken(true),
                  priority(0), strategyName() {}
};

/**
 * @brief 替换管理器
 *
 * 负责收集所有混淆策略的替换请求，并按正确顺序统一应用，
 * 避免多个策略直接操作 Rewriter 导致的 SourceLocation 错位问题
 */
class ReplacementManager {
public:
    ReplacementManager();
    ~ReplacementManager();

    /**
     * @brief 添加一个替换项
     *
     * @param loc 原始位置
     * @param original 原始文本
     * @param replacement 替换文本
     * @param priority 优先级（用于处理冲突）
     * @param strategyName 策略名称（用于调试）
     */
    void addReplacement(clang::SourceLocation loc,
                       const std::string& original,
                       const std::string& replacement,
                       int priority = 0,
                       const std::string& strategyName = "");

    /**
     * @brief 批量添加替换项
     */
    void addReplacements(const std::vector<Replacement>& replacements);

    /**
     * @brief 添加一个插入项
     *
     * @param loc 插入位置
     * @param text 要插入的文本
     * @param afterToken 是否在 token 之后插入（默认 true）
     * @param priority 优先级（用于处理冲突）
     * @param strategyName 策略名称（用于调试）
     */
    void addInsertion(clang::SourceLocation loc,
                      const std::string& text,
                      bool afterToken = true,
                      int priority = 0,
                      const std::string& strategyName = "");

    /**
     * @brief 批量添加插入项
     */
    void addInsertions(const std::vector<Insertion>& insertions);

    /**
     * @brief 应用所有替换到 Rewriter
     *
     * @param context AST 上下文
     * @param rewriter Rewriter 实例
     * @return 成功应用的替换数量
     */
    size_t applyAll(clang::ASTContext& context, clang::Rewriter& rewriter);

    /**
     * @brief 清空所有替换项
     */
    void clear();

    /**
     * @brief 获取替换项数量
     */
    size_t size() const { return replacements_.size() + insertions_.size(); }

    /**
     * @brief 检查是否为空
     */
    bool empty() const { return replacements_.empty() && insertions_.empty(); }

    /**
     * @brief 获取替换项数量（仅替换）
     */
    size_t replacementCount() const { return replacements_.size(); }

    /**
     * @brief 获取插入项数量（仅插入）
     */
    size_t insertionCount() const { return insertions_.size(); }

    /**
     * @brief 获取所有替换项（用于调试）
     */
    const std::vector<Replacement>& getAll() const { return replacements_; }

    /**
     * @brief 检测并处理重叠的替换
     *
     * @return 检测到的重叠数量
     */
    size_t detectOverlaps(const clang::SourceManager& SM);

private:
    std::vector<Replacement> replacements_;
    std::vector<Insertion> insertions_;

    /**
     * @brief 按位置排序替换项（从后往前）
     */
    void sortReplacements(const clang::SourceManager& SM);

    /**
     * @brief 按位置排序插入项（从后往前）
     */
    void sortInsertions(const clang::SourceManager& SM);

    /**
     * @brief 过滤重叠的替换
     */
    void filterOverlaps(const clang::SourceManager& SM);
};

} // namespace obfuscator

#endif // REPLACEMENT_MANAGER_H
