#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>

// 前向声明
namespace obfuscator {
    class NameGenerator;
}

#include "core/NameGenerator.h"

namespace obfuscator {

enum class SymbolType {
    CLASS,
    METHOD,
    PROPERTY,
    VARIABLE,
    PROTOCOL,
    CATEGORY,
    FILE,
    STRING,
    RESOURCE
};

struct Symbol {
    std::string originalName;
    std::string obfuscatedName;
    SymbolType type;
    bool isPublic;
    std::vector<std::string> dependencies;
    std::string filePath;

    Symbol() : type(SymbolType::CLASS), isPublic(false) {}
};

/**
 * @struct PropertyMethodMapping
 * @brief 属性的 getter/setter 方法映射信息
 *
 * 用于方法名混淆时查找属性关联的方法
 */
struct PropertyMethodMapping {
    std::string originalPropertyName;      // 原始属性名 (如 "name")
    std::string obfuscatedPropertyName;     // 混淆后的属性名 (如 "obfName")

    // Getter 方法
    std::string originalGetterName;        // 原始 getter 方法名
    std::string obfuscatedGetterName;      // 混淆后的 getter 方法名
    bool hasCustomGetter;                  // 是否有自定义 getter

    // Setter 方法
    std::string originalSetterName;        // 原始 setter 方法名 (如 "setName:")
    std::string obfuscatedSetterName;      // 混淆后的 setter 方法名 (如 "setObfName:")
    bool hasCustomSetter;                  // 是否有自定义 setter

    bool isBoolean;                        // 是否布尔属性（getter 有 is 前缀）

    PropertyMethodMapping() : hasCustomGetter(false), hasCustomSetter(false), isBoolean(false) {}
};

class SymbolTable {
public:
    SymbolTable(NameGenerator* nameGenerator);
    ~SymbolTable();

    // 添加符号
    bool addSymbol(const std::string& originalName,
                  SymbolType type,
                  bool isPublic = false,
                  const std::string& filePath = "");

    // 获取混淆后的名称
    std::string getObfuscatedName(const std::string& originalName) const;

    // 获取原始名称
    std::string getOriginalName(const std::string& obfuscatedName) const;

    // 检查符号是否存在
    bool hasSymbol(const std::string& originalName) const;

    // 获取符号信息
    const Symbol* getSymbol(const std::string& originalName) const;

    // 添加依赖关系
    void addDependency(const std::string& symbolName, const std::string& dependency);

    // 获取所有符号
    const std::map<std::string, Symbol>& getAllSymbols() const { return symbols_; }

    // ========== 属性方法映射相关方法 ==========

    /**
     * 添加属性方法映射（由 PropertyNameStrategy 调用）
     * @param mapping 属性的 getter/setter 映射信息
     */
    void addPropertyMapping(const PropertyMethodMapping& mapping);

    /**
     * 检查方法名是否是某个属性的 getter
     * @param methodName 方法名
     * @return 如果是属性 getter，返回混淆后的 getter 名；否则返回空字符串
     */
    std::string getPropertyGetter(const std::string& methodName) const;

    /**
     * 检查方法名是否是某个属性的 setter
     * @param methodName 方法名
     * @return 如果是属性 setter，返回混淆后的 setter 名；否则返回空字符串
     */
    std::string getPropertySetter(const std::string& methodName) const;

    /**
     * 检查方法是否是属性的 getter（不返回混淆名）
     */
    bool isPropertyGetter(const std::string& methodName) const;

    /**
     * 检查方法是否是属性的 setter（不返回混淆名）
     */
    bool isPropertySetter(const std::string& methodName) const;

    /**
     * 获取所有属性方法映射（只读）
     */
    const std::unordered_map<std::string, PropertyMethodMapping>& getPropertyMappings() const {
        return propertyMappings_;
    }

    // ========== 方法符号映射相关方法（用于属性引用查找方法getter） ==========

    /**
     * @struct MethodSymbolInfo
     * @brief 方法符号信息（用于 PropertyNameStrategy 查找方法 getter）
     */
    struct MethodSymbolInfo {
        std::string originalName;           // 原始方法名
        std::string obfuscatedName;         // 混淆后的方法名
        bool isInstanceMethod;             // 是否是实例方法
        std::string className;             // 所属类名
        bool isGetter;                     // 是否是 getter 风格的方法（无参数）

        MethodSymbolInfo() : isInstanceMethod(true), isGetter(false) {}
    };

    /**
     * 添加方法符号信息（由 MethodNameStrategy 调用）
     * @param methodName 方法名
     * @param info 方法符号信息
     */
    void addMethodSymbol(const std::string& methodName, const MethodSymbolInfo& info);

    /**
     * 查找方法符号信息
     * @param methodName 方法名
     * @return 方法符号信息，如果不存在返回 nullptr
     */
    const MethodSymbolInfo* findMethodSymbol(const std::string& methodName) const;

    /**
     * 检查方法是否存在
     * @param methodName 方法名
     * @return 如果方法存在返回 true
     */
    bool hasMethodSymbol(const std::string& methodName) const;

    // ========== 方法参数映射相关方法（用于 Block 内捕获的参数混淆） ==========

    /**
     * @struct MethodParameterMapping
     * @brief 方法参数映射信息
     */
    struct MethodParameterMapping {
        std::map<std::string, std::string> paramMapping;  // 原始参数名 -> 混淆后的参数名
    };

    /**
     * 添加方法参数映射（由 MethodNameStrategy 调用）
     * @param selector 方法选择器
     * @param paramMapping 参数映射
     */
    void addMethodParameterMapping(const std::string& selector, const std::map<std::string, std::string>& paramMapping);

    /**
     * 查找方法参数映射
     * @param selector 方法选择器
     * @return 参数映射，如果不存在返回 nullptr
     */
    const MethodParameterMapping* findMethodParameterMapping(const std::string& selector) const;

    /**
     * 查找参数的混淆名
     * @param selector 方法选择器
     * @param originalParamName 原始参数名
     * @return 混淆后的参数名，如果不存在返回空字符串
     */
    std::string getObfuscatedParameterName(const std::string& selector, const std::string& originalParamName) const;

    /**
     * 查找参数的混淆名（遍历所有方法）
     * @param originalParamName 原始参数名
     * @return 混淆后的参数名，如果不存在返回空字符串
     */
    std::string getObfuscatedParameterName(const std::string& originalParamName) const;

    // 清空
    void clear();

    // 获取统计信息
    size_t getSymbolCount() const { return symbols_.size(); }
    size_t getPublicSymbolCount() const;

private:
    std::map<std::string, Symbol> symbols_;
    std::map<std::string, std::string> reverseMap_; // obfuscated -> original
    NameGenerator* nameGenerator_;

    // 属性方法映射：原始属性名 → 映射信息
    std::unordered_map<std::string, PropertyMethodMapping> propertyMappings_;

    // 方法符号映射：方法名 → 方法信息（用于 PropertyNameStrategy 查找方法 getter）
    std::unordered_map<std::string, MethodSymbolInfo> methodSymbols_;

    // 方法参数映射：方法选择器 → 参数映射（用于 PropertyNameStrategy 混淆 Block 内捕获的参数）
    std::unordered_map<std::string, MethodParameterMapping> methodParameters_;

    std::string generateKey(const std::string& name, SymbolType type) const;
};

} // namespace obfuscator

#endif // SYMBOL_TABLE_H

