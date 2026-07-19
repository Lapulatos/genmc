# Statistics Appendix

## Data validation

- SC/TSO rows: 300; backends: GenMC, CAT, CAAT; repetitions: 10; cells/backend: 10。
- PSO rows: 100; backends: CAT, CAAT; repetitions: 10; cells/backend: 5。
- Duplicate comparison keys: 0。
- SC/TSO semantic contract: 100/100 paired groups have identical status, complete executions and blocked executions。
- PSO semantic contract: 50/50 paired groups一致。

## Unit of analysis

主 suite 推断以 repetition 为单位：每轮先对 10 个固定 SC/TSO cases 求和，再比较 backend。逐 cell 宏分析以 10 个 `model × program` cells 为描述单位。不能把 300 个进程运行当作 300 个独立 workload。

## Descriptive statistics

| Backend | Suite wall mean | Suite CPU mean | Mean process RSS | Complete executions/round |
|---|---:|---:|---:|---:|
| GenMC | 0.621 s | 0.493 s | 50.937 MiB | 59 |
| CAT | 0.719 s | 0.606 s | 50.938 MiB | 59 |
| CAAT | 0.645 s | 0.541 s | 50.928 MiB | 59 |

## Paired suite ratios

- CAT/GenMC wall ratio: mean 1.1579, SD 0.0333, t-based 95% CI [1.1341, 1.1817]。
- CAAT/GenMC wall ratio: mean 1.0391, SD 0.0318, 95% CI [1.0164, 1.0618]。
- CAAT/CAT wall ratio: mean 0.8977, SD 0.0259, 95% CI [0.8792, 0.9162]。
- CAT/GenMC CPU ratio: mean 1.2305, 95% CI [1.2031, 1.2579]。
- CAAT/GenMC CPU ratio: mean 1.0987, 95% CI [1.0615, 1.1359]。
- CAAT/CAT CPU ratio: mean 0.8928, 95% CI [0.8710, 0.9147]。

对 suite wall-time delta 使用 one-sample Wilcoxon signed-rank test；CAT/GenMC Holm p=0.0117，CAAT/GenMC Holm p=0.0125。由于 n=10 且时间量化为 0.01 s，p 值只作为重复稳定性的辅助证据；效应大小和原始分布优先。

## Cell-level macro results

- CAT/GenMC median-time ratio geometric mean: 1.095 [0.949, 1.263] across 10 cells。
- CAAT/GenMC: 1.032 [0.982, 1.085]。
- RSS ratio geometric means: CAT 1.00007，CAAT 0.99992；Holm-adjusted p=1 for both。

Cell-level intervals跨 workload，受短用例 0.01 s 量化和 workload 异质性影响，不应用来否定固定 suite 的 paired result。

## PSO

- CAT suite: 0.325 s；CAAT suite: 0.364 s。
- CAAT/CAT mean ratio 1.1237，SD 0.0635，95% CI [1.0782, 1.1691]。
- paired delta Wilcoxon p=0.00570。
- RSS ratio geometric mean 0.99970。

## Multiple comparisons and assumptions

- 两个相对 GenMC 的主 wall-time contrasts 使用 Holm 校正。
- CAAT/CAT 是直接决策 contrast，单独报告配对 ratio CI。
- t-based CI用于 repetition ratio 的均值；n=10 时正态假设证据有限，因此同时保留 Wilcoxon、SD、原始 TSV。
- 未对逐程序 10 组短时间做显著性 winner 声明，以避免量化、低功效和多重比较问题。
