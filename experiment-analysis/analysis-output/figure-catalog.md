# Figure Catalog

## Figure 1 — normalized wall time

- Files: `figures/figure-01-normalized-time.pdf`, `.svg`
- Purpose: 展示每个 model/program cell 中 CAT 与 CAAT 相对内置 GenMC 的中位 wall-time 倍率。
- Data source: `normalized-cell-ratios.tsv`。
- Caption requirements: y 轴为 log scale；基线 1；每个点基于 10 次运行中位数；短 cell 受 0.01 s 量化影响。
- Key observation: CAT 多数接近 1，仅 SC flat combiner 明显上升；CAAT 的最大回归同样集中在 SC flat combiner。
- Interpretation: 性能成本具有强 workload/model interaction，单一总倍率不能描述所有 cells。
- Caveat: 点不是误差条；精确分布见 cell summary 与 Figure 2。

## Figure 2 — flat-combiner distribution

- Files: `figures/figure-02-fcombiner-distribution.pdf`, `.svg`
- Purpose: 展示主导总体差异的 flat-combiner cell 在 SC/TSO 下的 10 次 wall-time 分布。
- Data source: `raw-results.tsv`。
- Caption requirements: log-scale wall time；箱体为 IQR、中线为 median、whisker 使用 R 默认 1.5×IQR。
- Key observation: SC/CAAT 分布稳定在约 1.44 s，远离 SC/CAT 约 0.165 s 与 SC/GenMC 0.09 s；TSO 差距较小。
- Interpretation: Phase 3 优化目标应先定位 SC mutation-heavy path，而不是平均优化所有短 litmus。
- Caveat: 单个 workload 不能代表全部数据结构程序。

## Figure 3 — peak RSS overhead

- Files: `figures/figure-03-memory-overhead.pdf`, `.svg`
- Purpose: 比较 10 个 cells 的中位 peak RSS 相对 GenMC 的 MiB 差值。
- Data source: `normalized-cell-ratios.tsv`。
- Caption requirements: 0 线代表内置 GenMC；process peak RSS 包含公共 LLVM/GenMC 内存。
- Key observation: 差值围绕 0，数量级约为数十 KiB，远小于约 51 MiB 的进程峰值。
- Interpretation: 当前端到端 RSS 不能分辨 checker-specific memory overhead。
- Caveat: 不等价于 heap profile，也不能证明零额外分配。
