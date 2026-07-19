# GenMC / GenMC+CAT / GenMC+CAAT 性能分析

## 结论

在本机固定的 10-case suite（SC/TSO × 5 programs）上，GenMC+CAT 的端到端总时间相对内置 GenMC 增加 **13.0%**，GenMC+CAAT 增加 **234.9%（3.35×）**。CAAT 的总体开销几乎全部由 SC 下的异步 flat-combining 用例触发：该 cell 的中位时间为 GenMC **0.09 s**、CAT **0.165 s**、CAAT **1.44 s**。

峰值 RSS 没有检测到实质差异。300 次运行的 backend 平均峰值约为 GenMC **50.933 MiB**、CAT **50.943 MiB**、CAAT **50.934 MiB**；cell-level RSS 比率的 95% CI 均紧贴 1。当前数据支持“时间开销负载相关、内存开销在测量噪声内”，不支持“CAAT 普遍节省时间或内存”。

## 实验合同

- GenMC：当前二进制的内置 `--sc` / `--tso` checker。
- GenMC+CAT：`models/cat/sc.cat` / `tso.cat`，非递归 Phase 1 evaluator。
- GenMC+CAAT：`models/cat/recursive-sc.cat` / `recursive-tso.cat`，Phase 3 online/incremental evaluator。
- 负载：SB、RMWFix、dynamic MS queue、dynamic Treiber stack、async flat combiner。
- 重复：每个 cell 预热 1 次后测量 10 次；共 300 条正式样本。
- 控制：同一 `RelWithDebInfo/bin/genmc`、单 worker、关闭 estimation 和 MM detector；每轮旋转 backend 顺序。
- 指标：完整命令 wall time、user/sys time、process peak RSS、complete/blocked executions。

三条路径在全部 100 个 `model × program × repetition` 配对组中具有相同的退出状态、完整 execution 数和 blocked execution 数。因此性能比较没有混入不同探索结果。

## 主要结果

| 固定 suite（每轮 10 cases） | 平均总时间 | 相对 GenMC | 95% CI | Holm 校正 p | 配对秩二列相关 |
|---|---:|---:|---:|---:|---:|
| GenMC | 0.614 s | 1.00× | — | — | — |
| GenMC+CAT | 0.694 s | 1.130× | [1.121, 1.139] | 0.0109 | 1.00 |
| GenMC+CAAT | 2.056 s | 3.349× | [3.331, 3.366] | 0.0109 | 1.00 |

这里的 CI 和检验回答的是“在这套固定 workload suite 上重复运行时，额外开销是否稳定”，不能外推为所有 GenMC 程序的总体分布。

跨 10 个 `model × program` cells 做宏平均时，CAT 的几何平均中位倍率为 **1.06×** [0.93, 1.22]，CAAT 为 **1.49×** [0.81, 2.72]。区间较宽说明负载异质性很强；尤其 CAAT 的 SC flat-combiner 16× cell 会被许多 0.05–0.08 s 的量化相等 cell 稀释。

## 负载分解

- SC flat combiner：GenMC 0.090 s，CAT 0.167 ± 0.008 s，CAAT 1.447 ± 0.023 s。中位倍率分别为 1.83× 和 16.0×。
- TSO flat combiner：GenMC 0.080 s，CAT 0.082 ± 0.004 s，CAAT 0.106 ± 0.005 s。CAAT 中位倍率为 1.375×。
- MS queue 与 Treiber stack：CAT 中位时间通常与内置路径相同；CAAT 多为 1.17–1.33×。
- SB 与 RMWFix：多数中位数相同，受 `/usr/bin/time` 的 0.01 s 显示粒度限制，不能据此声称零开销。

这说明 Phase 3 的成本不是稳定常数。递归 SC flat-combiner 的图变化会放大 checkpoint 全量状态复制、worklist 传播和 rebuild-heavy 分支的成本；该解释与 `doc/cat/phase-3-report.md` 已记录的机制和先前 1.29 s 结果一致，但本实验没有单独计量各内部机制的 CPU 占比，因此它仍是机制推断，不是 profiler 证明。

## PSO 补充二方比较

原生 GenMC 没有 PSO checker，因此 PSO 只比较 GenMC+CAT 与 GenMC+CAAT，不进入上述三方结论。在相同 5 programs、预热 1 次、10 次重复下：

| PSO 固定 suite（每轮 5 cases） | 平均总时间 | CAAT/CAT | 95% CI | p |
|---|---:|---:|---:|---:|
| GenMC+CAT | 0.308 s | 1.00× | — | — |
| GenMC+CAAT | 0.361 s | 1.173× | [1.149, 1.197] | 0.00548 |

PSO 下 CAAT 相对 CAT 慢约 **17.3%**；主要 cell 中位倍率为 flat combiner 1.375×、Treiber stack 1.333×、MS queue 1.167×，两个 litmus 在 0.01 s 粒度下均为 1.0×。RSS 几何平均比率为 **0.9997×**，仍无实质差异。100 条样本中两路径的退出状态、complete executions 和 blocked executions 全部一致。

## Claim Candidates

- Claim:
  - Source evidence: `raw-results.tsv` 的 300 条样本；`suite-total-summary.tsv`。
  - Allowed wording: 在本机固定 10-case suite 中，CAT 增加约 13% 总时间，online CAAT 增加约 2.35 倍额外时间（总计 3.35×）。
  - Forbidden stronger wording: CAAT 对所有 GenMC workloads 都慢 3.35×。
  - Uncertainty: 仅 Apple M2、单 worker、SC/TSO、5 个程序。
  - Next check: 扩展到更大图和多 worker，并用高分辨率计时与 profiler 分解热点。
  - Decision: keep

- Claim:
  - Source evidence: `normalized-cell-ratios.tsv` 和 `pairwise-summary.tsv`。
  - Allowed wording: 三条路径的 process peak RSS 在当前约 50–52 MiB 的端到端测量中无可辨识差异。
  - Forbidden stronger wording: CAAT 不需要额外内存，或三种 checker 内存完全相同。
  - Uncertainty: 公共 LLVM/GenMC 启动内存主导，process peak RSS 无法隔离 checker heap。
  - Next check: 对 warmed long-lived process 做 allocator/heap profiling。
  - Decision: keep

- Claim:
  - Source evidence: SC flat-combiner 10 次重复和 `figure-02-fcombiner-distribution.pdf`。
  - Allowed wording: online CAAT 的主要回归集中在至少一个 mutation-heavy workload，SC flat combiner 的中位数为内置路径的 16×。
  - Forbidden stronger wording: 所有递归 SC 程序都会出现 16× 回归。
  - Uncertainty: 目前只有一个用例表现出该数量级。
  - Next check: 用 `--cat-stats` 加 profiler 将回归关联到 rebuild、checkpoint copy 与 predicate worklist。
  - Decision: keep

## 风险与限制

- GenMC 没有内置 PSO checker，所以严格三方比较只覆盖 SC/TSO；PSO 不应伪装成三方 baseline。
- 当前二进制报告 GenMC v0.17.0、LLVM 20.1.7，内嵌 commit `d890211`；仓库 HEAD 为 `0171e63`，后续提交主要是测试和交付记录。复现实验应保存二进制 hash，而不仅依赖版本字符串。
- macOS `/usr/bin/time -lp` 的 wall-time 输出只有 0.01 s 粒度，短用例的 cell-level 显著性不足。
- 退出状态 42 的程序仍纳入，因为三条路径状态与探索计数一致；它们衡量的是相同错误发现路径的端到端成本。

## 建议

P0 优先优化 SC flat-combiner 路径：用 Instruments 或 `sample` 对该单一 cell 做 CPU/heap profile，并同时开启 `--cat-stats` 关联 rebuild 与 worklist counters。短 litmus 暂不值得继续加重复次数；计时分辨率和固定启动开销会限制收益。
