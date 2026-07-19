# 最终 GenMC / GenMC+CAT / GenMC+CAAT 开销对比

## 结论

经过优化 1--11 后，GenMC+CAAT 已从三者中最慢变为明显快于 GenMC+CAT，并接近内置 GenMC：在固定 SC/TSO 10-case suite 上，每轮 wall time 均值分别为 GenMC **0.621 s**、CAT **0.719 s**、CAAT **0.645 s**。相对 GenMC，CAT 增加 **15.8%**，CAAT 增加 **3.9%**；CAAT 相对 CAT 减少 **10.2%** wall time。

CPU 时间呈相同顺序：GenMC **0.493 s**、CAT **0.606 s**、CAAT **0.541 s**。相对 GenMC，CAT CPU 增加约 **23.1%**，CAAT 增加约 **9.9%**；CAAT 比 CAT 少约 **10.7%** CPU。

三者 process peak RSS 均值分别为 GenMC **50.937 MiB**、CAT **50.938 MiB**、CAAT **50.928 MiB**，差异小于 0.011 MiB，不能解释为 checker 独占内存相同或 CAAT 节省内存，只能说端到端 RSS 中没有可辨识差异。

## 实验合同

- 二进制：最终冻结的 `optimization-analysis/binaries/stage-11c-genmc`；
- 模型：SC、TSO；PSO 因无内置 GenMC 等价 checker，仅另做 CAT/CAAT 二方比较；
- 程序：SB、RMWFix、MS Queue、Treiber Stack、Flat Combiner；
- 设计：3 backends × 2 models × 5 programs × 10 repetitions = 300 条正式样本；每个 cell 预热 1 次；
- 控制：单 worker，关闭 estimation、MM detector、CAT stats 和 oracle，按 repetition 轮换 backend 顺序；
- 指标：wall/user/sys time、process peak RSS、complete/blocked executions；
- 语义检查：全部 100 个 `model × program × repetition` 配对组的 status、complete 和 blocked executions 一致。

## 总体开销

| 方法 | 每轮 wall time | 相对 GenMC | 95% CI | CPU time | CPU 相对 GenMC | complete exec/s |
|---|---:|---:|---:|---:|---:|---:|
| GenMC | 0.621 s | 1.000× | — | 0.493 s | 1.000× | 95.0 |
| GenMC+CAT | 0.719 s | 1.158× | [1.134, 1.182] | 0.606 s | 1.231× | 82.1 |
| GenMC+CAAT | 0.645 s | 1.039× | [1.016, 1.062] | 0.541 s | 1.099× | 91.5 |

这里的吞吐量是每轮固定 59 个 complete executions 除以 suite wall time，只用于同一固定 suite 内比较，不代表任意程序的通用吞吐。

CAAT/CAT 的配对 wall-time 比率为 **0.898×**，95% CI [0.879, 0.916]；即最终 CAAT 比 CAT 快约 8.4%--12.1%。CPU 比率为 **0.893×** [0.871, 0.915]。

## 同一例子上的差别

以下为 10 次重复的 wall-time 中位数：

| 模型 / 程序 | GenMC | CAT | CAAT | CAT/GenMC | CAAT/GenMC | CAAT/CAT |
|---|---:|---:|---:|---:|---:|---:|
| SC / SB | 0.05 | 0.05 | 0.05 | 1.00× | 1.00× | 1.00× |
| SC / RMWFix | 0.05 | 0.05 | 0.05 | 1.00× | 1.00× | 1.00× |
| SC / MS Queue | 0.06 | 0.06 | 0.06 | 1.00× | 1.00× | 1.00× |
| SC / Treiber | 0.06 | 0.06 | 0.06 | 1.00× | 1.00× | 1.00× |
| SC / Flat Combiner | 0.09 | 0.17 | 0.11 | 1.89× | 1.22× | 0.65× |
| TSO / SB | 0.05 | 0.05 | 0.05 | 1.00× | 1.00× | 1.00× |
| TSO / RMWFix | 0.05 | 0.05 | 0.05 | 1.00× | 1.00× | 1.00× |
| TSO / MS Queue | 0.06 | 0.06 | 0.06 | 1.00× | 1.00× | 1.00× |
| TSO / Treiber | 0.06 | 0.07 | 0.06 | 1.17× | 1.00× | 0.86× |
| TSO / Flat Combiner | 0.08 | 0.09 | 0.09 | 1.13× | 1.13× | 1.00× |

短用例大量落在 `/usr/bin/time` 的 0.01 秒显示粒度上，因此表中的 1.00× 不能解释成真正零开销。最有分析价值的是 SC Flat Combiner：

- GenMC：wall 0.091 ± 0.003 s，CPU 0.076 ± 0.005 s；
- CAT：wall 0.169 ± 0.006 s，CPU 0.151 ± 0.007 s；
- CAAT：wall 0.110 ± 0.000 s，CPU 0.098 ± 0.004 s；
- CAAT 比 CAT 少约 35% wall time，但仍比 GenMC 多约 22%。

这与优化机制一致：认证候选剪枝把 SC Flat Combiner 的一致性查询从 327 降到 43，小图 adaptive offline 又避免了高成本 history/worklist 路径；CAT 仍需对每个候选从头解释通用 relation IR。

## 内存开销

| 方法 | 300-run 平均 peak RSS | 相对 GenMC 的 cell-level RSS 几何比 |
|---|---:|---:|
| GenMC | 50.937 MiB | 1.0000× |
| GenMC+CAT | 50.938 MiB | 1.0001× [0.9994, 1.0008] |
| GenMC+CAAT | 50.928 MiB | 0.9999× [0.9993, 1.0005] |

SC Flat Combiner 的 RSS 均值也接近：GenMC 51.953 MiB、CAT 51.983 MiB、CAAT 51.973 MiB。公共 LLVM/GenMC 内存约 50--52 MiB，掩盖了 checker 内部几百 KiB 差异。内部 profiler 仍显示 CAAT delta trail 将 retained restoration packed storage 峰值从 733,280 B 降到 368,744 B；这与 process RSS “看起来相同”并不矛盾。

## 与优化前对比

优化前同一固定 SC/TSO suite 的结果为 GenMC 0.614 s、CAT 0.694 s、CAAT 2.056 s。当前 CAAT 为 0.645 s：

- CAAT suite 时间相对优化前减少约 **68.6%**，约为原来的 **0.314×**；
- CAAT 相对 GenMC 的开销从 **3.35×** 降至 **1.039×**；
- SC Flat Combiner 从约 1.44 s 降至 0.11 s，约 **13.1×** 加速；
- GenMC/CAT 的小幅变化主要是两次实验的系统噪声，二者实现未因 CAAT 优化发生同量级变化。

## PSO 补充结果

PSO 没有内置 GenMC baseline。10 次重复的固定 5-case suite 中，CAT 为 0.325 s，CAAT 为 0.364 s；CAAT/CAT = **1.124×** [1.078, 1.169]，即 CAAT 仍慢约 12.4%。主要来自 Flat Combiner 1.22×、Treiber 1.23×、MS Queue 1.17×；RSS 比率 0.9997×，无可辨识差异。

原因是最终 candidate certificate 和 adaptive offline 只对精确认证的 bundled SC/TSO 生效；PSO 保持通用 online CAAT 路径。这是当前最明确的后续优化对象。

## Claim Candidates

- Claim:
  - Source evidence: 300 条最终 SC/TSO 三方样本、`suite-total-summary.tsv`。
  - Allowed wording: 在本机固定 suite 中，最终 CAAT 的 wall-time 开销约为内置 GenMC 的 1.04×，并比 CAT 快约 10%。
  - Forbidden stronger wording: CAAT 在所有程序上都比 CAT 快，或已达到与 GenMC 完全相同的性能。
  - Uncertainty: Apple M2、单 worker、5 个小到中等程序；短用例计时量化明显。
  - Decision: keep

- Claim:
  - Source evidence: 300-run peak RSS 与 cell-level ratios。
  - Allowed wording: 三条路径的端到端 process peak RSS 在当前测量中没有可辨识差异。
  - Forbidden stronger wording: 三个 checker 的独占内存完全相同。
  - Uncertainty: 公共 LLVM/GenMC 内存主导。
  - Decision: keep

- Claim:
  - Source evidence: 100 条 PSO CAT/CAAT 样本。
  - Allowed wording: PSO 下 CAAT 在该固定 suite 中仍比 CAT 慢约 12%。
  - Forbidden stronger wording: PSO CAAT 对所有 workload 都慢 12%。
  - Uncertainty: 无内置 PSO baseline，且只有 5 个程序。
  - Decision: keep

## 限制

1. suite 很小，不能替代规模扩展实验；
2. 10 次 repetition 是同机重复运行，不是不同机器或随机 seed；
3. 固定 suite 的显著性只说明本机重复运行稳定，不支持总体 workload 分布推断；
4. status 42 的 TSO/PSO Flat Combiner 衡量相同错误发现路径，不是完整搜索；
5. peak RSS 不能隔离 checker heap；需要 allocator/heap profiler 才能比较独占内存。
