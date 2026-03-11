//
//  UIButton+Extensions.h
//  SMSDK
//
//  Created by qianxi on 2020/12/24.
//  Copyright © 2020 kaiser. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface UIButton (Extensions)

- (void)setEnlargeEdge:(CGFloat) size;
- (void)setEnlargeEdgeWithTop:(CGFloat) top right:(CGFloat) right bottom:(CGFloat) bottom left:(CGFloat) left;

@end

NS_ASSUME_NONNULL_END
