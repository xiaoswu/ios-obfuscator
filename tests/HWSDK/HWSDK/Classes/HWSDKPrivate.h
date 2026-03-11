//
//  HWSDKPrivate.h
//  HWSDK - Private Headers
//
//  Created by liang on 2026/1/17.
//

#import <Foundation/Foundation.h>

// HWSDK private interfaces and constants

#ifndef HWSDKPrivate_h
#define HWSDKPrivate_h

// HWSDK internal constants
extern NSString * const HWSDKVersion;
extern NSString * const HWSDKName;
extern NSString * const HWSDKBuildNumber;

// HWSDK error domain
extern NSString * const HWSDKErrorDomain;

typedef NS_ENUM(NSInteger, HWSDKErrorCode) {
    HWSDKErrorCodeNoError = 0,
    HWSDKErrorCodeNetworkError,
    HWSDKErrorCodeDataError,
    HWSDKErrorCodeConfigurationError
};

// HWSDK logging
#define HWSDK_LOG(fmt, ...) NSLog(@"[HWSDK] " fmt, ##__VA_ARGS__)
#define HWSDK_ERROR(fmt, ...) NSLog(@"[HWSDK ERROR] " fmt, ##__VA_ARGS__)

#endif /* HWSDKPrivate_h */
