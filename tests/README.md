# iOS Obfuscator 测试套件

本目录包含完整的回归测试框架，用于验证已实现的混淆策略并追踪未实现的策略。

## 运行测试

### 方式一：直接运行测试脚本

```bash
# 完整回归测试
./tests/run_all_tests.sh

# 快速测试 (仅验证核心功能)
./tests/run_quick_test.sh

# 方法名混淆策略专项测试
./tests/test_method_name.sh
```

### 方式二：使用 CTest (需要先编译)

```bash
cd build
cmake ..  # 首次需要配置
make

# 运行所有测试
make test

# 或使用 ctest 直接
ctest

# 运行特定标签的测试
ctest -L quick      # 仅快速测试
ctest -L regression # 完整回归测试

# 详细输出
ctest -V

# 并行运行
ctest -j$(sysctl -n hw.ncpu)
```

## 策略状态

| 策略 | 状态 | 描述 |
|------|------|------|
| ClassNameObfuscation | ✅ 已实现 | 类名混淆 - 混淆 @interface 和 @implementation 名称 |
| MethodNameObfuscation | ✅ 已实现 | 方法名混淆 - 混淆方法声明、消息表达式和 @selector() |
| FileNameObfuscation | ✅ 已实现 | 文件名混淆 - 重命名源文件并更新 import 语句 |
| SDKNameObfuscation | ✅ 已实现 | SDK名称混淆 - 重命名SDK框架本身 |
| FolderNameObfuscation | ✅ 已实现 | 文件夹名混淆 - 重命名包含类文件的文件夹 |
| PropertyNameObfuscation | ⏳ 未实现 | 属性名混淆 - 混淆 @property 声明 |
| VariableNameObfuscation | ⏳ 未实现 | 变量名混淆 - 混淆局部和实例变量 |
| ProtocolNameObfuscation | ⏳ 未实现 | 协议名混淆 - 混淆 @protocol 声明 |
| CategoryNameObfuscation | ⏳ 未实现 | 类别名混淆 - 混淆 @category 声明 |
| StringObfuscation | ⏳ 未实现 | 字符串混淆 - 加密字符串字面量 |
| ResourceObfuscation | ⏳ 未实现 | 资源混淆 - 混淆图片、storyboard等资源文件 |
| MetadataObfuscation | ⏳ 未实现 | 元数据混淆 - 清理和混淆调试信息 |
| ControlFlowObfuscation | ⏳ 未实现 | 控制流混淆 - 扁平化控制流 |

## 测试文件结构

```
tests/
├── run_all_tests.sh       # 完整回归测试脚本
├── run_quick_test.sh      # 快速测试脚本
├── test_method_name.sh    # 方法名混淆专项测试
├── fixtures/              # 测试用输入文件
│   ├── TestClass.m        # 类名混淆测试用例
│   ├── MethodNameTestClass.m
│   ├── FileNameTestClass.m
│   ├── HeaderFile.h
│   └── AnotherHeader.h
└── output/                # 测试输出目录 (自动生成)
```

## 添加新测试

### 1. 为新策略创建测试用例

在 `tests/fixtures/` 中创建包含待混淆代码的 `.m` 和 `.h` 文件：

```objc
// fixtures/NewStrategyTestClass.m
#import <Foundation/Foundation.h>

@interface NewStrategyTestClass : NSObject
// 待混淆的声明
@end
```

### 2. 在测试脚本中添加测试

编辑 `run_all_tests.sh`，在 `run_all_tests()` 函数中添加：

```bash
# NewStrategyObfuscation 测试
echo ""
echo -e "${GRAY}[N] NewStrategyObfuscation 测试${NC}"
run_strategy_test "NewStrategyObfuscation" "新策略描述" "NewStrategyTestClass.m" "newstrategy_test"
```

### 3. 更新策略状态

在 `run_all_tests.sh` 的策略状态声明部分更新：

```bash
STRATEGY_STATUS[NewStrategyObfuscation]="implemented"
STRATEGY_DESCRIPTION[NewStrategyObfuscation]="新策略描述"
```

## 持续集成

建议在提交代码前运行以下命令确保没有破坏现有功能：

```bash
# 1. 编译
./build.sh

# 2. 运行快速测试
./tests/run_quick_test.sh

# 3. 如果快速测试通过，运行完整测试
./tests/run_all_tests.sh
```

## 测试输出说明

测试脚本会输出以下信息：

- **策略状态概览** - 显示所有策略的实现状态
- **环境检查** - 验证可执行文件和依赖项
- **测试执行** - 逐个运行测试并显示结果
- **测试摘要** - 总测试数、通过数、失败数、跳过数

颜色说明：
- 🟢 绿色 - 测试通过
- 🔴 红色 - 测试失败
- 🟡 黄色 - 跳过 (策略未实现)
- 🔵 蓝色 - 信息提示
- ⚪ 灰色 - 次要信息

## 常见问题

### Q: 测试失败但混淆器看起来工作正常？
A: 检查 `tests/output/` 目录中的详细日志文件，查看具体错误信息。

### Q: 如何调试单个策略？
A: 使用 `--verbose` 标志运行混淆器：
```bash
./build/ios-obfuscator --config=config.json --input=test_input --output=test_output --verbose
```

### Q: 测试太慢怎么办？
A: 使用快速测试 `run_quick_test.sh`，它只验证核心功能。
