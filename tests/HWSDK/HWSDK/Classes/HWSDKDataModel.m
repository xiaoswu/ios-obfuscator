//
//  HWSDKDataModel.m
//  HWSDK
//
//  Created by liang on 2026/1/17.
//

#import "HWSDKDataModel.h"

@implementation HWSDKDataModel

- (instancetype)init {
    self = [super init];
    if (self) {
        _dataId = [[NSUUID UUID] UUIDString];
        _value = 0;
        _name = @"";
    }
    return self;
}

- (instancetype)initWithDictionary:(NSDictionary *)dict {
    self = [self init];
    if (self) {
        if (dict[@"dataId"]) {
            self.dataId = dict[@"dataId"];
        }
        if (dict[@"value"]) {
            self.value = [dict[@"value"] integerValue];
        }
        if (dict[@"name"]) {
            self.name = dict[@"name"];
        }
    }
    return self;
}

- (NSDictionary *)toDictionary {
    return @{
        @"dataId": self.dataId ?: @"",
        @"value": @(self.value),
        @"name": self.name ?: @""
    };
}

@end
