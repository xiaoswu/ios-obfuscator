//
//  HWSDKDataModel+JSON.m
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import "HWSDKDataModel+JSON.h"
#import "HWSDKDataModel.h"

@implementation HWSDKDataModel (JSON)

- (NSData *)hwsdk_toJSONData:(NSError **)error {
    // 模拟 JSON 序列化
    NSString *jsonString = @"{\"data\":\"sample\"}";
    return [jsonString dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSString *)hwsdk_toJSONString:(NSError **)error {
    NSData *data = [self hwsdk_toJSONData:error];
    if (data) {
        return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    }
    return nil;
}

+ (instancetype)hwsdk_fromJSONData:(NSData *)data error:(NSError **)error {
    // 模拟 JSON 反序列化
    return [[HWSDKDataModel alloc] init];
}

@end
