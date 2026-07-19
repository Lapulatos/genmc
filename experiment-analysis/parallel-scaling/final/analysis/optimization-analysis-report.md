# CAT / CAAT 下一阶段优化分析

## 结论

当前最值得做的不是继续统一地“增量化所有关系算子”，而是把优化分成三条窄而可验证的路径：

1. **把 SC/TSO 已验证有效的认证剪枝与自适应离线路径扩展到 PSO。** 单线程正式矩阵中，CAAT/CAT 的任务配对墙钟比在 SC、TSO、PSO 分别为 0.864、0.833、1.091；PSO 是唯一仍稳定变慢的模型。
2. **消除宿主图到 CAAT 的全量同步成本。** 当前每次查询仍物化完整 `BaseValues`，历史项仍保存完整基关系副本。应由 `ExecutionGraph` 提供事件、rf、co、cut 的 mutation journal，并只传 dirty primitive/word。
3. **只在大关系和高代价递归 SCC 上使用增量传播。** 已有实验表明全局 CoW bitset 使套件慢 8.2%，通用 semi-naive union/composition 使套件慢 25.8%。下一版应由关系规模、密度、算子类型和实测代价选择 full recompute 或 delta，而不能全局替换。

多线程不是眼下的 CAAT 优化方向。96 个任务、5 次重复的正式矩阵中，8 工作线程相对 1 线程的墙钟 speedup 只有 0.910--1.035，CPU efficiency 仅 0.731--0.766，而且并行内部失败降低了覆盖率。批量实验应优先采用 BenchExec 的任务级并行，每个 GenMC 进程先保持 `--nthreads=1`。

## 证据边界

- 正式输入：160 个 BenchExec XML、15,360 行结果、96 个任务、SC/TSO/PSO、1/2/4/8 工作线程、5 次重复。
- 正确性观测：正式矩阵没有错误 verdict；288 个 oracle 运行执行了 2,444 次全量重算检查，也没有发现分歧。这是有限样本证据，不是 soundness/completeness 证明。
- 方法开销统计：以“任务”为配对单位，先对同一任务的 5 次运行取中位数，再对共同完成任务的比值取几何均值；95% CI 为 10,000 次 task bootstrap。不同方法未共同完成的任务不进入该比较。
- RSS 是整个 GenMC 进程的 peak RSS，不能直接分解为 checker 自身内存。

| 模型 | 对比（分子/分母） | 共同任务 | 墙钟比 | task-bootstrap 95% CI | CPU 比 | RSS 比 |
|---|---:|---:|---:|---:|---:|---:|
| SC | CAT/GenMC | 89 | 1.520 | [1.242, 1.918] | 1.528 | 1.367 |
| SC | CAAT/GenMC | 90 | 1.260 | [1.114, 1.462] | 1.257 | 1.061 |
| SC | CAAT/CAT | 84 | 0.864 | [0.765, 0.953] | 0.857 | 0.990 |
| TSO | CAT/GenMC | 82 | 1.316 | [1.112, 1.620] | 1.326 | 1.045 |
| TSO | CAAT/GenMC | 89 | 1.288 | [1.111, 1.541] | 1.288 | 1.084 |
| TSO | CAAT/CAT | 82 | 0.833 | [0.730, 0.925] | 0.826 | 0.996 |
| PSO | CAAT/CAT | 79 | 1.091 | [1.037, 1.159] | 1.103 | 1.001 |

不能从上表得出“CAAT 总是比 CAT 快”。SC/TSO 的优势主要来自只对精确匹配的内置模型启用的认证候选剪枝和小图离线路径；PSO/自定义 CAT 模型仍走通用在线路径。

## 已经做过、无需重复投入的优化

| 项目 | 结果 | 决策 |
|---|---|---|
| 关闭 explanation 时避免复制派生值 | 语义中性，未测得稳定加速 | 保留，不再作为主优化点 |
| 缓存谓词依赖邻接 | 套件约快 4.4% | 已完成 |
| 稳定 ID 直接物化 | 核心样例约 1.254x；物化 182.1ms → 55.1ms | 已完成，但仍是每次全量物化 |
| 只物化模型引用的 primitive | 累计约 1.362x | 已完成；下一步应升级为程序特化 |
| SC/TSO 认证候选剪枝 | 核心样例查询 327 → 43，算子求值 4,489 → 421 | 已完成，是目前最大收益来源 |
| O(1) checkpoint + delta undo trail | undo packed storage 减少 49.7%；时间近中性 | 已完成；不要再把 checkpoint 描述成全量快照 |
| 全局 CoW packed bitset | 套件慢 8.2% | 已拒绝 |
| 通用 semi-naive union/composition | 套件慢 25.8% | 已拒绝；仅考虑有门控的混合版本 |
| alias/union 的 support-aware replacement | 内置递归 SC/TSO/PSO 不适用，主套件近中性 | 只作为基础设施保留 |
| SC/TSO 固定 512-event 自适应 offline | CAAT 套件相对原始基线累计约 2.45x | 已完成；阈值仍需数据驱动化 |

## 当前瓶颈到代码路径的映射

1. `StableGraphAdapter` 每次查询仍将执行图完整转换成稳定 ID 的基值；阶段 4 只移除了旧的临时 dense adapter/remap，并未建立宿主 mutation API。
2. `GraphSynchronizer::retainCurrent()` 把完整 `baseValues()` 放入最多 32 个历史项；回退时从新到旧执行完整 subset 检查，并在命中后恢复候选基值。
3. `IncrementalCaatEvaluator::tryInsert()` 为事务安全复制完整派生值容器；受影响算子仍按整个谓词重新执行。
4. `violations()` 在更新完成后重新检查全部约束/环，而此前状态已知一致时，只有新事实或受删除影响的事实可能改变答案。
5. `removeFactsAndShrink()` 和稀疏关系上的全矩阵操作可能扫描大量空位。
6. recursive SCC、composition、closure、difference 的 replacement 仍不能局部维护，因而回退到历史或 rebuild。

## 优先级路线图

### P0：建议立即实现和测量

| 优化 | 适用 | 预期机制 | 语义风险 | 实现/验收门槛 |
|---|---|---|---|---|
| PSO 认证剪枝 + backend certificate | CAT、CAAT | 减少候选图和一致性查询；复用 SC/TSO 最大收益机制 | 中 | 模型规范化 fingerprint 必须 fail closed；oracle 下禁用；PSO 全任务 execution count 与 generic 路径一致 |
| ExecutionGraph mutation journal | CAAT | 避免每次全量 stable materialization、相等比较和 primitive rebuild | 中 | 支持 append event、rf replacement、co move、cut/rollback；逐步与 legacy adapter 做逐查询差分 |
| 增量 violation/cycle frontier | CAAT | 已一致图的插入只从新增边/事件启动 sparse DFS/automaton | 中 | explanation 关闭时允许 first violation early exit；开启时保持 reason 完整；每次与 full `violations()` oracle 比较 |
| 数据驱动 online/offline selector | CAAT | 用实际成本替代固定 512 event 阈值 | 低（两个 backend 都精确） | 特征仅用查询前可知量；按程序分组交叉验证；选择器额外开销 <1%；错误选择只影响性能 |

PSO 应先做，因为它给出了清晰的反事实：SC/TSO CAAT 已快于 CAT，而 PSO CAAT/CAT 墙钟比为 1.091，且当前代码明确不为 PSO 启用 adaptive offline。先将 PSO 的合法枚举规则形式化为 certificate，再测是否缩小这 9.1% 的差距。

mutation journal 的最小 API 可只包含：`append_event(label)`、`set_rf(read, old, new)`、`move_co(write, old_pred, new_pred)`、`truncate(prefix)`，以及每个 primitive 的 dirty rows/words。不要第一版就暴露任意关系编辑。

### P1：P0 建立剖析数据后实现

| 优化 | 依据 | 关键门控 |
|---|---|---|
| 混合 full/delta 算子 | 通用 semi-naive 在小图上因分配和 merge 反而慢 25.8% | 仅对大/密/高代价 SCC 启用；预分配 scratch；以 EWMA 的实际 ns/operator 决策 |
| Kater 风格固定模型 checker | Kater 可从一类 CAT 模型生成稀疏、增量一致性检查；SC 示例通过 immediate 边把 DFS 降到线性 | 内置模型走生成 checker，任意 CAT 保留 generic evaluator；生成物带模型 hash/certificate |
| 程序特化 CAT | 当前 primitive mask 只看模型，不看程序 | 用事件类型、location、访问模式和静态 may/must 分析消除不可能 pair、空算子和冗余闭包；不得改变候选集合 |
| 持久化 delta history | 当前最多 32 份完整 BaseValues | full snapshot 每 K 层 + 中间 delta；per-primitive hash/size/dirty summary 先快速拒绝 subset；自适应历史深度 |
| common recursive replacement | rf/co replacement 是在线路径的核心退化点 | 先做 closure/常见 SCC 的 support count 或 delete-and-rederive；遇到 difference/未知算子立即 rebuild |
| sparse/dense 混合 Relation | 当前稀疏图仍承担 dense 扫描 | 以 row density 切换 small-vector/bitset；设置滞回阈值，避免表示来回抖动 |

Kater 的价值不在于“把整个 CAAT 删除”，而是为内置 SC/TSO/PSO 提供生成式 fast path，同时保留 CAAT 作为任意 CAT 模型、解释和通用在线增量的后端。这样既保住扩展目标，也避免用通用关系代数解释器重复承担固定模型成本。

### P2：研究型、语义风险较高

1. **按需 negative-predicate cutting。** CAAT 论文指出，非半正模型目前会把负向出现的派生谓词整体切开，但实际解释通常只涉及其中一部分；可只为 explanation 触及的区域编码/物化。这可能将 non-monotonic core 缩小为大部分正向增量模型，但需要新的 reason 完备性证明和差分 oracle。
2. **正模型化 + 局部 delete/rederive。** 对 difference 的负向依赖先通过按需切割移出递归核，再对剩余正 SCC 使用 support counting。不要直接承诺任意 non-monotonic Datalog 的高效 fully dynamic maintenance。
3. **查询驱动求值。** 从 consistency checks 反向切片并优先计算可尽早产生 violation 的谓词；普通检查找到首个 violation 即停，只有 `--explain-cat` 才保留完整 provenance。
4. **更紧凑的 explanation。** CAAT 使用 derivation length 和 transitive-closure shortest paths；可只在发现 violation 后懒构造 reason，普通 consistent 查询不维护完整解释元数据。

不建议把完整 DBSP/Differential Dataflow runtime 引入 GenMC。它们对通用递归查询维护很强，但当前关系小、更新细、查询频繁；已有 semi-naive 和 CoW 负实验说明运行时常数、分配和 merge 足以吞掉理论收益。可借鉴 support count、dirty SCC、delete-and-rederive，而不引入完整数据流引擎。

## 建议实验：每项优化必须能被证伪

### 数据集分层

- **微观层：** flat-combiner、每类 transition 的合成图；记录每查询事件数、关系密度、dirty facts、history hit distance、算子 ns、分配字节。
- **模型层：** SC/TSO/PSO 各自比较 generic CAT、generic online CAAT、认证 fast path；加入至少 3 个 fingerprint 只差一个 axiom 的近邻模型，确认 fail closed。
- **程序层：** 现有 96 任务用于快速回归；C.Concurrency 全量用于外部有效性。结果按程序而不是 XML row 作为统计单位。
- **正确性层：** 单线程 mutation oracle 全开；并行只在单线程通过后做。对所有 safe 完成任务比较完整 exploration count，对 unsafe 任务比较 verdict 并把 ABORTED 单列为 unknown。

### 逐项验收阈值

| 项目 | 继续条件 | 停止/回退条件 |
|---|---|---|
| PSO certificate | PSO CAAT/CAT 墙钟比的 task-bootstrap CI 上界 <1，且无 oracle mismatch | 只减少小于 5% 查询或引入任何不一致 |
| mutation journal | adapter+synchronize 时间中位数减少 ≥30%，总墙钟 geomean 改善 ≥10% | 日志维护使 CAT/GenMC host path 回归 >2% |
| incremental violation | violation 阶段减少 ≥30%，解释关闭路径总墙钟改善 ≥5% | explanation 结果不一致或稠密图持续变慢 >5% |
| adaptive selector | 相对 `min(online,offline)` oracle 的 regret ≤5%，留出程序上总墙钟改善 ≥5% | 只在训练程序上有效或选择器成本 ≥1% |
| hybrid delta | 目标大图/SCC 算子时间改善 ≥20%，全套件不回归 | 小图门控失效或全套件回归 >2% |

每轮至少报告：共同完成任务数、wall/CPU/RSS 比、95% task-bootstrap CI、失败类别、完整 exploration count mismatch、oracle mismatch、每种 transition 的命中率。仅报告总平均时间会把 timeout、coverage 和方法选择偏差混在一起。

## 推荐实施顺序

1. 增加 transition/dirty-density/operator-cost 计数器，不改算法。
2. 实现 PSO certificate，并保持 generic 和 oracle 路径可切换。
3. 实现最小 mutation journal；保留 legacy adapter 做逐查询 differential。
4. 在 journal 之上做增量 violation frontier。
5. 用收集的数据训练一个简单、可解释的 backend selector（分段阈值或小决策树），不要先用复杂模型。
6. 只有当数据证明大 SCC/高密关系占主要时间时，再实现 hybrid delta 和 sparse/dense relation。
7. 最后评估 Kater 生成 checker 与按需 cutting；二者都是架构/论文贡献级改动，应分别做 ablation。

## 文献依据

- CAAT：Haas et al., *Consistency as a Theory*, OOPSLA 2022，DOI <https://doi.org/10.1145/3563292>。本地全文：`/Users/sujie/Documents/Papers/ConcurrencyPaper/paper/C. Memory Consistency Models/04 Memory Models/04.03 Consistency Checking/OOPSLA'2022 - CAAT- Consistency as a Theory.pdf`。
- Kater：Kokologiannakis et al., *Kater: Automating Weak Memory Model Metatheory and Consistency Checking*, POPL 2023，DOI <https://doi.org/10.1145/3571212>。本地全文：`/Users/sujie/Documents/Papers/ConcurrencyPaper/paper/C. Memory Consistency Models/04 Memory Models/04.02 Semantics/POPL'2023 - Kater- Automating Weak Memory Model Metatheory and Consistency Checking.pdf`。
- Haas et al., *Static Analysis of Memory Models for SMT Encodings*, OOPSLA 2023。本地全文：`/Users/sujie/Documents/Papers/ConcurrencyPaper/paper/D. Model Checking & Verification/06 SMT-BMC-based Verification/OOPSLA'2023 - Static Analysis of Memory Models for SMT Encodings.pdf`。
- Kokologiannakis et al., *Enhancing GenMC's Usability and Performance*, TACAS 2024。本地全文：`/Users/sujie/Documents/Papers/ConcurrencyPaper/paper/D. Model Checking & Verification/05 Stateless Model Checking & POR/05.03 Reads-From Equivalence/TACAS'2024 - Enhancing GenMC-s Usability and Performance.pdf`。
- Tunç et al., *Optimal Reads-From Consistency Checking for C11-Style Memory Models*, PLDI 2023，<https://arxiv.org/abs/2304.03714>。其结果支持把模型特定、稀疏的一致性算法作为通用 CAT/CAAT 的 fast path，而不是要求一个通用求值器在所有模型上最优。
- 本地补充：`TOPLAS'2023 - Satisfiability Modulo Ordering Consistency Theory for SC, TSO, and PSO Memory Models.pdf`（增量一致性、冲突和传播）；`OOPSLA'2022 - Consistency-Preserving Propagation For SMT Solving of Concurrent Program Verification.pdf`（预防式传播）；`OOPSLA'2026 - RAT-CAT-SAT- Model Checking Memory Consistency Models.pdf`（新近的 CAT/SAT 专门化方向，尚不应作为已成熟结论）。

## 可使用与禁止使用的结论措辞

- 可用：**“在当前 96-task 正式矩阵的共同完成任务上，SC/TSO CAAT 快于 CAT，而 PSO CAAT 慢约 9%；差异与当前 fast path 的模型覆盖一致。”**
- 不可用：**“在线增量 CAAT 天生比离线 CAT 快/慢。”** 当前 backend 是混合选择，模型和任务覆盖也不同。
- 可用：**“现有 oracle 未发现不一致，因此这些优化可进入扩大验证阶段。”**
- 不可用：**“CAT/CAAT 不会误报或漏报。”** 有限实验不能替代证明；并行 ABORTED 也必须继续作为 unknown 报告。
