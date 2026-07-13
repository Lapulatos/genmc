# GenMC 通用 CAT 内存模型支持：可行性分析

## 结论

用户明确的首期目标——从命令行输入 SC、TSO、PSO 等 CAT 模型，并让扩展后的 GenMC 在这些模型下验证程序——具备较高可行性。首期可以准确表述为：

> 支持足以描述 SC、TSO、PSO 的 CAT relational core，通过通用 evaluator 驱动 GenMC；以现有 SC/TSO checker 为对照，以 PSO 证明新增模型确实来自输入文件。

这个 MVP 不需要完整 CAT，也不需要一开始解决 CAAT 的通用递归 fixed point。主要工程问题缩小为：CAT frontend、execution graph 到 base relations 的映射、通用 consistency evaluator，以及一个 correctness-first 的 GenMC exploration adapter。

整体评级：

| 目标 | 可行性 | 判断 |
|---|---:|---|
| 命令行读取 `.cat` 并解析、类型检查 | 高 | 标准编译器前端工作 |
| 对完整 execution graph 解释 CAT 关系并判定 axioms | 高 | 可先做正确性 oracle，性能未必好 |
| 输入 SC/TSO/PSO CAT 并完成 GenMC 验证 | 高 | 所需 CAT 子集较小，SC/TSO 有内置 oracle |
| 用通用 CAT 后端驱动 GenMC 探索 | 中 | 需要 corder、coherence、prefix/hb、revisit 等额外契约 |
| 常见模型 SC/TSO/RA/RC11/IMM/部分 ARM/Power/LKMM | 中高 | Kater 与 CAAT 已分别证明重要子问题可行 |
| 完整兼容 herd7 的任意合法 CAT，且维持 GenMC 性能 | 低 | CAT 全语言太宽，且在线增量固定点仍是研究问题 |

## 关键事实

### 1. 当前分支还没有 CAAT 基础

`genmc-caat`、本地 `master` 和 `origin/master` 都指向 `29b03a6`（GenMC v0.17.0）。当前模型在 `MemoryModel.hpp` 中硬编码为 SC、TSO、RA、RC11、IMM，命令行也使用静态 enum。

### 2. GenMC 的模型接口远大于 `consistent(graph)`

`ConsistencyChecker` 还负责：

- 新事件加入时的增量一致性检查；
- read-from 候选和 coherence placement 生成；
- revisit 过滤；
- prefix、hb 和模型专用 view 缓存；
- data race、warning 和错误检查；
- 是否跟踪 dependency。

因此，只把 herd 的 CAT evaluator 接到 `isConsistent(const ExecutionGraph&)`，最多得到一个末端过滤器。它可能探索大量注定失败的前缀，也无法自然实现 coherent candidates、revisit pruning 和 race semantics。

### 3. GenMC 对输入模型有语义前提

GenMC/TruSt 要求模型给出 causal order `corder`、一致性谓词和错误谓词，并要求：

- `corder` 在一致图中无环；
- 一致性对 `corder`-prefix closed；
- 加入 `corder`-maximal event 时，至少存在一个 `rf/co` 选择保持一致。

普通 CAT 文件不会声明或证明这些性质。合法 CAT 不等于合法 GenMC backend。原始 C/C++11 模型就是论文列出的反例之一。

### 4. herd7 不是 Java 实现

herdtools7 的主体是 OCaml；官方仓库当前语言统计中 OCaml 占 55.9%。CAAT artifact 所基于的 Dartagnan/Dat3M 与 JavaSMT 才是 Java 路线。

herd 值得复用的是：

- CAT 语法和动态语义的权威行为；
- 大量 `.cat` 模型；
- litmus 测试与预期 outcomes；
- differential testing oracle。

不建议逐文件把 OCaml herd 翻译为 C++。herd 会枚举 CAT 的 `with ... from ...` 等候选扩展，而 GenMC 在探索过程中动态选择 `rf/co`，两者控制流不同。

### 5. Kater 是比 herd 更接近本项目的起点

当前五个 `*Checker.cpp/.hpp` 都标注为 Kater 自动生成，五个 `.cpp` 合计 19,172 行。2024 年 GenMC 论文说明，扩展后的 Kater DSL 已能自动生成 SC、RA、RC11、IMM 所需的：

- KAT relation 的 acyclicity checker；
- coherence/vector-clock 优化；
- irreflexivity、emptiness、inclusion；
- error/warning checks；
- saved views 和静态 assertions。

这已经证明“声明式模型生成高效 GenMC checker”可行。缺口是 Kater DSL 并非完整 CAT，而且当前 release 仓库中只保留生成结果，没有发现生成器源码或模型源文件。

### 6. CAAT 解决了通用固定点，但没有解决 GenMC 式在线集成

CAAT 能处理递归 derived relations，并覆盖 TSO、Power、ARMv8、RISC-V、IMM、RC11、LKMM。其有效算法主要针对 normalized、domain-independent、semi-positive 模型；非 semi-positive 模型需要 cutting，把部分 derived relation 变成 eager encoding。

CAAT 论文明确把 online/incremental integration 列为困难的未来工作：添加 base edges 可以从旧 fixed point 继续，删除 edges 往往需要重新计算；difference 的非单调性和 explanation derivation tracking 会进一步增加难度。GenMC 的回溯、restrict graph 和 revisit 正好会触发这些问题。

## 推荐架构

```text
.cat file
   ↓ parse / include resolution / type check
Typed CAT AST
   ↓ desugar / normalize / dependency & polarity analysis
Relational IR + checks + flags
   ↓ static GenMC-admissibility validation
Model contract: corder, coherence, hb/race, dependencies
   ├─ Generic full-graph evaluator（正确性基线）
   └─ Specialized incremental plan（常用模型性能路径）
                         ↓
                  ConsistencyChecker adapter
                         ↓
                    GenMC exploration
```

核心设计决定：不要让纯 CAT 猜测所有 GenMC hooks。增加一个很薄的 GenMC profile/annotation 层，例如声明 `corder`、用于 coherence 的 stable view、race relation、dependency tracking 和不支持能力。CAT 负责模型语义，profile 负责探索策略。

## SC/TSO/PSO MVP 边界

首期 CAT 子集建议固定为：

- event sets/tests：`R`、`W`、`F`、`[S]`；
- base relations：`po`、`rf`、`co`/`mo`、`fr`、`loc`、`int`、`ext`；
- relational operators：`|`、`&`、`\`、`;`、inverse、`?`、`+`、`*`；
- definitions：非递归 `let`；
- checks：`acyclic`、`irreflexive`、`empty`；
- comments、括号和带文件/行/列的诊断。

SC、TSO、PSO 的常见公理主要是 preserved program order 与 communication relations 的 acyclicity。PSO 相比 TSO 进一步放松不同地址的 write→write order；这可以通过 `loc`、event-set tests 和 relation difference 表达。首期无需支持 recursive functions、procedures、scopes、`with ... from ...`、`classes` 或 `linearisations`。

建议的 CLI 是：

```text
genmc --model-file=models/sc.cat program.c
genmc --model-file=models/tso.cat program.c
genmc --model-file=models/pso.cat program.c
```

`--model-file` 与现有 `--model=sc|tso|...` 应互斥。文件路径存在但模型不在支持子集时，应在开始探索前失败，而不是运行过程中报错。

首期可以使用保守的 generic adapter：枚举所有同地址 read-from 候选和 coherence placements，再用 CAT evaluator 过滤；revisit 也先少剪枝。这样可能慢，但不会因为沿用 TSO 专用剪枝而漏掉 PSO execution。SC/TSO 与现有 checker 达成结果一致后，再把 CAT relations 编译成 Kater 风格的 specialized checks。

## 三阶段实施路线

### 阶段 1：GenMC 接收并执行 `--model-file=<model.cat>`

目标不是只增加参数，而是完成第一条可运行链路：

```text
model.cat → lexer/parser → typed CAT IR → full-graph evaluator
          → GenericCATChecker → GenMC verification
```

工作项：

1. 增加 `--model-file`，并与现有 `--model` 做互斥检查。
2. 实现 SC/TSO/PSO 所需的 CAT relational core；首期不实现通用 recursive `let`。
3. 建立三阶段共享的 typed relational IR，避免第二阶段重写 parser。
4. 把 `ExecutionGraph` 映射为 `po/rf/co/fr/loc/int/ext` 和 event sets。
5. 实现从头计算的 full-graph evaluator 和 correctness-first `GenericCATChecker`。
6. 保守枚举 `rf/co` candidates，再用 CAT consistency 过滤；暂不追求内置 checker 的性能。
7. 提供 `sc.cat`、`tso.cat`、`pso.cat`；PSO 不增加硬编码 `PSOChecker`。

验收标准：

- SC/TSO 的 execution count、allowed outcomes、错误集合与内置 checker 一致；
- SC/TSO/PSO 与 herd 的 litmus outcomes 一致；
- 只替换 `.cat` 文件即可改变模型；
- parse/type/unsupported 错误在探索前报告文件、行、列；
- 现有 `--model` 行为和 checker 保持不变。

### 阶段 2：实现离线 CAAT backend

这一阶段实现 CAAT 的模型求值和 explanation 能力，但允许每次从头计算，不要求维护在线状态：

1. 把 typed CAT IR normalization 为 CAAT 风格的 base/derived predicates 和 strata。
2. 实现 least-fixed-point evaluation 与 recursive relation definitions。
3. 检查 domain independence、stratification 和 semi-positivity；不支持的模型给出静态诊断。
4. 实现 `acyclic`、`irreflexive`、`empty` violation detection。
5. 为 inconsistency 生成由 base edges 组成的 explanation，为第三阶段保留稳定接口。
6. 评估 cutting；首版可以拒绝非 semi-positive 模型，而不必立即实现通用 eager encoding。
7. 扩展到至少一个阶段 1 之外、需要递归 derived relations 的模型。

验收标准：

- CAAT backend 与阶段 1 full evaluator 在 SC/TSO/PSO 上结果等价；
- recursive/semi-positive 模型与 herd outcomes 一致；
- 每个 inconsistency explanation 单独重放时足以再次触发 violation；
- 此时性能提升不是完成条件，正确性和 explanation 可验证性才是。

### 阶段 3：incremental/online CAAT integration

这一阶段才处理 CAAT 论文明确留下的研究问题：

1. 定义 `pushEvent`、`addBaseEdge`、`removeBaseEdge`、`pop/backtrack`、`restrictGraph` 接口。
2. 对单调 additions 增量维护 derived relations 和 least fixed points。
3. 对 deletion/backtrack 先实现可靠方案：undo log、persistent versions 或局部失效后重算；随后再优化动态删除。
4. explanation 必须 trail-valid，不能引用当前 GenMC prefix 中已撤销的 edges。
5. 在 `rf/co` candidate selection 和 revisit 过程中尽早传播 inconsistency，而不是只检查完整 graph。
6. 保留阶段 1 的 from-scratch evaluator 作为 debug oracle，随机或定期比较增量状态。
7. 最后再研究 non-monotonic difference、on-demand cutting 和 vector-clock/automata specialization。

验收标准：

- 任意 push/pop 序列后的增量结果与从头计算结果一致；
- GenMC backtracking、revisit 和 restricted graph 不留下 stale derived edges；
- 开启 online pruning 与关闭它时，完整 execution 集和错误集合完全一致；
- 在选定基准上减少 from-scratch consistency 次数和总运行时间，并单独报告额外内存。

### 阶段依赖原则

- 阶段 1 的 parser、typed IR 和 full evaluator 是后两阶段的语义基线，不能作为临时代码丢弃。
- 阶段 2 的 explanation interface 要按阶段 3 的 trail/backtrack 需求设计，但阶段 2 本身不维护 trail。
- 阶段 3 只改变求值时机和缓存方式，不改变 CAT/CAAT 的接受语义。
- Kater-style automata 和 vector-clock specialization 属于性能层，可以在阶段 2 后期或阶段 3 后期加入，不能先于 correctness oracle。

## 主要风险与边界

- **正确性风险最高**：错误的 candidate pruning 会漏掉执行，比慢更严重。因此通用 evaluator 与探索优化必须分层，优化只能在有证明或差分测试支持后启用。
- **完整 CAT 兼容会迅速膨胀范围**：recursive functions、pattern matching、procedures、scopes、`classes`、`linearisations`、`with` 和 flags/undefined 都需要单独语义设计。
- **CAT 事件宇宙与 GenMC LLVM events 不完全同构**：dependency、plain accesses、fences、scope 和 model-specific primitives 需要显式映射。
- **模型合法性不可只靠 parser 判断**：还要区分 CAT type-valid、evaluator-supported、CAAT-fragment、GenMC-admissible 四种状态。
- **性能不能用 herd 作为目标**：herd 是 reference simulator；GenMC 的优势来自在线剪枝和每个 `rf` 等价类只探索一次。

## 最终建议

按现在明确的顺序，这个项目值得直接进入阶段 1 设计：先交付 CAT-file-driven GenMC，再用离线 CAAT 扩展语义能力，最后研究 incremental/online CAAT。技术主线仍应以 herd 做外部语义 oracle、以阶段 1 full evaluator 做内部 oracle，并在正确性稳定后借鉴 Kater 的生成式增量 checker。

第一项实现工作不应是移植 herd，而应是写一份 `supported-cat.md`：逐项定义支持的语法、base relations、fixed-point 限制、GenMC profile，以及四类诊断（parse/type/unsupported/not-admissible）。这份契约会决定后续 parser、IR、checker 和测试能否稳定演进。

## 证据来源

- 当前仓库：`MemoryModel.hpp`、`ConsistencyChecker.hpp/.cpp`、`lli/main.cpp`、Kater-generated checkers。
- 本地论文：CAT syntax/semantics（2016）、CAAT（OOPSLA 2022）、Kater（POPL 2023）、GenMC（CAV 2021）、Enhancing GenMC（TACAS 2024）。
- 官方在线资料：herdtools7 GitHub、Kater project page、CAAT Zenodo artifact。
