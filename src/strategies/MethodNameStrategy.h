/**
 * @file MethodNameStrategy.h
 * @brief 方法名混淆策略头文件
 *
 * 定义MethodNameObfuscationStrategy类，用于混淆Objective-C方法名、参数名和Block变量名。
 */

#ifndef METHOD_NAME_STRATEGY_H
#define METHOD_NAME_STRATEGY_H

#include "strategies/ObfuscationStrategy.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <map>
#include <set>
#include <vector>
#include <unordered_set>

namespace obfuscator {

// 前向声明
class ReplacementManager;

/**
 * @struct MethodInfo
 * @brief 方法信息结构体
 */
struct MethodInfo {
    std::string originalName;           // 原始方法名
    std::string obfuscatedName;          // 混淆后的方法名（单参数方法）
    std::string selector;                // 完整selector（包含冒号）
    bool isInstanceMethod;              // 是否为实例方法（vs 类方法）
    std::string className;              // 所属类名
    std::vector<std::string> params;    // 参数名列表

    // 多参数方法的每个selector片段的混淆名（按顺序存储）
    // 例如: "doTask:withParam:" -> ["doTaskAbc", "withParamXyz"]
    std::vector<std::string> pieceObfuscatedNames;
};

/**
 * @struct BlockInfo
 * @brief Block变量信息结构体
 */
struct BlockInfo {
    std::string originalName;           // 原始变量名
    std::string obfuscatedName;          // 混淆后的变量名
    std::string varType;                // 变量类型
    bool isParameter;                   // 是否为参数（vs 局部变量）
};

/**
 * @struct SetterGetterPair
 * @brief Setter/Getter 方法对信息结构体
 *
 * 用于识别手动实现的 setter/getter 方法对（没有 @property 声明）
 * 例如: - (void)setQueen:(NSString *)queeen; 和 -(NSString *)queen;
 */
struct SetterGetterPair {
    std::string baseName;           // queen - 属性基础名
    std::string setterSelector;     // setQueen: - setter selector
    std::string getterSelector;     // queen (或 isQueen) - getter selector
    std::string obfuscatedBaseName; // adjustPreparing - 混淆后的基础名
    std::string className;          // 所属类名
    bool isBoolean;                // 是否布尔属性
    bool hasSetter;                // 是否有 setter
    bool hasGetter;                // 是否有 getter

    // 生成混淆后的 setter 名称
    std::string getObfuscatedSetter() const {
        if (!hasSetter) return "";
        std::string base = obfuscatedBaseName;
        if (!base.empty() && base[0] >= 'a' && base[0] <= 'z') {
            base[0] = base[0] - 32;  // 首字母大写
        }
        return "set" + base + ":";
    }

    // 生成混淆后的 getter 名称
    std::string getObfuscatedGetter() const {
        if (!hasGetter) return "";
        if (isBoolean) {
            std::string base = obfuscatedBaseName;
            if (!base.empty() && base[0] >= 'a' && base[0] <= 'z') {
                base[0] = base[0] - 32;
            }
            return "is" + base;
        }
        return obfuscatedBaseName;
    }
};

/**
 * @struct SelectorTraits
 * @brief Selector 预分析结构体（性能优化）
 *
 * 一次性分析 selector 的所有特征，避免重复的字符串操作。
 * 使用位标记和预计算值实现 O(1) 特征查询。
 * 预留扩展空间，方便后续添加新特性。
 */
struct SelectorTraits {
    // ========================================================================
    // 第一层：核心特征（热路径，使用位标记实现 O(1) 访问）
    // ========================================================================
    std::string selector;           // 完整 selector 字符串
    size_t hash;                    // 哈希值，用于快速比较
    size_t length;                  // 长度，避免重复调用 strlen()

    // 前缀标记（32 bits，预留扩展空间）
    uint32_t prefixFlags;
    enum PrefixFlag : uint32_t {
        FLAG_SET       = 1 << 0,   // "set"
        FLAG_IS        = 1 << 1,   // "is"
        FLAG_INIT      = 1 << 2,   // "init"
        FLAG_INITWITH  = 1 << 3,   // "initWith"
        FLAG_NEW       = 1 << 4,   // "new"
        FLAG_ALLOC     = 1 << 5,   // "alloc"
        FLAG_COPY      = 1 << 6,   // "copy" / "mutableCopy"
        FLAG_VIEW      = 1 << 7,   // "view" / "viewDid" / "viewWill"
        // 预留 8-31 位供未来扩展
    };

    // 内容标记（32 bits，预留扩展空间）
    uint32_t contentFlags;
    enum ContentFlag : uint32_t {
        FLAG_HAS_DID          = 1 << 0,  // 包含 "Did"
        FLAG_HAS_WILL         = 1 << 1,  // 包含 "Will"
        FLAG_HAS_SHOULD       = 1 << 2,  // 包含 "Should"
        FLAG_HAS_COLON        = 1 << 3,  // 包含 ":"
        FLAG_IS_LIFECYCLE     = 1 << 4,  // 是生命周期方法
        FLAG_IS_UI_DELEGATE   = 1 << 5,  // 是 UI delegate 方法
        FLAG_IS_SINGLETON     = 1 << 6,  // 单例方法 (shared, current)
        FLAG_HAS_SYSTEM_PREFIX = 1 << 7, // 有系统方法前缀 (init, view, did, will 等)
        // 预留 8-31 位供未来扩展
    };

    // ========================================================================
    // 第二层：结构化特征（中频使用）
    // ========================================================================
    struct ExtendedInfo {
        uint8_t paramCount;        // 参数数量（通过冒号数量计算）
        char firstChar;           // 首字符
        bool firstCharIsUpper;    // 首字母是否大写
        bool endsWithColon;       // 以冒号结尾
        bool isEmpty;             // 是否为空

        ExtendedInfo() : paramCount(0), firstChar('\0'),
                      firstCharIsUpper(false), endsWithColon(false), isEmpty(true) {}
    } ext;

    // ========================================================================
    // 便捷访问方法（内联，O(1)）
    // ========================================================================
    bool hasSetPrefix() const { return prefixFlags & FLAG_SET; }
    bool hasIsPrefix() const { return prefixFlags & FLAG_IS; }
    bool hasInitPrefix() const { return prefixFlags & FLAG_INIT; }
    bool hasInitWithPrefix() const { return prefixFlags & FLAG_INITWITH; }
    bool hasNewPrefix() const { return prefixFlags & FLAG_NEW; }
    bool hasAllocPrefix() const { return prefixFlags & FLAG_ALLOC; }
    bool hasCopyPrefix() const { return prefixFlags & FLAG_COPY; }
    bool hasViewPrefix() const { return prefixFlags & FLAG_VIEW; }

    bool hasDid() const { return contentFlags & FLAG_HAS_DID; }
    bool hasWill() const { return contentFlags & FLAG_HAS_WILL; }
    bool hasShould() const { return contentFlags & FLAG_HAS_SHOULD; }
    bool hasColon() const { return contentFlags & FLAG_HAS_COLON; }
    bool isLifecycle() const { return contentFlags & FLAG_IS_LIFECYCLE; }
    bool isUIDelegate() const { return contentFlags & FLAG_IS_UI_DELEGATE; }
    bool isSingleton() const { return contentFlags & FLAG_IS_SINGLETON; }
    bool hasSystemPrefix() const { return contentFlags & FLAG_HAS_SYSTEM_PREFIX; }

    // 组合判断
    bool isSetterPattern() const { return hasSetPrefix() && ext.paramCount == 1; }
    bool isGetterPattern() const { return hasIsPrefix() || (!hasSetPrefix() && !hasInitPrefix() && ext.paramCount == 0); }

    // ========================================================================
    // 静态工厂方法：分析 selector
    // ========================================================================
    static SelectorTraits analyze(const std::string& sel);
};

/**
 * @class MethodNameObfuscationStrategy
 * @brief 方法名混淆策略
 *
 * 该策略负责混淆：
 * - 方法名（自定义方法，不包括系统方法）
 * - 方法参数名
 * - 方法内的Block变量名
 * - 参数中的Block类型
 *
 * 保留不混淆：
 * - 系统方法（init、delegate、getter/setter、生命周期等）
 * - 属性名、成员变量、局部变量
 * - C函数
 */
class MethodNameObfuscationStrategy : public ObfuscationStrategy,
                                      public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    std::string getName() const override {
        return "MethodNameObfuscation";
    }

    std::string getDescription() const override {
        return "混淆Objective-C方法名、参数名和Block变量名";
    }

    void analyze(clang::ASTContext& context) override;
    // 【新架构】使用 collectReplacements 收集替换项
    void collectReplacements(clang::ASTContext& context, ReplacementManager& manager) override;
    bool validate(clang::ASTContext& context) const override;
    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    // 方法处理
    void handleMethod(const clang::ObjCMethodDecl* methodDecl);
    bool shouldSkipMethod(const clang::ObjCMethodDecl* methodDecl) const;
    bool isSystemMethod(const clang::ObjCMethodDecl* methodDecl) const;
    bool isProtocolMethod(const clang::ObjCMethodDecl* methodDecl) const;
    bool isInitMethod(const std::string& methodName) const;
    bool isGetterMethod(const clang::ObjCMethodDecl* methodDecl) const;
    bool isSetterMethod(const clang::ObjCMethodDecl* methodDecl) const;
    bool isDelegateMethod(const clang::ObjCMethodDecl* methodDecl) const;
    bool isLifecycleMethod(const std::string& methodName) const;

    // 检查是否重写系统类的方法
    bool isOverridingSystemMethod(const clang::ObjCMethodDecl* methodDecl) const;
    // 检查是否是系统属性的 setter
    bool isSetterOfSystemProperty(const clang::ObjCMethodDecl* methodDecl) const;
    // 检查是否是系统属性的 getter
    bool isGetterOfSystemProperty(const clang::ObjCMethodDecl* methodDecl) const;

    // 参数处理
    void processMethodParameters(const clang::ObjCMethodDecl* methodDecl);
    std::string generateParameterName(const std::string& originalName);
    std::string getObfuscatedParameterName(const std::string& originalName);

    // 协议处理
    void handleProtocolDecl(const clang::ObjCProtocolDecl* protocolDecl);

    // 【P0 性能优化】协议方法缓存初始化
    static void initializeProtocolMethodCache(clang::ASTContext& context);

    // Setter/Getter 配对处理
    void finalizeStandaloneMethods();  // 处理孤立的 setter/getter 方法

    // Block处理
    void handleBlockVariable(const clang::DeclStmt* declStmt);
    void processBlocksInMethod(const clang::Stmt* methodBody);
    void findAndProcessBlocks(const clang::Stmt* stmt);
    void handleBlockParameterRef(const clang::DeclRefExpr* declRef, clang::ASTContext* context);

    // P0 性能优化：批量替换方法，单次 AST 遍历处理所有替换
    void replaceAllInOnePass(clang::ASTContext& context, ReplacementManager& manager);

    // 处理宏定义值中的标识符替换
    void processMacroValueIdentifiers(
        clang::ASTContext& context,
        ReplacementManager& manager,
        const std::map<std::string, MethodInfo>& methodsToObfuscate,
        const std::unordered_map<std::string, std::string>& globalMethodObfuscationCache
    );

    // 辅助方法
    std::string getFullSelector(const clang::ObjCMethodDecl* methodDecl) const;
    std::string getMethodSignature(const clang::ObjCMethodDecl* methodDecl) const;
    std::string getClassNameFromMethod(const clang::ObjCMethodDecl* methodDecl) const;

    // 确保 selector 有正确数量的尾随冒号（防止空字符串崩溃）
    static std::string ensureTrailingColon(const std::string& selector, unsigned expectedColons);

    // 【重构】减少 handleMessageExpr 嵌套的辅助方法
    bool findObfuscatedNameForSelector(
        const std::string& selector,
        std::string& obfuscatedName,
        const std::vector<std::string>** pieceNamesPtr
    ) const;

    bool shouldSkipSystemMethodCall(
        const clang::ObjCMessageExpr* msgExpr,
        const std::string& selector,
        const clang::QualType& receiverType
    ) const;

    bool hasMethodInInterface(
        const clang::ObjCInterfaceDecl* interfaceDecl,
        const std::string& selector
    ) const;

    bool checkInheritanceChainForSystemMethod(
        const clang::ObjCInterfaceDecl* classDecl,
        const std::string& selector
    ) const;

    clang::ast_matchers::MatchFinder finder_;

    // 存储需要混淆的方法
    std::map<std::string, MethodInfo> methodsToObfuscate_;

    // 存储自定义协议方法声明（用于混淆 @protocol 中的方法）
    // key: 协议名, value: 需要混淆的方法列表
    std::map<std::string, std::vector<std::pair<std::string, MethodInfo>>> protocolMethodsToObfuscate_;

    // Setter/Getter 配对处理
    // 存储已配对的 setter/getter 对
    std::map<std::string, SetterGetterPair> setterGetterPairs_;
    // 存储未配对的 setter（等待对应的 getter）
    std::map<std::string, MethodInfo> unpairedSetters_;
    // 存储未配对的 getter（等待对应的 setter）
    std::map<std::string, MethodInfo> unpairedGetters_;

    // 全局方法名混淆缓存（实现全局同名同混淆）
    // 相同的 selector 使用相同的混淆名，保持多态性
    static std::unordered_map<std::string, std::string> globalMethodObfuscationCache_;

    // 【P0 性能优化】协议方法缓存
    // 存储所有系统协议方法的 selector，避免 isProtocolMethod() 中的嵌套循环
    // key: selector, value: 协议名集合（一个方法可能在多个协议中定义）
    static std::unordered_map<std::string, std::unordered_set<std::string>> globalProtocolMethodCache_;
    static bool protocolCacheInitialized_;

    // 参数名映射（方法内局部）
    std::map<std::string, std::map<std::string, std::string>> methodParameters_;

    // Block变量名映射
    std::map<std::string, std::map<std::string, BlockInfo>> blockVariables_;

    // 当前正在处理的方法
    std::string currentMethod_;
    std::string currentClass_;

    // P1 性能优化：使用 unordered_set 替代 set，实现 O(1) 查找
    // 注意：SourceLocation 需要自定义哈希函数
    struct SourceLocationHash {
        size_t operator()(const clang::SourceLocation& loc) const noexcept {
            return loc.getHashValue();
        }
    };
    std::unordered_set<clang::SourceLocation, SourceLocationHash> replacedLocations_;
};

} // namespace obfuscator

#endif // METHOD_NAME_STRATEGY_H
