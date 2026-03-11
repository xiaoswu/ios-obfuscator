//
//  HWSDKDataModel.h
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HWSDKDataModel : NSObject

@property (nonatomic, strong) NSString *dataId;
@property (nonatomic, assign) NSInteger value;
@property (nonatomic, copy) NSString *name;

- (instancetype)initWithDictionary:(NSDictionary *)dict;
- (NSDictionary *)toDictionary;

@end

NS_ASSUME_NONNULL_END
