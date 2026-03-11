//
//  HWSDKMethodTest.h
//  HWSDK
//
//  方法名混淆测试 - 涵盖所有场景
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#pragma mark - 测试类1：自定义方法（应混淆）

@interface HWSDKMethodTest1 : NSObject

// 自定义实例方法（应混淆）
- (void)customMethod;
- (NSString *)processData:(NSString *)data;
- (void)doTask:(NSString *)task withParam:(NSInteger)param;

// 自定义类方法（应混淆）
+ (instancetype)createInstance;
+ (NSString *)sharedData;

@end

#pragma mark - 测试类2：相同方法名（测试全局同名同混淆）

@interface HWSDKMethodTest2 : NSObject

// 与 HWSDKMethodTest1 相同的方法名
// 应该混淆成相同的结果（保持多态）
- (void)customMethod;
- (NSString *)processData:(NSString *)data;

@end

#pragma mark - 测试类3：系统方法（应保留）

@protocol HWSDKTestDelegate <NSObject>
@optional
- (NSInteger)tableView:(id)view numberOfRowsInSection:(NSInteger)section;
- (id)tableView:(id)view cellForRowAtIndexPath:(id)path;
@end

@interface HWSDKMethodTest3 : NSObject <HWSDKTestDelegate>

// 系统生命周期方法（应保留）
- (instancetype)init;
- (instancetype)initWithName:(NSString *)name;
- (void)viewDidLoad;
- (void)viewWillAppear:(BOOL)animated;

@end

#pragma mark - 测试类4：Delegate 方法（应保留）

@interface HWSDKMethodTest4 : NSObject <HWSDKTestDelegate>

@end

#pragma mark - 测试类5：@selector 测试

@interface HWSDKMethodTest5 : NSObject

- (void)methodWithSelector;
- (void)customSelectorMethod;

@end

#pragma mark - 测试类6：消息表达式测试

@interface HWSDKMethodTest6 : NSObject

- (void)testMessageExpressions;

@end

#pragma mark - 测试类7：参数名混淆

@interface HWSDKMethodTest7 : NSObject

- (void)methodWithParameters:(NSString *)inputData outputData:(NSString *)output;

@end

#pragma mark - 测试类8：Block 参数

@interface HWSDKMethodTest8 : NSObject

- (void)methodWithBlock:(void(^)(NSString *blockParam))block;

@end

#pragma mark - 测试类9：description 方法（系统方法，应保留）

@interface HWSDKMethodTest9 : NSObject

@end

#pragma mark - 测试类10：多参数方法

@interface HWSDKMethodTest10 : NSObject

- (void)methodWithArg1:(NSString *)arg1 arg2:(NSInteger)arg2 arg3:(BOOL)arg3;

@end

#pragma mark - 测试类11：无参数方法

@interface HWSDKMethodTest11 : NSObject

- (void)noParamMethod;
- (NSString *)anotherNoParamMethod;

@end

#pragma mark - 测试类12：带返回值的方法

@interface HWSDKMethodTest12 : NSObject

- (NSString *)getStringValue;
- (NSInteger)getIntValue;
- (BOOL)getBoolValue;
- (id)getIdValue;

@end

#pragma mark - 测试类13：setter/getter方法测试

@interface HWSDKMethodTest13 : NSObject

- (void)setQueen:(NSString *)queeen;

-(NSString *)queen;


- (void)textSetter;

@end

#pragma mark - 测试类14：if条件内的方法应该混淆

@interface HWSDKMethodTest14 : NSObject

- (BOOL)methodBOOLOne;

- (BOOL)methodBOOLTwo:(NSString *)oe;

- (void)runnings;

@end

NS_ASSUME_NONNULL_END
