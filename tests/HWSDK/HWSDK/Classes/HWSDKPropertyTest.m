//
//  HWSDKPropertyTest.m
//  HWSDK
//
//  属性名混淆测试实现
//

#import "HWSDKPropertyTest.h"
#import <UIKit/UIKit.h>
#import <StoreKit/StoreKit.h>

// MARK: - HWSDKTestClass1
@implementation HWSDKTestClass1

// 测试白名单属性的 getter/setter（不应混淆）
- (void)testWhitelistProperties {
    // 访问白名单属性的 getter（不应混淆）
    id dlg = self.delegate;
    id src = self.dataSource;

    // 设置白名单属性的 setter（不应混淆）
    self.delegate = nil;
    self.dataSource = nil;

    // 使用括号语法调用 getter（不应混淆）
    dlg = [self delegate];
    src = [self dataSource];

    // 使用括号语法调用 setter（不应混淆）
    [self setDelegate:nil];
    [self setDataSource:nil];
}

@end

// MARK: - HWSDKTestClass2
@implementation HWSDKTestClass2
@end

// MARK: - HWSDKTestClass3
@implementation HWSDKTestClass3
@end

// MARK: - HWSDKTestClass4
@implementation HWSDKTestClass4
@end

// MARK: - HWSDKTestClass5
@implementation HWSDKTestClass5
@end

// MARK: - HWSDKTestClass6
@implementation HWSDKTestClass6
@end

// MARK: - HWSDKTestClass7
@implementation HWSDKTestClass7
@end

// MARK: - HWSDKTestClass8 (点语法和成员变量)
@interface HWSDKTestClass8 ()
@property (nonatomic, strong) NSString *anotherProperty;
@end


@implementation HWSDKTestClass8

// 测试点语法访问
- (void)testDotSyntax {
    // 点语法访问 self.propertyValue
    self.propertyValue = @"test value";
    NSString *value = self.propertyValue;

    // 点语法访问 self.anotherProperty
    self.anotherProperty = @"another";
    NSString *another = self.anotherProperty;

    // 点语法链式调用
    self.anotherProperty = self.propertyValue;
    
    // 测试在混淆自定义名的时候，不混淆系统名
    SKPaymentTransaction *tr;
    self.productId = tr.payment.productIdentifier;

}

// 测试成员变量访问（下划线前缀）
- (void)testIvarAccess {
    // 直接访问成员变量 _propertyValue
    _propertyValue = @"direct ivar access";
    NSString *value = _propertyValue;

    // 混合使用
    _propertyValue = self.propertyValue;
    self.propertyValue = _propertyValue;
}

@end

// MARK: - HWSDKTestClass9 (KVO/KVC)
@implementation HWSDKTestClass9

// KVO 测试
- (void)testKVO {
    // addObserver:forKeyPath:options:context:
    [self addObserver:self
           forKeyPath:@"observedValue"
              options:NSKeyValueObservingOptionNew
              context:nil];

    [self addObserver:self
           forKeyPath:@"counter"
              options:NSKeyValueObservingOptionNew
              context:nil];

    // 触发 KVO
    self.observedValue = @"new value";
    self.counter = 100;

    // removeObserver:forKeyPath:
    [self removeObserver:self forKeyPath:@"observedValue"];
    [self removeObserver:self forKeyPath:@"counter"];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context {
    // KVO 回调
}

// KVC 测试
- (void)testKVC {
    // valueForKey:
    NSString *value1 = [self valueForKey:@"observedValue"];
    NSNumber *value2 = [self valueForKey:@"counter"];

    // setValue:forKey:
    [self setValue:@"KVC value" forKey:@"observedValue"];
    [self setValue:@200 forKey:@"counter"];

    // valueForKeyPath:
    NSString *pathValue = [self valueForKeyPath:@"observedValue"];

    // setValue:forKeyPath:
    [self setValue:@"path value" forKeyPath:@"observedValue"];
}

@end

// MARK: - HWSDKTestClass10 ([object instance].property 模式)
@implementation HWSDKTestClass10

+ (instancetype)sharedInstance {
    static HWSDKTestClass10 *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[HWSDKTestClass10 alloc] init];
    });
    return instance;
}

- (void)testObjectPropertyAccess {
    // 测试 [object instance].property 模式
    [[HWSDKTestClass10 sharedInstance].logArr stringByAppendingString:@"test"];
    NSString *data = [HWSDKTestClass10 sharedInstance].dataList;
    NSInteger count = [HWSDKTestClass10 sharedInstance].totalCount;

    // 多次访问
    [HWSDKTestClass10 sharedInstance].logArr = @"updated";
    [HWSDKTestClass10 sharedInstance].dataList = @"data";
    [HWSDKTestClass10 sharedInstance].totalCount = 100;
    
    // 多次访问之后在调用方法
    [HWSDKTestClass10 sharedInstance].muarray = [NSMutableArray array];
    [[HWSDKTestClass10 sharedInstance].muarray addObject:@"13"];
}

@end

// MARK: - HWSDKTestClass11 (链式属性访问 a.b.c)
@implementation HWSDKTestClass11

- (void)testChainedPropertyAccess {
    // 创建测试对象
    self.innerObject = [HWSDKTestClass12 createWithCity:@"Beijing" country:@"China"];
    self.firstName = @"John";
    self.lastName = @"Doe";

    // 链式访问: object.property.property
    NSString *city = self.innerObject.city;
    NSString *country = self.innerObject.country;
    NSInteger zipCode = self.innerObject.zipCode;

    // 多层链式访问
    NSString *combined = [NSString stringWithFormat:@"%@,%@", self.innerObject.city, self.innerObject.country];
}

@end

// MARK: - HWSDKTestClass12 (用于链式访问的内部类)
@implementation HWSDKTestClass12

+ (instancetype)createWithCity:(NSString *)city country:(NSString *)country {
    HWSDKTestClass12 *obj = [[HWSDKTestClass12 alloc] init];
    obj.city = city;
    obj.country = country;
    obj.zipCode = 100000;
    return obj;
}

@end

// MARK: - HWSDKTestClass13 (Block 内部属性访问)
@implementation HWSDKTestClass13

- (void)testBlockPropertyAccess {
    self.blockTestData = @"initial";
    self.blockCount = 0;

    // Block 内部访问属性
    void (^testBlock)(void) = ^{
        self.blockTestData = @"block test";
        self.blockCount = 100;
        NSString *data = self.blockTestData;
        NSInteger count = self.blockCount;
    };

    testBlock();

    // 嵌套 block
    [[HWSDKTestClass10 alloc] init].logArr = @"test";
    dispatch_async(dispatch_get_main_queue(), ^{
        self.blockTestData = @"async test";
        self.blockCount = 200;

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            self.blockCount = 300;
        });
    });
}

@end

// MARK: - HWSDKTestClass14 (复杂嵌套场景)
@implementation HWSDKTestClass14

- (void)testComplexNestedAccess {
    // 初始化对象
    self.manager = [HWSDKTestClass10 sharedInstance];
    self.user = [[HWSDKTestClass11 alloc] init];
    // 内部成员变量调用
    HWSDKTestClass12 *class12 = [[HWSDKTestClass12 alloc]init];
    class12.service = @"123";

    // 复杂嵌套: [object method].property
    [[HWSDKTestClass10 sharedInstance].logArr stringByAppendingString:@"complex"];
    [[HWSDKTestClass10 sharedInstance].dataList length];

    // 链式嵌套: object.property.property
    self.user.innerObject = [HWSDKTestClass12 createWithCity:@"Shanghai" country:@"China"];
    NSString *city = self.user.innerObject.city;
    NSInteger zip = self.user.innerObject.zipCode;

    // 混合模式
    NSString *result = [HWSDKTestClass10 sharedInstance].logArr;
    result = self.user.firstName;
    result = self.user.innerObject.country;
}

@end

// MARK: - HWSDKTestClass15 (多重括号表达式)
@implementation HWSDKTestClass15

- (void)testParenthesisExpression {
    // 测试括号表达式后的属性访问
    NSString *v1 = ((HWSDKTestClass15 *)[[HWSDKTestClass15 alloc] init]).value1;

    // 多层括号
    (((HWSDKTestClass15 *)self)).value1 = @"test";

    // 类型转换 + 括号
    ((HWSDKTestClass15 *)[HWSDKTestClass15 new]).value2 = @"test2";
}

@end

// MARK: - HWSDKTestClass16 (@dynamic 测试)
@implementation HWSDKTestClass16

// @dynamic 告诉编译器属性访问方法在运行时动态实现
@dynamic dynamicProperty;
@dynamic anotherDynamic;

@end

// MARK: - HWSDKTestClass17 (@synthesize 测试)
@implementation HWSDKTestClass17

// @synthesize 指定成员变量名或显式合成属性
// 这些 @synthesize 语句应该与属性声明保持一致的混淆名
@synthesize synthProperty;
@synthesize minSize;
@synthesize customIvarProperty = _customIvarProperty;  // 自定义成员变量名
@synthesize valueProperty = _valueProperty;  // 自定义成员变量名

- (void)testSynthesize {
    // 测试属性访问
    self.synthProperty = @"test";
    NSString *value = self.synthProperty;

    // 测试自定义成员变量的属性
    self.customIvarProperty = @"custom";
    self.valueProperty = 100;

    // 测试成员变量直接访问（下划线前缀）
    _customIvarProperty = @"direct ivar access";
    _valueProperty = 200;
    NSString *ivarValue = _customIvarProperty;

    // 测试直接使用成员变量名（不带下划线）访问
    // 当 @synthesize 没有指定成员变量名时，可以使用属性名直接访问
//    synthProperty = @"direct property name access";
    minSize = CGSizeZero;
}

@end

// MARK: - HWSDKTestClass18 (同名独立成员变量测试)
@implementation HWSDKTestClass18

- (void)testSharedDataAccess {
    // 测试属性访问
    self.sharedData = @"class18 data";
    NSString *value = self.sharedData;

    // 测试成员变量访问
    _sharedData = @"class18 ivar";
    value = _sharedData;
}

@end

// MARK: - HWSDKTestClass19 (独立成员变量 _sharedData)
@implementation HWSDKTestClass19 {
    // 独立成员变量声明（不是属性）
    NSString *_sharedData;  // 这个 _sharedData 不是属性，是独立成员变量
    NSInteger _count;       // 独立成员变量
}

- (void)testIndependentIvar {
    // 测试独立成员变量访问
    // 因为 HWSDKTestClass18 有属性 sharedData，所以这里的 _sharedData 也应该被混淆
    _sharedData = @"class19 independent ivar";
    NSString *data = _sharedData;

    _count = 100;
    NSInteger c = _count;
}

@end

// MARK: - HWSDKTestClass20 (@selector() 测试)
@implementation HWSDKTestClass20

- (void)testSelectorReferences {
    // 测试 @selector() getter 引用（应混淆）
    SEL sel1 = @selector(selectorName);
    SEL sel2 = @selector(selectorCount);
    SEL sel3 = @selector(isActive);

    // 测试 @selector() setter 引用（应混淆）
    SEL sel4 = @selector(setSelectorName:);
    SEL sel5 = @selector(setSelectorCount:);
    SEL sel6 = @selector(setIsActive:);

    // 测试自定义 getter/setter 的 @selector（应混淆）
    SEL sel7 = @selector(getDataValue);
    SEL sel8 = @selector(setDataValue:);

    // 测试 @selector() 在方法调用中的使用
    [self performSelector:sel1];
    [self performSelector:sel4 withObject:@"test"];

    // 测试 NSSelectorFromString
    NSString *selStr1 = NSStringFromSelector(@selector(selectorName));
    NSString *selStr2 = NSStringFromSelector(@selector(setSelectorCount:));

    // 测试响应检查
    BOOL responds = [self respondsToSelector:@selector(selectorName)];

    // 测试比较
    if (@selector(isActive) == @selector(isActive)) {
        // selector comparison
    }
    
    // 自定义getter 方法的使用
    NSString *se = [self getDataValue];
    NSString *oe = self.getDataValue;
    
    // 自定义setter方法的使用
    [self setDataValue:@"123"];
}



@end

// MARK: - HWSDKTestClass21 (括号语法与点语法一致性测试)
@implementation HWSDKTestClass21

- (void)testBracketAndDotSyntax {
    // 点语法设置值
    self.bracketTest1 = @"dot syntax set";
    self.bracketTest2 = @"another value";
    self.bracketCount = 100;

    // 括号语法获取值（getter方法调用）
    // 这些应该与点语法使用相同的混淆名
    NSString *value1 = [self bracketTest1];
    NSString *value2 = [self bracketTest2];
    NSInteger count = [self bracketCount];

    // 括号语法设置值（setter方法调用）
    [self setBracketTest1:@"bracket syntax set"];
    [self setBracketTest2:@"bracket value"];
    [self setBracketCount:200];

    // 混合使用：点语法设置，括号语法读取
    self.bracketTest1 = @"mixed test";
    NSString *mixed = [self bracketTest1];

    // 混合使用：括号语法设置，点语法读取
    [self setBracketTest2:@"mixed test 2"];
    NSString *mixed2 = self.bracketTest2;

    // 验证值一致
    if ([self.bracketTest1 isEqualToString:[self bracketTest1]]) {
        // 点语法和括号语法应返回相同的值
    }
}

@end

// MARK: - HWSDKTestClass22ViewController (重写系统类方法测试)
#import "HWSDKPropertyTest.h"

@implementation HWSDKTestClass22ViewController

// 系统方法重写（不应混淆）
- (void)viewDidLoad {
    [super viewDidLoad];  // 调用父类方法
    // 自定义初始化逻辑
    self.customData = @"initialized";
    self.customCount = 0;
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];  // 调用父类方法
    // 自定义逻辑
    [self customMethod];
}

- (void)setView:(UIView *)view {
    
}

// 自定义方法（应该混淆）
- (void)customMethod {
    self.customCount++;
    NSString *result = [self processData:@"test"];
    NSLog(@"Custom count: %ld, result: %@", (long)self.customCount, result);
}

- (NSString *)processData:(NSString *)data {
    return [data stringByAppendingString:@"_processed"];
}

@end

// MARK: - HWSDKTestClass23View (重写系统属性 setter 测试)
@implementation HWSDKTestClass23View

// 重写系统属性的 setter（不应混淆）
- (void)setAlpha:(CGFloat)alpha {
    [super setAlpha:alpha];  // 调用父类 setter
    // 自定义逻辑
    NSLog(@"Alpha set to: %f", alpha);
}

// 自定义属性的 setter（应该混淆）
- (void)setCustomText:(NSString *)text {
    _customText = text;
    NSLog(@"Custom text set: %@", text);
}

@end
