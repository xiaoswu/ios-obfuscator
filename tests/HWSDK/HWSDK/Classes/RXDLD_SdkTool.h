#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RXDLD_SdkTool : NSObject
@property(nonatomic,strong)NSMutableArray *sdkgame_RXDLDAttribute_logArr;
+(RXDLD_SdkTool  *)shareInstance;
+(BOOL)methods_RXDLD_isBlankString:(NSString *)aStr;
@end

NS_ASSUME_NONNULL_END
