# 面向 SV-COMP 2026 C.Concurrency 的 CAAT 扩展与优化分析

## 结论

现阶段不宜直接把全部 C.Concurrency 当成一组性能样例运行。官方验证结果口径包含 3,124 个有效任务（2,520 true、604 false），并混合 reachability、overflow、data-race 等性质。第一轮应先做“可解析、可建模、可完成”的兼容性普查，再对 GenMC、GenMC+CAT、GenMC+CAAT 做同任务配对比较。

当前最值得优先优化的不是继续微调固定的 512 事件阈值，而是：

1. **稳定事件域压缩**：稳定 ID 只增不减，每个稠密关系占 `N * ceil(N/64) * 8` 字节；大规模回溯可能使历史事件数远大于当前活跃事件数。
2. **基础关系增量物化**：当前每次查询重新生成基础关系，`po/loc/int/ext` 对活跃事件执行两层循环。
3. **历史状态去重与索引**：每个历史项复制完整 `BaseValues`，回溯候选又逐项运行平方级关系 subset 检查。
4. **删除与替换的局部传播**：目前通用路径主要支持插入，替换仅支持无环 alias/union；`rf/co` 变化及递归关系通常退化为整模型重建。

## 数据集进入条件

### 任务分层

每个任务至少记录 property、预期结果、编译结果、pthread/C 特性、前端支持结果、适用内存模型和最终状态。最终状态必须区分 `completed`、`timeout`、`OOM`、`unsupported` 和 `internal error`。

GenMC 的 SC/TSO/PSO 一致性检查不能自动覆盖 SV-COMP 的全部性质。例如 `no-data-race` 需要单独的性质判定，`no-overflow` 也不是 CAT 一致性检查本身。首轮主实验建议使用 GenMC 能正确表达的 `unreach-call` 子集，其他性质作为兼容性扩展组单列。

### 分层抽样

| 维度 | 建议分桶 |
|---|---|
| 静态规模 | LOC、函数数、循环数 |
| 并发规模 | 创建线程数、最大活跃线程数 |
| 内存事件 | 读/写/RMW/fence 数 |
| 探索规模 | consistency queries、explored executions |
| CAAT 规模 | active events、stable events、最大关系密度 |
| 难度 | <1 s、1–10 s、10–60 s、60–300 s、timeout/OOM |

先用每桶 20–50 个任务校准超时和日志，再运行全部兼容任务。源文件大小不能单独代表模型检查成本，执行图与分支数量更关键。

## 当前实现的主要瓶颈

### P0：稳定事件域与稠密关系相乘

`Relation` 的裸存储为：

```text
bytes(Relation) = N_stable × ceil(N_stable / 64) × 8
```

单个关系在 10,000 个稳定事件时约占 12.0 MiB；20 个同规模关系约 240 MiB，尚未计入临时值、历史基础关系和撤销增量。当前物化使用只增长的 `keys_.size()`，回溯不会降低宇宙大小。

**建议：epoch compaction。** 在整模型 rebuild、历史清空，或稳定域/活跃域比值及预计关系字节超过阈值时，把当前活跃事件重新编号为紧凑域，同时清空失效 checkpoint/undo trail。触发器同时观察 `N_stable/N_active`、预计稠密字节与近期 rebuild 率。

### P0：基础关系每次全量重建

直接物化避免了二次 remap，但仍每次新建所需 `EventSet/Relation`。`po/loc/int/ext` 遍历事件对，`fr` 还执行 `inverse(rf);co`。

**建议：维护 primitive delta。** 对事件添加/撤销、rf 替换、co 插入/删除生成边增量；`po` 按线程维护顺序索引，`loc/co` 按地址维护索引；`int/ext` 优先改为谓词过滤或惰性视图，避免显式全矩阵；`fr` 从 rf/co 增量派生。

### P0：历史项仍复制完整基础值

撤销 trail 降低了 evaluator checkpoint 的复制量，但 `HistoryEntry` 仍保存完整 `BaseValues`。保留上限固定为 32，没有按字节预算或相似性去重。历史搜索对每个候选调用 `baseSubset()`，关系部分逐 `(from,to)` 检查。

**建议：** HistoryEntry 保存父 checkpoint、primitive delta、结构哈希和事件数；用 mutation stamp/fingerprint 快速排除候选，仅对剩余候选做 word-wise 精确校验；按字节和命中率淘汰 checkpoint，并记录命中距离。

### P1：增量更新仍有全状态事务复制

`tryInsert()` 复制全部结果值和计数，把所有值 grow 到新事件域。每个受影响算子又重算完整结果，再做 subset、difference 和赋值。这是“增量调度 + 全值重算”，还不是算子级 delta evaluation。

此前 whole-value COW 或独立 `Value` delta 在小图上容易被分配、引用计数和 merge 成本抵消。适合大图的方向是 row/word-block COW：delta 表示成 `(row, changed-word-mask)`，算子原地合并 word delta，只为实际修改的 row 建撤销记录。小图保留 flat dense 快路径。

### P1：删除传播覆盖面过窄

替换仅允许 Base/Alias/Union，且拒绝递归分量，所以 reads-from 候选替换、co 调整或闭包删除容易 rebuild。

建议先按依赖图 SCC 做**局部重建**：只清空受影响 SCC 及下游 strata。随后对 union、composition、inverse、domain/range 引入 support count 或 DRed；transitive closure 最后采用专门算法。

### P1：固定阈值只适合当前小样本

当前 certified model 在 `eventCount <= 512` 时直接选择 offline，且证书只在 candidate profile 与 host profile 相等时启用。固定阈值没有考虑关系密度、算子、历史命中率或 stable/active 比值，也不能解决 PSO/custom model 的缺口。

建议使用带迟滞的在线成本模型：

```text
incremental estimate = materialize + copy + affected rows + history scan
offline estimate     = EWMA(previous offline cost by size/density bucket)
```

SC、TSO、PSO 分别训练参数；连续若干次一方预计更便宜才切换。

### P1：证书覆盖需要扩展

候选裁剪比 evaluator 微优化显示出更明确的收益，但当前只在严格认证配置生效。建议把完全摘要匹配升级为结构化 proof obligations，识别与 host model 等价的已知公理子图，并分别覆盖 SC/TSO/PSO。无法证明时继续 fail closed，不能按文件名猜测。

### P2：表示与算子特化

- 使用 sparse-row/dense-row 混合关系，按 row 密度迁移；
- composition 根据左右 row cardinality 选择遍历顺序；
- `int/ext/loc` 等规则关系优先惰性计算；
- violation 检查维护增量 witness，避免每次重扫整关系；
- 多 worker 共享只读 normalized plan 和证书，worker 状态保持独立。

## 实验矩阵

### A. 兼容性普查

对全部任务设短编译和运行上限，输出各 property 数、编译成功数、语义支持数及 unsupported 原因。该阶段不用于性能排名。

### B. 三方法严格配对

同一任务、property、内存模型、seed 和线程数运行 GenMC、GenMC+CAT offline、GenMC+CAAT；额外运行 CAAT forced-online/forced-offline 来验证控制器。每项至少重复 5 次。报告配对 speedup 的中位数、几何均值、95% bootstrap CI、胜/负/平比例，并用 performance profile 保留 timeout 信息。

### C. 机制消融

- stable compaction on/off；
- full materialize / primitive delta；
- full-base history / delta-indexed history；
- full rebuild / SCC-local rebuild；
- fixed-512 / cost controller；
- dense / hybrid relation。

### 必须补充的日志

现有统计已有 materialize、copy、worklist、history、rebuild 和 undo bytes。还需增加：

- `active-events`、`stable-events`、`stable-active-ratio`；
- base/derived/current/history/undo 分项字节与峰值；
- 每个 primitive 的边数和 density；
- history probes、fingerprint rejects、exact subset checks、hit distance；
- rebuild reason（shrink、deletion、recursive replacement、unsupported operator）；
- 每个 operator 的 eval 次数、输入/输出边数和耗时；
- candidate count、certificate-pruned count；
- peak RSS、CPU、wall、explored executions、consistency queries。

## 实施顺序

1. 做数据集导入与兼容性 census，补全日志。
2. 实现 stable epoch compaction 和按字节 history budget，先限制 OOM 风险。
3. 实现 primitive delta，先处理 po/rf/co/loc，并取消 int/ext 显式全矩阵。
4. 实现 SCC-local rebuild，降低 rf/co 替换造成的整模型重算。
5. 实现 row/word delta relation，用大图验证，同时保留小图快路径。
6. 最后替换固定 512 阈值，并扩展 SC/TSO/PSO 结构化证书。

## 成功判据

- 兼容任务上三种方法结论一致；抽样任务启用每查询 offline oracle；
- CAAT 的 P95 peak RSS 不随历史稳定域无界增长；
- 大图桶中 CAAT 相对 CAT 的 wall-time 与 consistency-check time 均改善；
- 小图桶相对当前自适应版本回退不超过 3%；
- PSO 不再因缺少证书/删除路径而系统性慢于 CAT；
- unsupported、timeout、OOM、wrong answer 分开报告。
