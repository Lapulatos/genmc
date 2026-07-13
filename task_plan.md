# Task Plan: GenMC 通用 CAT 内存模型可行性分析

## Goal
按三阶段路线扩展 GenMC：先通过 `--model-file=<model.cat>` 加载并执行 CAT 模型，再实现离线 CAAT consistency backend，最后实现 CAAT 所讨论的 incremental/online integration。

## Phases
- [x] Feasibility 1: 确认分支、工作区与材料位置
- [x] Feasibility 2: 分析 GenMC、CAT、CAAT、Kater 与 herdtools7
- [x] Feasibility 3: 形成风险边界和三阶段路线
- [ ] Implementation 1: CAT file support（SC/TSO/PSO full-graph consistency；详细计划已完成）
- [ ] Implementation 2: Offline CAAT backend（normalization/fixed point/explanations）
- [ ] Implementation 3: Incremental/online CAAT（push/pop/backtrack/early pruning）

## Key Questions
1. “任意有效 CAT”能否在 GenMC 的增量/完备性要求下直接解释执行？
2. 哪些 CAT 构造可统一实现，哪些需要限制或模型专用 hooks？
3. herdtools7 中可迁移的是解析器、语义、算法还是主要只有测试语料？
4. 最小可交付版本应覆盖哪些语法与内存模型？

## Decisions Made
- 本轮只分析，不修改 GenMC 实现代码。
- 以本地论文、当前分支源码和 herdtools7 官方仓库为主要证据。
- 用户明确首期不要求任意 CAT；MVP 以 SC、TSO、PSO 为目标，并保留后续扩展能力。
- 实施顺序固定为 CAT file support → offline CAAT → incremental/online CAAT。
- 第一阶段必须实现可运行的 full-graph consistency 闭环，而不只是增加 CLI 参数。
- 三阶段共享同一个 typed relational IR；后续阶段替换/增强 evaluator，不重写 parser。
- 每个实施子阶段必须先读 `doc/development.md`，再读 `doc/cat/PROJECT_CONSTRAINTS.md` 和当前阶段计划，完成复用调研、验证、差距分析、记录、commit 和 push。
- 生产热路径默认使用 C++23；herdtools7 作为语义/test oracle，未经许可证审查不复制其 CeCILL-B 源码。
- 新增代码遵循项目约束中的 Doxygen 与逻辑块注释标准。

## Errors Encountered
- 初次关键词检索被大量测试参数噪声淹没；后续按源码目录和核心类型定向检索。
- `git clone --depth 1 https://github.com/herd/herdtools7.git` 遇到 LibreSSL TLS 连接失败；改用官方 GitHub 页面和本地 CAT 论文核查，未下载仓库。
- Phase 1.0 首次链接失败：CMake 找到 hwloc 后以 `-lhwloc` 链接，但 Homebrew 路径不在 linker search path；使用 `-DCMAKE_EXE_LINKER_FLAGS=-L/opt/homebrew/opt/hwloc/lib` 建立基线，未修改仓库源码。
- `BUILD_TESTS=ON` 在 RapidCheck 的未使用 Catch submodule clone 长时间无进展；改用 `FETCHCONTENT_SOURCE_DIR_RAPIDCHECK` 指向已完整克隆的 RapidCheck 主源码。随后 unit tests 40/40 和 CTest fast-driver 1/1 均通过，无仓库源码改动。
- Phase 1.1 首次 CLI 测试发现 LLVM `cl::opt<std::string>` 对重复参数采用最后一个值而不报错；现在保存 occurrence count，并由 `Config::validate` 给出稳定的重复参数诊断，单元与 CLI 测试覆盖该回归。
- Phase 1.1 的 `clang-tidy` 使用生成的 compilation database 运行后，被本机工具链无法解析 libc++ C++23 `<format>` 阻塞；正常 CMake 编译、46/46 单元测试、CLI 1/1 和 fast-driver 1/1 均通过。
- Phase 1.2 首次 include/location 测试暴露了 `Parser` 构造参数中先 move source 还是先 lex 的求值顺序风险；现在明确先完成 tokenization，再移动 source，避免 `Lexer` 的 `string_view` 观察 moved-from 字符串。
- Phase 1.2 负例审查发现缺失左操作数可能让 binary fold 解引用空 AST；增加 early return 和专门回归测试。前端当前 focused 24/24、完整 unit 64/64、CLI 1/1、fast-driver 1/1。

## Status
**Phase 1.2 ready for Git delivery** - lexer/parser/include/source-span/diagnostics、性能记录和回归已完成；提交并 push 后进入 Phase 1.3 typed IR。
