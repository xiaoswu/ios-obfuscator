#import "RXDLD_SdkTool.h"

@implementation RXDLD_SdkTool

+(RXDLD_SdkTool *)shareInstance{
    static RXDLD_SdkTool *sdkTool;
    static dispatch_once_t oncetoken;
    dispatch_once(&oncetoken, ^{
        sdkTool = [[RXDLD_SdkTool  alloc] init];
    });
    return sdkTool;
}

-(NSMutableArray *)sdkgame_RXDLDAttribute_logArr{
    if (_sdkgame_RXDLDAttribute_logArr == nil) {
        _sdkgame_RXDLDAttribute_logArr = [NSMutableArray array];
    }
    return _sdkgame_RXDLDAttribute_logArr;
}

+ (BOOL)methods_RXDLD_isBlankString:(NSString *)aStr {
    if (!aStr) {
        return YES;
    }
    if ([aStr isKindOfClass:[NSNull class]]) {
        return YES;
    }
    NSCharacterSet *set = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    NSString *trimmedStr = [aStr stringByTrimmingCharactersInSet:set];
    if (!trimmedStr.length) {
        return YES;
    }
    return NO;
}




@end
