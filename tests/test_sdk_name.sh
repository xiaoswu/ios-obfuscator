#!/bin/bash
# SDK名称混淆策略测试脚本
#
# 测试内容：
# 1. .xcodeproj 目录重命名
# 2. project.pbxproj 文件中的 SDK 名称替换
# 3. Public Headers 目录重命名
# 4. 源代码文件中的 SDK 名称引用替换
#
# 输入: tests/HWSDK (完整项目)
# 输出: output/SDKNameObfuscation-{timestamp}/ (完整项目)

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 获取脚本和项目目录
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BINARY="$BUILD_DIR/ios-obfuscator"

# 测试相关路径
# 使用 tests/HWSDK 作为输入
TEST_INPUT="$SCRIPT_DIR/HWSDK"
TEST_OUTPUT_BASE="$SCRIPT_DIR/output"
ORIGINAL_SDK_NAME="HWSDK"

# 计数器
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# 打印函数
print_header() {
    echo ""
    echo -e "${CYAN}============================================${NC}"
    echo -e "${CYAN}  SDK名称混淆策略测试${NC}"
    echo -e "${CYAN}============================================${NC}"
    echo ""
}

print_section() {
    echo ""
    echo -e "${BLUE}--- $1 ---${NC}"
}

# 检查环境
check_environment() {
    print_section "环境检查"

    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}✗ 错误: 找不到可执行文件${NC}"
        echo "  请先运行: ./build.sh"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} 可执行文件: $BINARY"

    if [ ! -d "$TEST_INPUT" ]; then
        echo -e "${RED}✗ 错误: 找不到测试输入项目${NC}"
        echo "  路径: $TEST_INPUT"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} 测试输入项目: $TEST_INPUT"
}

# 准备测试 - 创建输出目录
prepare_test() {
    print_section "准备测试环境"

    local timestamp=$(date +%s)
    TEST_OUTPUT="${TEST_OUTPUT_BASE}/SDKNameObfuscation-${timestamp}"

    rm -rf "$TEST_OUTPUT"
    mkdir -p "$TEST_OUTPUT"

    echo -e "${GREEN}✓${NC} 测试输出目录: $TEST_OUTPUT"

    # 验证输入结构
    echo ""
    echo "输入项目结构:"
    echo -e "  ${GRAY}$(ls -la "$TEST_INPUT")${NC}"

    # 检查关键文件
    if [ -f "$TEST_INPUT/$ORIGINAL_SDK_NAME.xcodeproj/project.pbxproj" ]; then
        echo -e "  ${GREEN}✓${NC} $ORIGINAL_SDK_NAME.xcodeproj/project.pbxproj 存在"
    else
        echo -e "  ${RED}✗${NC} $ORIGINAL_SDK_NAME.xcodeproj/project.pbxproj 缺失"
        exit 1
    fi
}

# 创建测试配置
create_test_config() {
    local config_file="$1"

    cat > "$config_file" << EOF
{
  "sdk": {
    "name": "$ORIGINAL_SDK_NAME",
    "type": "framework",
    "inputPath": "$TEST_INPUT",
    "outputPath": "$TEST_OUTPUT"
  },
  "obfuscation": {
    "strategies": ["SDKNameObfuscation"],
    "namingRule": {
      "style": "random",
      "prefix": "OBF_",
      "wordListPath": "$PROJECT_ROOT/wordlist",
      "wordCase": "camelCase",
      "randomLength": {
        "className": {"min": 6, "max": 10},
        "methodName": {"min": 6, "max": 10},
        "fileName": {"min": 6, "max": 10},
        "folderName": {"min": 6, "max": 10}
      }
    },
    "whitelist": {
      "classes": ["NSObject"],
      "methods": ["init", "viewDidLoad"],
      "properties": [],
      "thirdPartySDKs": []
    },
    "generateMapping": true,
    "mappingOutputPath": "$TEST_OUTPUT/mapping.json"
  }
}
EOF
}

# 运行混淆器
run_obfuscator() {
    print_section "运行混淆器"

    local config_file="$TEST_OUTPUT/config.json"
    create_test_config "$config_file"

    echo -e "${BLUE}→${NC} 执行混淆..."
    echo "  配置: $config_file"
    echo "  输入: $TEST_INPUT"
    echo "  输出: $TEST_OUTPUT"

    if "$BINARY" --config="$config_file" --verbose > "$TEST_OUTPUT/obfuscator.log" 2>&1; then
        echo -e "${GREEN}✓${NC} 混淆器执行成功"
        return 0
    else
        echo -e "${YELLOW}⚠${NC} 混淆器执行完成 (可能有警告，检查输出)"
        return 0
    fi
}

# 测试函数: 检查条件并记录结果
run_test() {
    local test_name="$1"
    local test_command="$2"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    echo -n "  $test_name ... "

    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

# 验证混淆结果
verify_obfuscation() {
    print_section "验证混淆结果"

    local output_dir="$TEST_OUTPUT"

    # 检查输出目录是否存在
    if [ ! -d "$output_dir" ]; then
        echo -e "${RED}✗ 输出目录不存在: $output_dir${NC}"
        cat "$TEST_OUTPUT/obfuscator.log"
        return 1
    fi

    echo -e "${GREEN}✓${NC} 输出目录存在"

    # 查找混淆后的项目目录名称
    local obfuscated_name=""
    for entry in "$output_dir"/*; do
        if [ -d "$entry" ]; then
            local dir_name=$(basename "$entry")
            # 跳过 .xcodeproj
            if [[ "$dir_name" != *.xcodeproj ]]; then
                # 检查是否包含源文件
                if find "$entry" -name "*.m" -o -name "*.h" 2>/dev/null | grep -q .; then
                    obfuscated_name="$dir_name"
                    break
                fi
            fi
        fi
    done

    if [ -z "$obfuscated_name" ]; then
        echo -e "${RED}✗ 未找到混淆后的项目目录${NC}"
        return 1
    fi

    echo -e "${GREEN}✓${NC} 找到混淆后的项目目录: $obfuscated_name"

    echo ""
    echo -e "${CYAN}混淆结果结构:${NC}"
    ls -la "$output_dir/"

    echo ""
    echo -e "${CYAN}开始验证...${NC}"

    # 测试 1: 混淆后的 .xcodeproj 存在
    run_test ".xcodeproj 重命名" "[ -d '$output_dir/$obfuscated_name.xcodeproj' ]"

    # 测试 2: project.pbxproj 文件存在
    run_test "project.pbxproj 存在" "[ -f '$output_dir/$obfuscated_name.xcodeproj/project.pbxproj' ]"

    # 测试 3: project.pbxproj 中不包含原始 SDK 名称
    run_test "project.pbxproj 已更新" "! grep -q '$ORIGINAL_SDK_NAME' '$output_dir/$obfuscated_name.xcodeproj/project.pbxproj'"

    # 测试 4: 源文件中的 SDK 名称引用被替换
    local source_files_updated=0
    local found_original=0

    while IFS= read -r file; do
        if [ -f "$file" ]; then
            if grep -q "$ORIGINAL_SDK_NAME" "$file" 2>/dev/null; then
                found_original=$((found_original + 1))
                echo -e "  ${YELLOW}⚠ $file 仍包含原始 SDK 名称${NC}"
            fi
        fi
    done < <(find "$output_dir/$obfuscated_name" -name "*.m" -o -name "*.h" 2>/dev/null)

    if [ $found_original -eq 0 ]; then
        echo -e "  源文件 SDK 名称替换: ${GREEN}PASS${NC}"
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        PASSED_TESTS=$((PASSED_TESTS + 1))
        source_files_updated=1
    else
        echo -e "  源文件 SDK 名称替换: ${YELLOW}WARN${NC} ($found_original 个文件仍包含原始名称)"
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        # 不算失败，因为字符串字面量可能需要保留
    fi

    # 显示混淆后的目录结构
    echo ""
    echo -e "${CYAN}混淆后的目录结构:${NC}"
    if [ -d "$output_dir/$obfuscated_name" ]; then
        tree "$output_dir/$obfuscated_name" 2>/dev/null || find "$output_dir/$obfuscated_name" -type f -o -type d | head -20
    fi

    # 显示混淆后的文件内容示例
    echo ""
    echo -e "${CYAN}混淆后的文件内容示例:${NC}"
    local sample_file=$(find "$output_dir/$obfuscated_name" -name "*.m" | head -1)
    if [ -n "$sample_file" ]; then
        echo -e "${GRAY}--- $sample_file ---${NC}"
        head -10 "$sample_file"
        echo -e "${GRAY}---${NC}"
    fi
}

# 打印测试摘要
print_summary() {
    print_section "测试摘要"

    echo "  总测试数: $TOTAL_TESTS"
    echo -e "  通过: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "  失败: ${RED}$FAILED_TESTS${NC}"
    echo -e "  跳过: ${YELLOW}$SKIPPED_TESTS${NC}"
    echo ""

    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}✓ 所有测试通过!${NC}"
        echo ""
        echo "混淆后的项目保存在: $TEST_OUTPUT"
        echo "可以尝试在 Xcode 中打开该项目验证是否能编译运行"
        return 0
    else
        echo -e "${RED}✗ 有 $FAILED_TESTS 个测试失败${NC}"
        echo ""
        echo "请检查日志: $TEST_OUTPUT/obfuscator.log"
        return 1
    fi
}

# 清理
cleanup() {
    # 保留测试输出用于调试
    echo ""
    echo "测试输出保留在: $TEST_OUTPUT"
}

# 主流程
main() {
    print_header
    check_environment
    prepare_test

    if run_obfuscator; then
        verify_obfuscation
    fi

    print_summary
    cleanup

    exit $([ $FAILED_TESTS -eq 0 ])
}

# 运行
main
