//
//  HWSDK.h
//  HWSDK Framework
//
//  Created by liang on 2026/1/17.
//

#import <Foundation/Foundation.h>

//! Project version number for HWSDK.
FOUNDATION_EXPORT double HWSDKVersionNumber;

//! Project version string for HWSDK.
FOUNDATION_EXPORT const unsigned char HWSDKVersionString[];

// Main SDK Manager class
@interface HWSDKManager : NSObject

/// Get the shared instance of HWSDK
+ (instancetype)sharedManager;

/// Initialize HWSDK with configuration
- (void)initializeHWSDK;

/// Get SDK name
- (NSString *)sdkName;

/// Get SDK version
- (NSString *)sdkVersion;

@property (nonatomic, strong, readonly) NSString *configuration;

@end
