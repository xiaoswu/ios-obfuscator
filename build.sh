#!/bin/bash
# iOS混淆工具自动编译脚本

set -e  # 遇到错误立即退出

echo "=== iOS混淆工具编译脚本 ==="
echo ""

# 检查并安装CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake未安装"
    if command -v brew &> /dev/null; then
        echo "正在安装CMake..."
        brew install cmake
    else
        echo "错误：未找到Homebrew"
        echo "请先安装Homebrew："
        echo '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
        exit 1
    fi
else
    echo "✓ CMake已安装: $(cmake --version | head -1)"
fi

# 检查并安装LLVM
if ! command -v brew &> /dev/null; then
    echo "错误：未找到Homebrew，请先安装Homebrew"
    exit 1
fi

LLVM_PREFIX=$(brew --prefix llvm 2>/dev/null || echo "")
if [ -z "$LLVM_PREFIX" ] || [ ! -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]; then
    echo "❌ LLVM开发库未找到"
    echo "正在安装LLVM..."
    brew install llvm
    LLVM_PREFIX=$(brew --prefix llvm)
else
    echo "✓ LLVM已安装: $LLVM_PREFIX"
fi

# 进入项目目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo ""
echo "项目目录: $SCRIPT_DIR"
echo ""

# 创建build目录
echo "创建build目录..."
mkdir -p build
cd build

# 配置CMake
echo "正在配置CMake..."
echo "LLVM路径: $LLVM_PREFIX"

# 清理之前的配置（如果有）
if [ -f "CMakeCache.txt" ]; then
    echo "清理之前的配置..."
    rm -rf CMakeFiles CMakeCache.txt _deps
fi

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$LLVM_PREFIX/lib/cmake/llvm \
  -DClang_DIR=$LLVM_PREFIX/lib/cmake/clang

if [ $? -ne 0 ]; then
    echo "❌ CMake配置失败"
    echo "尝试自动查找LLVM..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

# 检查配置是否成功
if [ ! -f "CMakeCache.txt" ]; then
    echo "❌ CMake配置失败，请检查错误信息"
    exit 1
fi

echo "✓ CMake配置成功"

# 编译
echo ""
echo "正在编译（使用 $(sysctl -n hw.ncpu) 个CPU核心）..."
make -j$(sysctl -n hw.ncpu)

# 验证
echo ""
if [ -f "ios-obfuscator" ]; then
    echo "✓ 编译成功！"
    echo ""
    echo "可执行文件位置: $(pwd)/ios-obfuscator"
    echo ""
    echo "测试运行："
    ./ios-obfuscator --help | head -20
    echo ""
    echo "使用方法："
    echo "  cd build"
    echo "  ./ios-obfuscator --config=../config/config.json --input=../SDK --output=../obfuscated"
else
    echo "❌ 编译失败，请检查错误信息"
    exit 1
fi

