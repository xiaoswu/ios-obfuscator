//
//  HWSDKManager+Database.h
//  HWSDK
//
//  Created by test on 2026/1/17.
//

#import <Foundation/Foundation.h>
#import "HWSDKManager.h"

NS_ASSUME_NONNULL_BEGIN

@interface HWSDKManager (Database)

// 数据库操作
- (BOOL)hwsdk_saveToDatabase:(NSError **)error;
- (BOOL)hwsdk_loadFromDatabase:(NSError **)error;
- (void)hwsdk_clearDatabase;

@end

NS_ASSUME_NONNULL_END
