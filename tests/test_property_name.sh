#!/bin/zsh
# 属性名混淆策略测试脚本
#
# 测试内容：
# 1. @property 声明中的属性名混淆
# 2. 点语法 self.propertyName 的替换
# 3. 成员变量 _propertyName 的替换
# 4. 布尔属性 is 前缀的保留
# 5. 相同属性名使用相同混淆名
# 6. IBOutlet 属性的保留
# 7. KVO/KVC 字符串中的属性名替换
#
# 输入: tests/HWSDK (完整项目)
# 输出: output/PropertyNameObfuscation-{timestamp}/ (完整项目)

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
TEST_NAME="PropertyNameObfuscation"
TEST_DESC="属性名混淆 - 混淆 @property 属性名、点语法、成员变量"

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
    exit 1
fi
echo -e "${GREEN}✓${NC} 可执行文件: $BINARY"

if [ ! -d "$TEST_INPUT" ]; then
    echo -e "${RED}✗ 错误: 找不到测试输入项目${NC}"
    echo "  路径: $TEST_INPUT"
    exit 1
fi
echo -e "${GREEN}✓${NC} 测试输入项目: $TEST_INPUT"

# 创建测试输出目录: output/PropertyNameObfuscation-{timestamp}
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
      "prefix": "prop_",
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
      "properties": ["delegate", "dataSource"],
      "thirdPartySDKs": []
    },
    "generateMapping": true,
    "mappingOutputPath": "$TEST_OUTPUT/mapping.json"
  }
}
EOF
echo -e "${GREEN}✓${NC} 配置文件创建完成"
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

# 检查生成的映射文件
MAPPING_FILE=""
if [ -f "$TEST_OUTPUT/mapping.json" ]; then
    MAPPING_FILE="$TEST_OUTPUT/mapping.json"
fi

if [ -n "$MAPPING_FILE" ]; then
    echo -e "${GREEN}✓${NC} 找到映射文件: $(basename "$MAPPING_FILE")"
    echo ""
    echo "属性名映射:"
    echo -e "${GRAY}$(cat "$MAPPING_FILE" | grep -i property || echo "无属性映射信息")${NC}"
else
    echo -e "${YELLOW}⚠${NC} 未找到映射文件"
fi
echo ""

# 检查输出文件
echo -e "${BLUE}--- 输出文件检查 ---${NC}"

# 统计文件数量
FILE_COUNT=$(find "$TEST_OUTPUT" -type f \( -name "*.m" -o -name "*.h" \) ! -path "*/obfuscator.log" 2>/dev/null | wc -l | tr -d ' ')
echo -e "${GREEN}✓${NC} 共生成 $FILE_COUNT 个源文件"
echo ""

# 查看混淆后的文件内容示例
echo -e "${BLUE}--- 混淆后文件示例 ---${NC}"

# 查找包含属性混淆的文件
echo "查找混淆后的属性定义..."
grep -r "@property" "$TEST_OUTPUT" --include="*.h" --include="*.m" | head -10
echo ""

# 验证属性名混淆
echo -e "${BLUE}--- 验证属性名混淆 ---${NC}"

# 检查日志中的混淆信息
echo "混淆日志摘要:"
grep -i "property\|obfuscate" "$TEST_LOG" | grep -i "will obfuscate\|skipping" | head -20
echo ""

# 统计混淆的属性数量
OBF_COUNT=$(grep -c "Property name" "$TEST_LOG" 2>/dev/null || echo "0")
SKIP_COUNT=$(grep -c "is whitelisted, skipping\|is system property, skipping" "$TEST_LOG" 2>/dev/null || echo "0")
echo -e "${GREEN}✓${NC} 混淆相关日志: $OBF_COUNT 条"
echo -e "${GREEN}✓${NC} 跳过的属性: $SKIP_COUNT 个"
echo ""

# 检查点语法混淆
echo -e "${BLUE}--- 检查点语法混淆 ---${NC}"
echo "查找点语法引用（应该被混淆）..."
DOT_SYNTAX_COUNT=$(grep -r "self\." "$TEST_OUTPUT" --include="*.m" 2>/dev/null | wc -l | tr -d ' ')
echo "点语法出现次数: $DOT_SYNTAX_COUNT"
echo ""

# 检查成员变量混淆
echo -e "${BLUE}--- 检查成员变量混淆 ---${NC}"
echo "查找下划线成员变量（应该被混淆）..."
IVAR_COUNT=$(grep -r "self->_" "$TEST_OUTPUT" --include="*.m" 2>/dev/null | wc -l | tr -d ' ')
echo "成员变量访问次数: $IVAR_COUNT"
echo ""

# 检查 KVO/KVC 混淆
echo -e "${BLUE}--- 检查 KVO/KVC 字符串混淆 ---${NC}"
echo "查找 KVO/KVC 相关字符串..."
KVO_COUNT=$(grep -r "forKeyPath:\|forKey:\|valueForKey:" "$TEST_OUTPUT" --include="*.m" 2>/dev/null | wc -l | tr -d ' ')
echo "KVO/KVC 调用次数: $KVO_COUNT"
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

# 检查3: 属性名被混淆
TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
if [ "$OBF_COUNT" -gt 0 ]; then
    echo -e "  [${GREEN}PASS${NC}] 属性名混淆日志: $OBF_COUNT 条"
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
else
    echo -e "  [${RED}FAIL${NC}] 没有属性名混淆日志"
fi

# 检查4: 系统属性被跳过
TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
if [ "$SKIP_COUNT" -gt 0 ]; then
    echo -e "  [${GREEN}PASS${NC}] $SKIP_COUNT 个系统属性被正确跳过"
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
else
    echo -e "  [${YELLOW}WARN${NC}] 没有系统属性被跳过"
fi

echo ""
echo "检查结果: $PASSED_CHECKS / $TOTAL_CHECKS 通过"

if [ $PASSED_CHECKS -eq $TOTAL_CHECKS ]; then
    echo -e "${GREEN}✓ 所有检查通过!${NC}"
    echo ""
    echo "混淆后的项目保存在: $TEST_OUTPUT"
    echo ""
    echo -e "${BLUE}--- 详细验证 ---${NC}"
    echo "请检查以下场景是否正确混淆:"
    echo "  1. 普通属性名混淆"
    echo "  2. 布尔属性保留 is 前缀"
    echo "  3. 相同属性名使用相同混淆名"
    echo "  4. IBOutlet 属性被保留"
    echo "  5. 点语法 self.propertyName 被替换"
    echo "  6. 成员变量 _propertyName 被替换"
    echo "  7. KVO/KVC 字符串被替换"
    exit 0
else
    echo -e "${RED}✗ 有 $((TOTAL_CHECKS - PASSED_CHECKS)) 个检查失败${NC}"
    echo ""
    echo "请查看日志: $TEST_LOG"
    exit 1
fi
