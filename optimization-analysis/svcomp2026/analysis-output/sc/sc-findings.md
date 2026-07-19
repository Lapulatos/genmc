# SC 五轮配对实验阶段性分析

## 结论

在 96 个经 GenMC-SC 普查确认正确的 SV-COMP 2026 `unreach-call`
任务上，内置 GenMC 的覆盖率和扩展性明显最好。CAAT 比 CAT 更快且多完成
5 个任务，但尚未消除 CAT/CAAT 共有的 5 个 4 GB OOM。

## 覆盖率和总资源消耗

| 方法 | 每轮完成 | 每轮 timeout | 每轮 OOM | 每轮观测 wall time 均值 |
|---|---:|---:|---:|---:|
| GenMC | 96/96 | 0 | 0 | 37.96 s |
| GenMC+CAT | 85/96 | 6 | 5 | 555.06 s |
| GenMC+CAAT | 90/96 | 1 | 5 | 240.33 s |

相对 GenMC，包含 timeout/OOM 运行到停止点的整套观测 wall time，CAT 为
14.62 倍，CAAT 为 6.33 倍。这个数描述处理整批任务的实际预算，不等价于
“共同完成任务上的算法倍率”。

失败集合在 5 次重复中完全一致。CAT 和 CAAT 都在以下 5 个
`goblint-regression/28-race_reach` 任务达到 3,999,997,952 B 后 OOM：

- `01-simple_racing`
- `06-cond_racing1`
- `11-ptr_racing`
- `24-sound_lock_racing`
- `37-indirect_racing`

两者都在 `pthread/queue_longer` timeout。CAT 还在
`circular_buffer_ok`、`fib_safe-{5,6}`、`fib_unsafe-{6,7}` timeout，而
CAAT 完成了这 5 个任务。

## 共同完成任务上的配对开销

每个任务先取 5 次重复的中位数，再把“任务”作为独立配对单位：

| 对比 | 配对任务 | wall 几何均值比 | task-bootstrap 95% CI | CPU 比 | peak RSS 比 |
|---|---:|---:|---:|---:|---:|
| CAT / GenMC | 85 | 1.387 | [1.141, 1.742] | 1.409 | 1.076 |
| CAAT / GenMC | 90 | 1.289 | [1.120, 1.520] | 1.301 | 1.062 |
| CAAT / CAT | 85 | 0.854 | [0.753, 0.949] | 0.849 | 0.990 |

因此，在 CAT 和 CAAT 都完成的任务上，CAAT 的 wall time 几何均值比 CAT
低约 14.6%，CPU 低约 15.1%。经三个预设 contrast 的 Holm 校正后，精确
sign test 未达到 0.05；这是因为大量约 0.06 秒任务的胜负容易受启动噪声影响。
倍率 CI 和长任务的绝对差异更适合判断工程效果。

典型长任务上，CAAT 相对 CAT 的改进更大：

- `fib_unsafe-5`: 20.02 s -> 1.17 s
- `triangular-1`: 46.56 s -> 3.90 s
- `triangular-2`: 10.32 s -> 0.93 s
- `queue`: 0.742 s -> 0.076 s
- `queue_ok_longest`: 21.05 s -> 14.96 s

但 CAAT 仍明显慢于内置 GenMC，例如 `queue_ok_longest` 为 14.96 s 对
0.118 s，`fib_unsafe-7` 为 36.04 s 对 5.35 s。

按内置 GenMC 的任务中位时间分桶后，扩展趋势更清楚：

| GenMC 任务时间桶 | 任务数 | CAT 完成 | CAAT 完成 | CAT/GenMC wall | CAAT/GenMC wall |
|---|---:|---:|---:|---:|---:|
| < 0.1 s | 79 | 79 | 79 | 1.138x | 1.063x |
| 0.1--1 s | 5 | 3 | 5 | 68.50x | 7.50x |
| 1--10 s | 12 | 3 | 6 | 5.15x | 3.77x |

所以“多数短任务接近 GenMC”不能外推到较长任务；当前剩余问题恰好集中在
探索较大的程序上。0.1--1 秒桶只有 5 个任务且包含队列极端值，倍率用于定位
扩展趋势，不作为总体效应的独立估计。

## 内存

普通短任务三者的进程 peak RSS 中位数都约 26 MB。差异集中在大队列任务：

- `queue_ok_longest`: GenMC 26.1 MB，CAT 970.1 MB，CAAT 613.3 MB；
- `queue_ok_longer`: GenMC 26.1 MB，CAT 254.0 MB，CAAT 164.5 MB；
- `queue_longer`: 三者均 timeout；GenMC/CAT/CAAT 单轮 RSS 约
  1,041/204/457 MB，不能只按 RSS 排序，因为三者在同一停止时间到达了不同
  探索进度。

CAAT 在两个可完成的大队列任务上比 CAT 少约 35%--37% peak RSS，但共有的
5 个 OOM 表明根本的稠密关系/事件域扩展问题仍存在。

## 正确性与限制

- 85 个三者共同完成任务的 verdict 完全一致。
- SC XML 的自定义 execution 列因初版 BenchExec adapter 未实现
  `get_value_from_output` 而缺失，现已从校验通过的原始 log ZIP 回填。三者共同
  完成的 425 个“任务×重复”单元中，verdict、complete executions 和 blocked
  executions 全部一致；缺失计数和 mismatch 均为 0。证据见
  `exploration-equivalence.json` 和只有表头的 `exploration-mismatches.tsv`。
- TSO 进程在适配器修复前已经启动，也将从日志回填；PSO 会直接写入新列。
- 当前规模列来自 YAML `.i` 文件，不符合实际执行的 `.c`。清单代码已修正，
  待核心计时结束后在远端重新生成 `.c` 及递归本地头文件规模。
- 以上 solved-only 比率不掩盖失败；coverage 表和 failure-aware performance
  profile 是共同主证据。
