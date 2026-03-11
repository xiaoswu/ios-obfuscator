# iOS SDK 混淆工具

一个用于混淆iOS Objective-C SDK源代码的命令行工具。

## 功能特性

- ✅ 类名混淆
- ✅ 方法名混淆
- ✅ 属性名混淆
- ✅ 变量名混淆
- ✅ 协议名混淆
- ✅ 类别名混淆
- ✅ 文件名混淆
- ✅ 字符串混淆
- ✅ 资源文件混淆
- ✅ 元数据混淆
- ✅ 控制流混淆
- ✅ 运行时兼容性处理
- ✅ 映射表生成

## 构建要求

- CMake 3.15+
- LLVM/Clang (需要开发版本)
- C++17 编译器

## 构建步骤

### 快速开始（推荐）

使用自动编译脚本：
```bash
cd ios-obfuscator
./build.sh
```

### 手动编译

详细步骤请参考 `BUILD.md` 或 `BUILD_STEP_BY_STEP.md`

**基本步骤**：
```bash
# 1. 安装依赖（如果还没有）
brew install cmake llvm

# 2. 编译
cd ios-obfuscator
mkdir -p build && cd build
LLVM_PREFIX=$(brew --prefix llvm)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang
make -j$(sysctl -n hw.ncpu)

# 3. 验证
./ios-obfuscator --help
```

**注意**：如果遇到问题，请查看 `BUILD_STEP_BY_STEP.md` 获取详细帮助。

## 使用方法

### 前置条件

1. **编译工具**（如果还没有编译）：
   ```bash
   cd ios-obfuscator
   mkdir -p build && cd build
   cmake ..
   make
   ```

2. **准备配置文件**（可选）：
   ```bash
   cp config/config.json.example config/config.json
   # 编辑config.json，设置正确的路径
   ```

### 基本用法

**重要**：确保在正确的目录下运行，并且工具已经编译。

```bash
# 从build目录运行（推荐）
cd build
./ios-obfuscator --config=../config/config.json --input=../SDK --output=../obfuscated

# 或者使用绝对路径
./build/ios-obfuscator --input=/absolute/path/to/SDK --output=/absolute/path/to/output
```

**注意**：
- 如果出现 `bash: --input=./SDK: No such file or directory` 错误，说明命令格式不对
- 确保命令以 `./ios-obfuscator` 或完整路径开头
- 不要直接输入 `--input=./SDK`，前面必须有工具名称

### 命令行参数

- `--config=<file>`: 配置文件路径（默认: config.json）
- `--input=<path>`: SDK源代码路径
- `--output=<path>`: 混淆后代码输出路径
- `--verbose`: 启用详细输出

### 配置文件

配置文件使用JSON格式，示例见 `config/config.json.example`。

主要配置项：
- `sdk`: SDK基本信息（名称、类型、输入输出路径）
- `obfuscation.strategies`: 启用的混淆策略列表
- `obfuscation.namingRule`: 命名规则配置
- `obfuscation.whitelist`: 白名单配置

## 项目结构

```
ios-obfuscator/
├── src/
│   ├── core/           # 核心模块
│   │   ├── ConfigManager.cpp
│   │   ├── Logger.cpp
│   │   ├── SymbolTable.cpp
│   │   ├── NameGenerator.cpp
│   │   └── StrategyManager.cpp
│   ├── parser/          # 解析模块
│   ├── strategies/      # 混淆策略
│   ├── generator/       # 代码生成
│   └── main.cpp         # 主程序
├── config/              # 配置文件
├── tests/               # 测试
└── CMakeLists.txt
```

## 开发状态

当前版本实现了基础框架和类名混淆策略，其他策略正在开发中。

## 许可证

MIT License

