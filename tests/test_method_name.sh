#!/bin/bash
#
# test_method_name.sh
# 方法名混淆策略测试脚本
#
# 此脚本用于测试方法名混淆策略的各项功能。
# 使用真实的Objective-C代码文件进行测试。

set -e

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
TEST_FIXTURE_DIR="${PROJECT_ROOT}/tests/HWSDK"
TEST_OUTPUT_DIR="${PROJECT_ROOT}/tests/output"

# 测试文件
TEST_FILE="${TEST_FIXTURE_DIR}/MethodNameTestClass.m"
OUTPUT_FILE="${TEST_OUTPUT_DIR}/MethodNameTestClass_obfuscated.m"

echo "======================================"
echo "方法名混淆策略测试"
echo "======================================"
echo ""

# 创建输出目录
mkdir -p "${TEST_OUTPUT_DIR}"

# 检查测试文件是否存在
if [ ! -f "${TEST_FILE}" ]; then
    echo "错误: 测试文件不存在: ${TEST_FILE}"
    exit 1
fi

echo "测试文件: ${TEST_FILE}"
echo "输出文件: ${OUTPUT_FILE}"
echo ""

# 检查构建目录是否存在
if [ ! -d "${BUILD_DIR}" ]; then
    echo "错误: 构建目录不存在: ${BUILD_DIR}"
    echo "请先运行构建脚本: ./build.sh"
    exit 1
fi

# 检查可执行文件是否存在
EXECUTABLE="${BUILD_DIR}/ios-obfuscator"
if [ ! -f "${EXECUTABLE}" ]; then
    echo "错误: 可执行文件不存在: ${EXECUTABLE}"
    echo "请先运行构建脚本: ./build.sh"
    exit 1
fi

echo "可执行文件: ${EXECUTABLE}"
echo ""

# 创建测试配置
TEST_CONFIG="${TEST_OUTPUT_DIR}/test_config.json"
cat > "${TEST_CONFIG}" << EOF
{
  "namingRule": {
    "type": "random",
    "prefix": "OBF_",
    "length": 8
  },
  "strategies": {
    "methodName": {
      "enabled": true,
      "obfuscateParameters": true,
      "obfuscateBlocks": true
    }
  },
  "whitelist": {
    "methods": [
      "init",
      "initWithName:",
      "description",
      "viewDidLoad",
      "viewWillAppear:",
      "tableView:numberOfRowsInSection:",
      "tableView:cellForRowAtIndexPath:"
    ]
  }
}
EOF

echo "测试配置: ${TEST_CONFIG}"
echo ""

# 运行混淆器
echo "运行混淆器..."
echo ""

"${EXECUTABLE}" \
    --input "${TEST_FILE}" \
    --output "${OUTPUT_FILE}" \
    --config "${TEST_CONFIG}" \
    --strategy "MethodNameObfuscation" \
    --verbose

echo ""
echo "======================================"
echo "测试结果"
echo "======================================"
echo ""

# 检查输出文件是否存在
if [ ! -f "${OUTPUT_FILE}" ]; then
    echo "错误: 输出文件不存在: ${OUTPUT_FILE}"
    exit 1
fi

# 显示原始文件和混淆后文件的差异
echo "原始文件 vs 混淆后文件:"
echo "--------------------------------------"
diff -u "${TEST_FILE}" "${OUTPUT_FILE}" || true
echo ""

# 统计混淆结果
echo "混淆统计:"
echo "--------------------------------------"

# 统计原始文件中的方法数
ORIGINAL_METHOD_COUNT=$(grep -c "^\s*-\s*(\w*.*\w*)" "${TEST_FILE}" || echo "0")
echo "原始方法数: ${ORIGINAL_METHOD_COUNT}"

# 统计混淆后保留的自定义方法数（应该为0或减少）
# 这里简单统计，实际应该更精确

# 检查系统方法是否保留
if grep -q "viewDidLoad" "${OUTPUT_FILE}"; then
    echo "✓ 系统方法 (viewDidLoad) 已保留"
else
    echo "✗ 系统方法 (viewDidLoad) 未保留（错误）"
fi

if grep -q "tableView:numberOfRowsInSection:" "${OUTPUT_FILE}"; then
    echo "✓ Delegate方法 (tableView:numberOfRowsInSection:) 已保留"
else
    echo "✗ Delegate方法 (tableView:numberOfRowsInSection:) 未保留（错误）"
fi

# 检查自定义方法是否被混淆
if grep -q "customMethod" "${OUTPUT_FILE}"; then
    echo "✗ 自定义方法 (customMethod) 未被混淆（可能需要检查）"
else
    echo "✓ 自定义方法 (customMethod) 已被混淆"
fi

echo ""
echo "测试完成！"
echo "输出文件: ${OUTPUT_FILE}"
