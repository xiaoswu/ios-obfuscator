# iOS 混淆工具 - 性能分析

本文档对当前项目的性能瓶颈与可优化点进行系统分析，便于后续针对性优化。

---

## 一、整体流程与耗时分布（估算）

| 阶段 | 操作 | 预估占比 | 说明 |
|------|------|----------|------|
| 1 | 配置加载、SDK 检测、`buildCompileArgs` | 1–3% | 含一次项目目录递归遍历 |
| 2 | 源文件扫描（main） | 1–2% | 一次 `recursive_directory_iterator(inputDir)` |
| 3 | **Phase 1：收集阶段**（Clang 解析 + 各策略 analyze） | **40–50%** | 每个 .m/.h 解析一次 AST |
| 4 | **Phase 2：应用阶段**（Clang 解析 + collectReplacements + applyAll） | **40–50%** | 每个 .m/.h 再解析一次 |
| 5 | 复制工程结构 `copyProjectStructure` | 2–5% | 递归复制，I/O 为主 |
| 6 | 文件名策略 `FileNameStrategy::execute` | 5–15% | 多次目录遍历 + 大量字符串替换 |
| 7 | 文件夹重命名、SDK 重命名 | 1–3% | 目录遍历与重命名 |
| 8 | 日志、内存分配等 | 1–5% | 视日志级别与项目规模而定 |

**结论**：主要耗时集中在 **两遍 Clang 解析**（Phase 1 + Phase 2）和 **FileNameStrategy** 的多次目录遍历与字符串处理。

---

## 二、主要瓶颈详解

### 1. 两阶段 Clang 解析（最大瓶颈）

**位置**：`main.cpp` 中先 `collectionTool.run()` 再 `applicationTool.run()`。

**现象**：  
- 每个源文件被 **完整解析两次**（收集符号一遍、应用替换一遍）。  
- 单次解析已包含：词法、语法、语义、AST 构建、各策略的 `matchAST`。  
- 总代价约为 **2 × 文件数 × 单文件解析时间**。

**复杂度**：`O(2 × N × P)`，N = 源文件数，P = 单文件解析成本（与文件大小、头文件依赖相关）。

**优化方向**：  
- 评估是否可合并为单遍：在单次 FrontendAction 中先做“收集”（如只遍历声明、建符号表），再在同一 AST 上做“应用”。需解决跨 TU 的符号可见性（当前设计用两遍正是为了跨文件符号）。  
- 若必须两遍：可考虑并行化 **文件级**（如多进程/多线程处理不同文件），需注意 SymbolTable 等共享状态的线程安全与序列化。  
- 减少不必要的头文件解析：已通过排除 `.framework` 内文件、合理 `-I` 控制范围来缓解，可再审视 `buildCompileArgs` 的 `-I` 数量。

---

### 2. 项目目录的重复递归遍历

**位置**：  
- `main.cpp`：一次 `recursive_directory_iterator(inputDir)` 收集源文件。  
- `CompileOptions::buildCompileArgs(projectPath)`：一次 `recursive_directory_iterator(projectPath)` 收集所有子目录作为 `-I`。  
- `FileNameStrategy::execute`（启用文件名混淆且 `obfuscatedFiles` 非空时）：  
  - “Scanning for paired files”：一次完整 `recursive_directory_iterator(inputDir)`。  
  - “Scanning for system category files”：**两次**完整 `recursive_directory_iterator(inputDir)`（先收集 systemCategoryMap，再处理系统分类文件）。  
- `copyProjectStructure`：递归复制整棵树（等价于一次完整遍历 + 写文件）。  
- `ClassNameFolderStrategy::execute`：一次 `recursive_directory_iterator(outputDir)`。  
- `SDKNameStrategy`：对 outputDir 的每个一级子目录可能做一次 `recursive_directory_iterator`（找含 .m/.h 的目录）。

**现象**：  
- 仅 `FileNameStrategy::execute` 内就有 **最多 3 次** 对 `inputDir` 的完整递归遍历。  
- 加上 main 的 1 次、CompileOptions 的 1 次，对同一棵“输入树”的递归遍历次数很多，大项目下 I/O 与路径处理成本不可忽视。

**复杂度**：  
- 单次遍历：`O(节点数)`。  
- 总代价：`O(遍历次数 × 节点数)`，且每次遍历都会做 `isInFramework`、`shouldProcessFile` 等检查。

**优化方向**：  
- **合并遍历**：在 main 做源文件扫描时，同时收集：  
  - 所有 .m/.h 路径；  
  - 所有需要作为 `-I` 的目录（或至少收集目录列表供 CompileOptions 使用）；  
  - 可选：为 FileNameStrategy 预生成“配对文件列表”“系统分类列表”，避免在 execute 里再扫 3 遍。  
- **单次扫描 + 内存结构**：一次 `recursive_directory_iterator(inputDir)` 得到“文件列表 + 目录列表”，后续 CompileOptions、FileNameStrategy、copyProjectStructure 都基于该结构，而不是重复递归。  
- **SDKNameStrategy**：若可先从 project.pbxproj 或配置得到“主项目目录名”，可避免对多个一级目录做递归探测。

---

### 3. FileNameStrategy 的字符串替换成本

**位置**：  
- `updateImportsInCode`：对 `fileNameMap` 中每一项，在整份文件内容上多次 `find` + `replace`（#import / #include 等）。  
- `updateClassNameInCode`：对每个 `baseNameMap` 中的类名，在整份内容上做一次“全文件 find + 边界检查 + replace”。  
- 在 `execute` 写文件前，对每个要写出的文件：先 `updateImportsInCode`，再对 `baseNameMap` 中每个类名调用 `updateClassNameInCode`。

**现象**：  
- 单文件代价约为：  
  - `O(|fileNameMap| × |file_size|)`（import 更新）；  
  - `O(|baseNameMap| × |file_size|)`（类名更新），且类名更新内部又是一次全文件 `find(className)` 循环。  
- 若项目有 F 个文件、M 个文件名映射、C 个类名映射，总代价约为 **O(F × (M + C) × 平均文件大小)**，且伴随大量 string 拷贝与临时对象。

**优化方向**：  
- **单遍多模式替换**：用 Aho-Corasick（或类似多模式匹配）在单次扫描中找出所有“import 模板/类名”出现位置，再按位置从后往前做替换，避免对每个映射名扫一整遍文件。  
- **减少拷贝**：用 `std::string_view` / `llvm::StringRef` 做查找与边界判断，只在一轮替换后写回一个 buffer。  
- **只处理“会写出的”文件**：确保不对未修改的文件做不必要的 updateImports/updateClassName。

---

### 4. ReplacementManager 的过滤与调试开销

**位置**：  
- `filterOverlaps`：对每个 replacement，用 `usedOffsets`（`std::set<unsigned>`）检查 `[offset, endOffset)` 是否与已用区间重叠；若不重叠，则将 `[offset, endOffset)` 中每个 offset 插入 `usedOffsets`。  
- `applyAll`：内含大量针对 `repl.original == "isCancelled"` 的 DEBUG 分支（LOG、检查 buffer、range 等）。

**现象**：  
- `usedOffsets` 可能包含“所有被替换区间内的每一个字节的 offset”，规模可达 **O(替换总字符数)**。  
- 每个新 replacement 的检查需要遍历或查询该 set，总体接近 **O(替换数 × 平均区间长度)** 或与实现有关，可能成为大文件、多替换时的瓶颈。  
- “isCancelled” 等调试逻辑在 release 下仍会执行分支判断和字符串比较，并产生大量 LOG_INFO。

**优化方向**：  
- **区间重叠检测**：用“区间集合”结构（如按 start 排序的区间列表）做合并与重叠检测，避免逐字节 set；或先按 (offset, endOffset) 排序，再线性扫描合并重叠区间，再过滤 replacement。  
- **移除或门控调试代码**：将 `isCancelled` 及类似 DEBUG 分支用 `#ifdef` 或运行时 log level 关掉，避免生产路径上的字符串比较和日志。

---

### 5. 日志与 I/O

**位置**：  
- `Logger::log`：每次调用会 `std::time`、`std::localtime`、`put_time` 生成时间戳，再拼接字符串，输出到控制台/文件，并在写文件时 `flush`。  
- 代码中约 **260+** 处 LOG 调用（LOG_INFO/DEBUG/WARNING/ERROR），在 INFO 级别下很多会执行。

**现象**：  
- 每条日志都做一次时间格式化与一次（或两次）I/O，高频率日志会明显拉高 CPU 和 I/O。  
- 尤其在“每文件/每策略/每 replacement”的粒度打日志时，成本会随文件数和替换数线性增长。

**优化方向**：  
- **默认日志级别**：默认设为 WARNING 或 ERROR，减少 INFO/DEBUG 的调用量。  
- **批量/汇总日志**：例如“本文件应用了 N 处替换”“本策略共处理 M 个符号”，而不是每个替换一条。  
- **延迟格式化**：仅当 level 通过时再格式化时间和消息（当前已有 `level < currentLevel_` 提前 return，可再检查是否在所有宏展开前就避免构造 message 字符串，例如用 lambda 或宏封装）。  
- **减少 flush 频率**：例如按条数或按秒 flush，而不是每条 log 都 flush。

---

### 6. CompileOptions 的 -I 数量

**位置**：`CompileOptions::buildCompileArgs` 中，对 `projectPath` 的每个子目录都 push 一个 `-I`。

**现象**：  
- 项目目录结构越深、目录越多，`compileArgs` 越大，Clang 在解析每个文件时都要搜索大量 include 路径。  
- 可能增加头文件查找和磁盘访问，间接拉长两阶段解析时间。

**优化方向**：  
- 只添加“实际包含源文件/头文件的目录”作为 `-I`，而不是所有子目录。  
- 或提供配置项：用户指定若干根 `-I` 目录，工具不再自动递归添加全部子目录。

---

### 7. 复制工程结构时的 I/O

**位置**：`FileNameStrategy::copyProjectStructure` 的递归复制。

**现象**：  
- 整棵输入树（除跳过项）逐文件 `copy_file`，纯 I/O  bound。  
- 大项目、大量资源文件时，耗时不可忽视，但在当前设计中属于必要步骤。

**优化方向**：  
- 若输出目录已存在且可复用，可考虑“增量同步”（只复制变更或缺失文件），需谨慎处理删除与重命名。  
- 多线程/异步复制（注意文件系统并发与顺序依赖），或交给外部工具（如 rsync）做一次批量复制，工具只负责在复制结果上写混淆后的源文件。

---

### 8. 属性名混淆（PropertyNameStrategy）与 Aho-Corasick

**现状**：  
- 已使用 Aho-Corasick / MultiPatternMatcher 做多模式匹配，相比逐模式 `find` 已有明显提升。  
- 若仍有 KVC/KVO 等字符串的“逐模式 find + 边界判断”的 fallback 路径，在大文件、多属性时仍可能占一定比例。

**建议**：  
- 确保所有“在源码文本中查找属性名”的路径都走 Aho-Corasick（或同一多模式匹配器），避免遗留 O(属性数 × 文件大小) 的线性搜索。  
- 见此前《属性名混淆可优化点》中对 KVC/KVO 上下文与边界检查的优化建议。

---

## 三、各模块复杂度小结

| 模块 | 操作 | 时间复杂度/规模 | 备注 |
|------|------|------------------|------|
| main | 源文件扫描 | O(输入树节点数) | 1 次递归遍历 |
| CompileOptions | 构建编译参数 + 项目 -I | O(输入树目录数) | 1 次递归遍历 |
| Phase 1 / Phase 2 | Clang 解析 + 策略 | O(2 × N × 解析成本) | 主导耗时 |
| copyProjectStructure | 递归复制 | O(输入树节点数) + I/O | 必要 |
| FileNameStrategy::execute | 配对/系统分类扫描 | O(3 × 输入树节点数) | 可合并为 1 次 |
| FileNameStrategy | updateImports + updateClassName | O(F × (M+C) × L) | F=文件数,M=import映射,C=类名,L=文件长 |
| ClassNameFolderStrategy | 目录遍历 + 排序 + 重命名 | O(输出树目录数) | 1 次遍历 |
| ReplacementManager::filterOverlaps | 重叠过滤 | O(替换数 × 区间/集合操作) | 可改为区间合并 |
| ReplacementManager::applyAll | 应用替换 + 调试分支 | O(替换数) + 日志/分支 | 建议去掉 isCancelled 等调试 |
| Logger | 每条日志 | 时间格式化 + 控制台/文件 I/O + flush | 约 260+ 处调用 |

---

## 四、优化建议优先级

1. **高优先级**  
   - **两阶段解析**：评估单遍方案或文件级并行，预期收益最大。  
   - **FileNameStrategy 多遍遍历**：合并为一次扫描 + 内存结构，减少 2–3 次完整递归。  
   - **ReplacementManager**：移除或门控 isCancelled 等调试代码；优化 filterOverlaps 的区间表示与重叠检测。

2. **中优先级**  
   - **FileNameStrategy 字符串替换**：单遍多模式替换（Aho-Corasick/类名+import 联合）、减少 string 拷贝。  
   - **日志**：提高默认级别、批量汇总、降低 flush 频率。  
   - **main + CompileOptions**：一次扫描同时得到“源文件列表 + 目录列表”，供后续阶段复用。

3. **低优先级**  
   - **copyProjectStructure**：增量或并行复制（若需求明确）。  
   - **CompileOptions -I**：只添加必要目录或由配置控制。  
   - **SDKNameStrategy**：避免对每个一级目录做递归，改为配置或 pbxproj 解析。

---

## 五、总结

- **最大耗时**：两遍 Clang 解析（收集 + 应用），约占总时间的绝大部分。  
- **次要耗时**：FileNameStrategy 的多次目录遍历和按“每个映射名 × 每文件”的字符串替换，以及 ReplacementManager 的过滤与调试逻辑。  
- **辅助因素**：日志频率与 I/O、CompileOptions 的 `-I` 规模、复制整棵工程树的 I/O。

优先做“两阶段解析优化”和“FileNameStrategy 的遍历与替换优化”，再配合 ReplacementManager 与日志的瘦身，可在不改变功能的前提下显著缩短整体运行时间。若你希望，我可以针对某一项（例如“合并 FileNameStrategy 的 3 次遍历”或“filterOverlaps 的区间算法”）给出具体改法或补丁示例。
