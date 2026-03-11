//
//  UIView+HWSDKExt.h
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface UIView (HWSDKExt)

// 添加 shake 动画
- (void)hwsdk_shakeAnimation;

// 添加圆角和阴影
- (void)hwsdk_applyRoundedCorners:(CGFloat)radius shadowOpacity:(CGFloat)opacity;

@end

NS_ASSUME_NONNULL_END
