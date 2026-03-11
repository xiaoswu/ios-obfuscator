/**
 * @file DeadCodeTemplates.cpp
 * @brief 垃圾代码模板实现
 *
 * 实现各种假业务逻辑模板，生成看起来真实的垃圾代码。
 * 遵循三大原则：
 * 1. 编译器不会优化 - 使用运行时值
 * 2. Hopper 难判断 - 看起来像真实业务逻辑
 * 3. 无性能损耗 - 仅轻量级操作
 */

#include "templates/DeadCodeTemplates.h"
#include <sstream>
#include <random>
#include <algorithm>

namespace obfuscator {

// =============================================================================
// 辅助函数
// =============================================================================

namespace {
    // 随机数生成器（每个模板独立）
    thread_local std::mt19937 tls_rng(std::random_device{}());

    int randomInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(tls_rng);
    }

    std::string randomElement(const std::vector<std::string>& vec) {
        if (vec.empty()) return "";
        return vec[randomInt(0, vec.size() - 1)];
    }

    std::string escapeObjCString(const std::string& input) {
        std::string result;
        result.reserve(input.length() * 1.2);  // 预留一些空间
        for (char c : input) {
            switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                default:
                    result += c;
                    break;
            }
        }
        return result;
    }

    // 字符串常量池 - 按类别分组
    const std::vector<std::string> TOKEN_VALUES = {
        "user_session_key",
        "auth_token_abc123",
        "bearer_token_xyz",
        "access_token_2024",
        "session_id_token",
        "login_auth_key"
    };

    const std::vector<std::string> API_KEY_VALUES = {
        "app_key_2024",
        "api_secret_key",
        "client_api_token",
        "service_auth_key",
        "platform_access_key"
    };

    const std::vector<std::string> USER_ID_VALUES = {
        "user_123456",
        "user_789012",
        "account_345678",
        "member_901234",
        "profile_567890"
    };

    const std::vector<std::string> BASE_URL_VALUES = {
        "https://api.example.com",
        "https://service.app.io",
        "https://gateway.api.net"
    };

    const std::vector<std::string> JSON_VALUES = {
        "{\"code\":200,\"message\":\"success\"}",
        "{\"status\":\"ok\",\"data\":{}}",
        "{\"result\":true,\"info\":\"valid\"}"
    };

    const std::vector<std::string> PLAINTEXT_VALUES = {
        "sensitive_user_data",
        "private_info_data",
        "encrypted_content",
        "secret_payload_text"
    };

    const std::vector<std::string> RESOURCE_VALUES = {
        "profile_data",
        "user_settings",
        "cache_content",
        "session_info"
    };

    // 变量名前缀
    const std::vector<std::string> VAR_PREFIXES = {
        "token", "apiKey", "userId", "data", "result",
        "checksum", "signature", "timestamp", "nonce",
        "encrypted", "decoded", "validated", "extracted",
        "encoded", "hashed", "salted", "payload"
    };
} // anonymous namespace

// =============================================================================
// SessionTokenTemplate - 假会话令牌处理
// =============================================================================

std::string SessionTokenTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();

    std::string tokenValue = randomElement(TOKEN_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(tokenValue) << "\";\n";
    code << "NSData *" << var2 << " = [" << var1 << " dataUsingEncoding:NSUTF8StringEncoding];\n";
    code << "NSString *" << var3 << " = [" << var2 << " base64EncodedStringWithOptions:0];\n";

    // 添加条件判断（控制流污染）
    int choice = randomInt(0, 2);
    if (choice == 0) {
        code << "if (" << var3 << ".length % 2 == 0) {\n";
        code << "    " << var1 << " = [" << var3 << " substringToIndex:MIN(4, " << var3 << ".length)];\n";
        code << "    NSString *" << var4 << " = [NSString stringWithFormat:@\"len:%u\", (unsigned int)" << var3 << ".length];\n";
        code << "    (void)" << var4 << ";\n";
        code << "}\n";
    } else if (choice == 1) {
        code << "if (" << var3 << ".length > 0) {\n";
        code << "    NSString *" << var4 << " = [NSString stringWithFormat:@\"%u\", (unsigned int)[" << var3 << " hash]];\n";
        
        code << "    [NSString stringWithFormat:@\"hash:%@\", " << var4 << "];\n";
        code << "}\n";
    } else {
        code << "NSString *" << var4 << " = (" << var3 << ".length > 4) ? [" << var3 << " substringToIndex:4] : " << var3 << ";\n";
    
        code << "[NSString stringWithFormat:@\"token:%@\", " << var4 << "];\n";
    }

    return code.str();
}

// =============================================================================
// APISignatureTemplate - 假API签名
// =============================================================================

std::string APISignatureTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();
    std::string var5 = varNameGenerator();

    std::string apiKeyValue = randomElement(API_KEY_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(apiKeyValue) << "\";\n";
    code << "NSString *" << var2 << " = [NSString stringWithFormat:@\"%lld\", (long long)[NSDate date].timeIntervalSince1970];\n";
    code << "NSString *" << var3 << " = [" << var1 << " stringByAppendingString:" << var2 << "];\n";
    code << "NSData *" << var4 << " = [" << var3 << " dataUsingEncoding:NSUTF8StringEncoding];\n";
    code << "NSUInteger " << var5 << " = [" << var4 << " hash];\n";
    
    code << "[NSString stringWithFormat:@\"checksum_%lu\", " << var5 << "];\n";

    return code.str();
}

// =============================================================================
// DataValidationTemplate - 假数据验证
// =============================================================================

std::string DataValidationTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();

    std::string userIdValue = randomElement(USER_ID_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(userIdValue) << "\";\n";
    code << "BOOL " << var2 << " = ([" << var1 << " hasPrefix:@\"user_\"] && " << var1 << ".length > 5);\n";
    code << "BOOL " << var3 << " = ([" << var1 << " rangeOfString:@\"_\"].location != NSNotFound);\n";

    int choice = randomInt(0, 1);
    if (choice == 0) {
        code << "if (" << var2 << " && " << var3 << ") {\n";
        code << "    NSString *" << var4 << " = [" << var1 << " substringFromIndex:5];\n";
    
        code << "    [NSString stringWithFormat:@\"validated:%@\", " << var4 << "];\n";
        code << "}\n";
    } else {
        code << "NSString *" << var4 << " = (" << var2 << " && " << var3 << ") ? [" << var1 << " substringFromIndex:5] : " << var1 << ";\n";
        code << "[NSString stringWithFormat:@\"user:%@\", " << var4 << "];\n";
    }

    return code.str();
}

// =============================================================================
// RequestAssemblyTemplate - 假请求组装
// =============================================================================

std::string RequestAssemblyTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();

    code << "NSDictionary *" << var1 << " = @{\n";
    code << "    @\"X-Auth-Token\": @\"bearer_token_abc\",\n";
    code << "    @\"X-Request-ID\": @\"req_20240205_001\",\n";
    code << "    @\"Content-Type\": @\"application/json\"\n";
    code << "};\n";
    code << "NSString *" << var2 << " = @\"v2.1\";\n";
    code << "NSString *" << var3 << " = [NSString stringWithFormat:@\"/api/%@/status\", " << var2 << "];\n";
    code << "[NSString stringWithFormat:@\"%@_%@\", " << var1 << "[@\"X-Request-ID\"], " << var3 << "];\n";

    return code.str();
}

// =============================================================================
// ResponseParsingTemplate - 假响应解析
// =============================================================================

std::string ResponseParsingTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();

    std::string jsonValue = randomElement(JSON_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(jsonValue) << "\";\n";
    code << "NSData *" << var2 << " = [" << var1 << " dataUsingEncoding:NSUTF8StringEncoding];\n";
    code << "id " << var3 << " = [NSJSONSerialization JSONObjectWithData:" << var2 << " options:0 error:nil];\n";
    code << "if ([" << var3 << " isKindOfClass:[NSDictionary class]]) {\n";
    code << "    NSNumber *" << var4 << " = " << var3 << "[@\"code\"];\n";
    code << "    [" << var4 << " integerValue];\n";
    code << "}\n";

    return code.str();
}

// =============================================================================
// EncryptionDataTemplate - 假加密数据处理
// =============================================================================

std::string EncryptionDataTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();
    std::string var5 = varNameGenerator();

    std::string plaintextValue = randomElement(PLAINTEXT_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(plaintextValue) << "\";\n";
    code << "NSString *" << var2 << " = [NSString stringWithFormat:@\"%u\", arc4random()];\n";
    code << "NSString *" << var3 << " = [" << var1 << " stringByAppendingString:" << var2 << "];\n";
    code << "NSData *" << var4 << " = [" << var3 << " dataUsingEncoding:NSUTF8StringEncoding];\n";
    code << "NSString *" << var5 << " = [" << var4 << " base64EncodedStringWithOptions:0];\n";
    code << "[NSString stringWithFormat:@\"encoded_%@\", " << var5 << "];\n";

    return code.str();
}

// =============================================================================
// CacheKeyTemplate - 假缓存键生成
// =============================================================================

std::string CacheKeyTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();

    std::string userIdValue = randomElement(USER_ID_VALUES);
    std::string resourceValue = randomElement(RESOURCE_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(userIdValue) << "\";\n";
    code << "NSString *" << var2 << " = @\"" << escapeObjCString(resourceValue) << "\";\n";
    code << "NSString *" << var3 << " = [NSString stringWithFormat:@\"%@_%@_%@\", " << var1 << ", " << var2 << ", @(arc4random_uniform(1000))];\n";
    code << "NSUInteger " << var4 << " = [" << var3 << " hash];\n";
    code << "[NSString stringWithFormat:@\"cache_%lu\", (unsigned long)" << var4 << "];\n";

    return code.str();
}

// =============================================================================
// URLParamsTemplate - 假URL参数拼装
// =============================================================================

std::string URLParamsTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();

    std::string baseUrlValue = randomElement(BASE_URL_VALUES);

    code << "NSString *" << var1 << " = @\"" << escapeObjCString(baseUrlValue) << "\";\n";
    code << "NSDictionary *" << var2 << " = @{\n";
    code << "    @\"user_id\": @\"12345\",\n";
    code << "    @\"page\": @(1),\n";
    code << "    @\"limit\": @(20),\n";
    code << "    @\"timestamp\": @([[NSDate date] timeIntervalSince1970])\n";
    code << "};\n";
    code << "NSMutableArray *" << var3 << " = [NSMutableArray array];\n";
    code << "for (NSString *key in " << var2 << ") {\n";
    code << "    [" << var3 << " addObject:[NSString stringWithFormat:@\"%@=%@\", key, " << var2 << "[key]]];\n";
    code << "}\n";
    code << "[NSString stringWithFormat:@\"%@?%@\", " << var1 << ", [" << var3 << " componentsJoinedByString:@\"&\"]];\n";

    return code.str();
}

// =============================================================================
// UIKitViewTemplate - 假UIView操作
// =============================================================================

std::string UIKitViewTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();
    std::string var5 = varNameGenerator();

    code << "CGRect " << var1 << " = CGRectMake(0, 0, 100, 100);\n";
    code << "BOOL " << var2 << " = (CGRectGetWidth(" << var1 << ") > 50 && CGRectGetHeight(" << var1 << ") > 50);\n";

    int choice = randomInt(0, 1);
    if (choice == 0) {
        code << "if (" << var2 << ") {\n";
        code << "    CGPoint " << var4 << " = CGPointMake(" << var1 << ".origin.x + 10, " << var1 << ".origin.y + 10);\n";
        code << "    CGRect " << var5 << " = CGRectMake(" << var4 << ".x, " << var4 << ".y, 50, 50);\n";
        code << "    (void)" << var5 << ";\n";
        code << "}\n";
        code << "CGPoint " << var3 << " = " << var1 << ".origin;\n";
        code << "(void)" << var3 << ";\n";
    } else {
        code << "CGPoint " << var3 << " = " << var1 << ".origin;\n";
        code << "CGFloat " << var4 << " = (" << var2 << ") ? 10.0 : 0.0;\n";
        code << "CGRect " << var5 << " = CGRectMake(" << var3 << ".x + " << var4 << ", " << var3 << ".y + " << var4 << ", 50, 50);\n";
        code << "(void)" << var5 << ";\n";
    }

    return code.str();
}

// =============================================================================
// DeviceInfoTemplate - 假设备信息获取
// =============================================================================

std::string DeviceInfoTemplate::generate(
    const GenerationContext& context,
    std::function<std::string()> varNameGenerator
) {
    std::ostringstream code;

    std::string var1 = varNameGenerator();
    std::string var2 = varNameGenerator();
    std::string var3 = varNameGenerator();
    std::string var4 = varNameGenerator();

    code << "NSString *" << var1 << " = [[UIDevice currentDevice] systemName];\n";
    code << "NSString *" << var2 << " = [[UIDevice currentDevice] model];\n";
    code << "BOOL " << var3 << " = [" << var1 << " hasPrefix:@\"iOS\"];\n";

    int choice = randomInt(0, 1);
    if (choice == 0) {
        code << "if (" << var3 << ") {\n";
        code << "    NSString *" << var4 << " = [[UIDevice currentDevice] systemVersion];\n";
        code << "    [NSString stringWithFormat:@\"%@ %@ %@\", " << var1 << ", " << var2 << ", " << var4 << "];\n";
        code << "}\n";
    } else {
        code << "NSString *" << var4 << " = (" << var3 << ") ? " << var1 << " : " << var2 << ";\n";
        code << "[NSString stringWithFormat:@\"Device: %@\", " << var4 << "];\n";
    }

    return code.str();
}

// =============================================================================
// 模板工厂函数
// =============================================================================

std::vector<std::unique_ptr<DeadCodeTemplate>> createAllTemplates() {
    std::vector<std::unique_ptr<DeadCodeTemplate>> templates;

    templates.push_back(std::make_unique<SessionTokenTemplate>());
    templates.push_back(std::make_unique<APISignatureTemplate>());
    templates.push_back(std::make_unique<DataValidationTemplate>());
    templates.push_back(std::make_unique<RequestAssemblyTemplate>());
    templates.push_back(std::make_unique<ResponseParsingTemplate>());
    templates.push_back(std::make_unique<EncryptionDataTemplate>());
    templates.push_back(std::make_unique<CacheKeyTemplate>());
    templates.push_back(std::make_unique<URLParamsTemplate>());
    templates.push_back(std::make_unique<UIKitViewTemplate>());
    templates.push_back(std::make_unique<DeviceInfoTemplate>());

    return templates;
}

} // namespace obfuscator
