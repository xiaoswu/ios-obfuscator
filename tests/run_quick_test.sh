#!/bin/bash
# 快速测试脚本 - 仅验证基本功能和AST解析
# 用于在开发过程中快速验证核心功能

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/ios-obfuscator"
TEMP_DIR="/tmp/ios_obfuscator_quick_test"
CONFIG_FILE="$TEMP_DIR/config.json"

echo -e "${BLUE}--- iOS Obfuscator 快速测试 ---${NC}"
echo ""

# 检查二进制文件
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}✗ 错误: 找不到可执行文件 $BINARY${NC}"
    echo "请先运行 ./build.sh 编译项目"
    exit 1
fi
echo -e "${GREEN}✓${NC} 可执行文件存在"

# 创建临时测试目录
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/input"
mkdir -p "$TEMP_DIR/output"

# 创建测试文件
cat > "$TEMP_DIR/input/test.m" << 'EOF'
#import <Foundation/Foundation.h>

@interface QSTestClass : NSObject
@property (nonatomic, strong) NSString *name;
- (void)testMethod;
@end

@implementation QSTestClass
- (void)testMethod {
    NSLog(@"Test method called");
}
@end
EOF

# 创建配置
cat > "$CONFIG_FILE" << EOF
{
  "sdk": {
    "name": "QuickTestSDK",
    "type": "framework",
    "inputPath": "$TEMP_DIR/input",
    "outputPath": "$TEMP_DIR/output"
  },
  "obfuscation": {
    "strategies": ["ClassNameObfuscation"],
    "namingRule": {
      "style": "random",
      "prefix": "QS_"
    },
    "whitelist": {
      "classes": ["NSObject"],
      "methods": [],
      "properties": [],
      "thirdPartySDKs": []
    },
    "generateMapping": false
  }
}
EOF

# 运行测试
echo ""
echo -e "${BLUE}→${NC} 运行混淆器..."
if "$BINARY" --config="$CONFIG_FILE" > "$TEMP_DIR/log.txt" 2>&1; then
    echo -e "${GREEN}✓${NC} 混淆器执行成功"

    # 检查输出
    if [ -d "$TEMP_DIR/output" ]; then
        echo -e "${GREEN}✓${NC} 输出目录已创建"

        # 检查是否有输出文件
        if find "$TEMP_DIR/output" -name "*.m" -o -name "*.h" | grep -q .; then
            echo -e "${GREEN}✓${NC} 输出文件已生成"
        else
            echo -e "${YELLOW}⚠${NC} 未找到输出文件"
        fi
    else
        echo -e "${RED}✗${NC} 输出目录未创建"
        cat "$TEMP_DIR/log.txt"
        exit 1
    fi
else
    echo -e "${RED}✗${NC} 混淆器执行失败"
    cat "$TEMP_DIR/log.txt"
    exit 1
fi

# 清理
rm -rf "$TEMP_DIR"

echo ""
echo -e "${GREEN}✓ 快速测试通过!${NC}"
exit 0
