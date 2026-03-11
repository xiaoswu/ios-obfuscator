//
//  HWSDKDeadCodeTest.h
//  HWSDK
//
//  垃圾代码插入测试用例
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * 测试类1: 简单方法测试
 * 用于测试垃圾代码插入功能
 */
@interface HWSDKDeadCodeTest1 : NSObject

// 简单的实例方法
- (void)simpleMethod;

// 带参数的方法
- (NSString *)processData:(NSString *)data;

// 带多个参数的方法
- (void)doTask:(NSString *)task withParam:(NSInteger)param;

@end

/**
 * 测试类2: 类方法测试
 */
@interface HWSDKDeadCodeTest2 : NSObject

// 类方法
+ (instancetype)createInstance;

+ (NSString *)sharedData;

@end

/**
 * 测试类3: 空方法测试
 */
@interface HWSDKDeadCodeTest3 : NSObject

// 空方法
- (void)emptyMethod;

// 只有简单返回值
- (NSInteger)getValue;

@end

/**
 * 测试类4: 复杂逻辑方法
 */
@interface HWSDKDeadCodeTest4 : NSObject

// 带条件判断的方法
- (void)methodWithCondition:(BOOL)flag;

// 带循环的方法
- (void)methodWithLoop:(NSArray *)items;

@end

/**
 * 测试类5: 使用Foundation类的方法
 */
@interface HWSDKDeadCodeTest5 : NSObject

// 使用字符串操作
- (NSString *)stringManipulation:(NSString *)input;

// 使用数组操作
- (NSArray *)arrayManipulation:(NSArray *)input;

// 使用字典操作
- (NSDictionary *)dictionaryManipulation:(NSDictionary *)input;

@end

NS_ASSUME_NONNULL_END
