/**
 * @file PropertyNameStrategy.h
 * @brief 属性名混淆策略头文件
 *
 * 定义PropertyNameObfuscationStrategy类，用于混淆Objective-C属性名。
 */

#ifndef PROPERTY_NAME_STRATEGY_H
#define PROPERTY_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include "core/AhoCorasick.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/DeclObjC.h"
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <memory>

namespace obfuscator {

// 前向声明
class ReplacementManager;

/**
 * @struct PropertyInfo
 * @brief 属性信息结构体
 */
struct PropertyInfo {
    std::string originalName;           // 原始属性名
    std::string obfuscatedName;          // 混淆后的属性名
    std::string className;               // 所属类名
    std::string getterName;              // getter方法名（混淆后）
    std::string setterName;              // setter方法名（混淆后）
    std::string ivarName;                // 成员变量名
    std::string originalGetterName;      // 原始getter名称（如果有自定义）
    std::string originalSetterName;      // 原始setter名称（如果有自定义）
    bool isBoolean;                      // 是否布尔类型（带is前缀）
    bool hasCustomGetter;                // 是否自定义getter
    bool hasCustomSetter;                // 是否自定义setter
    bool isIBOutlet;                     // 是否IBOutlet
    bool isReadonly;                     // 是否只读

    PropertyInfo() : isBoolean(false), hasCustomGetter(false),
                     hasCustomSetter(false), isIBOutlet(false), isReadonly(false) {}
};

/**
 * @enum PropertyReplacementType
 * @brief 替换项类型（用于匿名命名空间的属性替换收集）
 */
enum class PropertyReplacementType {
    PropertyName,    // 属性名声明
    PropertyRef,     // 属性引用（点语法）
    IvarRef,         // 成员变量引用
    GetterMethod,    // getter方法
    SetterMethod     // setter方法
};

/**
 * @class PropertyNameObfuscationStrategy
 * @brief 属性名混淆策略
 *
 * 该策略负责混淆：
 * - @property 声明中的属性名
 * - 点语法访问 self.property
 * - 成员变量 _property
 * - 属性的 getter/setter 方法（同步处理）
 * - KVO/KVC 字符串中的属性名
 *
 * 保存到 SymbolTable 供方法名混淆使用：
 * - 属性的原始 getter/setter 名称 → 混淆后的 getter/setter 名称
 *
 * 保留不混淆：
 * - IBOutlet/IBInspectable 属性
 * - 白名单中的属性
 * - 系统属性（delegate、dataSource等）
 */
class PropertyNameObfuscationStrategy : public ObfuscationStrategy,
                                        public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    std::string getName() const override {
        return "PropertyNameObfuscation";
    }

    std::string getDescription() const override {
        return "混淆Objective-C属性名";
    }

    void analyze(clang::ASTContext& context) override;
    // 【新架构】使用 collectReplacements 收集替换项
    void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) override;
    bool validate(clang::ASTContext& context) const override;
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    // 属性处理
    void handleProperty(const clang::ObjCPropertyDecl* propDecl);
    bool shouldSkipProperty(const clang::ObjCPropertyDecl* propDecl) const;
    bool isIBOutletProperty(const clang::ObjCPropertyDecl* propDecl) const;
    bool isBooleanProperty(const clang::ObjCPropertyDecl* propDecl) const;
    bool isSystemProperty(const std::string& propName) const;

    // 名称生成
    std::string generateGetterName(const std::string& obfuscatedName, bool isBoolean);
    std::string generateSetterName(const std::string& obfuscatedName);
    std::string generateIvarName(const std::string& obfuscatedName);

    // 【新架构】收集属性相关的替换项
    void collectPropertyReplacements(clang::ASTContext& context, ReplacementManager& manager);

    // 【新架构】收集 KVO/KVC 字符串中的属性名替换
    void collectKeyPathReplacements(clang::ASTContext& context, ReplacementManager& manager);

    // 辅助方法
    std::string getPropertyKey(const std::string& className, const std::string& propName) const;
    std::string getClassNameFromProperty(const clang::ObjCPropertyDecl* propDecl) const;
    static std::string capitalizeFirstLetter(const std::string& str);

    // 【P1 性能优化】字符串操作工具函数
    static bool hasIsPrefix(const std::string& str);
    static std::string removeIsPrefix(const std::string& str);
    static std::string removeUnderscore(const std::string& str);

    // 【性能优化】构建 Aho-Corasick 多模式匹配器
    void buildPatternMatcher();

    clang::ast_matchers::MatchFinder finder_;

    // 存储需要混淆的属性
    std::map<std::string, PropertyInfo> propertiesToObfuscate_;

    // 原始名到混淆名的快速映射
    std::map<std::string, std::string> nameMapping_;

    // 全局属性混淆缓存：相同属性名使用相同混淆名
    // key: 原始属性名, value: 混淆后的名称
    std::map<std::string, std::string> globalPropertyObfuscationCache_;

    // 当前处理的类
    std::string currentClass_;

    // 性能优化：使用 unordered_set 替代 set，实现 O(1) 查找
    struct SourceLocationHash {
        size_t operator()(const clang::SourceLocation& loc) const noexcept {
            return loc.getHashValue();
        }
    };
    std::unordered_set<clang::SourceLocation, SourceLocationHash> replacedLocations_;

    // Aho-Corasick 多模式匹配器（性能优化：单次遍历匹配所有属性）
    std::unique_ptr<MultiPatternMatcher> patternMatcher_;
};

} // namespace obfuscator

#endif // PROPERTY_NAME_STRATEGY_H
