# Adaptive-offline 与强制 incremental 配对实验

## 决定

拒绝直接关闭 `<=512` adaptive-offline heuristic。保留默认策略，不把
`--cat-disable-adaptive-offline` 提升为生产优化。

## 方法

- 同一服务器 Docker、同一 Release 二进制、同一 15 例面板。
- baseline 与 candidate 顺序运行；每任务 60 CPU s、4 GiB、1 core，4 并发。
- candidate 唯一差异是 `--cat-disable-adaptive-offline`。
- 搜索、候选、CAT 模型和 verdict evaluator 均未改变。
- Server Release 144/144；GCC 13 ASan+UBSan 143/143，零 sanitizer 报告。

## 结果

- baseline：10 correct terminal + 5 TIMEOUT。
- candidate：9 correct terminal + 6 TIMEOUT；`mix035.oepc.yml` 从 46.72 s 退化为
  TIMEOUT。
- 9 个共同完成任务：CPU 96.359 s -> 127.896 s，`1.3273x`（+32.73%）。
- 共同完成任务的最大 RSS：25.672 MiB -> 25.594 MiB；绝对差异不足以支持内存收益。
- 共同完成任务的探索计数完全一致，包括 work-added/popped、validity queries、realized
  和 inconsistent prefixes。因此回退来自 evaluator，同 TruSt 搜索空间变化无关。

| 合计阶段（9 个共同 terminal） | baseline | candidate | 变化 |
|---|---:|---:|---:|
| CAT validity | 43.898 s | 75.086 s | +71.05% |
| revisit restore | 12.103 s | 12.176 s | +0.60% |
| RF+CO candidates | 0.470 s | 0.477 s | +1.48% |
| materialize | 23.543 s | 23.643 s | +0.42% |
| offline evaluator | 16.829 s | 15.086 s | -10.36% |
| insert attempt | 0 | 7.560 s | 新增 |
| history search | 0 | 25.196 s | 新增 |
| incremental worklist | 0 | 1.162 s | 新增 |
| transactional copy | 0 | 0.933 s | 新增 |

候选转换统计不是预期的 rollback reuse：

- unchanged = 116,756（两模式相同）
- baseline adaptive-offline = 233,553
- candidate insert = 24,026
- candidate rebuild = 209,527
- candidate rollback / rollback-insert / replace = 0

因此强制增量只让约 10.3% 的变化走 insert；其余 89.7% 在付出 insertion/history 搜索后
仍从头 rebuild。最大 snapshot-equivalent bytes 从 304,136 增至 608,272；最大 undo trail
为 243,336 bytes。

## 含义

问题不是阈值选错，而是 GraphSynchronizer 的最近 32-query snapshot history 与 GenMC 的
TruSt work-item/revisit 树没有状态身份关联。普通 snapshot 子集搜索在实际 revisit 上零命中。
继续增大 checkpoint 数可能线性增加完整 base snapshot 内存，却没有命中证据，不能直接做。

下一步先增加 observation-only 原因计数：examined history entries、base-subset matches、
rollback attempts/successes、insert/replace rejection。若 subset matches 始终为零，则需要把
checkpoint token 绑定到 GenMC work item/祖先，而非扩大无身份的 LRU history；若存在匹配但
rollback 失败，再修 checkpoint 生命周期。

## 证据

- 定义：`adaptive-offline-incremental-15.xml`
- 启动器：`launch-adaptive-offline-incremental-15.sh`
- 原始结果：`server-results/adaptive-offline-incremental-15-20260718a/`
