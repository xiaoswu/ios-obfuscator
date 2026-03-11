//
//  HWSDKDataModel+JSON.h
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import <Foundation/Foundation.h>
#import "HWSDKDataModel.h"

NS_ASSUME_NONNULL_BEGIN

@interface HWSDKDataModel (JSON)

// JSON 序列化
- (NSData *)hwsdk_toJSONData:(NSError **)error;
- (NSString *)hwsdk_toJSONString:(NSError **)error;
+ (instancetype)hwsdk_fromJSONData:(NSData *)data error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
