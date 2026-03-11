//
//  HWSDKManager.h
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HWSDKManager : NSObject

+ (instancetype)sharedManager;

- (void)initializeHWSDK;

@property (nonatomic, strong) NSString *configuration;

@end

NS_ASSUME_NONNULL_END
