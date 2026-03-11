/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现
 * 
 * 负责从JSON文件加载配置，解析配置项，并提供配置访问接口。
 * 使用nlohmann/json库进行JSON解析和序列化。
 */

#include "core/ConfigManager.h"
#include <fstream>
#include <sstream>
#include <iostream>

// 包含json头文件
// CMake已经将json_SOURCE_DIR/include添加到包含路径
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace obfuscator {

ConfigManager::ConfigManager() {
    setDefaults();
}

ConfigManager::~ConfigManager() {
}

/**
 * @brief 设置默认配置值
 * 
 * 当配置文件不存在或解析失败时，使用这些默认值。
 */
void ConfigManager::setDefaults() {
    // SDK默认配置
    config_.sdk.name = "SDK";
    config_.sdk.type = "framework";
    config_.sdk.inputPath = "./";
    config_.sdk.outputPath = "./obfuscated";
    
    // 默认启用所有混淆策略
    config_.obfuscation.strategies = {
        "ClassNameObfuscation",
        "MethodNameObfuscation",
        "PropertyNameObfuscation",
        "VariableNameObfuscation",
        "ProtocolNameObfuscation",
        "CategoryNameObfuscation",
        "FileNameObfuscation",
        "StringObfuscation",
        "ResourceObfuscation",
        "MetadataObfuscation",
        "ControlFlowObfuscation"
    };
    
    // 默认命名规则：随机字符串
    config_.obfuscation.namingRule.style = NamingRule::RANDOM;
    config_.obfuscation.namingRule.prefix = "OBF_";
    config_.obfuscation.namingRule.length = 8;
    config_.obfuscation.namingRule.wordCount = 2;
    config_.obfuscation.namingRule.wordLength = 3;
    config_.obfuscation.namingRule.charset = "alphanumeric";

    // 单词库默认配置
    config_.obfuscation.namingRule.wordListPath = "./wordlist";
    config_.obfuscation.namingRule.wordCase = NamingRule::CAMEL_CASE;
    
    // 默认第三方SDK白名单
    config_.obfuscation.whitelist.thirdPartySDKs = {
        "HY6SDK",
        "FacebookSDK"
    };

    // 死代码注入默认配置
    config_.obfuscation.deadCodeInjection.density = 0.2;
    config_.obfuscation.deadCodeInjection.maxStatementsPerMethod = 3;
    config_.obfuscation.deadCodeInjection.templateTypes = {};

    // 默认生成映射表
    config_.obfuscation.generateMapping = true;
    config_.obfuscation.mappingOutputPath = "./mapping.json";
}

/**
 * @brief 从文件加载配置
 * @param configPath 配置文件路径
 * @return 成功返回true，失败返回false
 */
bool ConfigManager::loadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot open config file: " << configPath << std::endl;
        std::cerr << "Using default configuration." << std::endl;
        // 使用默认配置，不返回false
        return true;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    bool result = loadFromString(buffer.str());
    if (!result) {
        std::cerr << "Warning: Failed to parse config file, using default configuration." << std::endl;
        // 即使解析失败，也使用默认配置继续运行
        return true;
    }
    
    return true;
}

/**
 * @brief 从JSON字符串加载配置
 * @param jsonString JSON格式的配置字符串
 * @return 成功返回true，失败返回false
 */
bool ConfigManager::loadFromString(const std::string& jsonString) {
    try {
        json j = json::parse(jsonString);
        return parseJSON(j);
    } catch (const json::parse_error& e) {
        std::cerr << "Error: JSON parse error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 解析JSON对象并填充配置结构
 * @param j JSON对象
 * @return 成功返回true，失败返回false
 */
bool ConfigManager::parseJSON(const json& j) {
    try {
        // 解析SDK配置
        if (j.contains("sdk") && j["sdk"].is_object()) {
            const auto& sdk = j["sdk"];
            
            if (sdk.contains("name") && sdk["name"].is_string()) {
                config_.sdk.name = sdk["name"];
            }
            
            if (sdk.contains("type") && sdk["type"].is_string()) {
                config_.sdk.type = sdk["type"];
            }
            
            if (sdk.contains("inputPath") && sdk["inputPath"].is_string()) {
                config_.sdk.inputPath = sdk["inputPath"];
            }
            
            if (sdk.contains("outputPath") && sdk["outputPath"].is_string()) {
                config_.sdk.outputPath = sdk["outputPath"];
            }
        }
        
        // 解析混淆配置
        if (j.contains("obfuscation") && j["obfuscation"].is_object()) {
            const auto& obf = j["obfuscation"];
            
            // 解析策略列表
            if (obf.contains("strategies") && obf["strategies"].is_array()) {
                config_.obfuscation.strategies.clear();
                for (const auto& strategy : obf["strategies"]) {
                    if (strategy.is_string()) {
                        config_.obfuscation.strategies.push_back(strategy);
                    }
                }
            }
            
            // 解析命名规则
            if (obf.contains("namingRule") && obf["namingRule"].is_object()) {
                const auto& naming = obf["namingRule"];

                if (naming.contains("style") && naming["style"].is_string()) {
                    std::string style = naming["style"];
                    if (style == "random") {
                        config_.obfuscation.namingRule.style = NamingRule::RANDOM;
                    } else if (style == "prefix_word") {
                        config_.obfuscation.namingRule.style = NamingRule::PREFIX_WORD;
                    } else if (style == "words") {
                        config_.obfuscation.namingRule.style = NamingRule::WORDS;
                    }
                }

                if (naming.contains("prefix") && naming["prefix"].is_string()) {
                    config_.obfuscation.namingRule.prefix = naming["prefix"];
                }

                if (naming.contains("length") && naming["length"].is_number_integer()) {
                    config_.obfuscation.namingRule.length = naming["length"];
                }

                if (naming.contains("wordCount") && naming["wordCount"].is_number_integer()) {
                    config_.obfuscation.namingRule.wordCount = naming["wordCount"];
                }

                if (naming.contains("wordLength") && naming["wordLength"].is_number_integer()) {
                    config_.obfuscation.namingRule.wordLength = naming["wordLength"];
                }

                if (naming.contains("charset") && naming["charset"].is_string()) {
                    config_.obfuscation.namingRule.charset = naming["charset"];
                }

                // 解析单词库配置
                if (naming.contains("wordListPath") && naming["wordListPath"].is_string()) {
                    config_.obfuscation.namingRule.wordListPath = naming["wordListPath"];
                }

                if (naming.contains("wordCase") && naming["wordCase"].is_string()) {
                    std::string wordCase = naming["wordCase"];
                    if (wordCase == "camelCase") {
                        config_.obfuscation.namingRule.wordCase = NamingRule::CAMEL_CASE;
                    } else if (wordCase == "PascalCase") {
                        config_.obfuscation.namingRule.wordCase = NamingRule::PASCAL_CASE;
                    } else if (wordCase == "snake_case") {
                        config_.obfuscation.namingRule.wordCase = NamingRule::SNAKE_CASE;
                    } else if (wordCase == "kebab-case") {
                        config_.obfuscation.namingRule.wordCase = NamingRule::KEBAB_CASE;
                    } else if (wordCase == "UPPER_CASE") {
                        config_.obfuscation.namingRule.wordCase = NamingRule::UPPER_CASE;
                    }
                }

                // 解析各元素类型的单词数量配置
                if (naming.contains("wordCount") && naming["wordCount"].is_object()) {
                    const auto& wc = naming["wordCount"];

                    // className
                    if (wc.contains("className") && wc["className"].is_object()) {
                        const auto& cc = wc["className"];
                        if (cc.contains("min") && cc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.classNameWordCount.min = cc["min"];
                        }
                        if (cc.contains("max") && cc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.classNameWordCount.max = cc["max"];
                        }
                    }

                    // methodName
                    if (wc.contains("methodName") && wc["methodName"].is_object()) {
                        const auto& mc = wc["methodName"];
                        if (mc.contains("min") && mc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.methodNameWordCount.min = mc["min"];
                        }
                        if (mc.contains("max") && mc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.methodNameWordCount.max = mc["max"];
                        }
                    }

                    // propertyName
                    if (wc.contains("propertyName") && wc["propertyName"].is_object()) {
                        const auto& pc = wc["propertyName"];
                        if (pc.contains("min") && pc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.propertyNameWordCount.min = pc["min"];
                        }
                        if (pc.contains("max") && pc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.propertyNameWordCount.max = pc["max"];
                        }
                    }

                    // fileName
                    if (wc.contains("fileName") && wc["fileName"].is_object()) {
                        const auto& fc = wc["fileName"];
                        if (fc.contains("min") && fc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.fileNameWordCount.min = fc["min"];
                        }
                        if (fc.contains("max") && fc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.fileNameWordCount.max = fc["max"];
                        }
                    }

                    // folderName
                    if (wc.contains("folderName") && wc["folderName"].is_object()) {
                        const auto& fc = wc["folderName"];
                        if (fc.contains("min") && fc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.folderNameWordCount.min = fc["min"];
                        }
                        if (fc.contains("max") && fc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.folderNameWordCount.max = fc["max"];
                        }
                    }

                    // parameterName
                    if (wc.contains("parameterName") && wc["parameterName"].is_object()) {
                        const auto& pc = wc["parameterName"];
                        if (pc.contains("min") && pc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.parameterNameWordCount.min = pc["min"];
                        }
                        if (pc.contains("max") && pc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.parameterNameWordCount.max = pc["max"];
                        }
                    }
                }

                // 解析各元素类型的随机字母长度配置
                if (naming.contains("randomLength") && naming["randomLength"].is_object()) {
                    const auto& rl = naming["randomLength"];

                    // className
                    if (rl.contains("className") && rl["className"].is_object()) {
                        const auto& cc = rl["className"];
                        if (cc.contains("min") && cc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.classNameRandomLength.min = cc["min"];
                        }
                        if (cc.contains("max") && cc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.classNameRandomLength.max = cc["max"];
                        }
                    }

                    // methodName
                    if (rl.contains("methodName") && rl["methodName"].is_object()) {
                        const auto& mc = rl["methodName"];
                        if (mc.contains("min") && mc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.methodNameRandomLength.min = mc["min"];
                        }
                        if (mc.contains("max") && mc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.methodNameRandomLength.max = mc["max"];
                        }
                    }

                    // propertyName
                    if (rl.contains("propertyName") && rl["propertyName"].is_object()) {
                        const auto& pc = rl["propertyName"];
                        if (pc.contains("min") && pc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.propertyNameRandomLength.min = pc["min"];
                        }
                        if (pc.contains("max") && pc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.propertyNameRandomLength.max = pc["max"];
                        }
                    }

                    // fileName
                    if (rl.contains("fileName") && rl["fileName"].is_object()) {
                        const auto& fc = rl["fileName"];
                        if (fc.contains("min") && fc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.fileNameRandomLength.min = fc["min"];
                        }
                        if (fc.contains("max") && fc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.fileNameRandomLength.max = fc["max"];
                        }
                    }

                    // folderName
                    if (rl.contains("folderName") && rl["folderName"].is_object()) {
                        const auto& fc = rl["folderName"];
                        if (fc.contains("min") && fc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.folderNameRandomLength.min = fc["min"];
                        }
                        if (fc.contains("max") && fc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.folderNameRandomLength.max = fc["max"];
                        }
                    }

                    // parameterName
                    if (rl.contains("parameterName") && rl["parameterName"].is_object()) {
                        const auto& pc = rl["parameterName"];
                        if (pc.contains("min") && pc["min"].is_number_integer()) {
                            config_.obfuscation.namingRule.parameterNameRandomLength.min = pc["min"];
                        }
                        if (pc.contains("max") && pc["max"].is_number_integer()) {
                            config_.obfuscation.namingRule.parameterNameRandomLength.max = pc["max"];
                        }
                    }
                }
            }
            
            // 解析白名单
            if (obf.contains("whitelist") && obf["whitelist"].is_object()) {
                const auto& whitelist = obf["whitelist"];
                
                if (whitelist.contains("classes") && whitelist["classes"].is_array()) {
                    config_.obfuscation.whitelist.classes.clear();
                    for (const auto& cls : whitelist["classes"]) {
                        if (cls.is_string()) {
                            config_.obfuscation.whitelist.classes.push_back(cls);
                        }
                    }
                }
                
                if (whitelist.contains("methods") && whitelist["methods"].is_array()) {
                    config_.obfuscation.whitelist.methods.clear();
                    for (const auto& method : whitelist["methods"]) {
                        if (method.is_string()) {
                            config_.obfuscation.whitelist.methods.push_back(method);
                        }
                    }
                }
                
                if (whitelist.contains("properties") && whitelist["properties"].is_array()) {
                    config_.obfuscation.whitelist.properties.clear();
                    for (const auto& prop : whitelist["properties"]) {
                        if (prop.is_string()) {
                            config_.obfuscation.whitelist.properties.push_back(prop);
                        }
                    }
                }

                if (whitelist.contains("thirdPartySDKs") && whitelist["thirdPartySDKs"].is_array()) {
                    config_.obfuscation.whitelist.thirdPartySDKs.clear();
                    for (const auto& sdk : whitelist["thirdPartySDKs"]) {
                        if (sdk.is_string()) {
                            config_.obfuscation.whitelist.thirdPartySDKs.push_back(sdk);
                        }
                    }
                }

                if (whitelist.contains("publicHeaders") && whitelist["publicHeaders"].is_array()) {
                    config_.obfuscation.whitelist.publicHeaders.clear();
                    for (const auto& header : whitelist["publicHeaders"]) {
                        if (header.is_string()) {
                            config_.obfuscation.whitelist.publicHeaders.push_back(header);
                        }
                    }
                }
            }
            
            // 解析映射表配置
            if (obf.contains("generateMapping") && obf["generateMapping"].is_boolean()) {
                config_.obfuscation.generateMapping = obf["generateMapping"];
            }
            
            if (obf.contains("mappingOutputPath") && obf["mappingOutputPath"].is_string()) {
                config_.obfuscation.mappingOutputPath = obf["mappingOutputPath"];
            }

            // 解析死代码注入配置
            if (obf.contains("deadCodeInjection") && obf["deadCodeInjection"].is_object()) {
                const auto& dc = obf["deadCodeInjection"];

                if (dc.contains("density") && dc["density"].is_number()) {
                    config_.obfuscation.deadCodeInjection.density = dc["density"];
                }

                if (dc.contains("maxStatementsPerMethod") && dc["maxStatementsPerMethod"].is_number_integer()) {
                    config_.obfuscation.deadCodeInjection.maxStatementsPerMethod = dc["maxStatementsPerMethod"];
                }

                if (dc.contains("templateTypes") && dc["templateTypes"].is_array()) {
                    config_.obfuscation.deadCodeInjection.templateTypes.clear();
                    for (const auto& type : dc["templateTypes"]) {
                        if (type.is_string()) {
                            config_.obfuscation.deadCodeInjection.templateTypes.push_back(type);
                        }
                    }
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 保存配置到文件
 * @param configPath 配置文件路径
 * @return 成功返回true，失败返回false
 */
bool ConfigManager::saveToFile(const std::string& configPath) const {
    try {
        json j;
        
        // 序列化SDK配置
        j["sdk"]["name"] = config_.sdk.name;
        j["sdk"]["type"] = config_.sdk.type;
        j["sdk"]["inputPath"] = config_.sdk.inputPath;
        j["sdk"]["outputPath"] = config_.sdk.outputPath;
        
        // 序列化混淆配置
        j["obfuscation"]["strategies"] = config_.obfuscation.strategies;
        
        // 序列化命名规则
        std::string styleStr;
        switch (config_.obfuscation.namingRule.style) {
            case NamingRule::RANDOM:
                styleStr = "random";
                break;
            case NamingRule::PREFIX_WORD:
                styleStr = "prefix_word";
                break;
            case NamingRule::WORDS:
                styleStr = "words";
                break;
        }
        j["obfuscation"]["namingRule"]["style"] = styleStr;
        j["obfuscation"]["namingRule"]["prefix"] = config_.obfuscation.namingRule.prefix;
        j["obfuscation"]["namingRule"]["length"] = config_.obfuscation.namingRule.length;
        j["obfuscation"]["namingRule"]["wordCount"] = config_.obfuscation.namingRule.wordCount;
        j["obfuscation"]["namingRule"]["wordLength"] = config_.obfuscation.namingRule.wordLength;
        j["obfuscation"]["namingRule"]["charset"] = config_.obfuscation.namingRule.charset;
        j["obfuscation"]["namingRule"]["wordListPath"] = config_.obfuscation.namingRule.wordListPath;

        std::string wordCaseStr;
        switch (config_.obfuscation.namingRule.wordCase) {
            case NamingRule::CAMEL_CASE:
                wordCaseStr = "camelCase";
                break;
            case NamingRule::PASCAL_CASE:
                wordCaseStr = "PascalCase";
                break;
            case NamingRule::SNAKE_CASE:
                wordCaseStr = "snake_case";
                break;
            case NamingRule::KEBAB_CASE:
                wordCaseStr = "kebab-case";
                break;
            case NamingRule::UPPER_CASE:
                wordCaseStr = "UPPER_CASE";
                break;
        }
        j["obfuscation"]["namingRule"]["wordCase"] = wordCaseStr;

        // 序列化各元素类型的单词数量配置
        j["obfuscation"]["namingRule"]["wordCount"]["className"]["min"] = config_.obfuscation.namingRule.classNameWordCount.min;
        j["obfuscation"]["namingRule"]["wordCount"]["className"]["max"] = config_.obfuscation.namingRule.classNameWordCount.max;
        j["obfuscation"]["namingRule"]["wordCount"]["methodName"]["min"] = config_.obfuscation.namingRule.methodNameWordCount.min;
        j["obfuscation"]["namingRule"]["wordCount"]["methodName"]["max"] = config_.obfuscation.namingRule.methodNameWordCount.max;
        j["obfuscation"]["namingRule"]["wordCount"]["propertyName"]["min"] = config_.obfuscation.namingRule.propertyNameWordCount.min;
        j["obfuscation"]["namingRule"]["wordCount"]["propertyName"]["max"] = config_.obfuscation.namingRule.propertyNameWordCount.max;
        j["obfuscation"]["namingRule"]["wordCount"]["fileName"]["min"] = config_.obfuscation.namingRule.fileNameWordCount.min;
        j["obfuscation"]["namingRule"]["wordCount"]["fileName"]["max"] = config_.obfuscation.namingRule.fileNameWordCount.max;
        j["obfuscation"]["namingRule"]["wordCount"]["folderName"]["min"] = config_.obfuscation.namingRule.folderNameWordCount.min;
        j["obfuscation"]["namingRule"]["wordCount"]["folderName"]["max"] = config_.obfuscation.namingRule.folderNameWordCount.max;
        j["obfuscation"]["namingRule"]["wordCount"]["parameterName"]["min"] = config_.obfuscation.namingRule.parameterNameWordCount.min;
        j["obfuscation"]["namingRule"]["wordCount"]["parameterName"]["max"] = config_.obfuscation.namingRule.parameterNameWordCount.max;

        // 序列化各元素类型的随机字母长度配置
        j["obfuscation"]["namingRule"]["randomLength"]["className"]["min"] = config_.obfuscation.namingRule.classNameRandomLength.min;
        j["obfuscation"]["namingRule"]["randomLength"]["className"]["max"] = config_.obfuscation.namingRule.classNameRandomLength.max;
        j["obfuscation"]["namingRule"]["randomLength"]["methodName"]["min"] = config_.obfuscation.namingRule.methodNameRandomLength.min;
        j["obfuscation"]["namingRule"]["randomLength"]["methodName"]["max"] = config_.obfuscation.namingRule.methodNameRandomLength.max;
        j["obfuscation"]["namingRule"]["randomLength"]["propertyName"]["min"] = config_.obfuscation.namingRule.propertyNameRandomLength.min;
        j["obfuscation"]["namingRule"]["randomLength"]["propertyName"]["max"] = config_.obfuscation.namingRule.propertyNameRandomLength.max;
        j["obfuscation"]["namingRule"]["randomLength"]["fileName"]["min"] = config_.obfuscation.namingRule.fileNameRandomLength.min;
        j["obfuscation"]["namingRule"]["randomLength"]["fileName"]["max"] = config_.obfuscation.namingRule.fileNameRandomLength.max;
        j["obfuscation"]["namingRule"]["randomLength"]["folderName"]["min"] = config_.obfuscation.namingRule.folderNameRandomLength.min;
        j["obfuscation"]["namingRule"]["randomLength"]["folderName"]["max"] = config_.obfuscation.namingRule.folderNameRandomLength.max;
        j["obfuscation"]["namingRule"]["randomLength"]["parameterName"]["min"] = config_.obfuscation.namingRule.parameterNameRandomLength.min;
        j["obfuscation"]["namingRule"]["randomLength"]["parameterName"]["max"] = config_.obfuscation.namingRule.parameterNameRandomLength.max;

        // 序列化白名单
        j["obfuscation"]["whitelist"]["classes"] = config_.obfuscation.whitelist.classes;
        j["obfuscation"]["whitelist"]["methods"] = config_.obfuscation.whitelist.methods;
        j["obfuscation"]["whitelist"]["properties"] = config_.obfuscation.whitelist.properties;
        j["obfuscation"]["whitelist"]["thirdPartySDKs"] = config_.obfuscation.whitelist.thirdPartySDKs;
        j["obfuscation"]["whitelist"]["publicHeaders"] = config_.obfuscation.whitelist.publicHeaders;
        
        // 序列化映射表配置
        j["obfuscation"]["generateMapping"] = config_.obfuscation.generateMapping;
        j["obfuscation"]["mappingOutputPath"] = config_.obfuscation.mappingOutputPath;
        
        // 写入文件（格式化输出，缩进2个空格）
        std::ofstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot create config file: " << configPath << std::endl;
            return false;
        }
        
        file << j.dump(2) << std::endl;
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 检查策略是否启用
 * @param strategyName 策略名称
 * @return 启用返回true，否则返回false
 */
bool ConfigManager::isStrategyEnabled(const std::string& strategyName) const {
    for (const auto& strategy : config_.obfuscation.strategies) {
        if (strategy == strategyName) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 检查符号是否在白名单中
 * @param name 符号名称
 * @param type 符号类型（"class", "method", "property"）
 * @return 在白名单中返回true，否则返回false
 */
bool ConfigManager::isWhitelisted(const std::string& name, const std::string& type) const {
    if (type == "class") {
        for (const auto& cls : config_.obfuscation.whitelist.classes) {
            if (cls == name) {
                return true;
            }
        }
    } else if (type == "method") {
        for (const auto& method : config_.obfuscation.whitelist.methods) {
            if (method == name) {
                return true;
            }
        }
    } else if (type == "property") {
        for (const auto& prop : config_.obfuscation.whitelist.properties) {
            if (prop == name) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 检查是否是第三方SDK
 * @param name 符号名称
 * @return 是第三方SDK返回true，否则返回false
 */
bool ConfigManager::isThirdPartySDK(const std::string& name) const {
    for (const auto& sdk : config_.obfuscation.whitelist.thirdPartySDKs) {
        // 检查名称中是否包含第三方SDK标识
        if (name.find(sdk) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 检查是否是保留注释的头文件
 * @param filePath 文件路径
 * @return 是保留注释的头文件返回true，否则返回false
 */
bool ConfigManager::isPublicHeader(const std::string& filePath) const {
    // 提取文件名
    std::string fileName = filePath;
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        fileName = filePath.substr(lastSlash + 1);
    }

    // 检查文件名是否在白名单中
    for (const auto& header : config_.obfuscation.whitelist.publicHeaders) {
        if (fileName == header || filePath.find(header) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace obfuscator
