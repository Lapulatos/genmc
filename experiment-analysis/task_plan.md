# Task Plan: GenMC / GenMC+CAT / GenMC+CAAT 性能对比

## Goal
在同一二进制、同一主机、同一模型语义与同一程序集合下，对比三条验证路径的端到端时间、峰值 RSS、吞吐与探索结果，并生成可复核的原始数据、统计附录和图表。

## Comparison Contract
- GenMC：内置 SC/TSO checker（PSO 无内置等价基线，因此不进入主三方比较）。
- GenMC+CAT：`models/cat/{sc,tso}.cat`，Phase 1 非递归全图 evaluator。
- GenMC+CAAT：`models/cat/recursive-{sc,tso}.cat`，Phase 3 online/incremental evaluator。
- 配对单位：同一 `model × program × repetition`；单 worker；固定命令参数。
- 主指标：端到端 wall time；次指标：峰值 RSS、executions/s、完整 execution 数一致性。

## Phases
- [x] P0: 确认实现入口、现有基准与环境边界
- [x] P0: 编写可复现实验驱动并做 smoke test
- [x] P0: 执行预热与重复测量，保存原始 TSV 和环境信息
- [x] P0: 校验探索结果可比性并运行配对统计分析
- [x] P1: 生成分析报告、统计附录、图表目录和 PDF/SVG 图表
- [x] P1: 复核产物、命令与 git 变更范围
- [x] P1: 补充 PSO 下 CAT vs CAAT 二方实验（不混入无等价 baseline 的三方统计）

## Decisions Made
- 只在 SC/TSO 上做严格三方比较，因为当前 GenMC 没有内置 PSO checker；PSO 可作为 CAT/CAAT 的补充二方比较，但不混入主效应。
- 使用当前 `RelWithDebInfo/bin/genmc`，避免不同编译器或不同提交造成混杂。
- 每个 cell 先预热，再至少重复 10 次；实验按 repetition 分块并轮换 backend 顺序，降低温度和顺序偏差。
- 不开启 `--cat-stats` 或 `--cat-oracle`，避免观测与 oracle 开销污染性能数据。
- 不修改 GenMC 实现或已有 `task_plan.md`、`notes.md`、阶段报告。

## Key Risks
- macOS `/usr/bin/time -lp` 的时间粒度可能使极短用例量化到 0.01 s；通过加入中等负载、配对重复和报告原始分布缓解。
- RSS 是单次进程峰值，不是 checker 独占内存；它包含 clang/LLVM 前端和 GenMC 公共开销。
- 同机顺序运行仍会受系统负载、CPU 频率和文件缓存影响；结果代表本机端到端性能，不自动外推到其他平台。

## Errors Encountered
- 尚无。

## Status
**Complete** — SC/TSO 三方 300 行和 PSO 二方 100 行均完成且语义计数一致；脚本语法、统计重跑、PDF 完整性、主图视觉和 `git diff --check` 已通过。
