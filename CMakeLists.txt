cmake_minimum_required(VERSION 3.15)
# 启用C和C++语言支持
# LLVM的配置脚本需要C语言支持来检查某些头文件
project(iOSObfuscator VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含nlohmann/json库
# 注意：nlohmann/json是header-only库，可以直接下载头文件
# 为了避免CMake版本兼容性问题，我们直接下载头文件而不是使用FetchContent的add_subdirectory

include(FetchContent)

# 设置CMake策略，允许子项目使用较旧的CMake版本
# 这可以解决nlohmann/json的CMake版本兼容性问题
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW)

# 下载nlohmann/json库（只下载，不配置子项目）
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
    # 不自动配置，我们手动处理
    OVERRIDE_FIND_PACKAGE
)

# 获取json库的源代码目录
FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    # 设置策略版本，允许子项目使用较旧的CMake版本
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    FetchContent_Populate(json)
    
    # 直接添加json头文件目录到包含路径
    # 因为nlohmann/json是header-only库，只需要头文件
    # 注意：json.hpp在json_SOURCE_DIR/include/nlohmann/json.hpp
    include_directories(${json_SOURCE_DIR}/include)
    
    message(STATUS "nlohmann/json downloaded to: ${json_SOURCE_DIR}")
    message(STATUS "nlohmann/json include path: ${json_SOURCE_DIR}/include")
endif()

# 注意：包含目录的设置应该在find_package之后
# 因为LLVM_INCLUDE_DIRS和CLANG_INCLUDE_DIRS是在find_package时设置的

# 添加编译定义
add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

# 查找LLVM和Clang
# 注意：LLVM配置可能会查找LibEdit（libedit-dev），这是可选的依赖
# 如果找不到LibEdit，LLVM仍然可以正常工作，只是某些终端功能不可用
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# 链接库
# 注意：Clang库应该通过LLVM的组件系统来链接
# 使用llvm_map_components_to_libnames来获取所有需要的库（包括Clang库）

# 使用LLVM提供的完整链接方式
# llvm_map_components_to_libnames会自动解析依赖关系
# 根据链接错误，需要添加以下LLVM组件：
# - object: 用于llvm::object::Archive
# - option: 用于llvm::opt::OptTable  
# - mc, mcparser: 用于llvm::MC*相关功能

# LLVM核心库（包含所有需要的组件）
llvm_map_components_to_libnames(llvm_libs
    core
    support
    object
    option
    mc
    mcparser
    binaryformat
    remarks
    bitstreamreader
    targetparser
    demangle
)

# Clang库（通过LLVM组件系统）
# llvm_map_components_to_libnames会自动解析Clang库的依赖关系
# 包括所有必需的LLVM依赖库
llvm_map_components_to_libnames(clang_libs
    clangBasic
    clangFrontend
    clangAST
    clangASTMatchers
    clangTooling
    clangToolingCore
    clangRewrite
    clangLex
    clangParse
    clangSema
    clangAnalysis
    clangEdit
    clangSerialization
    clangDriver
)

# 源文件
# 注意：只包含已存在的文件，其他文件将在后续实现中添加

set(CORE_SOURCES
    src/core/ConfigManager.cpp
    src/core/Logger.cpp
    src/core/SymbolTable.cpp
    src/core/NameGenerator.cpp
    src/core/StrategyManager.cpp
    src/core/ReplacementManager.cpp
    src/core/CompileOptions.cpp
    src/core/AhoCorasick.cpp
    src/core/DeadCodeGenerator.cpp
)

# Strategy模块（只包含已实现的策略）
set(STRATEGY_SOURCES
    src/strategies/ObfuscationStrategy.cpp
    src/strategies/ClassNameStrategy.cpp
    src/strategies/PropertyNameStrategy.cpp
    src/strategies/MethodNameStrategy.cpp
    src/strategies/VariableNameStrategy.cpp
    src/strategies/FileNameStrategy.cpp
    src/strategies/SDKNameStrategy.cpp
    src/strategies/ClassNameFolderStrategy.cpp
    src/strategies/ProtocolNameStrategy.cpp
    src/strategies/CommentRemovalStrategy.cpp
    src/strategies/DeadCodeInjectionStrategy.cpp
    # 其他策略将在后续实现中添加
    # src/strategies/CategoryNameStrategy.cpp
    # src/strategies/StringStrategy.cpp
    # src/strategies/ResourceStrategy.cpp
    # src/strategies/MetadataStrategy.cpp
    # src/strategies/ControlFlowStrategy.cpp
)

# Template模块
set(TEMPLATE_SOURCES
    src/templates/DeadCodeTemplates.cpp
)

# Parser模块和Generator模块暂时不包含
# 将在后续实现时添加
# set(PARSER_SOURCES
#     src/parser/ASTParser.cpp
#     src/parser/SymbolExtractor.cpp
#     src/parser/DependencyAnalyzer.cpp
# )
# 
# set(GENERATOR_SOURCES
#     src/generator/CodeGenerator.cpp
#     src/generator/MappingGenerator.cpp
#     src/generator/ReportGenerator.cpp
# )

set(MAIN_SOURCE
    src/main.cpp
)

# 可执行文件
# 注意：只包含已存在的源文件
# PARSER_SOURCES和GENERATOR_SOURCES暂时不包含，后续实现时再添加
add_executable(ios-obfuscator
    ${MAIN_SOURCE}
    ${CORE_SOURCES}
    ${STRATEGY_SOURCES}
    ${TEMPLATE_SOURCES}
    # ${PARSER_SOURCES}      # 暂时注释，后续实现
    # ${GENERATOR_SOURCES}   # 暂时注释，后续实现
)

# 设置目标特定的包含目录
# 使用target_include_directories确保包含路径被正确传递
target_include_directories(ios-obfuscator PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${json_SOURCE_DIR}/include
)

message(STATUS "Target include directories:")
message(STATUS "  LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")
message(STATUS "  CLANG_INCLUDE_DIRS: ${CLANG_INCLUDE_DIRS}")
message(STATUS "  Source dir: ${CMAKE_CURRENT_SOURCE_DIR}/src")
message(STATUS "  JSON include: ${json_SOURCE_DIR}/include")

# 链接库
# 注意：nlohmann/json是header-only库，不需要链接
# 只需要包含头文件即可（已在target_include_directories中添加）
# 
# 使用LLVM提供的链接方式：
# 由于llvm_map_components_to_libnames返回的库名格式（-lLLVMclangBasic）在当前系统上
# 链接器找不到，我们需要直接使用库文件路径
# LLVM会自动解析依赖关系，我们只需要列出所有需要的组件

# 确保链接器能找到LLVM库
if(LLVM_LIBRARY_DIRS)
    link_directories(${LLVM_LIBRARY_DIRS})
    message(STATUS "LLVM library directories: ${LLVM_LIBRARY_DIRS}")
endif()

# 直接使用库文件路径（按照依赖顺序）
# LLVM核心库（包含所有需要的组件）
# 根据链接错误，需要添加更多LLVM库
set(llvm_lib_files
    ${LLVM_LIBRARY_DIRS}/libLLVMCore.a
    ${LLVM_LIBRARY_DIRS}/libLLVMSupport.a
    ${LLVM_LIBRARY_DIRS}/libLLVMObject.a
    ${LLVM_LIBRARY_DIRS}/libLLVMOption.a
    ${LLVM_LIBRARY_DIRS}/libLLVMMC.a
    ${LLVM_LIBRARY_DIRS}/libLLVMMCParser.a
    ${LLVM_LIBRARY_DIRS}/libLLVMBinaryFormat.a
    ${LLVM_LIBRARY_DIRS}/libLLVMRemarks.a
    ${LLVM_LIBRARY_DIRS}/libLLVMBitstreamReader.a
    ${LLVM_LIBRARY_DIRS}/libLLVMTargetParser.a
    ${LLVM_LIBRARY_DIRS}/libLLVMDemangle.a
    ${LLVM_LIBRARY_DIRS}/libLLVMTextAPI.a
    ${LLVM_LIBRARY_DIRS}/libLLVMIRReader.a
    ${LLVM_LIBRARY_DIRS}/libLLVMBitReader.a
    ${LLVM_LIBRARY_DIRS}/libLLVMAsmParser.a
)

# Clang库（按照依赖顺序）
set(clang_lib_files
    ${LLVM_LIBRARY_DIRS}/libclangSerialization.a
    ${LLVM_LIBRARY_DIRS}/libclangDriver.a
    ${LLVM_LIBRARY_DIRS}/libclangFrontend.a
    ${LLVM_LIBRARY_DIRS}/libclangTooling.a
    ${LLVM_LIBRARY_DIRS}/libclangToolingCore.a
    ${LLVM_LIBRARY_DIRS}/libclangRewrite.a
    ${LLVM_LIBRARY_DIRS}/libclangASTMatchers.a
    ${LLVM_LIBRARY_DIRS}/libclangAST.a
    ${LLVM_LIBRARY_DIRS}/libclangParse.a
    ${LLVM_LIBRARY_DIRS}/libclangSema.a
    ${LLVM_LIBRARY_DIRS}/libclangAnalysis.a
    ${LLVM_LIBRARY_DIRS}/libclangEdit.a
    ${LLVM_LIBRARY_DIRS}/libclangLex.a
    ${LLVM_LIBRARY_DIRS}/libclangAPINotes.a
    ${LLVM_LIBRARY_DIRS}/libclangBasic.a
)

# 链接库时，需要注意库的顺序
# 被依赖的库应该放在后面，依赖其他库的应该放在前面
# 对于静态库，链接器从左到右解析符号，所以依赖关系要反向
# 查找zstd库
find_library(ZSTD_LIBRARY
    NAMES zstd libzstd
    PATHS /opt/homebrew/lib /usr/local/lib
    NO_DEFAULT_PATH
)

target_link_libraries(ios-obfuscator
    ${clang_lib_files}
    ${llvm_lib_files}
    # 系统库
    z          # zlib用于压缩功能
)

# 如果找到zstd库，则链接它
if(ZSTD_LIBRARY)
    target_link_libraries(ios-obfuscator ${ZSTD_LIBRARY})
    message(STATUS "Found zstd library: ${ZSTD_LIBRARY}")
else()
    # 如果找不到，尝试使用-lzstd（可能在不同位置）
    # 或者直接使用完整路径
    if(EXISTS "/opt/homebrew/lib/libzstd.dylib")
        target_link_libraries(ios-obfuscator /opt/homebrew/lib/libzstd.dylib)
    elseif(EXISTS "/opt/homebrew/Cellar/zstd/1.5.7/lib/libzstd.a")
        target_link_libraries(ios-obfuscator /opt/homebrew/Cellar/zstd/1.5.7/lib/libzstd.a)
    else()
        target_link_libraries(ios-obfuscator zstd)
    endif()
    message(STATUS "Linking zstd library")
endif()

# 添加链接器标志，允许未定义的符号
# 这些符号（RISCV、OpenMP相关）对于iOS混淆工具来说不是必需的
# 它们可能是弱符号或者可选功能
if(APPLE)
    # macOS上使用-undefined dynamic_lookup来允许未定义的符号
    # 或者使用-Wl,-undefined,dynamic_lookup
    set_target_properties(ios-obfuscator PROPERTIES
        LINK_FLAGS "-Wl,-undefined,dynamic_lookup"
    )
    message(STATUS "Added linker flag to allow undefined symbols (for optional features like RISCV/OpenMP)")
endif()

# 启用测试
enable_testing()

# 添加测试目标
add_test(NAME RegressionTests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_all_tests.sh)
set_tests_properties(RegressionTests PROPERTIES
    TIMEOUT 300
    LABELS "regression"
)

# 快速测试 (仅AST解析)
add_test(NAME QuickTest COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_quick_test.sh)
set_tests_properties(QuickTest PROPERTIES
    TIMEOUT 60
    LABELS "quick"
)

# 安装
install(TARGETS ios-obfuscator
    RUNTIME DESTINATION bin
)
