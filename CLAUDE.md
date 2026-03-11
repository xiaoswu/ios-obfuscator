# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Quick Build (Recommended)
```bash
./build.sh
```
This script automatically checks for dependencies (CMake, LLVM via Homebrew) and builds the project.

### Manual Build
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Build with specific LLVM path
```bash
cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm \
  -DClang_DIR=$(brew --prefix llvm)/lib/cmake/clang
make -j$(sysctl -n hw.ncpu)
```

### Debug Build
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

### Clean and Rebuild
```bash
cd build
make clean
# Or delete entire build directory:
cd .. && rm -rf build && mkdir build && cd build && cmake .. && make
```

## Running the Tool

```bash
# From build directory
./ios-obfuscator --config=../config/config.json --input=/path/to/SDK --output=/path/to/output --verbose

# The tool accepts these command-line arguments:
# --config=<file>   - Path to JSON config file (default: config.json)
# --input=<path>    - Source code path to obfuscate
# --output=<path>   - Output path for obfuscated code
# --verbose         - Enable debug-level logging
```

## Architecture

This is a Clang-based tool that obfuscates iOS Objective-C SDKs. The architecture centers around:

### Three-Phase Strategy Pattern
Each obfuscation strategy inherits from `ObfuscationStrategy` (src/strategies/ObfuscationStrategy.h) and implements three phases:
1. **analyze()** - Traverse the AST via AST Matchers to collect symbols for obfuscation
2. **transform()** - Use Clang's Rewriter to modify source code
3. **validate()** - Verify the transformation was successful

### Core Components

**src/core/StrategyManager** - Orchestrates all strategies. It:
- Loads strategies based on config
- Resolves strategy dependencies
- Executes analyze/transform/validate phases across all strategies

**src/core/SymbolTable** - Maintains bidirectional mapping between original and obfuscated names. Tracks:
- SymbolType (CLASS, METHOD, PROPERTY, VARIABLE, PROTOCOL, CATEGORY, FILE, STRING, RESOURCE)
- Public/private visibility
- File path and dependencies

**src/core/NameGenerator** - Generates obfuscated names based on configured style:
- `words` mode - Uses word lists from wordlist/ directory
- `random` mode - Generates random alphanumeric strings

**src/core/ConfigManager** - Loads JSON configuration (see config/config.json for structure)

**src/main.cpp** - Entry point that:
1. Parses CLI arguments (via LLVM's CommandLine library)
2. Scans input directory for .m/.h files
3. Creates ClangTool with appropriate compile flags for iOS
4. Runs obfuscation via ObfuscatorFrontendAction
5. Writes output files, applies file/folder renaming

### Compile Options (src/core/CompileOptions)
Automatically detects iOS SDK path and generates appropriate clang flags (framework paths, arc, etc.)

### Implemented Strategies
- **ClassNameStrategy** - Obfuscates @interface and @implementation names
- **MethodNameStrategy** - Obfuscates method declarations, message expressions, and @selector() references
- **FileNameStrategy** - Renames source files and updates imports
- **SDKNameStrategy** - Renames the SDK framework itself
- **ClassNameFolderStrategy** - Renames folders containing class files

### Planned Strategies (not yet implemented)
- PropertyNameStrategy, VariableNameStrategy, ProtocolNameStrategy, CategoryNameStrategy, StringStrategy, ResourceStrategy, MetadataStrategy, ControlFlowStrategy

## Important Implementation Details

### Third-Party Exclusions
Files in `/ThirdChannel/` or `/ThirdPart/` directories, and files inside `.framework/` bundles are automatically skipped from obfuscation.

### AST Matcher Pattern
Strategies use `clang::ast_matchers::MatchFinder` to locate AST nodes. The pattern:
```cpp
finder.addMatcher(objcInterfaceDecl(...).bind("interface"), this);
// In run() callback: const auto* decl = result.Nodes.getNodeAs<ObjCInterfaceDecl>("interface");
```

### Rewriter Usage
Key considerations:
- Check `SourceLocation` validity before replacing
- Use `SourceManager::isInMainFileID()` to avoid modifying headers
- Replace from end to start when making multiple changes to avoid offset issues
- Get modified code via `Rewriter::getRewriteBufferFor()`

### Clang Tooling Integration
The tool uses `clang::tooling::ClangTool` with a `FixedCompilationDatabase`. Compile flags must match iOS SDK paths for proper parsing.

## Configuration Structure

Config is JSON with two main sections:
- `sdk` - name, type, inputPath, outputPath
- `obfuscation` - strategies list, namingRule (style, wordListPath, wordCase, wordCount, randomLength), whitelist, generateMapping

See config/config.json for complete example.

## Word Lists

The `wordlist/` directory contains text files with words used for generating obfuscated names when `style: "words"` is configured.

## 要求
- 在编程过程中，要时刻注意性能
- 在tests文件夹主要用于单元测试，单元测试的输入文件是~/tests/HWSDK,输出的混淆代码放在~/tests/output里面，然后输出的混淆代码文件命名方式是混淆策略名+时间戳。
