# CAAT 决定性诊断（pthread-wmm 固定 15 例）

## 结论

当前主瓶颈不是 RF/CO 候选生成，也不是“已固定 RF/CO 导致 CAT 环”没有被及早学习；
而是每个 GenMC validity query 上重复执行的 CAT 图物化和离线求值。下一轮只应验证如何
减少这部分重复工作，暂停 RF UNSAT core、顶层 Z3 判环、CDCL/DecisionState 和新的 symbolic
lane 实现。

更具体地说，当前 recursive SC CAAT 在这个实际数据集上并未走真正的增量插入：15 例的
活动图都小于 512 个事件，触发 `adaptive offline small-graph epoch`。10 个正常结束的任务中，
439,144 次一致性查询有 292,774 次（66.67%）重新离线初始化，146,360 次（33.33%）仅命中
unchanged；`insert/rollback/rollback-insert/replace` 均为 0。

## 实验边界

- 服务器 Docker 镜像：`genmc15noble:sujie`；未在宿主机运行 GenMC/BenchExec。
- 二进制：`/data3/sujie/svcomp2026-caat/build/genmc-caat-llvm15noble/bin/genmc`。
- 配置：recursive SC CAT、`--cat-stats`，60 CPU s、4 GiB、1 core/task、4 并发。
- 固定面板：5 易 + 5 中 + 5 难，不是完整 283/725 数据集。
- `--cat-stats` 只计时和计数，不改变候选、revisit、搜索顺序或 verdict。

## 正确性门

- Server Release：9 个端到端差分/变异/有限路径测试 + 133 个 CAT/CAAT 核心测试，
  142/142 通过。
- Server GCC 13 ASan+UBSan：8 个端到端测试 + 同一 133 个核心测试，141/141 通过；
  零 sanitizer 报告。
- 面板：10 个已完成任务的 verdict 与冻结基线一致，5 个难例仍为 TIMEOUT；零 incorrect、
  零新增 ABORTED/unsupported。

## 核心计时证据

10 个正常结束任务的观测阶段合计：

| 阶段 | 时间 | 三个被测阶段占比 |
|---|---:|---:|
| CAT validity / consistency | 66.207 s | 79.02% |
| revisit 图恢复 | 17.038 s | 20.34% |
| RF + CO 候选生成 | 0.540 s | 0.64% |

CAT 内部又包含 33.619 s 图物化和 27.168 s offline evaluator。两者之和 60.787 s，
已经解释 CAT 时间的 91.81%。因此继续微调 RF candidate cursor 或 CO enumeration 不可能
产生核心收益。

## 剪枝方向为何应暂停

- 10 个完成例的 439,144 次 CAT 查询全部 accepted，`consistency-rejected=0`。
- 15 个任务截至终止/完成的所有 exploration 快照均为
  `inconsistent-revisit-prefixes=0`。
- 5 个难例在最后一个周期快照中已分别处理约 41k--74k validity queries，仍未记录一个
  inconsistent revisit prefix。

这说明在该面板上，“固定 RF/CO 后已蕴含 CAT 冲突”的可学习样本至少极稀疏。此前 V9
`all-pruned=0`、V10 conflict-core 虽有大量命中却不减少搜索的结果，与本次诊断一致。
因此 RF UNSAT core、顶层判环或把顶层关系交给 Z3，即使单次检查很快，也没有足够拒绝率
来抵消维护 solver/子句/关系编码的成本。

## 唯一保留的下一主方向

做一个 observation-equivalent 的 evaluator A/B，而不是新搜索器：

1. 以结构证书为边界，比较当前 `adaptive-offline<=512` 与关闭该阈值后的真实
   insert/rollback 路径；先确认过去的小图 heuristic 在实际高查询次数任务上是否仍成立。
2. 若增量路径仍慢，优先消除每次完整 `StableGraphSnapshot` 物化：由 ExecutionGraph 的
   revisit/append/rollback 事件直接驱动同步器，保持 bounded checkpoint history，完整 CAT
   evaluator 仍给出每个 query 的权威 verdict。
3. 固定 15 例做 paired A/B；要求 verdict、complete/blocked、work-added/work-popped、
   validity query 数完全相同，再比较 CPU、RSS、materialize/offline/sync 时间。
4. 只有中/难例出现明确收益且 RSS 不恶化，才进入 pthread-wmm 283；否则停止该方向。

这条路线保留 GenMC/TruSt 的最优探索和线性工作栈，不增加第二个路径搜索空间，也不会把
完备性转交给预算受限 solver。

## 局限

BenchExec 对 TIMEOUT 任务终止进程，因此 C++ 析构统计没有落盘；难例只有周期性的
exploration counters，没有完整阶段计时。79.02% 是 10 个正常结束例的精确加权合计，不能
伪装成 5 个 TIMEOUT 的直接 profiler 结果。下一 A/B 应增加独立于析构的周期 phase snapshot，
或使用外部 sampling profiler，但仍不得改变搜索。

## 原始证据

- 实验定义：`decisive-diagnostic-15.xml`
- 启动器：`launch-decisive-diagnostic-15.sh`
- 拉回结果：`server-results/decisive-diagnostic-15-20260718a/`
