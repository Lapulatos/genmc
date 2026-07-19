# GenMC CAT/CAAT 优化 1--11 实验报告

## 结论

已逐项实现或构造受限安全版本并测量优化 1--11；按要求排除优化 12（CAT 模型静态编译）。最终组合保留 1--7、受限的 10，以及修正后的 11；优化 8、9 因稳定回归已从源码回退，但二进制和原始数据保留。

最终 `stage-11c` 相对原始 `stage-0`：

- SC recursive Flat Combiner：CAAT 中位时间 `1.430 -> 0.110 s`，加速 `13.0x`；
- 全部 CAAT 测试单元的每轮总时间均值：`2.470 -> 1.010 s`，加速 `2.45x`；
- CAAT 进程 peak RSS 均值：`52.016 -> 52.006 MiB`，变化 `-0.010 MiB`，低于进程级 RSS 的可靠分辨能力；
- 所有阶段的 150 条主样本均保持退出状态及 complete/blocked execution 计数一致，`semantic_mismatches=0`。

最大的时间收益来自优化 6 的可证明候选剪枝，其次是优化 11 的小图 offline epoch。优化 7 的主要价值是把 retained checkpoint packed storage 峰值降低约 49.7%，不是稳定的时间加速。

## 实验设计

- 模型：SC、TSO、PSO；
- 程序：SB、RMWFix、MS Queue、Treiber Stack、Flat Combiner；
- 后端：CAT、CAAT；
- 每阶段：每个 cell 预热 1 次、正式重复 5 次，共 `3 x 5 x 2 x 5 = 150` 条；
- 单线程：`--nthreads=1 --disable-estimation --disable-mm-detector`；
- 核心压力用例：SC recursive Flat Combiner；原始阶段每次搜索有 327 次一致性查询、176 次 rollback-insert、122 次 rebuild 和 4,489 次 operator evaluation；
- 正确性：递归 CAT/CAAT differential、mutation oracle、随机 insertion/push-pop property，以及 status/complete/blocked executions 差分。

主结果来自 `results/stage-results.tsv`，聚合结果来自 `results/stage-summary.tsv`。下表的 suite 时间是每轮 15 个 CAAT model/program cell 的总时间均值，不是单个程序的平均延迟。

## 逐项结果

| 阶段 | 优化 | SC Flat Combiner 中位时间 | CAAT suite 均值 | CAAT peak RSS 均值 | 决策 |
|---|---|---:|---:|---:|---|
| 0 | 原始基线 | 1.430 s | 2.470 s | 52.016 MiB | 基线 |
| 1 | 非 explanation 模式不复制 `result.values` | 1.470 s | 2.430 s | 51.972 MiB | 保留；时间差在噪声内 |
| 2 | 构造期缓存 dependency adjacency | 1.410 s | 2.366 s | 51.994 MiB | 保留；suite 比基线快 4.4% |
| 3 | opt-in 分项 profiler | 1.410 s | 2.400 s | 52.047 MiB | 保留；普通路径不读时钟 |
| 4 | 直接 stable-ID graph materialization | 1.140 s | 1.968 s | 51.981 MiB | 保留；核心比基线快 25.4% |
| 5 | 只构造模型引用的 primitives | 1.050 s | 1.910 s | 51.975 MiB | 保留；核心累计 1.36x |
| 6 | 精确语义证书下的候选剪枝 | 0.170 s | 1.064 s | 52.019 MiB | 保留；本项核心约 6.18x |
| 7 | checkpoint delta trail | 0.180 s | 0.970 s | 52.009 MiB | 保留；主要是空间收益 |
| 8 | packed bitset copy-on-write | 0.190 s | 1.050 s | 51.981 MiB | 回退；suite 回归 8.2% |
| 9 | alias/union/composition 半朴素传播 | 0.190 s | 1.220 s | 52.038 MiB | 回退；suite 回归 25.8% |
| 10 | 受限 support-aware replacement | 0.180 s | 0.984 s | 52.003 MiB | 保留；主递归模型不适用，时间中性 |
| 11 | 小图 adaptive offline，首轮阈值 64 | 0.190 s | 1.096 s | 51.988 MiB | 阈值未触发，不作为结论 |
| 11b | 小图 adaptive offline，阈值 512（所有模型） | 0.100 s | 0.802 s | 51.969 MiB | 原型；范围过宽，破坏在线路径测试 |
| 11c | 只对认证 SC/TSO adaptive offline | **0.110 s** | **1.010 s** | **52.006 MiB** | 保留；最终组合 |

### 1. 删除无用 explanation-state 复制

普通 consistency query 只读取 violations；只有 `--explain-cat` 才复制完整 predicate values。该改动删除了确定的冗余工作，但当前小图未显示稳定的核心加速。

### 2. 缓存 dependency graph

reverse dependency adjacency 从每次 `tryInsert()` 构造改为 evaluator 构造一次。suite 从 2.470 降到 2.366 秒，方向性收益 4.4%。

### 3. 分项 profiling

原始 SC Flat Combiner 的同步时间约 1,264.6 ms：stable materialization 182.1 ms、insertion attempts 171.2 ms、history 路径 840.6 ms、rebuild 60.0 ms；worklist 461.0 ms。checkpoint/copy/rollback 合计只有 18.6 ms，因此后续优先处理 materialization 和候选数。

### 4. 直接 stable-ID materialization

online CAAT 直接从 `ExecutionGraph` 构造持久 stable-ID snapshot，消除 dense `GraphAdapter` 及第二次 remap。materialization 约 `182.1 -> 55.1 ms`，核心中位时间 `1.43 -> 1.14 s`。当前实现仍按 query 重建所需 base relations，并非完整事件/边 delta adapter；这是该项的明确限制。

### 5. primitive 使用掩码

normalized base predicates 在构造期生成 bool 使用掩码，不再构造 SC 未引用的集合/关系。一次负探针把 `std::set::contains` 放进 event-pair 内层循环，materialization 回归到约 360 ms，已拒绝；最终 bool mask 版本将核心中位时间降到 1.05 秒。

### 6. 可证明安全的候选剪枝

只有完整 deterministic normalized summary 精确匹配 bundled recursive SC/TSO 模型时，才委托生成 checker 做 coherent-store、revisit 和 co-placement 剪枝。模型重命名、语句调整、任一语义改动、自定义模型或 PSO 都 fail closed 到通用枚举。`--cat-oracle` 显式关闭剪枝，以保持 mutation stress 覆盖候选全集。

SC Flat Combiner 的 query `327 -> 43`、rollback-insert `176 -> 10`、rebuild `122 -> 12`、operator evaluation `4,489 -> 421`，这是最大单项收益。

### 7. checkpoint delta trail

checkpoint 从完整 base/result snapshot 改为 O(1) trail marker；每个 insertion 只记录新增 facts、evaluation-count delta 和旧 violation metadata。rollback 反向删除 facts 并缩小 universe。

- peak packed undo：368,744 B；
- 旧完整 snapshot 等价峰值：733,280 B；
- retained checkpoint packed storage 减少 49.7%；
- rollback 时间约 `5.1 -> 21.7 ms`，说明空间节省换来了撤销成本；
- 进程 peak RSS 约 52 MiB，无法可靠显示 0.35 MiB 级 checker 内部差异。上述 packed-byte 对比只计算 evaluator restoration state，不包含 `HistoryEntry` 的 base-map 副本、容器/string overhead 和 allocator metadata。

### 8. copy-on-write bitsets（拒绝）

`EventSet`/`Relation` 复制共享 packed words、修改时 detach。transactional copy 约 `0.83 -> 0.58 ms`，下降 30%；但引用计数与 detach 进入高频 Value 操作，suite 回归 8.2%。源码已回退。

### 9. 半朴素 worklist（拒绝）

实现 alias、union、composition 的 exact insertion-delta 规则；365/421 次 operator evaluation 命中半朴素路径。per-operand delta 的分配和合并成本在小 relation 上更高，suite 回归 25.8%。源码已回退。若未来图规模明显增大，应改成原地 row/word delta，而不是独立 `Value` 对象。

### 10. support-aware replacement

实现了安全受限版本：只有无递归且 derived operators 全为 alias/union 的模型才能局部处理 rf/co replacement。union 会检查全部 operand support，因此 tuple 在最后一个支持消失前不会被删除。composition、closure 或 recursive SCC 仍 fail closed 到 history/rebuild。

bundled recursive SC/TSO/PSO 不满足该证书，所以主 benchmark 时间中性。完整 recursive support-counted derivation graph 尚未实现；在当前剪枝后的核心用例中只剩 12 次 rebuild、合计约 4 ms，潜在收益小于实现和证明成本。

### 11. adaptive online/offline backend

首轮使用 persistent event universe `<=64`，没有 query 命中，属于无效阈值实验。修正为 `<=512` 后，SC Flat Combiner 的 34 个 changed queries 使用 offline epoch：

- online insert/rollback/rebuild 路径：sync 约 96.9 ms；
- adaptive offline：sync 约 16.7 ms；
- offline evaluation 35 次，总计约 11.3 ms；
- 核心中位时间 `0.18 -> 0.10 s`。

最初把该策略用于所有 CAAT 模型会绕过自定义模型的在线 insertion 覆盖，因此最终 stage 11c 进一步要求模型通过优化 6 的精确 SC/TSO 证书，并在 `--cat-oracle` 下禁用。PSO 和自定义模型继续使用 online/history/delta-trail。收紧后核心中位时间为 0.11 s，suite 为 1.010 s。阈值来自当前工作负载，扩大程序规模后应重新标定，或用在线移动平均代替固定值。

## 时空权衡

- 时间：最终主要通过减少候选 query（优化 6）和为小图选择更便宜的 offline evaluator（优化 11）获得，而不是微调 bitset copy。
- checker 内部空间：优化 7 把 retained packed state 峰值减少约一半；优化 11 的小图 offline epoch 进一步令该用例 `peak-undo-bytes=0`，完整 snapshot equivalent 只保留当前 epoch 约 193,688 B。
- 进程空间：LLVM、GenMC graph 和公共运行时占约 52 MiB，掩盖了几百 KiB 的 checker 差异；因此报告内部 packed bytes 与 process peak RSS 两套指标。
- 负优化：8、9 的局部计时改善不能抵消管理开销，说明当前测试规模下应优先减少 query/rebuild 数，而不是引入更复杂的通用增量数据结构。

## 可复现文件

- `run-stage-benchmark.sh`：150-row 阶段基准；
- `collect-stage-stats.sh`：内部 counters/timers；
- `analyze-stages.R`：聚合与语义计数核对；
- `results/stage-results.tsv`：全部原始性能记录；
- `results/stage-summary.tsv`：阶段汇总；
- `binaries/stage-*.genmc`：各阶段冻结二进制（实际文件名为 `stage-*-genmc`）。

共保存 14 个阶段标签、2,100 条正式性能记录；每个标签严格为 150 条。

## 最终验证

- RelWithDebInfo build：通过；仅有仓库既有的 `inconsistent-missing-override` warnings；
- `git diff --check`：通过；
- 141 项 unit tests：全部通过；
- 5 项 CAT/CAAT integration（SC、TSO、PSO、recursive differential、mutation stress）：全部通过；
- 全量 CTest 首轮：152 项中 148 项通过；3 个 adaptive/oracle 路径测试暴露策略范围过宽，收紧证书/禁用 oracle adaptive 后已分别重跑通过；
- 唯一未通过项：`run-parallel`，原因是当前仓库不存在 CTest 配置引用的 `scripts/run-parallel.sh`，与本次 CAT/CAAT 修改无关；
- `fast-driver`、10 轮 `randomize-driver`、relinche impl/spec 均通过。

## 限制

1. 主性能集是小到中等微基准；结论对大型应用和数百事件 relation 的外推仍需规模曲线。
2. macOS `/usr/bin/time` 的 wall time 粒度为 0.01 秒，每 cell 只有 5 次重复；小于约 5% 的差异不应视为稳定提升。
3. Stage 4 是 direct stable rebuild，不是完整 edge-delta adapter。
4. Stage 10 是受限 support-aware closure，不是递归模型的通用 derivation support counting。
5. Stage 11 的 512 阈值需要在更大程序集上交叉验证。
