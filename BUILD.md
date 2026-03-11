# 编译指南

## 前置要求

### 必需依赖

1. **CMake 3.15+**
   ```bash
   # 检查CMake版本
   cmake --version
   ```

2. **LLVM/Clang 开发库**
   - macOS: 通过Xcode Command Line Tools或Homebrew安装
   - Linux: 通过包管理器安装llvm-dev和clang-dev

3. **C++17 编译器**
   - macOS: Xcode的clang++（通常已安装）
   - Linux: g++ 7+ 或 clang++ 5+

### 检查依赖

```bash
# 检查CMake
cmake --version
# 应该显示 3.15 或更高版本

# 检查C++编译器
clang++ --version
# 或
g++ --version

# 检查LLVM/Clang（macOS）
xcode-select -p
# 应该显示Xcode路径

# 检查LLVM库（Linux）
pkg-config --modversion llvm
```

## 编译步骤

### macOS 编译步骤

#### 方法1：使用Xcode Command Line Tools（推荐）

```bash
# 1. 确保安装了Xcode Command Line Tools
xcode-select --install

# 2. 进入项目目录
cd /Users/liang/Downloads/cursor/ios-obfuscator

# 3. 创建build目录
mkdir -p build
cd build

# 4. 配置CMake
# 注意：需要指定LLVM路径（如果不在默认位置）
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/usr/local/opt/llvm/lib/cmake/llvm \
  -DClang_DIR=/usr/local/opt/llvm/lib/cmake/clang

# 如果上面的路径不对，尝试：
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/cmake/llvm

# 或者让CMake自动查找（推荐）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 5. 编译
make -j$(sysctl -n hw.ncpu)

# 6. 验证编译结果
ls -la ios-obfuscator
./ios-obfuscator --help
```

#### 方法2：使用Homebrew安装LLVM

```bash
# 1. 安装LLVM（如果还没有）
brew install llvm

# 2. 进入项目目录
cd /Users/liang/Downloads/cursor/ios-obfuscator
mkdir -p build
cd build

# 3. 配置CMake（指定Homebrew的LLVM路径）
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/usr/local/opt/llvm/lib/cmake/llvm \
  -DClang_DIR=/usr/local/opt/llvm/lib/cmake/clang

# 4. 编译
make -j$(sysctl -n hw.ncpu)
```

### Linux 编译步骤

```bash
# 1. 安装依赖（Ubuntu/Debian）
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  llvm-dev \
  clang \
  libclang-dev

# 2. 进入项目目录
cd ios-obfuscator
mkdir -p build
cd build

# 3. 配置CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. 编译
make -j$(nproc)

# 5. 验证
ls -la ios-obfuscator
./ios-obfuscator --help
```

## 详细编译流程

### 步骤1：准备环境

```bash
# 进入项目根目录
cd /Users/liang/Downloads/cursor/ios-obfuscator

# 检查当前目录
pwd
# 应该显示：/Users/liang/Downloads/cursor/ios-obfuscator

# 查看项目结构
ls -la
# 应该看到：CMakeLists.txt, src/, config/ 等目录
```

### 步骤2：创建build目录

```bash
# 创建build目录（如果不存在）
mkdir -p build

# 进入build目录
cd build

# 确认在build目录
pwd
# 应该显示：/Users/liang/Downloads/cursor/ios-obfuscator/build
```

### 步骤3：配置CMake

```bash
# 基本配置（让CMake自动查找LLVM）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 如果CMake找不到LLVM，尝试指定路径：
# macOS (Homebrew)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/usr/local/opt/llvm/lib/cmake/llvm \
  -DClang_DIR=/usr/local/opt/llvm/lib/cmake/clang

# macOS (Xcode)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/cmake/llvm

# 如果还是找不到，可以查看LLVM安装位置：
# macOS
brew --prefix llvm
# 或
xcode-select -p
```

### 步骤4：编译

```bash
# 单线程编译（如果遇到问题，使用这个）
make

# 多线程编译（推荐，更快）
make -j4  # 使用4个线程
# 或自动检测CPU核心数
make -j$(sysctl -n hw.ncpu)  # macOS
make -j$(nproc)              # Linux
```

### 步骤5：验证

```bash
# 检查可执行文件是否存在
ls -la ios-obfuscator

# 应该看到类似输出：
# -rwxr-xr-x  1 user  staff  1234567 Jan 12 18:00 ios-obfuscator

# 测试运行
./ios-obfuscator --help

# 应该显示帮助信息
```

## 常见问题

### 问题1：CMake找不到LLVM

**错误信息**：
```
CMake Error: Could not find a package configuration file provided by "LLVM"
```

**解决方法**：

1. **查找LLVM路径**：
   ```bash
   # macOS (Homebrew)
   brew --prefix llvm
   
   # 查看LLVM配置
   find /usr/local -name "LLVMConfig.cmake" 2>/dev/null
   find /Applications/Xcode.app -name "LLVMConfig.cmake" 2>/dev/null
   ```

2. **手动指定路径**：
   ```bash
   cmake .. \
     -DLLVM_DIR=/找到的路径/lib/cmake/llvm \
     -DClang_DIR=/找到的路径/lib/cmake/clang
   ```

3. **安装LLVM**（如果没有）：
   ```bash
   # macOS
   brew install llvm
   
   # Linux
   sudo apt-get install llvm-dev libclang-dev
   ```

### 问题2：找不到Clang头文件

**错误信息**：
```
fatal error: 'clang/AST/ASTContext.h' file not found
```

**解决方法**：

1. **检查Clang安装**：
   ```bash
   clang --version
   ```

2. **指定包含路径**：
   ```bash
   cmake .. \
     -DCMAKE_CXX_FLAGS="-I/usr/local/opt/llvm/include"
   ```

### 问题3：链接错误

**错误信息**：
```
undefined reference to `clang::...'
```

**解决方法**：

1. **检查LLVM库路径**：
   ```bash
   # macOS
   find /usr/local/opt/llvm -name "libclang*.dylib"
   ```

2. **设置库路径**：
   ```bash
   export DYLD_LIBRARY_PATH=/usr/local/opt/llvm/lib:$DYLD_LIBRARY_PATH
   ```

### 问题4：nlohmann/json下载失败

**错误信息**：
```
Failed to download nlohmann/json
```

**解决方法**：

1. **检查网络连接**
2. **手动下载**（如果需要）：
   ```bash
   # 编辑CMakeLists.txt，使用本地json库
   # 或设置代理
   export https_proxy=your_proxy
   ```

### 问题5：编译时间过长

**解决方法**：

1. **使用多线程编译**：
   ```bash
   make -j$(sysctl -n hw.ncpu)  # macOS
   make -j$(nproc)              # Linux
   ```

2. **只编译Release版本**：
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

## 编译选项

### Debug模式（开发调试）

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

### Release模式（生产使用）

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### 自定义安装路径

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make
make install
```

## 验证编译成功

编译成功后，应该：

1. **可执行文件存在**：
   ```bash
   ls -la build/ios-obfuscator
   # 文件应该存在且有执行权限
   ```

2. **可以运行**：
   ```bash
   ./build/ios-obfuscator --help
   # 应该显示帮助信息
   ```

3. **版本信息**：
   ```bash
   ./build/ios-obfuscator --version
   # 或查看日志输出
   ```

## 清理编译

如果需要重新编译：

```bash
# 清理编译文件
cd build
make clean

# 或删除整个build目录
cd ..
rm -rf build
mkdir build
cd build
cmake ..
make
```

## 下一步

编译成功后，参考以下文档：
- `QUICKSTART.md` - 快速开始
- `USAGE.md` - 使用指南
- `README.md` - 项目说明

