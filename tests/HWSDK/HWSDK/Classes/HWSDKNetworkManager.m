//
//  HWSDKNetworkManager.m
//  HWSDK
//
//  Created by liang on 2026/1/17.
//

#import "HWSDKNetworkManager.h"
#import "Masonry.h"

// HWSDK Network Constants
static NSString *const kHWSDKBaseURL = @"https://api.hwsdk.com";
static NSString *const kHWSDKVersion = @"v1";

@interface HWSDKNetworkManager()
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic,strong) UIView *msld;
@end

@implementation HWSDKNetworkManager

+ (instancetype)sharedManager {
    static HWSDKNetworkManager *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[HWSDKNetworkManager alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        NSURLSessionConfiguration *config = [NSURLSessionConfiguration defaultSessionConfiguration];
        config.timeoutIntervalForRequest = 30.0;
        _session = [NSURLSession sessionWithConfiguration:config];
    }
    return self;
}

- (void)ldoMasonry {
    [self.msld mas_makeConstraints:^(MASConstraintMaker *make) {
        make.size.mas_equalTo(CGSizeMake(230,345));
    }];
}

- (void)sendRequestWithURL:(NSString *)url
                completion:(void (^)(BOOL success, id data))completion {
    NSURL *requestURL = [NSURL URLWithString:url];
    if (!requestURL) {
        if (completion) {
            completion(NO, nil);
        }
        return;
    }

    NSURLSessionDataTask *task = [self.session dataTaskWithURL:requestURL
                                            completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        if (error) {
            if (completion) {
                completion(NO, error);
            }
        } else {
            if (completion) {
                completion(YES, data);
            }
        }
    }];
    [task resume];
}

- (void)fetchHWSDKInfo:(void (^)(NSDictionary *info))completion {
    NSString *url = [NSString stringWithFormat:@"%@/%@/info", kHWSDKBaseURL, kHWSDKVersion];
    [self sendRequestWithURL:url completion:^(BOOL success, id data) {
        if (success && completion) {
            completion(@{@"version": kHWSDKVersion});
        }
    }];
}

@end
