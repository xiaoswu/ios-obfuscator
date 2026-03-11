//
//  HWSDKMethodTest.m
//  HWSDK
//
//  方法名混淆测试实现
//

#import "HWSDKMethodTest.h"
#import "MBProgressHUD+NJ.h"

#pragma mark - HWSDKMethodTest1: 自定义方法（应混淆）

@implementation HWSDKMethodTest1

// 自定义方法实现
- (void)customMethod {
    NSLog(@"Custom method called");
}

- (NSString *)processData:(NSString *)data {
    return [data uppercaseString];
}

- (void)doTask:(NSString *)task withParam:(NSInteger)param {
    NSLog(@"Task: %@, Param: %ld", task, (long)param);
}

+ (instancetype)createInstance {
    return [[HWSDKMethodTest1 alloc] init];
}

+ (NSString *)sharedData {
    return @"shared";
}

@end

#pragma mark - HWSDKMethodTest2: 相同方法名（测试全局同名同混淆）

@implementation HWSDKMethodTest2

- (void)customMethod {
    NSLog(@"HWSDKMethodTest2 custom method");
}

- (NSString *)processData:(NSString *)data {
    return [data lowercaseString];
}

@end

#pragma mark - HWSDKMethodTest3: 系统方法（应保留）

@implementation HWSDKMethodTest3

// init 系列方法应保留
- (instancetype)init {
    self = [super init];
    return self;
}

- (instancetype)initWithName:(NSString *)name {
    self = [super init];
    return self;
}

// UIViewController 生命周期方法应保留
- (void)viewDidLoad {
    // 系统方法
}

- (void)viewWillAppear:(BOOL)animated {
    // 系统方法
}

@end

#pragma mark - HWSDKMethodTest4: Delegate 方法（应保留）

@implementation HWSDKMethodTest4

// Delegate 协议方法应保留
- (NSInteger)tableView:(id)view numberOfRowsInSection:(NSInteger)section {
    return 0;
}

- (id)tableView:(id)view cellForRowAtIndexPath:(id)path {
    return nil;
}

@end

#pragma mark - HWSDKMethodTest5: @selector 测试

@implementation HWSDKMethodTest5

- (void)methodWithSelector {
    // @selector() 引用应该随方法名一起混淆
    SEL sel1 = @selector(customSelectorMethod);
    SEL sel2 = @selector(customMethod);

    [self performSelector:sel1];
    [self performSelector:sel2];
}

- (void)customSelectorMethod {
    NSLog(@"Selector method");
}

- (void)customMethod {
    NSLog(@"Another custom method");
}

@end

#pragma mark - HWSDKMethodTest6: 消息表达式测试

@implementation HWSDKMethodTest6

- (void)testMessageExpressions {
    HWSDKMethodTest1 *obj1 = [[HWSDKMethodTest1 alloc] init];
    HWSDKMethodTest2 *obj2 = [[HWSDKMethodTest2 alloc] init];

    // 消息表达式中的方法名应该被混淆
    [obj1 customMethod];
    [obj2 customMethod];  // 应该与上面混淆成相同的方法名

    NSString *result1 = [obj1 processData:@"hello"];
    NSString *result2 = [obj2 processData:@"world"];

    [obj1 doTask:@"task1" withParam:100];
    
    // MBProgessHUD+NJ 方法调用
    [MBProgressHUD hideHUD];
}

@end

#pragma mark - HWSDKMethodTest7: 参数名混淆

@implementation HWSDKMethodTest7

- (void)methodWithParameters:(NSString *)inputData outputData:(NSString *)output {
    // 参数名 inputData 和 output 应该被混淆
    NSLog(@"Input: %@, Output: %@", inputData, output);
}

@end

#pragma mark - HWSDKMethodTest8: Block 参数

@implementation HWSDKMethodTest8

- (void)methodWithBlock:(void(^)(NSString *blockParam))block {
    // Block 参数名 blockParam 应该被混淆
    if (block) {
        block(@"test");
    }
}

@end

#pragma mark - HWSDKMethodTest9: description 方法（系统方法，应保留）

@implementation HWSDKMethodTest9

// description 是系统方法，应保留
- (NSString *)description {
    return @"HWSDKMethodTest9";
}

@end

#pragma mark - HWSDKMethodTest10: 多参数方法

@implementation HWSDKMethodTest10

- (void)methodWithArg1:(NSString *)arg1 arg2:(NSInteger)arg2 arg3:(BOOL)arg3 {
    NSLog(@"arg1: %@, arg2: %ld, arg3: %d", arg1, (long)arg2, arg3);
}

@end

#pragma mark - HWSDKMethodTest11: 无参数方法

@implementation HWSDKMethodTest11

- (void)noParamMethod {
    NSLog(@"No param method");
}

- (NSString *)anotherNoParamMethod {
    return @"no param";
}

@end

#pragma mark - HWSDKMethodTest12: 带返回值的方法

@implementation HWSDKMethodTest12

- (NSString *)getStringValue {
    return @"string";
}

- (NSInteger)getIntValue {
    return 42;
}

- (BOOL)getBoolValue {
    return YES;
}

- (id)getIdValue {
    return @"id value";
}

@end

@implementation HWSDKMethodTest13

- (void)setQueen:(NSString *)queeen {
    NSLog(@"setWueen");
}


-(NSString *)queen {
    return @"queen";
}

- (void)textSetter {
    self.queen = @"123";
    NSString *oen = self.queen;
    
}

@end


@implementation HWSDKMethodTest14
- (BOOL)methodBOOLOne {
    return YES;
}

- (BOOL)methodBOOLTwo:(NSString *)oe {
    return NO;
}

- (void)runnings {
    
    BOOL varib = YES;
    NSString *accook = @"12";
    NSString *pss = @"123";
    if ([self methodBOOLOne] && [self methodBOOLTwo:pss] && varib && accook.length != 0) {
        NSLog(@"if 条件内的方法与变量名要混淆");
    }
}

@end
