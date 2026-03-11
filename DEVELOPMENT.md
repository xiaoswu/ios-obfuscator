# iOS混淆工具开发文档

## 代码结构说明

### 核心模块 (src/core/)

#### ConfigManager
- **功能**: 配置管理，负责加载和解析JSON配置文件
- **依赖**: nlohmann/json库
- **主要方法**:
  - `loadFromFile()`: 从文件加载配置
  - `loadFromString()`: 从字符串加载配置
  - `parseJSON()`: 解析JSON对象
  - `saveToFile()`: 保存配置到文件

#### Logger
- **功能**: 日志系统，支持多级别日志输出
- **日志级别**: DEBUG, INFO, WARNING, ERROR
- **使用方式**: 通过宏 `LOG_DEBUG()`, `LOG_INFO()` 等

#### NameGenerator
- **功能**: 生成混淆后的名称
- **支持模式**:
  - RANDOM: 随机字符串（如 `OBF_A1B2C3D4`）
  - PREFIX_WORD: 前缀+单词组（如 `OBF_cat_dog`）
- **特点**: 自动避免命名冲突

#### SymbolTable
- **功能**: 符号表管理，存储原始名称和混淆名称的映射
- **符号类型**: CLASS, METHOD, PROPERTY, VARIABLE, PROTOCOL, CATEGORY, FILE, STRING, RESOURCE
- **主要方法**:
  - `addSymbol()`: 添加符号
  - `getObfuscatedName()`: 获取混淆后的名称
  - `getOriginalName()`: 获取原始名称

#### StrategyManager
- **功能**: 策略管理器，负责加载和执行混淆策略
- **工作流程**:
  1. 根据配置加载策略
  2. 解析策略依赖关系
  3. 按顺序执行策略的分析、转换、验证阶段

### 策略模块 (src/strategies/)

#### ObfuscationStrategy (基类)
- **功能**: 所有混淆策略的基类
- **生命周期**:
  1. `initialize()`: 初始化
  2. `analyze()`: 分析阶段，收集需要混淆的符号
  3. `transform()`: 转换阶段，执行混淆
  4. `validate()`: 验证阶段，检查结果

#### ClassNameObfuscationStrategy
- **功能**: 类名混淆策略
- **实现方式**:
  - 使用AST Matcher匹配类声明
  - 使用Rewriter替换源代码
- **处理内容**:
  - @interface声明
  - @implementation声明
  - 类型引用（待完善）

## 代码替换原理

### Clang Rewriter使用

Rewriter是Clang提供的代码重写工具，可以安全地修改源代码。

#### 基本用法

```cpp
// 1. 创建Rewriter
Rewriter rewriter;
rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

// 2. 获取SourceLocation
SourceLocation loc = decl->getLocation();

// 3. 执行替换
rewriter.ReplaceText(loc, original.length(), obfuscated);

// 4. 获取修改后的代码
std::string modifiedCode = rewriter.getRewrittenText(SourceRange(...));
```

#### 注意事项

1. **位置有效性**: 替换前必须检查SourceLocation是否有效
2. **文件范围**: 使用`SM.isInMainFile()`检查是否在主文件中
3. **替换顺序**: 从后往前替换，避免位置偏移问题
4. **字符串字面量**: 避免替换字符串中的类名（使用AST信息判断）

### AST Matcher使用

AST Matcher用于匹配特定的AST节点。

#### 基本用法

```cpp
// 1. 创建MatchFinder
MatchFinder finder;

// 2. 添加匹配器
finder.addMatcher(
    objcInterfaceDecl(unless(hasName("NSObject"))).bind("interface"),
    this  // MatchCallback对象
);

// 3. 执行匹配
finder.matchAST(context);

// 4. 实现回调
void run(const MatchResult& result) override {
    if (const auto* decl = result.Nodes.getNodeAs<ObjCInterfaceDecl>("interface")) {
        // 处理匹配到的节点
    }
}
```

#### 常用匹配器

- `objcInterfaceDecl()`: 匹配@interface声明
- `objcImplementationDecl()`: 匹配@implementation声明
- `objcMethodDecl()`: 匹配方法声明
- `objcPropertyDecl()`: 匹配属性声明
- `hasName("ClassName")`: 匹配特定名称
- `unless(...)`: 排除匹配

## JSON配置格式

### 完整配置示例

```json
{
  "sdk": {
    "name": "SDK名称",
    "type": "framework",
    "inputPath": "输入路径",
    "outputPath": "输出路径"
  },
  "obfuscation": {
    "strategies": ["策略列表"],
    "namingRule": {
      "style": "random|prefix_word",
      "prefix": "OBF_",
      "length": 8,
      "wordCount": 2,
      "wordLength": 3,
      "charset": "alphanumeric"
    },
    "whitelist": {
      "classes": [],
      "methods": [],
      "properties": [],
      "thirdPartySDKs": []
    },
    "generateMapping": true,
    "mappingOutputPath": "./mapping.json"
  }
}
```

### 配置项说明

- **sdk.name**: SDK名称，用于标识
- **sdk.type**: SDK类型，通常是"framework"
- **sdk.inputPath**: 源代码路径
- **sdk.outputPath**: 混淆后代码输出路径
- **obfuscation.strategies**: 启用的混淆策略列表
- **obfuscation.namingRule**: 命名规则配置
- **obfuscation.whitelist**: 白名单配置
- **obfuscation.generateMapping**: 是否生成映射表
- **obfuscation.mappingOutputPath**: 映射表输出路径

## 开发指南

### 添加新策略

1. 创建策略类，继承`ObfuscationStrategy`
2. 实现必要的方法：
   - `getName()`: 返回策略名称
   - `getDescription()`: 返回策略描述
   - `analyze()`: 分析阶段
   - `transform()`: 转换阶段
   - `validate()`: 验证阶段

3. 在`StrategyManager::createStrategy()`中注册

### 调试技巧

1. 使用`LOG_DEBUG()`输出调试信息
2. 使用AST Matcher的`bind()`功能定位节点
3. 使用`SourceManager`检查SourceLocation
4. 使用`Rewriter`的`getRewrittenText()`查看修改结果

## 常见问题

### Q: 如何避免替换字符串中的类名？

A: 使用AST信息判断，而不是简单的文本替换。通过AST可以知道哪些是字符串字面量。

### Q: 如何处理import语句？

A: 需要在transform阶段查找所有import语句，并替换其中的类名引用。

### Q: 如何确保替换顺序正确？

A: 从后往前替换，或者先收集所有位置，排序后再替换。

## 参考资料

- [Clang AST Matcher Reference](https://clang.llvm.org/docs/LibASTMatchersReference.html)
- [Clang Rewriter Documentation](https://clang.llvm.org/doxygen/classclang_1_1Rewriter.html)
- [nlohmann/json Documentation](https://json.nlohmann.me/)

