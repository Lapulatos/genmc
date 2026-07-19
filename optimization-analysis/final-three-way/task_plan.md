# Final Three-way Comparison Plan

## Goal
使用最终 `stage-11c` 二进制重新比较 built-in GenMC、GenMC+CAT、GenMC+CAAT 的端到端时间、CPU、peak RSS 和探索计数。

## Contract
- SC/TSO：3 backends × 5 programs × 10 repetitions = 300 rows。
- PSO：因无 built-in GenMC 等价 checker，仅补充 CAT/CAAT 二方结果，不混入三方统计。
- 每个 cell 预热 1 次；单 worker；关闭 estimation、MM detector、CAT stats 和 oracle。
- 同一 model/program/repetition 的 status、complete、blocked executions 必须一致。

## Steps
- [x] 采集最终 SC/TSO 三方原始数据
- [x] 验证语义合同并计算配对描述统计/效应量
- [x] 生成主图、内存图、精确数表和分析报告
- [x] 复核产物与限制

## Status
Complete — SC/TSO 300 rows、PSO 100 rows；语义合同通过；三张图、报告和统计附录已生成。
