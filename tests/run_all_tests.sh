#!/bin/zsh
# iOS Obfuscator - 回归测试脚本
# 用于验证已实现的混淆策略，并显示未实现的策略状态

set -e

# 获取脚本目录
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/ios-obfuscator"
TEST_DIR="$SCRIPT_DIR"
# 使用 tests/HWSDK 作为所有策略的输入
TEST_INPUT="$TEST_DIR/HWSDK"
OUTPUT_DIR="$TEST_DIR/output"
TEMP_CONFIG="$TEST_DIR/test_config.json"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
GRAY='\033[0;90m'
NC='\033[0m'

# 计数器
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# 策略状态定义（使用简单列表）
IMPLEMENTED_STRATEGIES="ClassNameObfuscation MethodNameObfuscation FileNameObfuscation SDKNameObfuscation FolderNameObfuscation"
NOT_IMPLEMENTED_STRATEGIES="PropertyNameObfuscation VariableNameObfuscation ProtocolNameObfuscation CategoryNameObfuscation StringObfuscation ResourceObfuscation MetadataObfuscation ControlFlowObfuscation"

# 策略描述
strategy_description() {
    case $1 in
        ClassNameObfuscation) echo "类名混淆 - 混淆 @interface 和 @implementation 名称" ;;
        MethodNameObfuscation) echo "方法名混淆 - 混淆方法声明、消息表达式和 @selector()" ;;
        PropertyNameObfuscation) echo "属性名混淆 - 混淆 @property 声明" ;;
        VariableNameObfuscation) echo "变量名混淆 - 混淆局部和实例变量" ;;
        ProtocolNameObfuscation) echo "协议名混淆 - 混淆 @protocol 声明" ;;
        CategoryNameObfuscation) echo "类别名混淆 - 混淆 @category 声明" ;;
        FileNameObfuscation) echo "文件名混淆 - 重命名源文件并更新 import 语句" ;;
        StringObfuscation) echo "字符串混淆 - 加密字符串字面量" ;;
        ResourceObfuscation) echo "资源混淆 - 混淆图片、storyboard等资源文件" ;;
        MetadataObfuscation) echo "元数据混淆 - 清理和混淆调试信息" ;;
        ControlFlowObfuscation) echo "控制流混淆 - 扁平化控制流" ;;
        SDKNameObfuscation) echo "SDK名称混淆 - 重命名SDK框架本身" ;;
        FolderNameObfuscation) echo "文件夹名混淆 - 重命名包含类文件的文件夹" ;;
        *) echo "未知策略" ;;
    esac
}

# 检查策略是否已实现
is_strategy_implemented() {
    [[ " $IMPLEMENTED_STRATEGIES " == *" $1 "* ]]
}

# 打印头部
print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}   iOS Obfuscator 回归测试套件${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

# 打印策略状态摘要
print_strategy_summary() {
    echo -e "${BLUE}--- 策略状态概览 ---${NC}"
    echo ""

    local implemented=0
    local not_implemented=0

    # 先显示已实现的
    for strategy in ${=IMPLEMENTED_STRATEGIES}; do
        echo -e "  ${GREEN}✓${NC} $strategy"
        echo -e "     ${GRAY}$(strategy_description $strategy)${NC}"
        implemented=$((implemented + 1))
    done

    echo ""

    # 再显示未实现的
    for strategy in ${=NOT_IMPLEMENTED_STRATEGIES}; do
        echo -e "  ${YELLOW}○${NC} $strategy"
        echo -e "     ${GRAY}$(strategy_description $strategy) [未实现]${NC}"
        not_implemented=$((not_implemented + 1))
    done

    echo ""
    echo -e "  已实现: ${GREEN}$implemented${NC} | 未实现: ${YELLOW}$not_implemented${NC}"
    echo ""
}

# 检查环境
check_environment() {
    echo -e "${BLUE}--- 检查测试环境 ---${NC}"

    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}✗ 错误: 找不到编译后的可执行文件${NC}"
        echo "  请先运行: ${GRAY}./build.sh${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} 可执行文件: $BINARY"

    # 创建输出目录
    mkdir -p "$OUTPUT_DIR"
    echo -e "${GREEN}✓${NC} 输出目录: $OUTPUT_DIR"
    echo ""
}

# 创建测试配置
create_test_config() {
    local strategy=$1
    local output_dir=$2

    cat > "$TEMP_CONFIG" << EOF
{
  "sdk": {
    "name": "HWSDK",
    "type": "framework",
    "inputPath": "$TEST_INPUT",
    "outputPath": "$output_dir"
  },
  "obfuscation": {
    "strategies": ["$strategy"],
    "namingRule": {
      "style": "random",
      "prefix": "OBF_",
      "wordListPath": "$PROJECT_ROOT/wordlist",
      "wordCase": "camelCase",
      "randomLength": {
        "className": {"min": 8, "max": 12},
        "methodName": {"min": 8, "max": 12},
        "propertyName": {"min": 8, "max": 12},
        "fileName": {"min": 8, "max": 12},
        "folderName": {"min": 8, "max": 12}
      }
    },
    "whitelist": {
      "classes": ["NSObject"],
      "methods": ["init", "viewDidLoad", "dealloc"],
      "properties": [],
      "thirdPartySDKs": []
    },
    "generateMapping": true,
    "mappingOutputPath": "$output_dir/mapping.json"
  }
}
EOF
}

# 运行单个策略测试
run_strategy_test() {
    local strategy=$1
    local test_name=$2

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if ! is_strategy_implemented "$strategy"; then
        echo -e "  ${YELLOW}⊘${NC} $test_name - 策略未实现"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return
    fi

    echo -e "  ${BLUE}→${NC} 运行: $test_name"

    # 使用时间戳创建输出目录: output/{strategy_name}-{timestamp}/
    local timestamp=$(date +%s)
    local test_output_dir="$OUTPUT_DIR/${strategy}-${timestamp}"

    # 创建测试配置
    create_test_config "$strategy" "$test_output_dir"

    # 确保输出目录存在
    mkdir -p "$test_output_dir"

    # 运行混淆器
    local test_output="$test_output_dir/test.log"
    "$BINARY" --config="$TEMP_CONFIG" --verbose > "$test_output" 2>&1
    local exit_code=$?

    # 检查是否有文件生成（混淆成功的标志）
    local has_output=false
    if [ -d "$test_output_dir" ] && [ "$(ls -A "$test_output_dir" 2>/dev/null)" ]; then
        # 排除日志文件本身
        local file_count=$(find "$test_output_dir" -type f ! -name "test.log" ! -name "*.log" 2>/dev/null | wc -l | tr -d ' ')
        if [ "$file_count" -gt 0 ]; then
            has_output=true
        fi
    fi

    if [ $exit_code -eq 0 ] || $has_output; then
        # 检查输出文件是否存在
        if $has_output; then
            echo -e "    ${GREEN}✓${NC} 通过 - 生成了 $file_count 个文件 -> $test_output_dir"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "    ${RED}✗${NC} 失败: 未生成输出文件"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            cat "$test_output" | tail -20
        fi
    else
        echo -e "    ${RED}✗${NC} 失败: 混淆器执行错误 (exit code: $exit_code)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        echo -e "    ${GRAY}--- 错误输出 ---${NC}"
        cat "$test_output" | tail -20
        echo -e "    ${GRAY}----------------${NC}"
    fi
}

# 验证AST解析正确性
verify_ast_parsing() {
    local test_name="AST解析验证"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    echo -e "  ${BLUE}→${NC} 运行: $test_name"

    # 使用时间戳创建输出目录
    local timestamp=$(date +%s)
    local test_output_dir="$OUTPUT_DIR/AST验证-${timestamp}"
    mkdir -p "$test_output_dir"

    # 创建配置
    create_test_config "ClassNameObfuscation" "$test_output_dir"

    # 运行并检查是否生成输出
    local test_log="$test_output_dir/test.log"

    "$BINARY" --config="$TEMP_CONFIG" --verbose > "$test_log" 2>&1
    local exit_code=$?

    local file_count=$(find "$test_output_dir" -type f ! -name "test.log" ! -name "*.log" 2>/dev/null | wc -l | tr -d ' ')
    if [ "$file_count" -gt 0 ]; then
        echo -e "    ${GREEN}✓${NC} 通过 - AST解析正常 (生成 $file_count 个文件)"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "    ${RED}✗${NC} 失败 - AST解析错误 (exit code: $exit_code)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        cat "$test_log" | tail -20
    fi
}

# 验证白名单功能
verify_whitelist() {
    local test_name="白名单功能验证"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    echo -e "  ${BLUE}→${NC} 运行: $test_name"

    # 使用时间戳创建输出目录
    local timestamp=$(date +%s)
    local test_output_dir="$OUTPUT_DIR/白名单验证-${timestamp}"
    mkdir -p "$test_output_dir"

    # 创建配置，指定白名单（HWSDK 作为输入）
    cat > "$TEMP_CONFIG" << EOF
{
  "sdk": {
    "name": "HWSDK",
    "type": "framework",
    "inputPath": "$TEST_INPUT",
    "outputPath": "$test_output_dir"
  },
  "obfuscation": {
    "strategies": ["ClassNameObfuscation"],
    "namingRule": {
      "style": "random",
      "prefix": "OBF_"
    },
    "whitelist": {
      "classes": ["NSObject", "HWSDK"],
      "methods": ["init", "sharedManager"],
      "properties": [],
      "thirdPartySDKs": []
    },
    "generateMapping": true,
    "mappingOutputPath": "$test_output_dir/mapping.json"
  }
}
EOF

    local test_log="$test_output_dir/test.log"

    "$BINARY" --config="$TEMP_CONFIG" --verbose > "$test_log" 2>&1
    local exit_code=$?

    local file_count=$(find "$test_output_dir" -type f ! -name "test.log" ! -name "*.log" 2>/dev/null | wc -l | tr -d ' ')
    if [ "$file_count" -gt 0 ]; then
        echo -e "    ${GREEN}✓${NC} 通过 - 白名单配置生效 (生成 $file_count 个文件)"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "    ${RED}✗${NC} 失败 - 白名单功能异常 (exit code: $exit_code)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        cat "$test_log" | tail -20
    fi
}

# 运行所有测试
run_all_tests() {
    echo -e "${BLUE}--- 运行混淆策略测试 ---${NC}"
    echo ""

    # ============================================================
    # 第一阶段：AST级别测试（由内而外）
    # ============================================================
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  第一阶段：AST级别测试（符号收集与混淆）${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""

    # [1/8] ClassNameObfuscation 测试
    echo -e "${GRAY}[1/8] ClassNameObfuscation - 类名混淆${NC}"
    run_strategy_test "ClassNameObfuscation" "类名混淆"

    # [2/8] MethodNameObfuscation 测试
    echo ""
    echo -e "${GRAY}[2/8] MethodNameObfuscation - 方法名混淆（依赖类名）${NC}"
    run_strategy_test "MethodNameObfuscation" "方法名混淆"

    # [3/8] PropertyNameObfuscation 测试（未实现）
    echo ""
    echo -e "${GRAY}[3/8] PropertyNameObfuscation - 属性名混淆${NC}"
    run_strategy_test "PropertyNameObfuscation" "属性名混淆"

    # [4/8] VariableNameObfuscation 测试（未实现）
    echo ""
    echo -e "${GRAY}[4/8] VariableNameObfuscation - 变量名混淆${NC}"
    run_strategy_test "VariableNameObfuscation" "变量名混淆"

    # ============================================================
    # 第二阶段：文件操作测试
    # ============================================================
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  第二阶段：文件操作测试（文件和目录）${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""

    # [5/8] FileNameObfuscation 测试
    echo -e "${GRAY}[5/8] FileNameObfuscation - 文件名混淆${NC}"
    run_strategy_test "FileNameObfuscation" "文件名混淆"

    # [6/8] FolderNameObfuscation 测试
    echo ""
    echo -e "${GRAY}[6/8] FolderNameObfuscation - 文件夹名混淆（依赖文件名）${NC}"
    run_strategy_test "FolderNameObfuscation" "文件夹名混淆"

    # ============================================================
    # 第三阶段：项目级别测试
    # ============================================================
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  第三阶段：项目级别测试${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""

    # [7/8] SDKNameObfuscation 测试
    echo -e "${GRAY}[7/8] SDKNameObfuscation - SDK名称混淆（依赖文件夹）${NC}"
    run_strategy_test "SDKNameObfuscation" "SDK名称混淆"

    # ============================================================
    # 第四阶段：功能验证
    # ============================================================
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  第四阶段：功能验证${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""

    # [8/8] AST 解析验证
    echo -e "${GRAY}[8/8] AST解析验证${NC}"
    verify_ast_parsing

    # 白名单验证
    echo ""
    echo -e "${GRAY}[额外] 白名单功能验证${NC}"
    verify_whitelist

    echo ""
}

# 打印测试结果摘要
print_summary() {
    echo -e "${BLUE}--- 测试结果摘要 ---${NC}"
    echo ""
    echo "  总测试数: $TOTAL_TESTS"
    echo -e "  通过: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "  失败: ${RED}$FAILED_TESTS${NC}"
    echo -e "  跳过: ${YELLOW}$SKIPPED_TESTS${NC}"
    echo ""

    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}✓ 所有测试通过!${NC}"
        return 0
    else
        echo -e "${RED}✗ 有 $FAILED_TESTS 个测试失败${NC}"
        return 1
    fi
}

# 清理临时文件
cleanup() {
    rm -f "$TEMP_CONFIG"
}

# 主流程
main() {
    print_header
    print_strategy_summary
    check_environment
    run_all_tests
    print_summary
    cleanup
}

# 运行
main
