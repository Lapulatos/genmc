# TSO 五重复实验结论

## 覆盖与正确性

- 数据为 96 个 `unreach-call` 任务、3 种方法、5 次轮换顺序重复，共
  1,440 个 BenchExec 运行；每任务限制为 1 核、60 s、4 GB。
- GenMC 每轮完成 96/96；CAT 每轮完成 82/96；CAAT 每轮完成 89/96。
- CAT 每轮有 9 个 timeout 和 5 个 OOM；CAAT 每轮有 2 个 timeout 和
  5 个 OOM。五轮失败集合完全稳定。
- 三者共同完成的 82 个任务对应 410 个任务-重复单元。日志回填后，三者的
  verdict、complete-execution count 和 blocked-execution count 全部一致；缺失
  count 和 mismatch 均为 0。因此共同完成任务上的额外开销不是由探索更多
  execution 导致的。

## 配对时间与空间开销

单位是任务；每个任务先取五次重复的中位数，再计算配对几何平均和 task
bootstrap 95% CI。失败不进入 solved-only 比值，但保留在覆盖图和 performance
profile 中。

- CAAT/GenMC：wall `1.34973x`（95% CI `1.14324–1.63094`），CPU
  `1.36491x`，peak RSS `1.08515x`，`n=89`。
- CAT/GenMC：wall `1.30844x`（95% CI `1.09610–1.62978`），CPU
  `1.33966x`，peak RSS `1.04671x`，`n=82`。
- CAAT/CAT：wall `0.854544x`（95% CI `0.745459–0.952066`），CPU
  `0.842517x`，peak RSS `0.995502x`，`n=82`。

在三者共同完成的 82 个任务上，CAAT 比 CAT 平均快约 14.5%，而普通任务的
内存几乎相同。CAAT/GenMC 的聚合比值略高于 CAT/GenMC，不能解释为 CAAT
整体更差：CAAT 还完成了 CAT timeout 的 7 个更难任务，两个比值的任务集合
不同。

## 整套运行成本（包含资源失败前的消耗）

每轮观测 wall time 的五轮均值为 GenMC `38.312 s`、CAT `712.324 s`、CAAT
`325.236 s`。因此整套观测 wall cost 为 CAT/GenMC `18.593x`、CAAT/GenMC
`8.489x`、CAAT/CAT `0.4566x`。这些比值包含 timeout/OOM 运行到停止点的成本，
用于表示实际批量实验吞吐，不是未删失的算法运行时间比。

## 长任务案例

- `queue_ok_longer`：GenMC `0.091 s / 24.9 MiB`，CAT
  `27.942 s / 458.5 MiB`，CAAT `3.368 s / 298.8 MiB`。CAAT 相对 CAT
  同时降低时间和内存，但仍明显高于内置 GenMC。
- `queue_ok_longest`：GenMC `0.117 s / 24.9 MiB`；CAAT完成于
  `19.789 s / 1149.7 MiB`；CAT 在约 `61.047 s / 862.6 MiB` 时 timeout。
  timeout 的 RSS 只代表停止点进度，不能据此断言 CAT 完整执行会比 CAAT 省内存。
- `fib_safe-6`：GenMC `4.249 s`，CAAT `46.222 s`，CAT timeout。
- `triangular-1`：GenMC `1.741 s`，CAAT `6.337 s`，CAT timeout。
- `queue_longer`：GenMC `4.240 s` 找到错误；CAT 和 CAAT 均 timeout。

## 可支持的结论与限制

- 支持：TSO 下 CAAT 显著改善 CAT 的困难任务覆盖和整套吞吐，并在共同完成任务
  上降低约 14.5% wall time；两者仍比内置 GenMC 有明显关系求值开销。
- 支持：普通小任务的 RSS 差异很小，空间问题集中在较大执行图和五个稳定 OOM
  任务，而非固定启动开销。
- 不支持：仅凭当前计数判断具体开销来自 stable-event 膨胀、基础关系密度还是
  history copy。该机制归因需要后续 `--cat-stats` 诊断批次。
- 不支持：把 timeout 时的 RSS 当作完整运行峰值进行严格排序。
