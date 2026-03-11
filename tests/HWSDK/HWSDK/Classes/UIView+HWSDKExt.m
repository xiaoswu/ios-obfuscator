//
//  UIView+HWSDKExt.m
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import "UIView+HWSDKExt.h"
#import <QuartzCore/QuartzCore.h>

@implementation UIView (HWSDKExt)

- (void)hwsdk_shakeAnimation {
    CABasicAnimation *animation = [CABasicAnimation animationWithKeyPath:@"position"];
    animation.duration = 0.05;
    animation.repeatCount = 5;
    animation.autoreverses = YES;
    animation.fromValue = [NSValue valueWithCGPoint:CGPointMake(self.center.x - 5, self.center.y)];
    animation.toValue = [NSValue valueWithCGPoint:CGPointMake(self.center.x + 5, self.center.y)];
    [self.layer addAnimation:animation forKey:@"shake"];
}

- (void)hwsdk_applyRoundedCorners:(CGFloat)radius shadowOpacity:(CGFloat)opacity {
    self.layer.cornerRadius = radius;
    self.layer.masksToBounds = YES;
    self.layer.shadowOpacity = opacity;
    self.layer.shadowRadius = 5.0;
    self.layer.shadowOffset = CGSizeMake(0, 2);
}

@end
