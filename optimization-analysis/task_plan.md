# Task Plan: GenMC CAT/CAAT 优化 1--11 逐项实现与测量

## Goal
按附件中的顺序逐项尝试优化 1--11（明确排除 12：CAT 静态编译），每项保留可复现实现、正确性证据以及相对前一阶段和原始基线的时间、peak RSS、内部 counters 变化。

## Experimental Contract
- 每项优化是一个明确 stage；报告 cumulative result，同时以 `stage N - stage N-1` 估计该项边际影响。
- 核心性能用例：SC recursive flat-combiner（已知最大回归）；支持用例：SC/TSO/PSO 的 SB、RMWFix、MS queue、Treiber、flat-combiner。
- 正确性门槛：退出状态、complete/blocked executions 与基线一致；CAAT focused tests；每个大阶段运行 CAT/CAAT CTest 与 oracle。
- 性能测量关闭 `--cat-oracle`；内部诊断使用单独的 `--cat-stats` / profiling run，避免污染主时间。
- 每个性能 cell 先预热，再配对重复；保存原始 TSV，不只报告最佳值。
- 原始用户文件 `.DS_Store` 和既有 `experiment-analysis/` 不修改或删除。

## Optimization Stages
- [x] 0. 冻结基线二进制、环境、benchmark driver 和基线数据
- [x] 1. 正常模式不复制 `result.values`
- [x] 2. dependency graph 移到 evaluator 构造阶段
- [x] 3. profile GraphAdapter / synchronization / copy-checkpoint / worklist / rebuild
- [x] 4. 增量维护 GraphAdapter（直接稳定 ID materialization，消除 dense adapter/remap）
- [x] 5. 避免构造模型未引用的 primitives
- [x] 6. 恢复带 admissibility certificate 的安全候选剪枝
- [x] 7. checkpoint 改为 delta trail
- [x] 8. relation/checkpoint 使用 copy-on-write 或持久化 bitsets（实测回归，已回退）
- [x] 9. worklist operator 使用 delta / 半朴素传播（实测回归，已回退）
- [x] 10. `rf/co` replacement 使用受限 support-aware deletion propagation
- [x] 11. 自适应选择 online/offline CAAT backend
- [x] Final. 全量验证、逐项对比表、时空开销报告和限制审计

## Per-stage Evidence
每项在 `stage-results.tsv` 中至少记录：stage、commit/source hash、model、program、repetition、status、wall/user/sys time、peak RSS、complete/blocked executions。适用时记录 initialize/insert/rollback/rebuild/eval-ops/value-changes/queue-pushes/offline-evals 和细分 profiler timers。

## Decisions Made
- 不创建 11 个长期分支；在当前工作树顺序实现，benchmark 保存每个 stage 的二进制副本与 source diff/hash，最终代码为所有被验证且有意义的兼容优化组合。
- 如果某项优化在实现后回归正确性或稳定变慢，保留实验数据和结论，但回退该项代码后再进入下一项；不会为了“完成编号”保留负优化。
- 优化 3 是测量基础设施，不应预期直接产生性能提升；其价值用定位比例与后续决策记录。
- 优化 4、7--10 涉及架构和语义证明，必须先做最小正确版本，再由 oracle/differential evidence 决定是否进入最终组合。

## Key Risks
- 11 项中 4、7--10 是研究级改造，可能跨多轮完成；任何局部 green test 都不能替代 broad differential/oracle 验证。
- macOS `/usr/bin/time` wall time 粒度为 0.01 s；核心用例用足够长的 SC flat-combiner，并报告多次分布。
- peak RSS 包含 LLVM/GenMC 公共开销；checkpoint/relation 独占内存需要 profiler bytes/counters 辅助解释。

## Errors Encountered
- 尚无。

## Status
**Complete** — stages 0--11（含 11b/11c 策略修正）均已采集 150 条主性能样本；8、9 因回归已回退，10 以安全受限版本保留。最终 stage 11c 的 SC flat-combiner 中位时间 0.11 s（相对 1.43 s 基线 13.0x），CAAT suite 1.010 s（2.45x），execution counts 无差异。141 项 unit tests 和 5 项 CAT/CAAT integration tests 全部通过；全量 CTest 唯一未解决项是仓库缺失 `scripts/run-parallel.sh`。
