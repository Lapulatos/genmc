# Research Question Card

## Question
在 SV-COMP 2026 C.Concurrency 的更广程序族和规模上，通用 CAT/CAAT 相对内置 GenMC 的时间与空间扩展曲线如何，哪些增量机制能在保持探索结果一致的前提下缩小或逆转开销？

## Hypotheses

- H1: 小图上失败的 COW/半朴素方案在 relation storage 或 operator work 超过阈值后可能转为正收益，但必须改成 row/word-level delta，不能沿用独立 `Value` 对象实现。
- H2: 当前 stable universe 随探索累计增长，会让 relation 的 O(N²/64) 存储和 materialization 主导大程序成本，即使 active graph 较小。
- H3: SC/TSO certificate 带来的候选剪枝收益大于 evaluator 微优化；PSO/自定义模型缺少证书是主要剩余差距。
- H4: 固定 512 阈值不能跨程序族泛化，基于观测成本的 online/offline controller 会更稳定。

## Existing Evidence

来自本仓库 2,100 条阶段数据、最终 300 条 SC/TSO 三方数据及内部 profiler；尚无 SV-COMP 任务上的 GenMC 数据。

## Missing Evidence

- 可解析/可链接/可执行任务数量；
- active 与 stable event universe 分布；
- relation density、fixed-point bytes、query/rebuild/rollback 分布；
- timeout/OOM/correctness mismatch 分类；
- 按程序族和规模分层的配对重复。

## Support Criteria

至少三个规模层级中，优化后 wall time、CPU 或 checker bytes 的斜率下降，并保持 status、complete/blocked executions 或 witness 语义一致。

## Falsification Criteria

若收益只来自少数小任务、扩大规模后 timeout/OOM 增多，或探索结果/错误 witness 不一致，则对应优化假设被否定。

## Minimal Next Action

从官方 benchmark definition 获取任务列表，做 compile/execute compatibility census，再选择每个程序族的代表性规模点。
