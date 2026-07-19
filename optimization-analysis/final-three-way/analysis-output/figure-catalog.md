# Figure Catalog

## Figure 1 — normalized time

- Files: `figures/figure-01-normalized-time.pdf`, `.svg`
- Purpose: 展示每个 SC/TSO program cell 的 CAT/CAAT 中位 wall-time 相对 GenMC 倍率。
- Data: `normalized-cell-ratios.tsv`。
- Reader should notice: 大多数短 cell 量化为 1×；SC Flat Combiner 上 CAT 1.89×、CAAT 1.22×。
- Decision impact: 优化后 CAAT 的主要剩余 SC 开销已小于 CAT；继续优化应转向 PSO和规模扩展，而不是短 litmus。
- Caveat: y 轴倍率来自 0.01 s 精度的中位时间。

## Figure 2 — Flat Combiner distribution

- Files: `figures/figure-02-fcombiner-distribution.pdf`, `.svg`
- Purpose: 对比三种方法在最有区分度 workload 上的 10-run wall-time 分布。
- Data: `raw-results.tsv` 中 Flat Combiner rows。
- Reader should notice: SC 下 GenMC < CAAT < CAT；TSO 三者接近。
- Decision impact: 认证剪枝和 adaptive offline 已逆转优化前 CAAT 远慢于 CAT 的关系。
- Caveat: TSO Flat Combiner status=42，表示相同错误发现路径。

## Figure 3 — memory overhead

- Files: `figures/figure-03-memory-overhead.pdf`, `.svg`
- Purpose: 展示 CAT/CAAT 相对 GenMC 的 cell-level peak RSS delta。
- Data: `normalized-cell-ratios.tsv`。
- Reader should notice: delta 围绕 0 波动，量级小于 0.1 MiB。
- Decision impact: process RSS 不足以判断 checker 内部数据结构空间；需要 heap profiling。
- Caveat: 包含 LLVM、程序加载和 GenMC 公共内存。
