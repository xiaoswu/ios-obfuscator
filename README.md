# iOS 代码混淆工具

一个基于 Clang/LLVM 的命令行工具，用于混淆 iOS Objective-C SDK 源代码，保护知识产权，增加逆向工程难度。

## 功能特性

### 已实现的混淆策略

| 策略 | 说明 | 文件 |
|------|------|------|
| **类名混淆** | 混淆 `@interface` 和 `@implementation` 声明中的类名 | `ClassNameStrategy.cpp` |
| **方法名混淆** | 混淆自定义方法名，保留系统方法 | `MethodNameStrategy.cpp` |
| **属性名混淆** | 混淆 `@property` 属性名，同步处理 getter/setter | `PropertyNameStrategy.cpp` |
| **变量名混淆** | 混淆实例变量和局部变量名 | `VariableNameStrategy.cpp` |
| **协议名混淆** | 混淆 `@protocol` 声明和引用 | `ProtocolNameStrategy.cpp` |
| **文件名混淆** | 重命名 `.h` 和 `.m` 文件，更新 import 语句 | `FileNameStrategy.cpp` |
| **文件夹名混淆** | 重命名包含类文件的文件夹 | `ClassNameFolderStrategy.cpp` |
| **SDK 名混淆** | 重命名整个 SDK 框架名称 | `SDKNameStrategy.cpp` |
| **注释删除** | 移除源代码中的注释 | `CommentRemovalStrategy.cpp` |
| **死代码注入** | 在方法中插入无意义的代码片段 | `DeadCodeInjectionStrategy.cpp` |

### 核心特性

- **两阶段处理**：先收集符号，再应用混淆，确保跨文件引用正确
- **智能白名单**：保护系统类、关键方法和公共接口不被混淆
- **灵活配置**：支持 JSON 配置文件自定义混淆规则
- **映射表生成**：输出原始名与混淆名的对应关系，便于调试
- **性能优化**：使用 Aho-Corasick 多模式匹配算法提高处理速度

## 环境要求

- **CMake** 3.15+
- **LLVM/Clang** 开发库
- **C++17** 编译器
- **macOS** 或 **Linux**

## 快速开始

### 一键编译

```bash
cd ios-obfuscator
./build.sh
```

编译成功后，可执行文件位于 `build/ios-obfuscator`。

### 基本使用

```bash
cd build
./ios-obfuscator --config=../config/config.json --input=../SDK --output=../obfuscated
```

## 编译指南

### macOS 编译

#### 使用 Homebrew（推荐）

```bash
# 安装依赖
brew install cmake llvm

# 编译
mkdir build && cd build
LLVM_PREFIX=$(brew --prefix llvm)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang
make -j$(sysctl -n hw.ncpu)
```

#### 使用 Xcode

```bash
# 确保 Xcode Command Line Tools 已安装
xcode-select --install

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Linux 编译

```bash
# 安装依赖
sudo apt-get update
sudo apt-get install -y build-essential cmake llvm-dev clang libclang-dev

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 验证编译

```bash
./build/ios-obfuscator --help
```

## 使用指南

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--config=<file>` | 配置文件路径（默认: config.json） |
| `--input=<path>` | SDK 源代码路径 |
| `--output=<path>` | 混淆后代码输出路径 |
| `--verbose` | 启用详细输出 |

### 配置文件

配置文件使用 JSON 格式：

```json
{
  "sdk": {
    "name": "MySDK",
    "type": "framework",
    "inputPath": "/path/to/sdk",
    "outputPath": "/path/to/output"
  },
  "obfuscation": {
    "strategies": [
      "ClassNameObfuscation",
      "MethodNameObfuscation",
      "PropertyNameObfuscation"
    ],
    "namingRule": {
      "style": "words",
      "prefix": "",
      "wordListPath": "./wordlist",
      "wordCase": "camelCase",
      "wordCount": {
        "className": {"min": 2, "max": 3},
        "methodName": {"min": 2, "max": 3}
      },
      "randomLength": {
        "className": {"min": 6, "max": 12}
      }
    },
    "whitelist": {
      "classes": ["NSObject", "UIViewController"],
      "methods": ["init", "viewDidLoad", "dealloc"],
      "properties": ["delegate"],
      "publicHeaders": ["PublicAPI.h"]
    },
    "deadCodeInjection": {
      "density": 0.2,
      "maxStatementsPerMethod": 10
    },
    "generateMapping": true,
    "mappingOutputPath": "./mapping.json"
  }
}
```

### 配置项说明

#### sdk 节
- `name`: SDK 名称
- `type`: SDK 类型（通常为 "framework"）
- `inputPath`: 源代码输入路径
- `outputPath`: 混淆后代码输出路径

#### obfuscation 节
- `strategies`: 启用的混淆策略列表

可用的策略名称：
- `ClassNameObfuscation`
- `MethodNameObfuscation`
- `PropertyNameObfuscation`
- `VariableNameObfuscation`
- `ProtocolNameObfuscation`
- `FileNameObfuscation`
- `ClassNameFolderObfuscation`
- `SDKNameObfuscation`
- `CommentRemoval`
- `DeadCodeInjection`

- `namingRule`: 命名规则配置
  - `style`: 命名风格（`random` 随机字符 / `words` 有意义单词）
  - `prefix`: 名称前缀
  - `wordListPath`: 单词库路径（style 为 words 时有效）
  - `wordCase`: 单词大小写格式（`camelCase` / `PascalCase`）
  - `randomLength`: 随机名称长度范围

- `whitelist`: 白名单配置
  - `classes`: 不混淆的类名列表
  - `methods`: 不混淆的方法名列表
  - `properties`: 不混淆的属性名列表
  - `publicHeaders`: 公共头文件列表（文件内容不混淆）

- `deadCodeInjection`: 死代码注入配置
  - `density`: 注入密度（0.0 - 1.0）
  - `maxStatementsPerMethod`: 每个方法最大插入语句数

- `generateMapping`: 是否生成映射表
- `mappingOutputPath`: 映射表输出路径

## 项目结构

```
ios-obfuscator/
├── src/
│   ├── core/                      # 核心模块
│   │   ├── ConfigManager.cpp      # 配置管理
│   │   ├── SymbolTable.cpp        # 符号表（双向映射）
│   │   ├── NameGenerator.cpp      # 名称生成器
│   │   ├── StrategyManager.cpp    # 策略管理器
│   │   ├── ReplacementManager.cpp # 替换管理器
│   │   ├── CompileOptions.cpp     # 编译选项生成
│   │   ├── AhoCorasick.cpp        # 多模式匹配算法
│   │   └── DeadCodeGenerator.cpp  # 死代码生成器
│   ├── strategies/                # 混淆策略实现
│   │   ├── ObfuscationStrategy.cpp     # 策略基类
│   │   ├── ClassNameStrategy.cpp       # 类名混淆
│   │   ├── MethodNameStrategy.cpp      # 方法名混淆
│   │   ├── PropertyNameStrategy.cpp    # 属性名混淆
│   │   ├── VariableNameStrategy.cpp    # 变量名混淆
│   │   ├── ProtocolNameStrategy.cpp    # 协议名混淆
│   │   ├── FileNameStrategy.cpp        # 文件名混淆
│   │   ├── ClassNameFolderStrategy.cpp # 文件夹名混淆
│   │   ├── SDKNameStrategy.cpp         # SDK名混淆
│   │   ├── CommentRemovalStrategy.cpp  # 注释删除
│   │   └── DeadCodeInjectionStrategy.cpp # 死代码注入
│   ├── templates/                 # 死代码模板
│   └── main.cpp                   # 程序入口
├── config/                        # 配置文件
│   └── config.json
├── wordlist/                      # 单词库
│   ├── nouns.txt
│   ├── verbs.txt
│   └── adjectives.txt
├── tests/                         # 测试用例
├── CMakeLists.txt                 # CMake 构建配置
└── build.sh                       # 一键编译脚本
```

### 核心模块说明

| 模块 | 职责 |
|------|------|
| **ConfigManager** | 加载和解析 JSON 配置文件 |
| **SymbolTable** | 维护原始名和混淆名的双向映射 |
| **NameGenerator** | 根据配置生成混淆后的名称 |
| **StrategyManager** | 注册和管理所有混淆策略 |
| **ReplacementManager** | 统一管理代码替换操作，避免冲突 |
| **CompileOptions** | 自动检测 iOS SDK 路径，生成编译参数 |
| **AhoCorasick** | 多模式字符串匹配算法，用于高效查找 |
| **DeadCodeGenerator** | 生成无意义的死代码片段 |

## 开发指南

### 添加新混淆策略

1. 在 `src/strategies/` 创建策略类，继承 `ObfuscationStrategy`

```cpp
class MyObfuscationStrategy : public ObfuscationStrategy {
public:
    std::string getName() const override { return "MyObfuscation"; }
    std::string getDescription() const override { return "我的混淆策略"; }

    void analyze(clang::ASTContext& context) override;
    void transform(clang::ASTContext& context) override;
    bool validate() override;
};
```

2. 在 `StrategyManager::createStrategy()` 中注册

3. 在配置文件的 `strategies` 列表中添加策略名称

### AST Matcher 常用用法

```cpp
// 匹配 ObjC 类声明
finder.addMatcher(
    objcInterfaceDecl().bind("interface"),
    this
);

// 匹配方法声明
finder.addMatcher(
    objcMethodDecl().bind("method"),
    this
);

// 排除特定名称
finder.addMatcher(
    objcInterfaceDecl(unless(hasName("NSObject"))).bind("interface"),
    this
);

// 在回调中获取匹配结果
void run(const MatchResult& result) override {
    if (const auto* decl = result.Nodes.getNodeAs<ObjCInterfaceDecl>("interface")) {
        // 处理匹配到的节点
    }
}
```

### Clang Rewriter 注意事项

1. **检查位置有效性**：替换前必须检查 `SourceLocation` 是否有效
2. **文件范围检查**：使用 `SM.isInMainFile()` 避免修改头文件
3. **从后往前替换**：避免位置偏移问题
4. **字符串字面量**：使用 AST 信息判断，避免替换字符串中的内容

## 常见问题

### 编译问题

**Q: CMake 找不到 LLVM**

```
CMake Error: Could not find a package configuration file provided by "LLVM"
```

**A**: 手动指定 LLVM 路径

```bash
# macOS (Homebrew)
LLVM_PREFIX=$(brew --prefix llvm)
cmake .. -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm
```

**Q: 找不到 Clang 头文件**

```
fatal error: 'clang/AST/ASTContext.h' file not found
```

**A**: 确保安装了 LLVM 开发库

```bash
brew install llvm
```

### 使用问题

**Q: 混淆后代码无法编译？**

**A**: 检查以下几点：
1. 确保公共接口在白名单中
2. 检查系统类和方法是否被错误混淆
3. 查看映射表确认混淆结果

**Q: 某些类没有被混淆？**

**A**: 可能原因：
1. 类在白名单中
2. 类位于 `.framework` 内部（自动跳过）
3. 类文件在 `ThirdParty/` 或 `ThirdPart/` 目录下

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request。
