# Task Plan: SV-COMP 2026 C.Concurrency 扩展实验与优化分析

## Goal
核实 C.Concurrency 数据集和规则，审计当前 CAT/CAAT 实现对更大程序的扩展瓶颈，给出按优先级排序、可测量和可证伪的优化路线。

## Phases
- [x] 核实 SV-COMP 2026 数据集、属性、任务获取方式和结果结构
- [x] 审计当前实现的复杂度、阈值和大图风险
- [x] 将历史 profiling/负优化结果映射到 C.Concurrency 规模
- [x] 形成优化优先级、实验矩阵和正确性门槛
- [x] 复核来源、限制与交付文档

## Key Questions
1. C.Concurrency 有多少任务、哪些程序族和属性，哪些可被当前 GenMC 前端执行？
2. 当前优化 6/11 对哪些模型生效，哪些路径在大图上重新成为瓶颈？
3. 优化 4、8、9、10 如何改造成适合大 relation/长探索的版本？
4. 如何区分算法收益、模型语义差异、前端不兼容和 timeout/OOM？

## Decisions Made
- 不把结果表上的 verifier 成绩直接当作 GenMC 可运行任务数；必须核实 benchmark definition 和 property。
- 不把当前小图阈值 512 直接外推；先记录 active/stable universe、query/rebuild 和 relation density。

## Errors Encountered
- `defuddle` CLI unavailable; no content was extracted. Fell back to official-page browsing instead of changing global npm state.

## Status
Complete — analysis delivered in `optimization-analysis.md`.
