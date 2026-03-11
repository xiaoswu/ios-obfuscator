//
//  HWSDKManager.m
//  HWSDK
//
//  Created by liang on 2026/1/17.
//

#import "HWSDK.h"
#import "HWSDKPrivate.h"
#import "Hpub_YYKeychain.h"
#import "RXDLD_SdkTool.h"

// SDK Constants
NSString *const HWSDKVersion = @"1.0.0";
NSString *const HWSDKName = @"HWSDK";
NSString *const HWSDKBuildNumber = @"100";

@interface HWSDKManager()
@property (nonatomic, strong, readwrite) NSString *configuration;
@property (nonatomic, strong) dispatch_queue_t processingQueue;
@end

@implementation HWSDKManager

+ (instancetype)sharedManager {
    static HWSDKManager *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[HWSDKManager alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _configuration = @"default";
        _processingQueue = dispatch_queue_create("com.hwsdk.processing", DISPATCH_QUEUE_SERIAL);
        [self initializeHWSDK];
    }
    return self;
}

- (void)initializeHWSDK {
    NSLog(@"[HWSDK] Initializing HWSDK version %@", HWSDKVersion);
    NSLog(@"[HWSDK] SDK Name: %@", HWSDKName);

    // Load configuration
    [self loadConfiguration];
    Hpub_YYKeychainItem *imeiItem = [[Hpub_YYKeychainItem alloc] init];
    imeiItem.service = @"123";
    imeiItem.password = @"123";
    imeiItem.account = nil;

    NSLog(@"[HWSDK] Initialization complete");
    
    [[RXDLD_SdkTool shareInstance].sdkgame_RXDLDAttribute_logArr addObject:@"234"];
    if ([RXDLD_SdkTool methods_RXDLD_isBlankString:@"123"]) {
        NSLog(@"");
    }
}

- (void)loadConfiguration {
    // Load HWSDK configuration
    self.configuration = @"production";
}

- (NSString *)sdkName {
    return HWSDKName;
}

- (NSString *)sdkVersion {
    return HWSDKVersion;
}

- (void)processData:(NSData *)data completion:(void(^)(BOOL success))completion {
    dispatch_async(self.processingQueue, ^{
        // Process data
        BOOL success = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) {
                completion(success);
            }
        });
    });
}

@end
