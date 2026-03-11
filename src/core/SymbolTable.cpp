#include "core/SymbolTable.h"
#include "core/NameGenerator.h"
#include <algorithm>

namespace obfuscator {

SymbolTable::SymbolTable(NameGenerator* nameGenerator) 
    : nameGenerator_(nameGenerator) {
}

SymbolTable::~SymbolTable() {
}

std::string SymbolTable::generateKey(const std::string& name, SymbolType type) const {
    return name + "::" + std::to_string(static_cast<int>(type));
}

bool SymbolTable::addSymbol(const std::string& originalName,
                            SymbolType type,
                            bool isPublic,
                            const std::string& filePath) {
    std::string key = generateKey(originalName, type);
    
    // 检查是否已存在
    if (symbols_.find(key) != symbols_.end()) {
        return false;
    }
    
    Symbol symbol;
    symbol.originalName = originalName;
    symbol.type = type;
    symbol.isPublic = isPublic;
    symbol.filePath = filePath;
    
    // 生成混淆名称
    std::string typeStr = "";
    switch (type) {
        case SymbolType::CLASS: typeStr = "class"; break;
        case SymbolType::METHOD: typeStr = "method"; break;
        case SymbolType::PROPERTY: typeStr = "property"; break;
        case SymbolType::VARIABLE: typeStr = "variable"; break;
        case SymbolType::PROTOCOL: typeStr = "protocol"; break;
        case SymbolType::CATEGORY: typeStr = "category"; break;
        case SymbolType::FILE: typeStr = "file"; break;
        case SymbolType::STRING: typeStr = "string"; break;
        case SymbolType::RESOURCE: typeStr = "resource"; break;
    }
    
    symbol.obfuscatedName = nameGenerator_->generate(originalName, typeStr);
    
    // 添加到映射表
    symbols_[key] = symbol;
    reverseMap_[symbol.obfuscatedName] = originalName;
    
    return true;
}

std::string SymbolTable::getObfuscatedName(const std::string& originalName) const {
    // 尝试所有类型
    for (int i = 0; i < static_cast<int>(SymbolType::RESOURCE) + 1; ++i) {
        SymbolType type = static_cast<SymbolType>(i);
        std::string key = generateKey(originalName, type);
        auto it = symbols_.find(key);
        if (it != symbols_.end()) {
            return it->second.obfuscatedName;
        }
    }
    return originalName; // 未找到，返回原始名称
}

std::string SymbolTable::getOriginalName(const std::string& obfuscatedName) const {
    auto it = reverseMap_.find(obfuscatedName);
    if (it != reverseMap_.end()) {
        return it->second;
    }
    return obfuscatedName;
}

bool SymbolTable::hasSymbol(const std::string& originalName) const {
    for (int i = 0; i < static_cast<int>(SymbolType::RESOURCE) + 1; ++i) {
        SymbolType type = static_cast<SymbolType>(i);
        std::string key = generateKey(originalName, type);
        if (symbols_.find(key) != symbols_.end()) {
            return true;
        }
    }
    return false;
}

const Symbol* SymbolTable::getSymbol(const std::string& originalName) const {
    for (int i = 0; i < static_cast<int>(SymbolType::RESOURCE) + 1; ++i) {
        SymbolType type = static_cast<SymbolType>(i);
        std::string key = generateKey(originalName, type);
        auto it = symbols_.find(key);
        if (it != symbols_.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

void SymbolTable::addDependency(const std::string& symbolName, const std::string& dependency) {
    for (int i = 0; i < static_cast<int>(SymbolType::RESOURCE) + 1; ++i) {
        SymbolType type = static_cast<SymbolType>(i);
        std::string key = generateKey(symbolName, type);
        auto it = symbols_.find(key);
        if (it != symbols_.end()) {
            it->second.dependencies.push_back(dependency);
            return;
        }
    }
}

void SymbolTable::clear() {
    symbols_.clear();
    reverseMap_.clear();
    propertyMappings_.clear();
    methodSymbols_.clear();
    methodParameters_.clear();
}

// ========== 属性方法映射相关方法实现 ==========

void SymbolTable::addPropertyMapping(const PropertyMethodMapping& mapping) {
    // 使用原始属性名作为 key
    propertyMappings_[mapping.originalPropertyName] = mapping;
}

std::string SymbolTable::getPropertyGetter(const std::string& methodName) const {
    // 遍历所有属性映射，查找是否有属性的 getter 是该方法名
    for (const auto& pair : propertyMappings_) {
        const PropertyMethodMapping& mapping = pair.second;
        if (mapping.originalGetterName == methodName) {
            return mapping.obfuscatedGetterName;
        }
    }
    return "";  // 不是属性 getter
}

std::string SymbolTable::getPropertySetter(const std::string& methodName) const {
    // 遍历所有属性映射，查找是否有属性的 setter 是该方法名
    for (const auto& pair : propertyMappings_) {
        const PropertyMethodMapping& mapping = pair.second;
        if (mapping.originalSetterName == methodName) {
            return mapping.obfuscatedSetterName;
        }
    }
    return "";  // 不是属性 setter
}

bool SymbolTable::isPropertyGetter(const std::string& methodName) const {
    for (const auto& pair : propertyMappings_) {
        if (pair.second.originalGetterName == methodName) {
            return true;
        }
    }
    return false;
}

bool SymbolTable::isPropertySetter(const std::string& methodName) const {
    for (const auto& pair : propertyMappings_) {
        if (pair.second.originalSetterName == methodName) {
            return true;
        }
    }
    return false;
}

// ========== 方法符号映射相关方法实现 ==========

void SymbolTable::addMethodSymbol(const std::string& methodName, const MethodSymbolInfo& info) {
    methodSymbols_[methodName] = info;
}

const SymbolTable::MethodSymbolInfo* SymbolTable::findMethodSymbol(const std::string& methodName) const {
    auto it = methodSymbols_.find(methodName);
    if (it != methodSymbols_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SymbolTable::hasMethodSymbol(const std::string& methodName) const {
    return methodSymbols_.find(methodName) != methodSymbols_.end();
}

// ========== 方法参数映射相关方法实现 ==========

void SymbolTable::addMethodParameterMapping(const std::string& selector,
                                            const std::map<std::string, std::string>& paramMapping) {
    MethodParameterMapping mapping;
    mapping.paramMapping = paramMapping;
    methodParameters_[selector] = mapping;
}

const SymbolTable::MethodParameterMapping* SymbolTable::findMethodParameterMapping(
        const std::string& selector) const {
    auto it = methodParameters_.find(selector);
    if (it != methodParameters_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string SymbolTable::getObfuscatedParameterName(const std::string& selector,
                                                    const std::string& originalParamName) const {
    const MethodParameterMapping* mapping = findMethodParameterMapping(selector);
    if (mapping) {
        auto it = mapping->paramMapping.find(originalParamName);
        if (it != mapping->paramMapping.end()) {
            return it->second;
        }
    }
    return "";
}

std::string SymbolTable::getObfuscatedParameterName(const std::string& originalParamName) const {
    // 遍历所有方法，查找包含该参数名的方法
    for (const auto& [selector, mapping] : methodParameters_) {
        auto it = mapping.paramMapping.find(originalParamName);
        if (it != mapping.paramMapping.end()) {
            return it->second;
        }
    }
    return "";
}

size_t SymbolTable::getPublicSymbolCount() const {
    size_t count = 0;
    for (const auto& pair : symbols_) {
        if (pair.second.isPublic) {
            count++;
        }
    }
    return count;
}

} // namespace obfuscator

