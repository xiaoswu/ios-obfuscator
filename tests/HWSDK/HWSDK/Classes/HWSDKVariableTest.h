//
//  HWSDKVariableTest.h
//  HWSDK
//
//  变量名混淆测试
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// 全局变量（应混淆）
NSString *globalConfigString = @"config";
NSInteger globalProcessingCount = 0;

// MARK: - 测试类1：普通局部变量
@interface HWSDKVariableTestClass1 : NSObject

- (void)testLocalVariables;

@end

// MARK: - 测试类2：循环变量
@interface HWSDKVariableTestClass2 : NSObject

- (void)testLoopVariables;

@end

// MARK: - 测试类3：静态局部变量
@interface HWSDKVariableTestClass3 : NSObject

- (void)testStaticLocal;

@end

// MARK: - 测试类4：与系统名重名的局部变量
@interface HWSDKVariableTestClass4 : NSObject
@property (nonatomic,copy)NSString *delegate;

- (void)testVariablesWithSystemNames;

@end

// MARK: - 测试类5：不同函数的同名变量
@interface HWSDKVariableTestClass5 : NSObject

- (void)methodOne;
- (void)methodTwo;

@end

NS_ASSUME_NONNULL_END
