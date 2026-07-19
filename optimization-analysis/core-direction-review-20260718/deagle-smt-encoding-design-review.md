# 并发 SMT/EOG 方法对 GenMC finite lane 的设计审计

## 结论

先不实现 staged encoder。文献范围不能缩成“照搬 Deagle”，而应比较两种不同的首层
架构和三种后续增强：Yogar-CBMC 的 scheduling-constraint abstraction refinement、
PLDI 2021/TOPLAS 2023 的精确 ordering consistency theory、PPoPP 2022 的 interference
decision guidance、OOPSLA 2022 的 preventive propagation，以及 Deagle 对后几项的工具化
组合。

当前最值得优先验证的是 Yogar 式初始抽象：SAT 先只求 guard、SSA、error 和 RF/value
choice，不把完整 scheduling/CO rank 放进第一次查询；抽象反例再由 EOG/ordering theory
验证，并把可证明的 kernel reason 作为 refinement clause 返回。这一结构直接针对当前
225 个 admitted 任务在 assignment 1 之前超时的症状。完整候选仍由通用 CAT evaluator
与普通 GenMC replay 复核。

直接照搬 Deagle 也不可接受。它针对 SC/TSO/PSO 只显式编码部分 ordering primitives，
并在 theory 中推导 WS/FR；我们的公开接口允许 CAT 模型，因此只有对结构认证通过的
SC/TSO/PSO 模型才能走专用 theory。其他 CAT 必须保留完整 CO/RF assignment 和通用 CAT
fallback。

## 相关工作不是一条 Deagle 线

| 工作 | 主要减少什么 | 位于求解流程的哪一层 | 对当前问题的作用 |
|---|---|---|---|
| Yogar-CBMC / scheduling-constraint abstraction | 第一次 SAT 查询中的完整调度公式 | SAT 外层 CEGAR | 最直接解决 assignment 1 前超时 |
| EOG graph validation + kernel reasons | 抽象反例验证和 refinement clause 大小 | CEGAR validator | 用小原因排除一组伪反例，而非完整 assignment blocker |
| PLDI 2021 ordering consistency theory | clock/timestamp 与预编码 FR | DPLL(T) theory | 提供精确增量一致性检查、冲突原因和 theory propagation |
| TOPLAS 2023 SC/TSO/PSO 扩展 | 各内存模型下的冗余 ordering constraints | DPLL(T) theory | 是 SC/TSO/PSO 专用规则的主要规格 |
| PPoPP 2022 interference-guided solving | 通用 solver 的无差别决策 | DPLL decision policy | RF/WS 等 interference variables 优先；是增强而非公式抽象 |
| OOPSLA 2022 consistency-preserving propagation | 事后 cycle check/conflict generation | theory propagation | 潜在收益高，但正确性审计风险最高，最后评估 |
| Deagle | 前端、MiniSAT 和 ordering theory 的工程组合 | 完整工具 | 用于核对实现调用链，不能代替逐篇算法审查 |

贺飞团队的 control-flow-guided SMT 工作可能帮助 SSA/branch decision ordering，但它不
消除当前全量 scheduling/CO 公式，因此只列为第二阶段 datapath heuristic，不能替代上述
并发核心架构。

Yogar 系列还包括针对 TSO/PSO 的 weak-memory EOG 扩展。这说明 EOG 并非只能用于 SC；
但这些规则仍是指定内存模型的算法，不能据此宣称支持任意 CAT。

## 本轮实验对设计的约束

1. 原始 monolithic Z3 lane 在完整 725 上确认 2 个既有错误，却把正确结果 398 降到
   318，说明“在 native verifier 前枚举完整 assignment”不可接受。
2. 加入 `error-active` 硬约束后，225 个 admitted `pthread-wmm` 全部在 assignment 1
   之前超时。逻辑上更强的公式反而更难，瓶颈是通用 SMT 对全量 guard/RF/CO/bit-vector
   公式的第一次搜索，而不是 CAT evaluator。
3. 当前 encoder 为每个地址的所有 store 预建 bit-vector CO rank，并对所有 store pair
   添加 conditional distinct/order decision。这正是论文批评的“精确求 timestamp/rank，
   以及无论是否生效都穷举公理实例”。
4. 降低 assignment 上限无法修复 assignment 1 前超时；继续调 Z3 参数也不能作为核心
   方案。

## 论文中可直接借鉴的部分

### 编码边界

- 验证条件是 `program SSA ∧ error condition ∧ ordering constraints`；error 不是事后过滤。
- RF 使用 Boolean interference variable，并分成 RF-Val、RF-Ord、RF-Some。
- Deagle 的 ordering encoding 只显式保留 `po-loc`、`ppo`、`rf`；WS 和 FR 在 solver
  中按需推导，不给每个 event 分配必须求出具体值的 timestamp。
- guard 决定 event 是否存在；RF decision 同时要求读写 guard、值相等和对应 order。

### 求解边界

- interference variables 不是普通无差别 Boolean：DPLL 决策顺序优先选择它们，并按
  类型排序。
- theory 收到正 RF literal 或 guard literal 后增量加入 order，做 transitivity、WS、FR
  derivation；形成 self-loop 时返回原因 clause。
- 关键收益来自减少 decisions，而不是减少前端源程序事件数。PPoPP 论文报告同解实例
  在 SC/TSO/PSO 上分别约 1.49x/1.87x/1.89x，但这不能直接外推到我们的 60 秒数据。

## 与当前实现的映射

| Deagle 概念 | 当前 GenMC finite lane | 需要改变 |
|---|---|---|
| guard / SSA / error | Z3 block/value；error 事后检查 | error 进入 SAT，但不能继续用当前全量 SMT 公式 |
| RF Boolean | 已有 RF selector | 保留，优先决策，并只为地址/width 可配对项创建 |
| WS/CO | 每 store 一个 BV rank + pair constraints | SC/TSO/PSO theory 中取消 rank；按需维护 order |
| FR | 完整 assignment 后由 CAT `rf^-1;co` 物化 | theory 按 RF + order + active write 推导，最终 CAT 复核 |
| conflict reason | CAAT Reasoner 可解释完整 CAT violation | 增加 partial-order 原因；每个 clause 必须可独立重放 |
| final verdict | Deagle SAT/UNSAT | 我们只允许 replay-confirmed FALSE；TRUE 仍由 native verifier |

## Deagle 实际源码调用链（只作架构证据）

官方源码并不是把 ordering checker 放在 SAT 完成模型之后。`ClosureSolver` 直接继承
MiniSAT `Solver`，前端通过 `addOC` 和 `addGuard` 登记 ordering edge/literal 与 event guard。
在每次 Boolean propagation 中，solver 读取刚进入 trail 的 literal，激活对应 edge 或
guard，再调用 graph closure：

1. 新 edge/guard 形成 cycle 时，将 `graph.conflict_lv` 取反构造 MiniSAT conflict clause；
2. theory 推出的 Boolean literal 通过带 reason 的 learned clause 入队；
3. `newDecisionLevel()` 对 graph `push_scope()`，`cancelUntil()` 对 graph `pop_scope(level)`；
4. 若 theory propagation 又产生 literal，源码中的 `one_more_time` 会递归执行
   `propagate()`，直到没有新增 edge/literal。

第四点很重要：2025 独立复现审计指出，这个源码 fixpoint 行为与论文原始 prospective
算法不同，并与正确性结果直接相关。因此我们不能只按 OOPSLA 伪代码重写，也不能把
Deagle release/source 的结果当作 preventive algorithm 本身的证明。可借鉴的是 trail、
reason、backtrack 的接口形状；规则正确性必须由独立 oracle 重建。

Deagle 的 TACAS 工具论文还说明 SAT 时 theory solver 会返回 event total order 用于 witness。
这进一步证明“省略显式 WS/FR”不是省略最终 order completion，而是把 completion 移到
dedicated theory 内。

## P0 census 的精确定义

设基本块数为 `B`，CFG edge 数为 `G`，值位宽和为 `Σwidth(v)`；对每个 read `r`，合法
RF source 数为 `k_r`；对每个地址 `a`，候选 write 数为 `n_a`。不调用 solver 的 census
必须按程序输出并汇总以下量：

- 当前 encoder：`B` 个 block Boolean、`G` 个 edge Boolean、所有 SSA BV bits；
- 当前 RF：`Σ k_r` selectors、`Σ C(k_r,2)` pairwise at-most-one constraints、每 source
  的 load/store activation implication、RF-Val，以及每 active read 的 RF-Some；
- 当前 CO：`Σ n_a * max(1, ceil(log2(n_a)))` rank bits、`Σ C(n_a,2)` distinct constraints、
  同样数量的 `co_before` Boolean 与 defining constraints；
- Yogar 初始抽象：保留 CFG/SSA/error/RF-Val/RF-Some/at-most-one，CO rank、`co_before`
  和 scheduling constraints 均记为 removed，不把 removed 数量算成实际 clause；
- ordering/EOG validator：初始 PPO/PO-loc edge 数、RF edge 上限、每地址潜在 WS pair 和
  FR derivation 上限、最大单地址 `n_a` 和 `k_r`；
- 热点分布：p50/p90/p95/p99/max，而不只报 225 个任务总和，避免少数大图被平均值隐藏。

P0 的通过条件不是笼统的“变量更少”。至少要证明：(1) 225 个 admitted 中每个 read 的
source 集合与现 encoder 完全相同；(2) 首次查询移除全部 CO rank/`co_before`；(3) 最大
地址热点不会迫使 validator 枚举 `n_a!`；(4) 156 个 admitted TIMEOUT 子集单独报告，
不能用 69 个易例稀释。

## 推荐原型顺序（代码前评审）

### P0：只测编码规模，不改变 verdict

实现一个离线 encoder census，针对 283 个 `pthread-wmm` 只输出以下三套表示的计数，
不调用 solver：

- guard/SSA Boolean 与 bit 数；
- RF variables 和 RF-Some clauses；
- 当前 CO-rank bit 数、pair constraints；
- Yogar 初始抽象保留的 guard/SSA/error/RF-value constraints，以及删去的 scheduling
  constraints；
- PLDI/TOPLAS 式 explicit order primitives、潜在 FR/WS derivations；
- 按地址的 read/write 数和理论最大 derivation edges。

通过标准：新表示在 225 admitted 任务上显著减少 variables/clauses，且没有遗漏任何
active read 的 RF source。否则不进入实现。

### P1：Yogar 式 scheduling-constraint CEGAR（只找 FALSE）

初始 SAT 公式只包含 guard、bit-precise SSA、error、RF-Val 和 RF-Some，不创建 BV CO
rank，也不预编码完整 scheduling constraint。每个抽象 error model 交给 EOG validator：

- 对结构认证的 SC/TSO/PSO，验证该 RF/control candidate 是否存在一致的 ordering/CO
  completion；
- 不可行时返回带来源的 kernel-reason clause；无法证明原因时只阻塞当前抽象 model；
- 可行时物化完整 RF/CO，交给通用 CAT evaluator，再交给普通 GenMC replay；
- 任意 CAT、EOG unknown、budget 和 UNSAT 全部回退 native verifier。

第一目标不是证明 TRUE，而是在 225 个 admitted 任务上测得首个 abstract model 时间、
refinement 次数、clause 大小和最终 replay-confirmed FALSE。这里省略 CO/scheduling 只是
抽象；最终可行性验证必须补全它们，不能把“未编码”误当成“不需要”。

### P2：认证模型的 PLDI/TOPLAS on-demand ordering theory

仅对现有 structural certificate 认出的 recursive SC/TSO/PSO 启用：

- RF/guard literal 增量传播 order；
- WS/FR 按需派生；
- cycle reason 转为 SAT clause；
- 每个 conflict clause 用独立穷举小图 oracle 与通用 CAT 重放；
- theory 不支持的 operator/model 立即回退 P1 或 native verifier。

### P3：PPoPP interference decision guidance

在 P1/P2 已能稳定产生模型后，再优先 RF/WS/guard interference literals，并单独测量
decisions、conflicts、propagations 和首个可行 EOG 时间。它不改变候选集合，理论风险低于
preventive propagation，但也不能修复一个仍被全量公式卡死的首次查询。

### P4：经过独立审计后才考虑 preventive propagation

OOPSLA 的 preventive rules 不是第一版目标。此前正确性审计和我们自己的通用 CAT 约束
都要求保留精确 fallback。只有 P2/P3 在实际包级已减少首次模型时间和 refinement/CAT
calls，才评估 preventive rule。

## 正确性与完备性义务

- finite lane 永不单独返回 TRUE；UNSAT、budget、unsupported 都回退 native verifier。
- FALSE 必须由约束后的真实 LLVM 经普通 GenMC + 通用 CAT/CAAT 找到错误。
- 每个 learned clause 必须只排除无一致扩展的 partial assignment；无法给出完整原因时
  只能阻塞当前完整 assignment。
- Yogar 式初始公式是 scheduling 的 over-approximation；只有 validator 找到完整一致
  completion 后才允许进入 replay。抽象 UNSAT 可以结束 finite lane，但不能结束整个
  GenMC 验证。
- SC/TSO/PSO 专用 derivation 只能在模型结构认证成功后启用。
- mutation oracle、18 primitive differential、SC/TSO/PSO verdict oracle、ASan+UBSan、
  864 broad 和实际 `pthread-wmm` terminal matrix 均为推广前门禁。

## 当前决定

暂停编码实现。先完成 P0 三表示 census、Deagle 源码 hook 对照和 Yogar EOG/refinement
规格表，再决定第一版是复用 Z3 做抽象 SAT 外层，还是接入 MiniSAT。ordering theory
backend 的选择属于第二个决定，不能和“初始公式是否包含 scheduling”混为一个问题。
不能仅因本地已安装 Z3 就继续沿用 monolithic Z3。

## 代码前淘汰条件

- 如果 Yogar 初始抽象没有移除当前占主导的 CO/order variables 或 pair constraints，停止；
- 如果 validator 不能为拒绝给出可独立重放的原因，只允许 exact abstract-model blocker；
- 如果一个 EOG candidate 仍要求枚举所有 CO permutation 才能判断，P1 不进入生产实验；
- 如果结构认证不能区分 recursive SC/TSO/PSO 与其他 CAT，专用路径不得启用；
- 如果 P0 只有变量数下降、却没有首个 query 预估和最大地址热点分布，不据此写 solver。

## P0 actual pthread-wmm 结果与方向修正

283 个实际任务的 solver-free census 已完成：225 个建成骨架，58 个 fail-open，与既有
census D 完全一致；225 个中没有 read 缺少 RF source。156 个 admitted baseline TIMEOUT
的中位数为 5,436 RF pairs、735 CO pairs、4,890 SSA value bits。全部 225 个的 RF pairs
总数 1,059,950，CO pairs 总数 141,179，比例约 7.5:1。

这否定了“只去掉 CO 就进入求解实验”的方案。下一版 P1 首次查询必须同时：(1) 省略
CO/scheduling completion；(2) 将 RF pairwise at-most-one 改成线性或 solver 原生
cardinality。否则仍保留当前最大的二次 constraint family。

另一个重要修正是：当前 encoder 本来就不预编码 FR；`potential-fr` 只是在完整 assignment
后由 CAT 可能派生的上限。因此 Deagle/PLDI 相对传统 eager-FR encoding 的收益不是我们
可再次获得的收益。下一步先实现无 verdict 的 abstract-first-model 计时器；在 EOG
completion 完成前，abstract SAT 不得进入 replay 或影响 verdict。完整数据见
`finite-representation-census-report.md`。
