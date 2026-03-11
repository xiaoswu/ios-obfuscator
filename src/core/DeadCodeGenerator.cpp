/**
 * @file DeadCodeGenerator.cpp
 * @brief 垃圾代码生成器实现
 *
 * 根据配置和上下文生成假业务逻辑代码。
 */

#include "core/DeadCodeGenerator.h"
#include "core/ConfigManager.h"
#include "templates/DeadCodeTemplates.h"
#include <random>
#include <sstream>
#include <algorithm>
#include <functional>

namespace obfuscator {

// =============================================================================
// 辅助函数
// =============================================================================

namespace {
    // 字符串哈希函数
    size_t hashString(const std::string& str) {
        std::hash<std::string> hasher;
        return hasher(str);
    }
} // anonymous namespace

// =============================================================================
// DeadCodeGenerator 实现
// =============================================================================

DeadCodeGenerator::DeadCodeGenerator(const DeadCodeConfig& config)
    : config_(config)
    , rng_(std::random_device{}())
{
    initializeFrameworkMappings();
    initializeTemplates();
}

DeadCodeGenerator::~DeadCodeGenerator() = default;

void DeadCodeGenerator::initializeFrameworkMappings() {
    // Foundation 框架类
    std::set<std::string> foundationClasses = {
        "NSString", "NSArray", "NSDictionary", "NSData", "NSDate",
        "NSURL", "NSUUID", "NSUserDefaults", "NSSet", "NSValue",
        "NSAttributedString", "NSCharacterSet", "NSRegularExpression",
        "NSPredicate", "NSMutableString", "NSMutableData", "NSMutableArray",
        "NSMutableDictionary", "NSMutableSet", "NSJSONSerialization",
        "NSProcessInfo", "NSBundle", "NSLocale", "NSTimeZone", "NSThread"
    };
    frameworkClasses_["Foundation"] = foundationClasses;

    // UIKit 框架类
    std::set<std::string> uiKitClasses = {
        "UIView", "UIViewController", "UIScreen", "UIColor", "UIImage",
        "UIWindow", "UIButton", "UILabel", "UITableView", "UICollectionView",
        "UIBezierPath", "UIInterfaceOrientation", "UIDevice", "UIApplication"
    };
    frameworkClasses_["UIKit"] = uiKitClasses;

    // AVFoundation 框架类
    std::set<std::string> avFoundationClasses = {
        "AVAsset", "AVPlayer", "AVAudioSession", "AVCaptureSession",
        "AVPlayerItem", "AVPlayerLayer", "AVAudioPlayer", "AVAssetExportSession"
    };
    frameworkClasses_["AVFoundation"] = avFoundationClasses;

    // CoreGraphics 框架类
    std::set<std::string> coreGraphicsClasses = {
        "CGPoint", "CGSize", "CGRect", "CGAffineTransform",
        "CGFloat", "CGPath", "CGColor", "CGContext"
    };
    frameworkClasses_["CoreGraphics"] = coreGraphicsClasses;

    // CoreLocation 框架类
    std::set<std::string> coreLocationClasses = {
        "CLLocation", "CLLocationManager", "CLLocationCoordinate2D"
    };
    frameworkClasses_["CoreLocation"] = coreLocationClasses;

    // Security 框架类
    std::set<std::string> securityClasses = {
        "SecKey", "SecCertificate", "SecTrust", "SecIdentity"
    };
    frameworkClasses_["Security"] = securityClasses;
}

void DeadCodeGenerator::initializeTemplates() {
    templates_ = createAllTemplates();

    // 如果配置指定了模板类型，过滤启用
    if (!config_.templateTypes.empty()) {
        std::vector<std::unique_ptr<DeadCodeTemplate>> filtered;
        for (auto& template_ptr : templates_) {
            const std::string& name = template_ptr->getName();
            for (const auto& enabledType : config_.templateTypes) {
                if (name.find(enabledType) != std::string::npos) {
                    filtered.push_back(std::move(template_ptr));
                    break;
                }
            }
        }
        templates_ = std::move(filtered);
    }
}

void DeadCodeGenerator::setEnabledTypes(const std::vector<std::string>& types) {
    enabledTypes_ = types;
}

void DeadCodeGenerator::setAvailableFrameworks(const std::vector<FrameworkInfo>& frameworks) {
    // 更新可用框架信息
    for (const auto& fw : frameworks) {
        frameworkClasses_[fw.name] = fw.availableClasses;
    }
}

bool DeadCodeGenerator::isClassAvailable(const std::string& className) const {
    for (const auto& pair : frameworkClasses_) {
        if (pair.second.find(className) != pair.second.end()) {
            return true;
        }
    }
    return false;
}

std::string DeadCodeGenerator::generateVarName(const std::string& prefix) {
    std::ostringstream name;
    if (prefix.empty()) {
        name << "___obf_" << varCounter_++;
    } else {
        name << "___obf_" << prefix << "_" << varCounter_++;
    }
    return name.str();
}

std::string DeadCodeGenerator::generateStringConstant(const std::string& category) {
    // 根据类别返回不同的字符串常量
    if (category == "token") {
        return "user_session_key";
    } else if (category == "userId") {
        return "user_123456";
    } else if (category == "apiKey") {
        return "app_key_2024";
    } else if (category == "data") {
        return "sensitive_data";
    } else if (category == "url") {
        return "https://api.example.com";
    }
    return "value";
}

int DeadCodeGenerator::generateRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

size_t DeadCodeGenerator::stringHash(const std::string& str) const {
    return hashString(str);
}

DeadCodeTemplate* DeadCodeGenerator::selectTemplate(const GenerationContext& context) {
    if (templates_.empty()) {
        return nullptr;
    }

    // iOS 项目中 Foundation 和 UIKit 总是可用的（通过 .pch 预编译头）
    // 不再进行框架检查，所有模板都可用
    std::vector<DeadCodeTemplate*> availableTemplates;
    for (auto& template_ptr : templates_) {
        availableTemplates.push_back(template_ptr.get());
    }

    if (availableTemplates.empty()) {
        return nullptr;
    }

    // 随机选择一个模板
    size_t index = generateRandomInt(0, availableTemplates.size() - 1);
    return availableTemplates[index];
}

std::string DeadCodeGenerator::generate(const GenerationContext& context) {
    DeadCodeTemplate* template_ptr = selectTemplate(context);
    if (!template_ptr) {
        return "";
    }

    // 生成变量名生成器
    auto varNameGen = [this]() -> std::string {
        return generateVarName();
    };

    return template_ptr->generate(context, varNameGen);
}

} // namespace obfuscator
