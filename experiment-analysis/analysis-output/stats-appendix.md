# Statistical Appendix

## Unit of analysis

- 固定-suite稳定性：每个 repetition 内汇总相同 10 cases，形成 10 个配对总时间；比较 CAT/CAAT 与同轮 GenMC。
- 跨负载异质性：先取每个 `backend × model × program` 的 10 次中位数，再形成 10 个独立 workload/model cells。
- 300 条进程运行仅用于 cell 描述统计；没有把它们作为 300 个独立 workload 样本。

## Descriptive statistics

完整 cell 统计在 `cell-summary.tsv`，包括 n、mean、SD、median、t-based 95% CI、Q1/Q3、RSS mean/SD 和探索计数。

固定 suite 的每轮总时间：

- GenMC：0.614 s mean。
- GenMC+CAT：0.694 s mean，倍率 1.1303 ± 0.0129 SD，95% CI [1.1211, 1.1395]。
- GenMC+CAAT：2.056 s mean，倍率 3.3486 ± 0.0245 SD，95% CI [3.3311, 3.3661]。

## Inferential tests

时间差使用 two-sided one-sample Wilcoxon signed-rank test，输入为每轮 backend 总时间减同轮 GenMC 总时间。两个 planned contrasts 使用 Holm correction：

| Contrast | raw p | Holm p | rank-biserial effect |
|---|---:|---:|---:|
| CAT − GenMC | 0.00545 | 0.01090 | 1.00 |
| CAAT − GenMC | 0.00570 | 0.01090 | 1.00 |

效应量 1.00 表示 10 轮中差值方向全部为正，不表示开销量级相同。由于 n=10 且存在计时离散化，p 值只作为固定 suite 重复稳定性的辅助证据。

跨 workload cells 的时间倍率用 log scale 几何平均与 t interval 描述；Wilcoxon 结果受大量恰好 1.0 的短用例 ties 影响，因此不作为主要显著性结论。CAT 为 1.062× [0.926, 1.219]；CAAT 为 1.488× [0.813, 2.723]。

RSS 使用 cell median 相对 GenMC 的差值和比率。CAT 的几何平均 RSS 比率为 1.00009 [0.99922, 1.00096]，CAAT 为 0.99977 [0.99911, 1.00042]；Holm 校正检验均不显著（p=1）。因此只能报告“未检测到差异”，不能接受“完全相等”的零假设。

## Assumptions and corrections

- 同一 repetition、model、program 的三 backend 是配对测量；backend 顺序按轮旋转。
- suite ratios 的 t interval 用于重复运行的均值不确定性；未声称 workload 总体正态。
- Wilcoxon 避免依赖差值正态性；ties 使用 normal approximation。
- planned pairwise contrasts 为 CAT vs GenMC、CAAT vs GenMC；分别对 time 与 RSS family 做 Holm correction。
- 未删除异常值。原始 300 行全部保留。

## Validation

- Rows: 300。
- Expected unique keys: 300；duplicate keys: 0。
- Semantic paired groups: 100。
- Status/execution/blocked mismatch groups: 0。
- Missing timing/RSS values: 0。

## PSO supplemental analysis

- Rows: 100；paired groups: 50；semantic mismatch groups: 0。
- Comparison: CAAT vs CAT only；10 repetition-level paired suite totals。
- CAT mean suite time: 0.308 s；CAAT: 0.361 s。
- Mean ratio: 1.1727 ± 0.0335 SD；95% t CI [1.1487, 1.1966]。
- Wilcoxon signed-rank p=0.00548；只有一个预先指定的 PSO contrast，不做多重校正。
- Peak-RSS cell geometric mean ratio: 0.99969；不解释为节省内存。
