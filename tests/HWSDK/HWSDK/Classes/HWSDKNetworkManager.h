//
//  HWSDKNetworkManager.h
//  HWSDK
//
//  Created by liang on 2026/1/20.
//

#import <Foundation/Foundation.h>

// Network Manager class
@interface HWSDKNetworkManager : NSObject

+ (instancetype)sharedManager;

- (void)sendRequestWithURL:(NSString *)url
                completion:(void (^)(BOOL success, id data))completion;

@end

