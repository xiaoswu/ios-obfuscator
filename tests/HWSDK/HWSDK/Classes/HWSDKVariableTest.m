//
//  HWSDKVariableTest.m
//  HWSDK
//
//  变量名混淆测试实现
//

#import "HWSDKVariableTest.h"

// MARK: - 全局变量（在头文件中声明）
// 这些应该被混淆

// MARK: - HWSDKVariableTestClass1 实现
@implementation HWSDKVariableTestClass1

- (void)testLocalVariables {
    // 应混淆的局部变量
    NSString *userName = @"test";
    NSInteger userAge = 25;
    BOOL isValid = YES;
    NSArray *items = @[@"item1", @"item2"];
    
    // 列入白名单的局部变量不应混淆
    NSString *bezel = @"bezel";

    NSLog(@"%@, %ld, %d, %@, %@", userName, (long)userAge, isValid, items, bezel);

    // 测试变量引用
    NSString *upperName = [userName uppercaseString];
    NSLog(@"%@", upperName);
}

@end

// MARK: - HWSDKVariableTestClass2 实现
@implementation HWSDKVariableTestClass2

- (void)testLoopVariables {
    // 短循环变量（可能保留）
    for (int i = 0; i < 10; i++) {
        NSLog(@"%d", i);
    }

    // 较长名称的循环变量（应混淆）
    for (NSInteger index = 0; index < 10; index++) {
        NSLog(@"%ld", (long)index);
    }

    // 嵌套循环
    for (NSInteger row = 0; row < 5; row++) {
        for (NSInteger col = 0; col < 5; col++) {
            NSLog(@"%ld-%ld", (long)row, (long)col);
        }
    }

    // foreach 循环
    NSArray *data = @[@"a", @"b", @"c"];
    for (NSString *element in data) {
        NSLog(@"%@", element);
    }
}

@end

// MARK: - HWSDKVariableTestClass3 实现
@implementation HWSDKVariableTestClass3

- (void)testStaticLocal {
    // 静态局部变量（应混淆）
    static NSString *kSharedKey = @"key";
    static NSInteger counter = 0;

    counter++;
    NSLog(@"%@: %ld", kSharedKey, (long)counter);
}

- (void)anotherMethod {
    // 访问同一个静态局部变量
    static NSString *kSharedKey = @"key";
    NSLog(@"%@", kSharedKey);
}

@end

// MARK: - HWSDKVariableTestClass4 实现
@implementation HWSDKVariableTestClass4

- (void)testVariablesWithSystemNames {
    // 与系统属性同名的局部变量（应混淆）
    NSString *delegate = @"test";
    NSString *dataSource = @"source";
    NSString *view = @"main";

    NSLog(@"%@, %@, %@", delegate, dataSource, view);

    // 对比：系统属性访问（不应混淆）
    // self.delegate 和 self.dataSource 是属性访问
    // delegate 和 dataSource 是局部变量
    NSLog(@"local: %@, property: %@", delegate, self.delegate);
    
    
}



@end

// MARK: - HWSDKVariableTestClass5 实现
@implementation HWSDKVariableTestClass5

- (void)methodOne {
    // 同名变量（在不同函数中应该独立混淆）
    NSString *data = @"data1";
    NSInteger count = 10;
    NSLog(@"%@: %ld", data, (long)count);
}

- (void)methodTwo {
    // 同名变量（应该与 methodOne 中的混淆名不同）
    NSString *data = @"data2";
    NSInteger count = 20;
    NSLog(@"%@: %ld", data, (long)count);
}

@end
