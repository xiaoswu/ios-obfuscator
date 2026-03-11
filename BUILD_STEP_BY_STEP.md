# 详细编译步骤（针对你的环境）

## 当前环境检查结果

- ✅ clang++ 已安装（Apple clang version 16.0.0）
- ✅ Xcode 已安装（/Applications/Xcode.app/Contents/Developer）
- ❌ CMake 未安装
- ❌ LLVM开发库未找到

## 完整安装和编译步骤

### 步骤1：安装CMake

#### 方法1：使用Homebrew（推荐）

```bash
# 1. 检查是否已安装Homebrew
brew --version

# 如果没有Homebrew，先安装：
# /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2. 安装CMake
brew install cmake

# 3. 验证安装
cmake --version
# 应该显示 3.15 或更高版本
```

#### 方法2：使用MacPorts

```bash
sudo port install cmake
```

#### 方法3：从官网下载

1. 访问：https://cmake.org/download/
2. 下载macOS安装包
3. 安装后，可能需要添加到PATH：
   ```bash
   export PATH="/Applications/CMake.app/Contents/bin:$PATH"
   ```

### 步骤2：安装LLVM开发库

#### 方法1：使用Homebrew（推荐）

```bash
# 安装LLVM（包含开发库）
brew install llvm

# 安装后，查找LLVM路径
brew --prefix llvm
# 通常输出：/usr/local/opt/llvm 或 /opt/homebrew/opt/llvm

# 验证安装
ls -la $(brew --prefix llvm)/lib/cmake/llvm
# 应该看到 LLVMConfig.cmake 文件
```

#### 方法2：使用Xcode的LLVM（不推荐，可能不完整）

Xcode自带的LLVM可能不包含开发库，建议使用Homebrew安装。

### 步骤3：编译工具

```bash
# 1. 进入项目目录
cd /Users/liang/Downloads/cursor/ios-obfuscator

# 2. 创建build目录
mkdir -p build
cd build

# 3. 配置CMake
# 如果使用Homebrew安装的LLVM：
LLVM_PREFIX=$(brew --prefix llvm)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang

# 如果CMake自动找到了LLVM，可以直接：
# cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. 编译
make -j$(sysctl -n hw.ncpu)

# 5. 验证
ls -la ios-obfuscator
./ios-obfuscator --help
```

## 一键安装脚本

创建以下脚本可以自动完成安装和编译：

```bash
#!/bin/bash
# 保存为 build.sh

set -e  # 遇到错误立即退出

echo "=== iOS混淆工具编译脚本 ==="

# 检查并安装CMake
if ! command -v cmake &> /dev/null; then
    echo "CMake未安装，正在安装..."
    if command -v brew &> /dev/null; then
        brew install cmake
    else
        echo "错误：未找到Homebrew，请先安装Homebrew或手动安装CMake"
        exit 1
    fi
else
    echo "✓ CMake已安装: $(cmake --version | head -1)"
fi

# 检查并安装LLVM
if [ ! -f "$(brew --prefix llvm 2>/dev/null)/lib/cmake/llvm/LLVMConfig.cmake" ]; then
    echo "LLVM开发库未找到，正在安装..."
    if command -v brew &> /dev/null; then
        brew install llvm
    else
        echo "错误：未找到Homebrew，请先安装Homebrew或手动安装LLVM"
        exit 1
    fi
else
    echo "✓ LLVM已安装"
fi

# 进入项目目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# 创建build目录
mkdir -p build
cd build

# 配置CMake
echo "正在配置CMake..."
LLVM_PREFIX=$(brew --prefix llvm)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang

# 编译
echo "正在编译..."
make -j$(sysctl -n hw.ncpu)

# 验证
if [ -f "ios-obfuscator" ]; then
    echo "✓ 编译成功！"
    echo "可执行文件位置: $(pwd)/ios-obfuscator"
    ./ios-obfuscator --help
else
    echo "✗ 编译失败，请检查错误信息"
    exit 1
fi
```

使用方法：
```bash
chmod +x build.sh
./build.sh
```

## 手动编译步骤（详细版）

### 1. 安装依赖

```bash
# 安装Homebrew（如果还没有）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装CMake
brew install cmake

# 安装LLVM
brew install llvm

# 验证安装
cmake --version
brew --prefix llvm
```

### 2. 准备编译环境

```bash
# 进入项目目录
cd /Users/liang/Downloads/cursor/ios-obfuscator

# 查看项目结构
ls -la
# 应该看到：CMakeLists.txt, src/, config/ 等

# 创建build目录
mkdir -p build
cd build
```

### 3. 配置CMake

```bash
# 获取LLVM路径
LLVM_PREFIX=$(brew --prefix llvm)
echo "LLVM路径: $LLVM_PREFIX"

# 检查LLVM配置文件是否存在
ls -la $LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake
ls -la $LLVM_PREFIX/lib/cmake/clang/ClangConfig.cmake

# 配置CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang

# 如果配置成功，会看到类似输出：
# -- Configuring done
# -- Generating done
# -- Build files have been written to: ...
```

### 4. 编译

```bash
# 查看CPU核心数
sysctl -n hw.ncpu

# 编译（使用所有CPU核心）
make -j$(sysctl -n hw.ncpu)

# 或者使用固定线程数（如果遇到问题）
make -j4

# 编译过程会显示进度，最后应该看到：
# [100%] Built target ios-obfuscator
```

### 5. 验证

```bash
# 检查可执行文件
ls -lh ios-obfuscator
# 应该看到文件大小和权限

# 测试运行
./ios-obfuscator --help
# 应该显示帮助信息

# 查看版本信息（如果有）
./ios-obfuscator --version
```

## 可能遇到的问题

### 问题1：Homebrew未安装

**解决方法**：
```bash
# 安装Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 添加到PATH（如果是Apple Silicon Mac）
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
eval "$(/opt/homebrew/bin/brew shellenv)"
```

### 问题2：CMake找不到LLVM

**错误信息**：
```
CMake Error: Could not find a package configuration file provided by "LLVM"
```

**解决方法**：
```bash
# 1. 确认LLVM已安装
brew list llvm

# 2. 查找LLVM路径
LLVM_PREFIX=$(brew --prefix llvm)
echo $LLVM_PREFIX

# 3. 检查配置文件
ls -la $LLVM_PREFIX/lib/cmake/llvm/

# 4. 手动指定路径
cmake .. \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang
```

### 问题3：编译错误

**常见错误**：
- 头文件找不到：检查LLVM安装是否完整
- 链接错误：检查库路径是否正确
- 权限错误：确保有写入权限

**解决方法**：
```bash
# 清理后重新编译
cd build
make clean
rm -rf *
cmake ..
make
```

## 编译成功后

编译成功后，可以：

1. **运行工具**：
   ```bash
   cd build
   ./ios-obfuscator --help
   ```

2. **复制到系统路径**（可选）：
   ```bash
   sudo cp build/ios-obfuscator /usr/local/bin/
   ```

3. **开始使用**：
   ```bash
   ./build/ios-obfuscator --config=../config/config.json --input=../SDK --output=../obfuscated
   ```

## 快速参考

```bash
# 完整命令序列（复制粘贴执行）
cd /Users/liang/Downloads/cursor/ios-obfuscator

# 安装依赖（如果还没有）
brew install cmake llvm

# 编译
mkdir -p build && cd build
LLVM_PREFIX=$(brew --prefix llvm)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang
make -j$(sysctl -n hw.ncpu)

# 验证
./ios-obfuscator --help
```

