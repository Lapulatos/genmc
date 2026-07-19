# GenMC TIMEOUT、Deagle 正确任务的根因分析

## 结论

这批差距首先是**探索空间问题**，不是 CAT/CAAT 单次一致性查询或内存占用问题。
在 725 个适配任务中，当前 GenMC+CAAT-SC 有 240 个 TIMEOUT，其中 200 个被
Deagle 正确完成；这 200 个任务里，原生 GenMC 也有 192 个 TIMEOUT。也就是说，
CAAT 只让 8 个原生 GenMC 可完成任务跨过 60 秒线，不能解释主体差距。

但“Deagle 对这 200 个都显著更快”不能直接作为公平性能结论：190 个任务含
`nondet`，GenMC 侧使用固定 seed 1995，而 Deagle 侧保留符号非确定输入；另有
11 个 Deagle TRUE 来自未能静态确定循环时的 fallback unwind=3，按 Deagle 自己
的说明并不完备。排除这两类后只剩 9 个严格可比任务，其中原生 GenMC 仍有 8 个
TIMEOUT，Deagle 用 0.266--54.803 秒完成。这 9 个任务才是当前最可靠的算法差距
证据。

要攻克 TIMEOUT，优先级不应继续放在压低一次 CAT query 的常数，而应把
CAAT 的冲突 witness 提升为**跨候选执行图可复用的 learned nogood**，并让探索器
按冲突涉及的 RF/CO/guard 决策进行非时间顺序回跳。对可静态有限展开的程序，
还应增加“事件骨架一次构造、选择符号化、CAT/CAAT 作为 theory propagator”的
混合通道。这两项分别对应 Deagle 最关键的 clause learning/backjumping 和
symbolic formula 优势，同时仍由通用 CAT/CAAT 判定一致性，不是调用 GenMC
内置 checker。

## 1. 分析对象与证据边界

- 数据集：同一批 725 个适配后的 SV-COMP 2026 `C.Concurrency` 任务。
- 资源限制：每任务 60 CPU 秒、4 GiB、单核。
- 主 cohort：当前 GenMC+CAAT-SC 为 TIMEOUT，Deagle category 为 correct。
- 主 cohort 数量：200，占 CAAT-SC 全部 240 个 TIMEOUT 的 83.3%。
- 对照：原生 GenMC、此前 CAAT-SC、当前 CAAT-SC/TSO/PSO、Deagle 的同任务结果。
- Deagle 实现：官方仓库当前 `main` 的 V4.1.0 源码；默认选择
  `deagle-closure`/`ClosureSolver`。[官方 README 与源码](https://github.com/thufv/Deagle)

TIMEOUT 日志在 BenchExec kill 前没有输出 complete-execution 或 `--cat-stats`
计数；Deagle wrapper 又把内部 solver 输出重定向到临时文件并删除。因此，下文
对 Deagle 冲突数、decision 数和 propagation 数不做伪精确推断；机制结论来自
实际源码路径，任务结论来自 XML、wrapper 日志和输入源码。

## 2. 共同特征

### 2.1 任务高度集中在 `pthread-wmm`

| family | cohort 任务 | family 全部任务 | 进入 cohort 的比例 |
|---|---:|---:|---:|
| pthread-wmm | 179 | 283 | 63.3% |
| pthread | 8 | 42 | 19.0% |
| weaver | 7 | 174 | 4.0% |
| pthread-ext | 4 | 35 | 11.4% |
| pthread-theta | 1 | 13 | 7.7% |
| goblint-regression | 1 | 103 | 1.0% |

`pthread-wmm` 单独占 cohort 的 89.5%。这 179 个任务全部无源码循环，Deagle
全部识别为 `No loops!`，CPU 时间为 0.217--0.560 秒，中位数 0.277 秒。它们
不是“大源码/深循环”问题，而是源到源弱内存编码经过显式调度后的组合爆炸。

这组程序的源码形状非常一致：

- 3--4 个 `pthread_create`，没有 `pthread_join`；
- 15--22 个 `__VERIFIER_atomic_begin`，中位数 19；
- 1--10 个 nondet 调用，中位数 3；
- 每个任务正好 2 个 assume；
- 使用 `$w_buff`、`$r_buff`、`$flush_delayed`、`weak$$choice` 等显式缓冲编码；
- 主线程通过 `__unbuffered_cnt` 的 assume 等待工作线程完成，再检查断言。

对 GenMC 而言，这些编码已经把一个紧凑的弱内存事件关系展开成许多 SC 原子块、
buffer 状态和分支。GenMC 虽按 reads-from equivalence/执行图进行约简，并非简单
枚举每个线程调度，但仍需逐个构造候选执行图和 revisit；通用 CAT 路径在没有
证明时保守保留所有 revisit。编码层引入的 choices 因此直接乘入探索空间。

### 2.2 这批 TIMEOUT 不是内存爆炸

- GenMC+CAAT-SC peak RSS 中位数 26.284 MB，P90 26.543 MB。
- 190/200 低于 64 MB；0/200 达到 512 MB。
- 最大值 111.833 MB，仍远低于 4 GiB 限制。

这说明当前累计优化已经把这批任务从“关系状态/历史快照增长”压到了低内存区，
但没有减少要访问的候选图数量。对这批任务继续用时间换空间意义很小；核心指标
应改为每秒处理的候选选择数、被提前拒绝的 revisit 数和 learned nogood 命中数。

### 2.3 FALSE 任务占主体

200 个任务中 expected FALSE 为 154 个，TRUE 为 46 个。对 bug-finding，Deagle
的 SAT 搜索可以同时选择分支、nondet、RF 和 order，使 error literal 成立；GenMC
固定一个 nondet stream 后再探索并发执行，既可能根本不含 Deagle 找到 bug 所需
的输入，也缺少由 assertion cone 驱动的全局选择顺序。这提示“面向属性的反例优先
搜索”可能比继续平均加速所有 consistency query 更快减少 TIMEOUT。

## 3. 公平性分层

### 3.1 190 个 nondet 任务不是相同输入语义

GenMC 侧 compatibility adapter 把 nondet 固定到 seed 1995；Deagle 侧把 nondet
作为符号变量。190/200 存在该差异，其中 expected FALSE 为 150 个、TRUE 为 40 个。
因此，Deagle 在 0.3 秒找到 FALSE 可能依赖另一个输入赋值，不能与“固定 seed 下
GenMC 60 秒未完成”直接计算 speedup。固定 seed 下的 GenMC TRUE 也只能证明该
输入流，不能作为正式 SV-COMP TRUE。

这 190 个任务仍有诊断价值：它们显示显式执行探索对大量 atomic block/choice
的敏感性，也给出了混合 symbolic lane 的目标 workload；但它们不能支持严格的
工具速度排名。

### 3.2 11 个 fallback-bound TRUE 不完备

Deagle 日志中 12 个任务无法静态确定所有循环，wrapper 在 5 秒自动尝试失败后
对未知循环使用 unwind=3。其中 11 个返回 TRUE。Deagle README 明确说明，这种
TRUE 可能不完备，学术使用时应启用 unwinding assertions 或扩大界限。因此这
11 个结果不能算“Deagle 已证明、GenMC 超时”；唯一的 FALSE 仍是有效反例。

### 3.3 严格核心集

排除 nondet 语义差异和 bounded-TRUE 后，剩余 9 个任务：

| task | expected | 原生 GenMC | Deagle CPU (s) |
|---|---:|---|---:|
| pthread-theta/exponential-4.yml | TRUE | TIMEOUT | 0.266 |
| pthread/fib_safe-7.yml | TRUE | true | 4.195 |
| pthread/fib_safe-10.yml | TRUE | TIMEOUT | 18.778 |
| pthread/fib_safe-11.yml | TRUE | TIMEOUT | 54.803 |
| pthread/fib_unsafe-10.yml | FALSE | TIMEOUT | 12.397 |
| pthread/fib_unsafe-11.yml | FALSE | TIMEOUT | 27.184 |
| pthread/fib_unsafe-12.yml | FALSE | TIMEOUT | 51.665 |
| pthread/triangular-longer-1.yml | TRUE | TIMEOUT | 14.515 |
| pthread/triangular-longer-2.yml | FALSE | TIMEOUT | 6.160 |

这些任务的循环都由 Deagle 静态得到有限 unwindset。9 个任务的 GenMC+CAAT-SC
RSS 只有 25.82--25.96 MB。`fib` 随参数增长时 Deagle 时间也明显增长，但仍能把
有限程序一次编码成公式；GenMC 则在低内存下持续枚举执行图直到 60 秒。这是
“symbolic finite skeleton”最干净的首批原型集。

## 4. Deagle 为什么能处理这些任务

官方实现的关键不是“它使用了 SAT”这一句，而是把并发一致性推理放进了 CDCL
传播和学习循环：

1. CBMC 前端对有限展开程序构造一次 SSA/Boolean 公式，branch、guard、nondet、
   RF/order 选择成为同一个求解问题中的 literals。
2. SV-COMP 默认路径选择 `deagle-closure`，后端是扩展 MiniSAT 的
   `ClosureSolver`，不是运行后再调用一个独立图检查器。
3. Boolean literal 被赋值时，相关 PO/RF/CO/FR/order edge 立即激活；闭包结构
   增量检查 cycle，并通过 vital edge/triplet 传播把不可能的 RF/guard 置假。
4. cycle 形成 conflict reason，进入标准 `analyze()`；solver 学习 clause，回跳到
   冲突涉及的较早 decision level，而非按候选执行顺序逐个回退。
5. 每个 decision level 用 edge、vital、triplet 等 trail 做 `push_scope/pop_scope`，
   learned clauses 跨后续搜索继续生效。

这与当前 GenMC+CAAT 的差别是：CAAT 能更快判断“当前候选图不一致”，也能返回
witness，但 witness 尚未被编译成能覆盖一族未来候选图的选择约束。结果是相似
的坏 RF/CO/guard 组合可能在不同 revisit 路径中被反复发现。

## 5. 在既有六项之外，最值得新增的方向

### P0-A：跨执行图冲突学习（CDEL）

把 CAAT/Reasoner 的 cycle witness 反投影到造成该 cycle 的最小 RF、CO、branch/
guard 决策集，生成形如 `¬(r1<-w2 ∧ w2<co w3 ∧ g7)` 的 nogood。用 watched
literals 或 choice-signature 索引，在构造执行图前检查，而不是缓存完整 snapshot。

- 降低步骤：`calcRevisits/calcCoOrderings` 入队、worklist 出队、图 cut/rebuild、
  CAT transition/query 次数全部下降。
- 时间收益来源：一次冲突拒绝一族未来执行图；不是让单次 query 再快几个百分点。
- 空间代价：增加 learned nogood database；需要 clause size/activity/LBD 类回收，
  防止重演 P0.2 snapshot cache 的内存问题。
- 正确性条件：nogood 必须由 CAT relation derivation 的可审计 reason 支持；无法
  解释的 witness 只做当前图拒绝，不能学习。

### P0-B：基于冲突层级的非时间顺序回跳

为 RF、CO placement、optional/branch 引入显式 decision stack。冲突 reason 给出
最高和次高 decision level 后，直接撤销到 asserting level，并立即禁止冲突组合。

- 降低步骤：跳过与冲突无关的后缀执行、重复 replay 和逐层 rollback。
- 与既有“支持 rollback 的增量 EOG”不同：rollback 只让恢复便宜；backjump 决定
  **恢复到哪里**，直接减少探索节点数。
- 风险：必须保持 GenMC reads-from equivalence 的覆盖证明；第一版只对独立、可解释
  的 RF/CO decisions 启用，遇到 speculative/ABA/IPR 特例回退原 worklist。

### P0-C：静态有限区域的 symbolic event-skeleton lane

对循环可完全静态展开、线程数有限、无不支持外部调用的任务，一次构造事件骨架，
把 branch/nondet/RF/CO choices 表示为 literals；CAT/CAAT 作为增量 theory
propagator 判定关系，不调用内置 SC/TSO checker。候选模型 SAT 后再物化完整
ExecutionGraph 并用 CAT evaluator 复核。

- 直接目标：9 个严格核心任务；完成同 seed 的复核后再扩展到 179 个 loop-free
  `pthread-wmm` 任务。
- 降低步骤：避免每个 choice 重新执行 interpreter、构造/切割图和解释同一程序前缀。
- 空间代价：保留一份 SSA/事件骨架和 clause database，可能高于当前约 26 MB；
  这是有边界的空间换大幅搜索时间，而非保存每个执行状态。
- 完备性：只有静态有限性证明成功才可返回 TRUE；否则回退 stateless lane。

### P1-D：面向 assertion cone 的反例优先决策

从 `reach_error` 逆向切片，给能影响断言的 loads、RF candidates、guards 和线程
更高 priority；FALSE 找到即停，TRUE 仍完整探索，因此不改变完备性。

- 主要目标：154 个 expected FALSE 差距任务。
- 降低步骤：缩短第一个错误执行前的候选图数量；不降低证明 TRUE 的最坏上界。
- 验证：只比较发现同一 property violation 所需的 completed/blocked executions，
  不按 benchmark expected verdict 在运行时选择策略。

### P1-E：WMM 编码识别与事件级折叠

识别 `$w_buff/$r_buff/$flush_delayed/weak$$choice` 和配套 atomic block，把已知的
源到源弱内存模板恢复成紧凑的“原始访问 + flush/read choice”事件宏，再交给 CAT
模型。它不是按任务名选择 checker，而是一个带 proof obligation 的语义保持前端。

- 潜在覆盖：179/200 的最大结构簇。
- 降低步骤：减少 interpreter 指令、atomic block、共享 buffer 变量和由其产生的
  revisit/RF 候选。
- 风险最高：模板版本多、C alias/溢出/atomic boundary 可能破坏逆转换。只应先对
  可精确认证的模板输出 translation certificate；未认证输入保持原程序。

### P1-F：有限循环摘要，而不是固定 unwind 猜测

对 affine/常量界循环和递归展开模式生成精确事件模板或 recurrence summary，
保留 `unwinding assertion` 等价证明。它针对 `fib`/`triangular` 核心集，不采用
Deagle fallback=3 的不完备 TRUE 策略。

- 降低步骤：共享不同 iteration 的 control skeleton，减少重复解释和图前缀复制。
- 风险：summary 对共享访问次序、overflow 和 alias 必须精确；第一版仅接受静态
  常量界、无跨迭代 pointer escape 的循环。

## 6. 与既有六项的关系

- 编译式 epsilon-closure、增量 EOG 和 delta checkpoint 降低单个 query/rollback
  成本；P0-A/B 降低 query 和 rollback **发生次数**。
- EOG CEGAR 减少抽象候选图；P0-A 学习的是具体 CAT 冲突在 choice space 中的
  可复用子句，可作为 CEGAR refinement 的低层证书。
- 通用 CAT preventive pruning 与 Deagle 的 vital/triplet propagation方向一致；
  本报告不把它重复列为新项，而是建议其 reason 系统与 P0-A 共用。
- TruSt/Awamoche/Mixer/Spore 等价约简处理“哪些执行等价”；P0-C 处理“是否暂不物化
  执行、在同一个符号搜索中决定 choices”，两者可叠加。

## 7. 推荐实施顺序与验收指标

1. **先修实验语义，不改算法**：给 Deagle 固定同一 seed，或只跑无 nondet 集；
   所有 TRUE 开启/验证 unwinding assertions。否则无法判断优化是否真的缩小差距。
2. **P0-A 最小原型**：只学习 SC acyclicity cycle 的 RF/CO nogood；在 9 个核心任务
   上记录 learned clauses、平均 clause 长度、命中数、跳过 revisits 和 peak RSS。
3. **加入 P0-B**：比较 chronological rollback 与 conflict backjump 的 explored
   graph 数；要求最终 verdict、错误 witness 和 complete execution oracle 一致。
4. **并行设计 P0-C，但单独 feature flag**：先支持静态 finite、无 nondet 的 9 个
   任务；SAT model 必须由通用 CAT full evaluator 二次确认。
5. **再做 P1-D**：以 FALSE time-to-bug 为指标；不得读取 expected verdict 调度。
6. **最后评估 P1-E/F**：只有模板/循环可产生可验证 translation certificate 时保留。

第一轮成功标准不应写成“CPU 降低 5%”，而应是：9 个严格核心任务至少新增 3 个
60 秒内 terminal，且无 verdict/oracle mismatch；200-task cohort 的 learned nogood
命中率和 skipped revisits 可解释新增覆盖；RSS P90 不超过 256 MB。达到这个门槛后，
再扩展到完整 725 任务和 SC/TSO/PSO。

