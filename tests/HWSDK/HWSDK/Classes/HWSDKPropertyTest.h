//
//  HWSDKPropertyTest.h
//  HWSDK
//
//  属性名混淆测试 - 涵盖所有场景
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - 测试类1：普通属性
@interface HWSDKTestClass1 : NSObject

// 普通属性（应混淆）
@property (nonatomic, strong) NSString *userName;
@property (nonatomic, strong) NSString *userId;
@property (nonatomic, assign) NSInteger age;
@property (nonatomic, assign) CGFloat height;
@property (nonatomic,copy) NSString *msg;
@property (nonatomic,copy)NSString *imageView;
@property (nonatomic,copy) NSString *userInfo;
@property (nonatomic,copy) NSString *line;

// 白名单属性（不应混淆，getter/setter 也不应混淆）
@property (nonatomic, weak) id delegate;
@property (nonatomic, weak) id dataSource;

@end

// MARK: - 测试类2：布尔属性（is 前缀）
@interface HWSDKTestClass2 : NSObject

// 布尔属性（应混淆，保留 is 前缀）
@property (nonatomic, assign) BOOL isValid;
@property (nonatomic, assign) BOOL isEnabled;
@property (nonatomic, assign) BOOL isLoading;

@end

// MARK: - 测试类3：相同属性名（不同类）
// 与 HWSDKTestClass1 有相同的 userName 属性
// 应该混淆成相同的名称
@interface HWSDKTestClass3 : NSObject

@property (nonatomic, strong) NSString *userName;  // 应与 TestClass1 相同混淆名
@property (nonatomic, strong) NSString *email;

@end

// MARK: - 测试类4：readonly 属性
@interface HWSDKTestClass4 : NSObject

@property (nonatomic, strong, readonly) NSString *identifier;
@property (nonatomic, assign, readonly) NSInteger count;

@end

// MARK: - 测试类5：IBOutlet 属性（应保留）
// 使用 id 类型代替具体的 UI 控件类型，避免导入 UIKit
@interface HWSDKTestClass5 : NSObject

@property (nonatomic, strong) id titleLabel;   // 普通属性，应混淆
@property (nonatomic, strong) id actionButton; // 普通属性，应混淆
@property (nonatomic, strong) NSString *internalData;          // 应混淆

@end

// MARK: - 测试类6：各种属性类型
@interface HWSDKTestClass6 : NSObject

//@property (nonatomic, copy) NSString *copyString;
@property (nonatomic, weak) id<NSObject> weakDelegate;
@property (nonatomic, unsafe_unretained) void *unsafePointer;
@property (nonatomic, strong) NSArray *strongArray;
@property (class, nonatomic, readonly) NSString *classProperty;

@end

// MARK: - 测试类7：自定义 getter/setter
@interface HWSDKTestClass7 : NSObject

@property (nonatomic, assign, getter=isCustomEnabled, setter=setCustomEnabled:) BOOL customEnabled;
@property (nonatomic, strong, getter=getDataValue) NSString *dataValue;

@end

// MARK: - 测试类8：点语法和成员变量测试
@interface HWSDKTestClass8 : NSObject

@property (nonatomic, strong) NSString *propertyValue;
@property (nonatomic,copy)NSString *productId;

- (void)testDotSyntax;
- (void)testIvarAccess;

@end

// MARK: - 测试类9：KVO/KVC 测试
@interface HWSDKTestClass9 : NSObject

@property (nonatomic, strong) NSString *observedValue;
@property (nonatomic, assign) NSInteger counter;

- (void)testKVO;
- (void)testKVC;

@end

// MARK: - 测试类10：[object instance].property 模式
@interface HWSDKTestClass10 : NSObject

@property (nonatomic, strong) NSString *logArr;
@property (nonatomic, strong) NSString *dataList;
@property (nonatomic, assign) NSInteger totalCount;

@property (nonatomic, strong)NSMutableArray *muarray;

+ (instancetype)sharedInstance;
- (void)testObjectPropertyAccess;

@end
@class HWSDKTestClass12;
// MARK: - 测试类11：链式属性访问 a.b.c
@interface HWSDKTestClass11 : NSObject

@property (nonatomic, strong) HWSDKTestClass12 *innerObject;
@property (nonatomic, strong) NSString *firstName;
@property (nonatomic, strong) NSString *lastName;

- (void)testChainedPropertyAccess;

@end

// MARK: - 测试类12：用于链式访问的内部类
@interface HWSDKTestClass12 : NSObject

@property (nonatomic, strong) NSString *city;
@property (nonatomic, strong) NSString *country;
@property (nonatomic, assign) NSInteger zipCode;
@property (nonatomic,copy) NSString *service;

+ (instancetype)createWithCity:(NSString *)city country:(NSString *)country;

@end

// MARK: - 测试类13：Block 内部属性访问
@interface HWSDKTestClass13 : NSObject

@property (nonatomic, strong) NSString *blockTestData;
@property (nonatomic, assign) NSInteger blockCount;

- (void)testBlockPropertyAccess;

@end

// MARK: - 测试类14：复杂嵌套场景
@interface HWSDKTestClass14 : NSObject

@property (nonatomic, strong) HWSDKTestClass10 *manager;
@property (nonatomic, strong) HWSDKTestClass11 *user;

- (void)testComplexNestedAccess;

@end

// MARK: - 测试类15：多重括号表达式
@interface HWSDKTestClass15 : NSObject

@property (nonatomic, strong) NSString *value1;
@property (nonatomic, strong) NSString *value2;

- (void)testParenthesisExpression;

@end

// MARK: - 测试类16：@dynamic 测试
@interface HWSDKTestClass16 : NSObject

@property (nonatomic, strong) NSString *dynamicProperty;
@property (nonatomic, strong) NSString *anotherDynamic;

@end

// MARK: - 测试类17：@synthesize 测试
@interface HWSDKTestClass17 : NSObject

@property (nonatomic, strong) NSString *synthProperty;
@property (nonatomic, strong) NSString *customIvarProperty;
@property (nonatomic, assign) NSInteger valueProperty;
@property (assign) CGSize minSize;

@end

// MARK: - 测试类18：同名独立成员变量测试
// 验证：A类有属性 sharedData，B类有独立成员变量 _sharedData（非属性）
// 混淆后：B类的 _sharedData 声明和访问也应被混淆为 _xxx
@interface HWSDKTestClass18 : NSObject

@property (nonatomic, strong) NSString *sharedData;  // A类属性

- (void)testSharedDataAccess;

@end

@interface HWSDKTestClass19 : NSObject

// B类有独立成员变量 _sharedData（不是属性，直接在@implementation中声明）
- (void)testIndependentIvar;

@end

// MARK: - 测试类20：@selector() 测试
// 验证：属性混淆后，@selector(getter) 和 @selector(setter:) 也应被混淆
@interface HWSDKTestClass20 : NSObject

@property (nonatomic, strong) NSString *selectorName;
@property (nonatomic, assign) NSInteger selectorCount;
@property (nonatomic, assign) BOOL isActive;
@property (nonatomic, strong, getter=getDataValue, setter=setDataValue:) NSString *customSelector;

- (void)testSelectorReferences;

@end

// MARK: - 测试类21：括号语法与点语法一致性测试
// 验证：self.property（点语法）和 [self property]（括号语法）中的 property 混淆名应一致
@interface HWSDKTestClass21 : NSObject

@property (nonatomic, strong) NSString *bracketTest1;
@property (nonatomic, strong) NSString *bracketTest2;
@property (nonatomic, assign) NSInteger bracketCount;

- (void)testBracketAndDotSyntax;

@end

// MARK: - 测试类22：重写系统类方法测试
// 验证：子类重写系统类（UIViewController）的方法不应被混淆
@interface HWSDKTestClass22ViewController : UIViewController

// 自定义属性（应该混淆）
@property (nonatomic, strong) NSString *customData;
@property (nonatomic, assign) NSInteger customCount;

// 重写 UIViewController 的方法（不应混淆）
- (void)viewDidLoad;              // 系统方法，不应混淆
- (void)viewWillAppear:(BOOL)animated;  // 系统方法，不应混淆

// 自定义方法（应该混淆）
- (void)customMethod;
- (NSString *)processData:(NSString *)data;

@end

// MARK: - 测试类23：重写系统属性 setter 测试
@interface HWSDKTestClass23View : UIView

// 重写系统属性的 setter（不应混淆）
@property (nonatomic, assign) CGFloat alpha;  // 系统属性
- (void)setAlpha:(CGFloat)alpha;      // 不应混淆

// 自定义属性（应该混淆）
@property (nonatomic, strong) NSString *customText;
- (void)setCustomText:(NSString *)text;  // 应该混淆

@end

NS_ASSUME_NONNULL_END
