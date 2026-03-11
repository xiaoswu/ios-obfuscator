//
//  SystemProperties.h
//  ios-obfuscator
//
//  系统属性名白名单（PropertyNameStrategy 和 MethodNameStrategy 共用）
//  这些属性名不应被混淆
//

#ifndef IOS_OBFUSCATOR_SYSTEM_PROPERTIES_H
#define IOS_OBFUSCATOR_SYSTEM_PROPERTIES_H

#include <unordered_set>
#include <string>

namespace obfuscator {

// 全局系统属性名白名单
// 这些属性名是系统类的属性，不应被混淆
static const std::unordered_set<std::string> SYSTEM_PROPERTY_NAMES = {
    // UIView 常用属性
    "delegate", "dataSource",
    "view", "window", "controller",
    "parent", "superview",
    "tag", "layer",
    "frame", "bounds", "center",
    "alpha", "hidden", "opaque",
    "backgroundColor", "tintColor", "transform",
    "nextResponder", "subviews",
    // UIImage 常用属性
    "CGImage", "scale", "imageOrientation",
    "size", "width", "height", "image",
    "capInsets", "alignmentRectInsets",
    "flippedForRightToLeftLayoutDirection",
    "leftCapWidth", "topCapHeight",
    "resizingMode", "animations", "imageView",
    // CGRect/CGPoint/CGSize 结构体成员（系统结构体，不应混淆）
    "x", "y", "origin",
    // 常用系统属性
    "hash", "superclass", "description",
    "debugDescription", "class",
    "self", "isa",
    // 其他常用属性
    "state", "completionBlock", "completedBlock",
    "top", "bottom", "right", "left",
    "edges", "trailing", "leading", "count",
    "location", "name", "title", "code", "centerX",
    "animationType", "opacity", "color",
    "indicator", "square", "margin", "mode",
    "progress", "removeFromSuperViewOnHide",
    "customView", "showStarted", "xOffset",
    "annular", "yOffset", "minSize", "taskInProgress", "constraints",
    "cachePolicy", "HTTPShouldHandleCookies", "HTTPShouldUsePipelining",
    "userInfo", "error", "msg", "body", "response", "rootViewController",
    "timeoutInterval","streamError","request",
    // UIViewController 属性
    "navigationItem", "tabBarItem",
    // UIControl 属性
    "enabled", "selected", "highlighted",
    // UILabel 属性
    "text", "font", "textColor", "textAlignment",
    // UIButton 属性
    "titleLabel",
    // NSOperation 属性
    "cancelled", "isCancelled", "executing", "isExecuting", "finished", "isFinished",
    "asynchronous", "isAsynchronous", "ready", "isReady",
    // NSURL 属性
    "URL", "cachePolicy",
    "MIMEType", "expectedContentLength", "textEncodingName", "suggestedFilename",
    // NSRecursiveLock 属性
    "lock",
    // UITextField
    "enable","keyboardAppearance"
};

} // namespace obfuscator

#endif // IOS_OBFUSCATOR_SYSTEM_PROPERTIES_H
