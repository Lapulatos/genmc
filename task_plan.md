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

## Status
**Phase 1 planned; implementation not started** - 详细计划位于 `doc/cat/phase-1-plan.md`；下一目标为 Phase 1.0 baseline/specification freeze。
