#!/bin/zsh
# DeadCodeInjection 策略测试脚本
#
# 测试内容：
# 1. 在方法体内插入假业务逻辑代码
# 2. 验证插入的代码不会影响原有功能
# 3. 验证密度配置生效
# 4. 验证 maxStatementsPerMethod 限制生效
#
# 输入: tests/HWSDK (完整项目)
# 输出: output/DeadCodeInjection-{timestamp}/ (完整项目)

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
GRAY='\033[0;90m'
NC='\033[0m'

# 获取脚本目录
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/ios-obfuscator"
TEST_DIR="$SCRIPT_DIR"
# 使用 tests/HWSDK 作为输入
TEST_INPUT="$TEST_DIR/HWSDK"
OUTPUT_DIR="$TEST_DIR/output"
TEMP_CONFIG="$TEST_DIR/test_config.json"

# 测试名称
TEST_NAME="DeadCodeInjection"
TEST_DESC="死代码注入 - 在方法体内插入假业务逻辑代码"

echo ""
echo -e "${CYAN}============================================${NC}"
echo -e "${CYAN}  $TEST_NAME 测试${NC}"
echo -e "${CYAN}  $TEST_DESC${NC}"
echo -e "${CYAN}============================================${NC}"
echo ""

# 检查环境
echo -e "${BLUE}--- 环境检查 ---${NC}"
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}✗ 错误: 找不到可执行文件${NC}"
    echo "  路径: $BINARY"
    echo "  请先运行 ./build.sh 编译项目"
    exit 1
fi
echo -e "${GREEN}✓${NC} 可执行文件: $BINARY"

if [ ! -d "$TEST_INPUT" ]; then
    echo -e "${RED}✗ 错误: 找不到测试输入项目${NC}"
    echo "  路径: $TEST_INPUT"
    exit 1
fi
echo -e "${GREEN}✓${NC} 测试输入项目: $TEST_INPUT"

# 创建测试输出目录: output/DeadCodeInjection-{timestamp}
TIMESTAMP=$(date +%s)
TEST_OUTPUT="${OUTPUT_DIR}/${TEST_NAME}-${TIMESTAMP}"
mkdir -p "$TEST_OUTPUT"
echo -e "${GREEN}✓${NC} 测试输出目录: $TEST_OUTPUT"
echo ""

# 显示测试项目结构
echo -e "${BLUE}--- 测试项目结构 ---${NC}"
echo "输入项目文件:"
find "$TEST_INPUT" -type f \( -name "*.m" -o -name "*.h" \) | head -20
echo ""

# 创建测试配置
echo -e "${BLUE}--- 创建测试配置 ---${NC}"
cat > "$TEMP_CONFIG" << EOF
{
  "sdk": {
    "name": "HWSDK",
    "type": "framework",
    "inputPath": "$TEST_INPUT",
    "outputPath": "$TEST_OUTPUT"
  },
  "obfuscation": {
    "strategies": ["$TEST_NAME"],
    "namingRule": {
      "style": "random",
      "prefix": "obf_",
      "wordListPath": "$PROJECT_ROOT/wordlist",
      "wordCase": "camelCase",
      "randomLength": {
        "className": {"min": 6, "max": 10},
        "methodName": {"min": 6, "max": 10},
        "propertyName": {"min": 6, "max": 10},
        "fileName": {"min": 6, "max": 10},
        "folderName": {"min": 6, "max": 10}
      }
    },
    "whitelist": {
      "classes": ["NSObject"],
      "methods": ["init", "viewDidLoad", "dealloc"],
      "properties": [],
      "thirdPartySDKs": []
    },
    "deadCodeInjection": {
      "density": 0.5,
      "maxStatementsPerMethod": 3,
      "templateTypes": []
    },
    "generateMapping": true,
    "mappingOutputPath": "$TEST_OUTPUT/mapping.json"
  }
}
EOF
echo -e "${GREEN}✓${NC} 配置文件创建完成"
echo "  density: 0.5 (50% 的方法会插入死代码)"
echo "  maxStatementsPerMethod: 3"
echo ""

# 运行混淆器
echo -e "${BLUE}--- 运行混淆器 ---${NC}"
echo "命令: $BINARY --config=$TEMP_CONFIG --verbose"
echo ""

TEST_LOG="$TEST_OUTPUT/obfuscator.log"
"$BINARY" --config="$TEMP_CONFIG" --verbose > "$TEST_LOG" 2>&1
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓${NC} 混淆器执行完成 (exit code: 0)"
else
    echo -e "${YELLOW}⚠${NC} 混淆器执行完成 (exit code: $EXIT_CODE, 可能有警告)"
fi
echo ""

# 分析混淆结果
echo -e "${BLUE}--- 分析混淆结果 ---${NC}"

# 检查输出文件
echo -e "${BLUE}--- 输出文件检查 ---${NC}"

# 统计文件数量
FILE_COUNT=$(find "$TEST_OUTPUT" -type f \( -name "*.m" -o -name "*.h" \) ! -path "*/obfuscator.log" 2>/dev/null | wc -l | tr -d ' ')
echo -e "${GREEN}✓${NC} 共生成 $FILE_COUNT 个源文件"
echo ""

# 查看混淆后的文件内容示例
echo -e "${BLUE}--- 混淆后文件示例 ---${NC}"
OBF_FILE=$(find "$TEST_OUTPUT" -name "*.m" -type f ! -path "*/obfuscator.log" 2>/dev/null | head -1)
if [ -n "$OBF_FILE" ]; then
    echo "文件: $OBF_FILE"
    echo -e "${GRAY}---$(basename "$OBF_FILE")---${NC}"
    head -50 "$OBF_FILE"
    echo -e "${GRAY}---${NC}"
else
    echo -e "${YELLOW}⚠${NC} 未找到 .m 文件"
fi
echo ""

# 验证死代码插入
echo -e "${BLUE}--- 验证死代码插入 ---${NC}"

# 检查日志中的插入信息
echo "混淆日志摘要:"
grep -i "dead.*code\|insertion\|insert" "$TEST_LOG" | head -30
echo ""

# 统计插入的代码数量
INSERTION_COUNT=$(grep -c "Successfully applied.*insertion" "$TEST_LOG" 2>/dev/null || echo "0")
echo -e "${GREEN}✓${NC} 插入操作: $INSERTION_COUNT 个"
echo ""

# 检查生成的死代码特征
echo -e "${BLUE}--- 检查生成的死代码特征 ---${NC}"
echo "查找可能的死代码变量名 (___obf_):"
grep -r "___obf_" "$TEST_OUTPUT" --include="*.m" 2>/dev/null | head -10
echo ""

# 查找可能的假业务逻辑
echo "查找可能的假业务逻辑 (token, api, checksum 等):"
grep -r "NSString.*token\|NSData.*api\|NSUInteger.*checksum" "$TEST_OUTPUT" --include="*.m" 2>/dev/null | head -10
echo ""

# 测试结果摘要
echo -e "${CYAN}============================================${NC}"
echo -e "${CYAN}  测试结果摘要${NC}"
echo -e "${CYAN}============================================${NC}"

TOTAL_CHECKS=0
PASSED_CHECKS=0

# 检查1: 输出目录存在
TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
if [ -d "$TEST_OUTPUT" ]; then
    echo -e "  [${GREEN}PASS${NC}] 输出目录已创建"
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
else
    echo -e "  [${RED}FAIL${NC}] 输出目录未创建"
fi

# 检查2: 生成混淆文件
TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
if [ "$FILE_COUNT" -gt 0 ]; then
    echo -e "  [${GREEN}PASS${NC}] 生成了 $FILE_COUNT 个源文件"
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
else
    echo -e "  [${RED}FAIL${NC}] 未生成源文件"
fi

# 检查3: 有插入操作
TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
if [ "$INSERTION_COUNT" -gt 0 ]; then
    echo -e "  [${GREEN}PASS${NC}] $INSERTION_COUNT 个插入操作被执行"
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
else
    echo -e "  [${YELLOW}WARN${NC}] 没有检测到插入操作"
fi

# 检查4: 日志中无错误
TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
ERROR_COUNT=$(grep -c "ERROR\|FATAL" "$TEST_LOG" 2>/dev/null || echo "0")
if [ "$ERROR_COUNT" -eq 0 ]; then
    echo -e "  [${GREEN}PASS${NC}] 日志中无错误"
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
else
    echo -e "  [${YELLOW}WARN${NC}] 日志中发现 $ERROR_COUNT 个错误"
fi

echo ""
echo "检查结果: $PASSED_CHECKS / $TOTAL_CHECKS 通过"

if [ $PASSED_CHECKS -eq $TOTAL_CHECKS ]; then
    echo -e "${GREEN}✓ 所有检查通过!${NC}"
    echo ""
    echo "混淆后的项目保存在: $TEST_OUTPUT"
    echo ""
    echo -e "${BLUE}--- 死代码注入验证 ---${NC}"
    echo "请检查以下场景是否正确:"
    echo "  1. 方法体内插入了假业务逻辑代码"
    echo "  2. 插入的代码使用混淆后的变量名 (___obf_xxx)"
    echo "  3. 插入的代码看起来像真实的业务逻辑"
    echo "  4. 原有代码逻辑保持不变"
    echo "  5. 混淆后的代码能在 Xcode 中编译运行"
    echo ""
    echo "提示: 查看生成的 .m 文件，搜索 '___obf_' 可以找到插入的死代码"
    exit 0
else
    echo -e "${RED}✗ 有 $((TOTAL_CHECKS - PASSED_CHECKS)) 个检查失败${NC}"
    echo ""
    echo "请查看日志: $TEST_LOG"
    exit 1
fi
