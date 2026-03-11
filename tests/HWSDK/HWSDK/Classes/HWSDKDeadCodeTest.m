//
//  HWSDKDeadCodeTest.m
//  HWSDK
//
//  垃圾代码插入测试实现
//

#import "HWSDKDeadCodeTest.h"

#pragma mark - HWSDKDeadCodeTest1: 简单方法测试

@implementation HWSDKDeadCodeTest1

// 简单的实例方法
- (void)simpleMethod {
    NSLog(@"Simple method called");
}

// 带参数的方法
- (NSString *)processData:(NSString *)data {
    return [data uppercaseString];
}

// 带多个参数的方法
- (void)doTask:(NSString *)task withParam:(NSInteger)param {
    NSLog(@"Task: %@, Param: %ld", task, (long)param);
}

@end

#pragma mark - HWSDKDeadCodeTest2: 类方法测试

@implementation HWSDKDeadCodeTest2

// 类方法
+ (instancetype)createInstance {
    return [[HWSDKDeadCodeTest2 alloc] init];
}

+ (NSString *)sharedData {
    return @"shared";
}

@end

#pragma mark - HWSDKDeadCodeTest3: 空方法测试

@implementation HWSDKDeadCodeTest3

// 空方法
- (void)emptyMethod {
    // 空方法，适合测试垃圾代码插入
}

// 只有简单返回值
- (NSInteger)getValue {
    return 42;
}

@end

#pragma mark - HWSDKDeadCodeTest4: 复杂逻辑方法

@implementation HWSDKDeadCodeTest4

// 带条件判断的方法
- (void)methodWithCondition:(BOOL)flag {
    if (flag) {
        NSLog(@"Flag is true");
    } else {
        NSLog(@"Flag is false");
    }
}

// 带循环的方法
- (void)methodWithLoop:(NSArray *)items {
    for (NSString *item in items) {
        NSLog(@"Item: %@", item);
    }
}

@end

#pragma mark - HWSDKDeadCodeTest5: 使用Foundation类的方法

@implementation HWSDKDeadCodeTest5

// 使用字符串操作
- (NSString *)stringManipulation:(NSString *)input {
    NSString *trimmed = [input stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    return [trimmed lowercaseString];
}

// 使用数组操作
- (NSArray *)arrayManipulation:(NSArray *)input {
    NSMutableArray *result = [NSMutableArray array];
    for (id item in input) {
        if ([item isKindOfClass:[NSString class]]) {
            [result addObject:[item uppercaseString]];
        }
    }
    return [result copy];
}

// 使用字典操作
- (NSDictionary *)dictionaryManipulation:(NSDictionary *)input {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    for (NSString *key in input) {
        NSString *value = input[key];
        if ([value isKindOfClass:[NSString class]]) {
            result[key] = [value stringByAppendingString:@"_processed"];
        }
    }
    return [result copy];
}

@end
