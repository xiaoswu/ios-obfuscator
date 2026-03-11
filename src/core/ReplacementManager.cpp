#include "core/ReplacementManager.h"
#include "core/Logger.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"
#include <algorithm>

namespace obfuscator {

ReplacementManager::ReplacementManager() {
}

ReplacementManager::~ReplacementManager() {
}

void ReplacementManager::addReplacement(clang::SourceLocation loc,
                                        const std::string& original,
                                        const std::string& replacement,
                                        int priority,
                                        const std::string& strategyName) {
    replacements_.emplace_back(loc, original, replacement, original.length(),
                                priority, strategyName);
}

void ReplacementManager::addReplacements(const std::vector<Replacement>& replacements) {
    replacements_.insert(replacements_.end(), replacements.begin(), replacements.end());
}

void ReplacementManager::addInsertion(clang::SourceLocation loc,
                                     const std::string& text,
                                     bool afterToken,
                                     int priority,
                                     const std::string& strategyName) {
    insertions_.emplace_back(loc, text, afterToken, priority, strategyName);
}

void ReplacementManager::addInsertions(const std::vector<Insertion>& insertions) {
    insertions_.insert(insertions_.end(), insertions.begin(), insertions.end());
}

void ReplacementManager::clear() {
    replacements_.clear();
    insertions_.clear();
}

void ReplacementManager::sortReplacements(const clang::SourceManager& SM) {
    // 按原始 offset 从大到小排序
    // 这样从后往前替换，不会影响前面的位置
    // 【修复】使用拼写位置来处理宏内的代码
    std::sort(replacements_.begin(), replacements_.end(),
        [&SM](const Replacement& a, const Replacement& b) {
            clang::SourceLocation spellLocA = SM.getSpellingLoc(a.loc);
            clang::SourceLocation spellLocB = SM.getSpellingLoc(b.loc);
            unsigned offsetA = SM.getFileOffset(spellLocA);
            unsigned offsetB = SM.getFileOffset(spellLocB);
            if (offsetA != offsetB) {
                return offsetA > offsetB;  // 降序
            }
            // 如果 offset 相同，按优先级排序
            return a.priority > b.priority;
        });
}

void ReplacementManager::filterOverlaps(const clang::SourceManager& SM) {
    if (replacements_.empty()) return;

    std::vector<Replacement> filtered;
    std::set<unsigned> usedOffsets;

    for (const auto& repl : replacements_) {
        // 【修复】使用拼写位置来处理宏内的代码
        clang::SourceLocation spellLoc = SM.getSpellingLoc(repl.loc);
        unsigned offset = SM.getFileOffset(spellLoc);
        unsigned endOffset = offset + repl.originalLength;

        // 【调试】输出 isCancelled 的过滤信息
        if (repl.original == "isCancelled") {
            std::string filePath = SM.getFilename(spellLoc).str();
            std::string fileName = filePath.substr(filePath.find_last_of("/") + 1);
            LOG_INFO("DEBUG FILTER: processing isCancelled in " + fileName +
                     " at offset " + std::to_string(offset) +
                     ", endOffset=" + std::to_string(endOffset));
        }

        // 检查是否与已使用的区间重叠
        bool overlaps = false;
        std::string overlapWith;
        for (unsigned used : usedOffsets) {
            if (offset < used && endOffset > used) {
                overlaps = true;
                overlapWith = "offset " + std::to_string(used);
                break;
            }
        }

        if (repl.original == "isCancelled" && overlaps) {
            LOG_INFO("DEBUG FILTER: isCancelled overlaps with " + overlapWith + ", will be filtered out!");
        }

        if (!overlaps) {
            filtered.push_back(repl);
            // 标记这个区间已被使用
            for (unsigned o = offset; o < endOffset; ++o) {
                usedOffsets.insert(o);
            }
        }
    }

    if (filtered.size() != replacements_.size()) {
        LOG_INFO("Filtered " + std::to_string(replacements_.size() - filtered.size()) +
                 " overlapping replacements");
    }

    replacements_ = std::move(filtered);
}

void ReplacementManager::sortInsertions(const clang::SourceManager& SM) {
    // 按原始 offset 从大到小排序
    // 这样从后往前插入，不会影响前面的位置
    std::sort(insertions_.begin(), insertions_.end(),
        [&SM](const Insertion& a, const Insertion& b) {
            clang::SourceLocation spellLocA = SM.getSpellingLoc(a.loc);
            clang::SourceLocation spellLocB = SM.getSpellingLoc(b.loc);
            unsigned offsetA = SM.getFileOffset(spellLocA);
            unsigned offsetB = SM.getFileOffset(spellLocB);
            if (offsetA != offsetB) {
                return offsetA > offsetB;  // 降序
            }
            // 如果 offset 相同，按优先级排序
            return a.priority > b.priority;
        });
}

size_t ReplacementManager::detectOverlaps(const clang::SourceManager& SM) {
    if (replacements_.empty()) return 0;

    size_t overlapCount = 0;
    std::map<unsigned, const Replacement*> offsetMap;

    for (const auto& repl : replacements_) {
        // 【修复】使用拼写位置来处理宏内的代码
        clang::SourceLocation spellLoc = SM.getSpellingLoc(repl.loc);
        unsigned offset = SM.getFileOffset(spellLoc);
        unsigned endOffset = offset + repl.originalLength;

        // 检查这个区间是否与已有替换重叠
        for (unsigned o = offset; o < endOffset; ++o) {
            if (offsetMap.find(o) != offsetMap.end()) {
                overlapCount++;
                const Replacement* existing = offsetMap[o];
                LOG_WARNING("Overlap detected: " + repl.strategyName +
                           " (offset " + std::to_string(offset) + ")" +
                           " overlaps with " + existing->strategyName);
                break;
            }
        }

        // 记录这个区间
        for (unsigned o = offset; o < endOffset; ++o) {
            offsetMap[o] = &repl;
        }
    }

    return overlapCount;
}

size_t ReplacementManager::applyAll(clang::ASTContext& context, clang::Rewriter& rewriter) {
    const clang::SourceManager& SM = context.getSourceManager();

    if (replacements_.empty() && insertions_.empty()) {
        LOG_INFO("No replacements or insertions to apply");
        return 0;
    }

    LOG_INFO("Applying " + std::to_string(replacements_.size()) + " replacements and " +
             std::to_string(insertions_.size()) + " insertions");

    // 检测重叠
    size_t overlaps = detectOverlaps(SM);
    if (overlaps > 0) {
        LOG_WARNING("Detected " + std::to_string(overlaps) + " overlapping replacements");
        // 过滤重叠的替换
        filterOverlaps(SM);
    }

    // 按位置排序
    sortReplacements(SM);

    // 应用替换
    size_t applied = 0;
    std::set<unsigned> appliedOffsets;

    for (const auto& repl : replacements_) {
        if (!repl.loc.isValid()) {
            LOG_WARNING("Invalid location for replacement in " + repl.strategyName);
            continue;
        }

        // 【修复】对于宏内的代码，获取宏扩展位置而不是宏定义位置
        // getSpellingLoc() 返回拼写位置（宏调用的位置），而不是宏定义的位置
        clang::SourceLocation spellLoc = SM.getSpellingLoc(repl.loc);

        // 【重要修复】FileID 也必须使用拼写位置，否则会导致 FileID 与 offset 不匹配
        // 如果 repl.loc 在宏中，getFileID(repl.loc) 可能返回宏定义文件的 FileID
        // 而 getFileOffset(spellLoc) 返回的是宏调用位置的 offset，两者不匹配
        clang::FileID fileID = SM.getFileID(spellLoc);

        // 【方案 1】只应用主文件中的替换
        // 当处理 .m 文件时，如果属性声明在 #import 的 .h 文件中，
        // fileID 会指向 .h 文件，但当前 Rewriter 只初始化了 .m 文件的 buffer
        // 这种情况下跳过该替换，该替换会在处理 .h 文件时应用
        clang::FileID mainFileID = SM.getMainFileID();
        if (fileID != mainFileID) {
            // 记录跳过的替换（调试用）
            static int skipCount = 0;
            static std::string lastSkippedFile;
            std::string currentFile = SM.getFilename(spellLoc).str();
            if (currentFile != lastSkippedFile) {
                LOG_INFO("Skipping " + std::to_string(++skipCount) + " replacements in non-main file: " + currentFile);
                lastSkippedFile = currentFile;
            }
            continue;
        }

        // 检查是否在系统头文件中
        if (SM.isInSystemHeader(spellLoc)) {
            continue;
        }

        // 检查是否已经应用过（避免重复）
        unsigned offset = SM.getFileOffset(spellLoc);
        if (appliedOffsets.find(offset) != appliedOffsets.end()) {
            continue;
        }

        // 【调试】检查 Rewriter 是否有此文件的 buffer
        bool hasRewriteBuffer = rewriter.getRewriteBufferFor(fileID) != nullptr;

        // 【调试】输出 isCancelled 的详细调试信息
        std::string filePath = SM.getFilename(spellLoc).str();
        std::string fileName = filePath.substr(filePath.find_last_of("/") + 1);
        if (repl.original == "isCancelled") {
            // 获取主文件的 FileID
            clang::FileID mainFileID = SM.getMainFileID();
            clang::FileID locFileID = SM.getFileID(repl.loc);

            LOG_INFO("DEBUG APPLY: file='" + fileName + "'" +
                     ", fileID(from spellLoc)=" + std::to_string(fileID.getHashValue()) +
                     ", fileID(from repl.loc)=" + std::to_string(locFileID.getHashValue()) +
                     ", mainFileID=" + std::to_string(mainFileID.getHashValue()) +
                     ", offset=" + std::to_string(offset) +
                     ", hasBuffer=" + (hasRewriteBuffer ? "yes" : "no") +
                     ", isInMainFile=" + (SM.isInMainFile(spellLoc) ? "yes" : "no"));

            // 检查主文件是否有 buffer
            bool mainFileHasBuffer = rewriter.getRewriteBufferFor(mainFileID) != nullptr;
            LOG_INFO("DEBUG APPLY: mainFileHasBuffer=" + std::string(mainFileHasBuffer ? "yes" : "no"));

            // 如果 fileID 和 mainFileID 不同，检查 spellLoc 的 FileID
            if (fileID != mainFileID) {
                clang::FileID spellFileID = SM.getFileID(spellLoc);
                LOG_INFO("DEBUG APPLY: spellFileID=" + std::to_string(spellFileID.getHashValue()) +
                         ", fileID==spellFileID? " + (fileID == spellFileID ? "yes" : "no"));
            }
        }

        // 【修复】如果 Rewriter 没有 buffer，创建一个
        if (!hasRewriteBuffer) {
            // 调用 getEditBuffer 会自动创建 buffer（如果不存在）
            rewriter.getEditBuffer(fileID);
        }

        // 应用替换（使用拼写位置的偏移，但原始 FileID）
        // 【修复】尝试使用 CharSourceRange 来替换
        // 这比直接使用 SourceLocation + length 更可靠
        clang::CharSourceRange charRange = clang::CharSourceRange::getCharRange(
            spellLoc,
            spellLoc.getLocWithOffset(repl.originalLength)
        );

        // 【调试】检查 range 是否有效
        if (repl.original == "isCancelled") {
            bool rangeValid = charRange.isValid();
            bool rangeBeginValid = charRange.getBegin().isValid();
            bool rangeEndValid = charRange.getEnd().isValid();

            // 【调试】获取实际的源代码文本
            bool invalid = false;
            const char* startChar = SM.getCharacterData(spellLoc, &invalid);
            std::string actualText;
            if (!invalid && startChar) {
                actualText = std::string(startChar, std::min(30UL, strlen(startChar)));
            }

            LOG_INFO("DEBUG APPLY: rangeValid=" + std::string(rangeValid ? "yes" : "no") +
                     ", beginValid=" + std::string(rangeBeginValid ? "yes" : "no") +
                     ", endValid=" + std::string(rangeEndValid ? "yes" : "no") +
                     ", actualText at loc='" + actualText + "'");

            // 【调试】检查 CharSourceRange 的 token 范围
            clang::CharSourceRange tokenRange = clang::CharSourceRange::getTokenRange(
                spellLoc,
                spellLoc.getLocWithOffset(repl.originalLength)
            );
            bool tokenRangeValid = tokenRange.isValid();
            LOG_INFO("DEBUG APPLY: tokenRangeValid=" + std::string(tokenRangeValid ? "yes" : "no"));

            // 【调试】检查 isRewriteBufferFor 的结果
            clang::FileID checkFileID = SM.getFileID(spellLoc);
            auto* buffer = rewriter.getRewriteBufferFor(checkFileID);
            if (buffer) {
                LOG_INFO("DEBUG APPLY: buffer size=" + std::to_string(buffer->size()) +
                         ", offset=" + std::to_string(offset));
            }
        }

        bool success = rewriter.ReplaceText(charRange, repl.replacement);
        if (success) {
            applied++;
            appliedOffsets.insert(offset);
            if (repl.original == "isCancelled") {
                LOG_INFO("DEBUG APPLY: SUCCESSfully applied isCancelled replacement");
            }
        } else {
            // 【调试】记录失败的替换
            LOG_WARNING("Failed to apply replacement in " + fileName +
                       " at offset " + std::to_string(offset) +
                       ": '" + repl.original + "' → '" + repl.replacement + "'");
        }
    }

    LOG_INFO("Successfully applied " + std::to_string(applied) + " replacements");

    // ========================================
    // 应用插入操作（在替换之后执行）
    // ========================================
    if (!insertions_.empty()) {
        LOG_INFO("Applying " + std::to_string(insertions_.size()) + " insertions");

        // 按位置排序插入操作
        sortInsertions(SM);

        // 应用插入
        size_t inserted = 0;
        std::set<unsigned> insertedOffsets;

        for (const auto& ins : insertions_) {
            if (!ins.loc.isValid()) {
                LOG_WARNING("Invalid location for insertion in " + ins.strategyName);
                continue;
            }

            clang::SourceLocation spellLoc = SM.getSpellingLoc(ins.loc);
            clang::FileID fileID = SM.getFileID(spellLoc);
            clang::FileID mainFileID = SM.getMainFileID();

            // 只处理主文件中的插入
            if (fileID != mainFileID) {
                continue;
            }

            // 检查是否在系统头文件中
            if (SM.isInSystemHeader(spellLoc)) {
                continue;
            }

            // 检查是否已经应用过（避免重复）
            unsigned offset = SM.getFileOffset(spellLoc);
            if (insertedOffsets.find(offset) != insertedOffsets.end()) {
                continue;
            }

            // 确保 Rewriter 有 buffer
            if (!rewriter.getRewriteBufferFor(fileID)) {
                rewriter.getEditBuffer(fileID);
            }

            // 应用插入
            bool success = false;
            if (ins.afterToken) {
                success = rewriter.InsertTextAfterToken(spellLoc, ins.text);
            } else {
                success = rewriter.InsertText(spellLoc, ins.text);
            }

            if (success) {
                inserted++;
                insertedOffsets.insert(offset);
            } else {
                std::string filePath = SM.getFilename(spellLoc).str();
                std::string fileName = filePath.substr(filePath.find_last_of("/") + 1);
                LOG_WARNING("Failed to apply insertion in " + fileName +
                           " at offset " + std::to_string(offset));
            }
        }

        LOG_INFO("Successfully applied " + std::to_string(inserted) + " insertions");
        applied += inserted;
    }

    return applied;
}

} // namespace obfuscator
