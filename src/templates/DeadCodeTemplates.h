/**
 * @file DeadCodeTemplates.h
 * @brief 垃圾代码模板定义
 *
 * 定义各种假业务逻辑模板，用于生成看起来真实的垃圾代码。
 */

#ifndef DEAD_CODE_TEMPLATES_H
#define DEAD_CODE_TEMPLATES_H

#include "core/DeadCodeGenerator.h"
#include <string>
#include <vector>
#include <set>

namespace obfuscator {

/**
 * @class DeadCodeTemplate
 * @brief 垃圾代码模板基类
 *
 * 所有垃圾代码模板的基类，定义模板接口。
 */
class DeadCodeTemplate {
public:
    virtual ~DeadCodeTemplate() = default;

    /**
     * @brief 生成垃圾代码
     * @param context 生成上下文
     * @param varNameGenerator 变量名生成器
     * @return 生成的代码字符串
     */
    virtual std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) = 0;

    /**
     * @brief 获取模板名称
     * @return 模板名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取模板所需的框架
     * @return 框架名称集合
     */
    virtual std::set<std::string> requiredFrameworks() const = 0;
};

// =============================================================================
// 模板类型 1: SessionTokenTemplate - 假会话令牌处理
// =============================================================================

class SessionTokenTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "SessionToken";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 2: APISignatureTemplate - 假API签名
// =============================================================================

class APISignatureTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "APISignature";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 3: DataValidationTemplate - 假数据验证
// =============================================================================

class DataValidationTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "DataValidation";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 4: RequestAssemblyTemplate - 假请求组装
// =============================================================================

class RequestAssemblyTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "RequestAssembly";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 5: ResponseParsingTemplate - 假响应解析
// =============================================================================

class ResponseParsingTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "ResponseParsing";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 6: EncryptionDataTemplate - 假加密数据处理
// =============================================================================

class EncryptionDataTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "EncryptionData";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 7: CacheKeyTemplate - 假缓存键生成
// =============================================================================

class CacheKeyTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "CacheKey";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 8: URLParamsTemplate - 假URL参数拼装
// =============================================================================

class URLParamsTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "URLParams";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"Foundation"};
    }
};

// =============================================================================
// 模板类型 9: UIKitViewTemplate - 假UIView操作
// =============================================================================

class UIKitViewTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "UIKitView";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"UIKit", "Foundation"};
    }
};

// =============================================================================
// 模板类型 10: DeviceInfoTemplate - 假设备信息获取
// =============================================================================

class DeviceInfoTemplate : public DeadCodeTemplate {
public:
    std::string generate(
        const GenerationContext& context,
        std::function<std::string()> varNameGenerator
    ) override;

    std::string getName() const override {
        return "DeviceInfo";
    }

    std::set<std::string> requiredFrameworks() const override {
        return {"UIKit", "Foundation"};
    }
};

// =============================================================================
// 模板工厂函数
// =============================================================================

/**
 * @brief 创建所有可用模板
 * @return 模板指针列表
 */
std::vector<std::unique_ptr<DeadCodeTemplate>> createAllTemplates();

} // namespace obfuscator

#endif // DEAD_CODE_TEMPLATES_H
