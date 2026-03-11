/**
 * @file CommentRemovalStrategy.h
 * @brief Comment Removal Strategy Header
 *
 * Defines CommentRemovalStrategy class for removing all comments from source code.
 */

#ifndef COMMENT_REMOVAL_STRATEGY_H
#define COMMENT_REMOVAL_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include <string>
#include <map>

namespace obfuscator {

// Forward declaration
class ReplacementManager;

/**
 * @class CommentRemovalStrategy
 * @brief Comment removal strategy
 *
 * This strategy removes all comments from source code:
 * - Single-line comments //
 * - Multi-line comments /* * /
 *
 * Processing flow:
 * 1. Protect string literals (avoid removing comment symbols inside strings)
 * 2. Remove multi-line comments
 * 3. Remove single-line comments
 * 4. Restore string literals
 *
 * Special handling:
 * - Preserve // and /* * / inside strings
 * - Preserve // in URLs
 * - Preserve comments in import lines
 */
class CommentRemovalStrategy : public ObfuscationStrategy {
public:
    std::string getName() const override {
        return "CommentRemoval";
    }

    std::string getDescription() const override {
        return "Remove all comments from source code";
    }

    // No AST analysis needed, comment removal done in text processing phase
    void analyze(clang::ASTContext& context) override {
        // Empty implementation, no AST analysis needed
    }

    // No replacement collection needed, comment removal done in text processing phase
    void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) override {
        // Empty implementation, no replacement collection needed
    }

    bool validate(clang::ASTContext& context) const override {
        return true;
    }

    /**
     * @brief Remove all comments from source code
     * @param source Original source code
     * @return Source code with comments removed
     */
    static std::string removeComments(const std::string& source);

private:
    /**
     * @brief Protect string literals by replacing with placeholders
     * @param source Source code
     * @param stringMap Map from placeholder to original string
     * @return Processed source code
     */
    static std::string protectStrings(const std::string& source,
                                     std::map<std::string, std::string>& stringMap);

    /**
     * @brief Restore string literals
     * @param source Source code
     * @param stringMap Map from placeholder to original string
     * @return Restored source code
     */
    static std::string restoreStrings(const std::string& source,
                                     const std::map<std::string, std::string>& stringMap);

    /**
     * @brief Remove multi-line comments /* * /
     * @param source Source code
     * @return Source code with multi-line comments removed
     */
    static std::string removeBlockComments(const std::string& source);

    /**
     * @brief Remove single-line comments //
     * @param source Source code
     * @return Source code with single-line comments removed
     */
    static std::string removeLineComments(const std::string& source);

    /**
     * @brief Generate a unique placeholder
     * @param index Index
     * @return Placeholder string
     */
    static std::string generatePlaceholder(size_t index);
};

} // namespace obfuscator

#endif // COMMENT_REMOVAL_STRATEGY_H
