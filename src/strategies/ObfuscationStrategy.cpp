#include "strategies/ObfuscationStrategy.h"
#include "core/ConfigManager.h"
#include "core/ReplacementManager.h"
#include "core/Logger.h"
#include <algorithm>

namespace obfuscator {

// 【新架构】transform 的默认实现
// 调用 collectReplacements 收集所有替换，然后由 StrategyManager 统一应用
// 子类可以重写此方法以直接使用 rewriter（向后兼容旧的实现方式）
void ObfuscationStrategy::transform(clang::ASTContext& context, clang::Rewriter& rewriter) {
    // 默认实现：空，子类应该实现 collectReplacements
    // 这样 ReplacementManager 就可以统一管理所有替换
}

bool ObfuscationStrategy::isWhitelisted(const std::string& name, const std::string& type) const {
    if (!config_) return false;
    return config_->isWhitelisted(name, type);
}

bool ObfuscationStrategy::isSystemSymbol(const std::string& name) const {
    // 系统类前缀
    if (name.length() >= 2 && name[0] == 'N' && name[1] == 'S') {
        return true; // NSObject, NSString等
    }
    if (name.length() >= 2 && name[0] == 'U' && name[1] == 'I') {
        return true; // UIViewController等
    }
    if (name.length() >= 2 && name[0] == 'C' && name[1] == 'G') {
        return true; // CGPoint等
    }
    if (name.length() >= 2 && name[0] == 'C' && name[1] == 'A') {
        return true; // CALayer等
    }

    // 系统方法
    std::vector<std::string> systemMethods = {
        "init", "dealloc", "viewDidLoad", "viewWillAppear", "viewDidAppear",
        "viewWillDisappear", "viewDidDisappear", "didReceiveMemoryWarning"
    };
    for (const auto& method : systemMethods) {
        if (name == method || name.find(method) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool ObfuscationStrategy::isThirdPartySDK(const std::string& name) const {
    if (!config_) return false;
    return config_->isThirdPartySDK(name);
}

} // namespace obfuscator

