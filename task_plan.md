# Task Plan: GenMC 通用 CAT 内存模型可行性分析

## Goal
按三阶段路线扩展 GenMC：先通过 `--model-file=<model.cat>` 加载并执行 CAT 模型，再实现离线 CAAT consistency backend，最后实现 CAAT 所讨论的 incremental/online integration。

## Phases
- [x] Feasibility 1: 确认分支、工作区与材料位置
- [x] Feasibility 2: 分析 GenMC、CAT、CAAT、Kater 与 herdtools7
- [x] Feasibility 3: 形成风险边界和三阶段路线
- [x] Implementation 1: CAT file support（SC/TSO/PSO full-graph consistency；实现、验证与报告已完成）
- [x] Validation 1: 已运行 288 个不同程序（288 个有效），576 组 SC/TSO 差异清零
- [x] Implementation 2: Offline CAAT backend（Phase 2.0--2.6 已实现、广泛验证、提交并 push）
- [ ] Implementation 3: Incremental/online CAAT（push/pop/backtrack/early pruning）

## Active optimization priority override (2026-07-18)

The current optimization order is the completed evidence review under
`optimization-analysis/core-direction-review-20260718/`: the core path is SC-RVF (or an
equally complete consistent-class quotient) under a whole-program/whole-suffix certificate,
beginning with proved property-endpoint semantics. Generic regional fallback is unsound
in the current task decomposition, and V10 already occupies the generation boundary, so
its positive cores cannot be promoted into subtree blockers without a stronger no-
consistent-extension certificate. P1 exploration/history compression remains the separate
pre-CAT OOM track. Older unchecked items below are
historical records, not active work, unless this override explicitly restates them.
Cursor/cache/local-scan/evaluator-only work is deferred until a core branch has reduced
actual candidate/class/work counts on the full workload.

**2026-07-18 correction:** cross-execution learned CAT nogoods are not an untried P0
direction. The archived `continuous/learned-cat-nogoods` V1--V4 series already tested
complete rejected-graph learning, exact future matching, lazy-preserving provenance, and
warm-up admission. V3 and V4 both passed their correctness matrices but regressed formal
task-model CPU (1.01032x and 1.01162x). A duplicate prototype started on 2026-07-18 was
removed before any server job. Do not reopen this line without a mechanism that removes
the measured Reasoner/literal-matching cost rather than retuning admission thresholds.

## Active workload funnel (2026-07-18)

- [ ] Run new core candidates first on the actual `pthread-wmm` package, preserving its
  ordinary per-task limits and baseline command line.
- [ ] Compare terminal classification first: correct terminal, TIMEOUT, OOM, error kind,
  and unsupported/fail-open; raw execution counts are diagnostic rather than equality
  requirements for a coarser complete quotient.
- [ ] Require a clear reduction in TIMEOUT/OOM with no correctness or coverage loss before
  expanding to other benchmark packages.
- [ ] After that gate passes, run the unchanged candidate on the remaining packages and
  finally the complete 725-task workload.

**Scope decision:** `pthread-wmm` is the P0 experimental funnel because it contributes
179 of the 239 observed TIMEOUTs. Its dominant cases are loop-free, so loop-bound tuning
is not the first mechanism to test. Intermediate formal/performance matrices do not
replace this actual-package terminal gate.

## P0 finite symbolic event-skeleton lane (2026-07-18)

- [x] Confirm that neither a finite symbolic lane nor property-directed ordering was
  implemented by earlier optimization rounds.
- [x] Add an opt-in transformed-LLVM admission census for finite control, recursion,
  indirect calls, static addresses, guards, nondet, memory events, and external effects.
- [x] Run the census on every actual `pthread-wmm` task in the server Docker pipeline.
- [x] Freeze the first encoder subset from measured coverage.
- [x] Add the optional Z3 solver interface and finite skeleton IR. (Z3 interface complete;
  pointer-free finite skeleton IR now builds on all 225 admitted pthread-wmm tasks; the
  executable constraint translation remains.)
- [x] Encode guard/nondet/event activation and RF/CO, with exact CAT fallback.
- [x] Add conservative interpreter replay for errors. TRUE remains on the unchanged native
  verifier; the finite lane never returns TRUE or removes a native execution.
- [x] Pass local and server Release/ASan+UBSan correctness gates.
- [x] Run the paired baseline/candidate actual 725-task workload directly and analyze it.
- [ ] Try property-directed ordering only after the exact symbolic lane runs end to end.

Protocol: `optimization-analysis/core-direction-review-20260718/finite-symbolic-lane-protocol.md`.
The local admission analyzer builds and reports `finite-control=1 encodable-subset=1` on
the loop-free SB smoke fixture; `fast-driver` and `sc-rvf-outcomes` pass. The analyzer is
diagnostic only and default exploration is unchanged.

**Server admission result:** authoritative rerun B records 283/283 complete skeleton
records. All 283 are finite-control; 225 are in the first static-address subset. This
includes 156/179 baseline TIMEOUT rows. The remaining 58 tasks (23 TIMEOUT) have exactly
one dynamic address and are fail-open scope. No `pthread-wmm` row is OOM. Evidence and
decision: `finite-skeleton-census-report.md` and
`server-results/finite-skeleton-pthread-wmm-20260718b/`.

**Build/analysis corrections:** the first host build attempt was invalid because the
Docker-owned build tree is not writable by the SSH user and GCC 13 exists only in the
container. The authoritative rebuild used `genmc15noble:sujie` with `/data3` mounted at
its recorded absolute path. Census A scanned every defined transformed helper and was
therefore retained only as diagnostic evidence. Census B restricts analysis to `main`,
direct callees, and transformed thread-entry operands; it is the authoritative admission
result. The 58 dynamic-address blockers remain after this correction and are real
reachable sites rather than unreachable helper noise.

**IR-construction checkpoint:** authoritative server rerun C has manifest status zero and
283/283 complete records. All 225 static-address tasks build the pointer-free IR, while
all 58 dynamic-address tasks fail open before construction with no new blocker class.
The admitted IR totals 105,309 integer/Boolean SSA values and 60,414 event sites. This is
not yet symbolic verification evidence; executable guard/RF/CO/CAT constraints and replay
remain required.

**Constraint/CAT checkpoint:** guard/nondet/activation/RF/CO encoding and direct generic
CAT/recursive CAAT materialization are executable locally. Recursive Reasoner explanations
now replay exactly into Z3 after fixing premature model invalidation, with an end-to-end
unit test. The mechanism is not promoted: on actual `mix000`, 100 explanation clauses cost
3.83 s / 279.8 MB versus 2.89 s / 204.3 MB for graph blocking and did not reduce the first
100 CAT calls. Strict admission, interpreter replay, CAT-adapter differential validation,
and a complete TRUE certificate remain before any verdict-changing lane or package run.

**Replay checkpoint:** a stable instruction identity now maps symbolic values back to the
transformed LLVM module. The first CAT-consistent error candidate can be emitted with exact
nondet values and active-load value assumptions, then checked by the ordinary interpreter
and recursive CAT backend. Registered `finite-skeleton-error-replay` passes end to end and
reports a real safety violation. This is still diagnostic and fail-open: RF/CO are not
forced, replay failure is inconclusive, and TRUE is not returned. A GraphAdapter source
audit also corrected `SC` classification and added fence events before TSO/PSO testing.

**Strict-admission checkpoint:** direct production-adapter differentials match all 18 CAT
base predicates for memory RF/CO and split-lock RMW snapshots. Switch, GEP/alloca, non-
integer memory, unbound calls/arguments, joins, and ambiguous thread/mutex identities now
fail open. Server census D has 283/283 complete records and preserves exactly 225 built /
58 dynamic-address fallback tasks and the prior IR totals. This does not yet establish
whole-program TRUE equivalence or package terminal gains.

**Production FALSE-lane checkpoint:** `--finite-symbolic-errors` enumerates only strictly
admitted finite assignments, checks them with generic recursive CAT, and asks an unchanged
ordinary GenMC run to rediscover every candidate error. Only that native error confirmation
can terminate early; unsupported, UNSAT, budget exhaustion, and failed replay all fall back
to native verification. Server Release passed 203/204 unit tests with one expected Z3 skip
and all three finite CTests. Server ASan+UBSan passed the 14 focused tests (13 pass, one
expected skip) and all three finite CTests without sanitizer findings. The paired 725 run is
active in `formal-results/finite-symbolic-full-725-20260718a` using 60 s, 4 GiB and disjoint
24-core queues for baseline and candidate.

**Full-725 decision:** reject the current finite symbolic FALSE lane. Correct terminals
fall from 398 to 318, TIMEOUT rises from 239 to 302, ABORTED rises from 2 to 21, and total
CPU rises 21.1%. Only two replay errors were confirmed and both were already fast baseline
FALSE results. Preserve the opt-in implementation for diagnosis only; next work is the
uncaught `std::out_of_range` and the fundamental 10,000-assignment front-loaded cost, not
another broad run. The 725-row HTML and 96-row diff are in the local server-results tree.

**Crash and P2 experiment:** the `std::out_of_range` was caused by a builder reference
invalidated when constant operands grew `program.values`; the indexed write repair and a
structural fail-open validator pass Release and ASan+UBSan gates. A direct property-target
constraint was then rejected on all 283 actual `pthread-wmm` tasks: 104 -> 35 correct and
179 -> 248 TIMEOUT, with the 225 admitted tasks timing out before assignment 1. The hard
constraint was removed. The next core design must stage/lazily instantiate RF/CO after a
property-relevant control slice; assignment-limit tuning cannot fix first-query timeout.

**Design pause after literature review:** do not implement the previously proposed staged
encoder yet. Deagle's actual architecture prioritizes Boolean RF interference variables
inside DPLL and derives WS/FR/order consistency on demand, whereas our current Z3 formula
eagerly creates BV CO ranks and pair constraints for the whole skeleton. Before new solver
code, run a no-solver representation census over all 283 `pthread-wmm` tasks and complete
the source-hook mapping. The reviewed design and proof obligations are in
`deagle-smt-encoding-design-review.md`.

**Literature-scope correction:** Deagle is not the complete design space. The active review
now separates Yogar-CBMC scheduling-constraint CEGAR/EOG kernel-reason refinement from the
PLDI 2021/TOPLAS 2023 exact ordering theory, PPoPP 2022 interference decision guidance, and
OOPSLA 2022 preventive propagation. The leading P1 hypothesis is now a Yogar-style initial
abstraction that omits scheduling/CO from the first query, because the measured failure is
before assignment 1. Ordering theory is the validator/completion layer, not a substitute for
this formula-level decision. No solver implementation starts before the three-representation
census and source/spec mapping are complete.

**Representation census decision:** the 283-task actual `pthread-wmm` census completed with
225 built / 58 fail-open and zero built reads without an RF source. In the 156 admitted baseline
TIMEOUT tasks, median RF pairwise constraints are 5,436 versus 735 CO pairs; across all 225,
RF pairs total 1,059,950 versus 141,179 CO pairs. A CO-only Yogar prototype is therefore
rejected before implementation. P1a must combine scheduling/CO omission with linear or native
RF cardinality and first measure abstract first-model time without affecting verdict. Report:
`finite-representation-census-report.md`.

**Iteration-panel decision:** tool-development performance runs now use the frozen 15-task
easy/medium/hard panel in `pthread-wmm-15-panel.tsv` and `.md`. It contains 3 FALSE + 2 TRUE
easy completions, 3 FALSE + 2 TRUE 10--50 s medium completions, and the 5 largest admitted
TIMEOUTs by RF+CO pair count. Full 283-task runs occur only after this panel shows a clear gain
with no correctness loss; the panel never replaces final package validation. This is the
default truncation policy while the tool is still being completed: run exactly 5 easy, 5 medium,
and 5 hard tasks for each performance-bearing iteration, rather than repeatedly running all 283.
Unit, sanitizer, differential, CAT validation, and native replay correctness gates remain
separate and must still pass before a candidate can be promoted.

**P1a/P1b checkpoint:** P1a passes its representation gate: eager and no-CO/pairwise modes
produce 9/15 and 10/15 first models, while no-CO/native-cardinality produces 15/15 with total
CPU 102.37 s and peak RSS 69.9 MB. P1b exact SC completion passes 24/24 local, server Release,
and Linux ASan+UBSan focused gates plus an exhaustive RF/CO oracle. The first abstract candidate
is infeasible on all 15 tasks. Whole-graph blocking and deletion/QuickXplain RF refinement are
rejected as performance mechanisms: the best core run still has 13/15 TIMEOUT and 274.87 s CPU.
The next core step is a single active-event ordering query with fixed RF assumptions and native
UNSAT-core extraction; do not tune the repeated-check core prototype further. That rank-SMT
prototype was subsequently rejected: 15/15 time out in the first ordering call and panel CPU
rises to 315.58 s. The corrected next step is a graph-native incremental ordering theory with
on-demand derived edges and conflict explanations, not another global rank formula. Reports:
`finite-first-model-panel-report.md` and `finite-sc-completion-panel-report.md`.

**Superseded Graph-native P1c design checkpoint:** the source audit confirms that `LazyCycle`,
`IncrementalEvaluator`, and `ConflictCore` cannot resolve existential ordering alternatives,
and a dead-end explanation from `SCGoodWritesSolver` would not cover all prefixes.  The frozen
replacement is a rollback ordering graph over SC visibility/RMW disjunctions with RF and
internal-branch proof literals.  Only a root conflict with every branch literal resolved may
block an RF assignment.  The exact contract, fallback rules, counters, and pre-panel gates are
in `graph-native-sc-theory-design.md`.  Implementation is now limited to this SC theory and its
small exhaustive oracle; property ordering and generic CAT integration remain deferred.
This standalone plan was superseded before implementation after the architecture audit
showed it would retain a separate whole-program SMT/SC search outside GenMC.

**Unified GenMC/CAAT/CDCL checkpoint:** `BasicCATChecker` already owns the real integration
surface: actual RF-source and CO-placement hooks, analyzer-certified preventive checked
orders, focus/full-root reachability, top-level cycle derivation, stable event identities,
and `GraphSynchronizer` insert/rollback/replace.  The missing capability is CDCL over stable
choice literals across revisits; `ConflictCoreDatabase` is only linear positive-core
matching and its V3/V4 regression must not be repeated.  The active next gate is a read-only
opportunity census measuring conflict recurrence and prefix compatibility before adding a
solver.  Design: `genmc-caat-cdcl-integration-design.md`.

**Existing-evidence refinement:** the archived V9 725 logs aggregate to 5,814,007 RF and
12,899,587 CO preventive prunes, but zero all-sibling-pruned fallbacks and zero already-
inconsistent prefixes.  V10's 14,193,035 core hits likewise changed no common-correct search
counter and regressed CPU 4.99%.  Immediate candidate cycles therefore cannot justify a new
CDCL implementation.  The next diagnostic is restricted to non-local complete-CAAT
conflicts after candidate installation and must measure actual potential backjump depth.

**Implementation hold and algorithm freeze:** no CDCL/choice-ownership implementation may
start before review of `genmc-caat-cdcl-algorithm-plan.md`.  The plan assigns schedules to
GenMC/DPOR, RF/CO Boolean combinations to a single worker-local lazy CDCL engine, and CAT
relations to the existing incremental CAAT state.  Because GenMC's worklist is not a SAT
trail, effective backjumping requires graph-matched `DecisionState` sidecars and immutable,
structurally shared work-item snapshots plus clause-based rejection before restore.  A
linear parent trail is invalid for vector-clock backward revisits with holes.  The cost
model forbids eager event
matrices/ranks, budgets clauses/literals/additional heap, and requires actual queued-prefix
reduction before implementation.

**Census correction:** the first observation prototype was removed before an accepted run.
`EventLabel::Stamp` records insertion order, not RF/CO decision depth; forward, backward,
and in-place revisits can replace a choice on an older label, while backward graph copying
can select a non-linear vector-clock view and resets stamps.  The pre-implementation
census therefore requires an observation-only graph-matched `DecisionState` plus exact
work-item snapshots, explicit restore/cut/revisit semantics, and graph-consistency
assertions.  No stamp-derived
backjump metric may be used as evidence.

**Search-ownership and resource correction:** reducing the solver to an advisory certificate
cache would repeat the V10 architecture and cannot deliver CDCL traversal.  Ownership is
partitioned: GenMC is complete for control/schedules/events, while the solver is complete
for each delegated RF/CO subspace.  Every feasible delegated assignment must remain current,
queued, or explicitly present in the solver frontier.  The 50k-clause / 1m-literal / 32 MiB
figures are observation thresholds, not hard fallback limits.  Redundant unlocked learned
clauses may be garbage-collected normally; losing the delegated frontier is resource
exhaustion and forbids TRUE.  Exact fallback is deferred until the remaining frontier can
be exported or restarted from a completeness checkpoint.

**TruSt optimality boundary:** the CAT/CDCL extension may not generate program paths,
replace `getRevisitView()`, change `isMaximalExtension()`, or introduce a new equivalence
relation while claiming native optimality.  Every native revisit must be explored through
the unchanged TruSt path or carry a complete no-consistent-extension certificate covering
its whole activation scope.  Concrete-suffix UNSAT is insufficient.  The executable gate
compares bounded execution-graph representative signatures and audits every native revisit
ID for exactly one `EXPLORE`/`PROOF_PRUNED` terminal classification.

**Prefix-nogood hypothesis, not yet selected:** further TruSt-preserving pruning could use
extension-closed prefix nogoods, but V9/V10 evidence indicates direct fixed-prefix cycles
may rarely eliminate a whole subtree.  The census must separate repeated local hits from
clauses unit before installation, resolution-derived parent UNSAT, and actual work items
skipped before restore.  Parent UNSAT additionally requires a domain-closure certificate,
because future writes/backward revisits can introduce new RF alternatives.  If these
strong categories are negligible, this direction is rejected rather than promoted.

The basic learnable object remains:
A compact mixed RF/CO clause is learnable only when a sparse CAAT proof plus analyzer
certificate shows that those already-fixed choices derive a cycle in a prefix-monotone
checked root.  Any native TruSt item whose exact snapshot entails the clause's cause has no
consistent extension and may be proof-pruned; all others follow unchanged TruSt logic.
State would be scoped to the live execution family and bounded proportionally to active events
plus retained native work items; unlocked redundant clauses and dead scopes are reclaimed.

## Key Questions
1. “任意有效 CAT”能否在 GenMC 的增量/完备性要求下直接解释执行？
2. 哪些 CAT 构造可统一实现，哪些需要限制或模型专用 hooks？
3. herdtools7 中可迁移的是解析器、语义、算法还是主要只有测试语料？
4. 最小可交付版本应覆盖哪些语法与内存模型？

## Decisions Made
- 可行性子阶段只分析、不修改代码；后续实现严格按已评审的 Phase 1
  子阶段计划执行。
- 以本地论文、当前分支源码和 herdtools7 官方仓库为主要证据。
- 用户明确首期不要求任意 CAT；MVP 以 SC、TSO、PSO 为目标，并保留后续扩展能力。
- 实施顺序固定为 CAT file support → offline CAAT → incremental/online CAAT。
- 第一阶段必须实现可运行的 full-graph consistency 闭环，而不只是增加 CLI 参数。
- 三阶段共享同一个 typed relational IR；后续阶段替换/增强 evaluator，不重写 parser。
- 每个实施子阶段必须先读 `doc/development.md`，再读 `doc/cat/PROJECT_CONSTRAINTS.md` 和当前阶段计划，完成复用调研、验证、差距分析、记录、commit 和 push。
- 生产热路径默认使用 C++23；herdtools7 作为语义/test oracle，未经许可证审查不复制其 CeCILL-B 源码。
- 新增代码遵循项目约束中的 Doxygen 与逻辑块注释标准。

## Errors Encountered
- 初次关键词检索被大量测试参数噪声淹没；后续按源码目录和核心类型定向检索。
- `git clone --depth 1 https://github.com/herd/herdtools7.git` 遇到 LibreSSL TLS 连接失败；改用官方 GitHub 页面和本地 CAT 论文核查，未下载仓库。
- Phase 1.0 首次链接失败：CMake 找到 hwloc 后以 `-lhwloc` 链接，但 Homebrew 路径不在 linker search path；使用 `-DCMAKE_EXE_LINKER_FLAGS=-L/opt/homebrew/opt/hwloc/lib` 建立基线，未修改仓库源码。
- `BUILD_TESTS=ON` 在 RapidCheck 的未使用 Catch submodule clone 长时间无进展；改用 `FETCHCONTENT_SOURCE_DIR_RAPIDCHECK` 指向已完整克隆的 RapidCheck 主源码。随后 unit tests 40/40 和 CTest fast-driver 1/1 均通过，无仓库源码改动。
- Phase 1.1 首次 CLI 测试发现 LLVM `cl::opt<std::string>` 对重复参数采用最后一个值而不报错；现在保存 occurrence count，并由 `Config::validate` 给出稳定的重复参数诊断，单元与 CLI 测试覆盖该回归。
- Phase 1.1 的 `clang-tidy` 使用生成的 compilation database 运行后，被本机工具链无法解析 libc++ C++23 `<format>` 阻塞；正常 CMake 编译、46/46 单元测试、CLI 1/1 和 fast-driver 1/1 均通过。
- Phase 2.4 的 `clang-tidy` 再次被本机独立工具无法定位 libc++ 的
  `<algorithm>`/`<cstddef>` 阻塞；CMake 使用同一 compilation database 正常构建，
  116/116 单元/性质测试和 4/4 CAT 集成测试通过。可读性提示已人工审阅，
  不将现有 checker override 提示或工具链错误混入本子阶段。
- Phase 1.2 首次 include/location 测试暴露了 `Parser` 构造参数中先 move source 还是先 lex 的求值顺序风险；现在明确先完成 tokenization，再移动 source，避免 `Lexer` 的 `string_view` 观察 moved-from 字符串。
- Phase 1.2 负例审查发现缺失左操作数可能让 binary fold 解引用空 AST；增加 early return 和专门回归测试。前端当前 focused 24/24、完整 unit 64/64、CLI 1/1、fast-driver 1/1。
- Phase 1.3 直接从 Phase 1.2 AST lower，不重新读取模型；`Config` 现在保存 `shared_ptr<const ModelIR>`。SC/TSO/PSO 分别形成 17/43/45 节点的精确 golden DAG，完整 unit 75/75、CLI 1/1、fast-driver 1/1。
- Phase 1.4 新增 word-packed set/relation 与独立 pure evaluator；RapidCheck 对照 `std::set`，SC 方程无需模型名分派即可解释。完整 unit 85/85、CLI 1/1、fast-driver 1/1，64→512 事件基准约 2.09 ms。
- Phase 1.5 新增只读 `ExecutionGraph` 快照适配器；稳定 dense ID 覆盖真实标签及按地址展开的虚拟初始写，并构造 `R/W/F/IW/SC`、`po/rf/co/fr/rmw/loc/int/ext/tc/tj`。虚拟初始写映回 `InitLabel + 地址`，避免跨地址伪造 `fr`。
- Phase 1.6 新增 `CATChecker` 纵向执行路径；通用 evaluator 判定候选图，SC host profile 只复用视图/错误基础设施，`rf/co/revisit` 使用保守枚举。SC smoke/error 集与内置 checker 的状态、执行数和错误类别一致。
- Phase 1.7 增加 herd-compatible 的显式 `@genmc host-profile` 元数据；TSO 模型选择生成式 TSO 宿主视图但仍由通用 evaluator 判定一致性。七个 GenMC 差分案例和两个 herd X86 oracle 均通过。
- Phase 1.8 让 PSO 显式复用 TSO host profile，只通过 CAT `ppo` 方程放松跨地址 W→W；同一 WW+RR 程序仅替换模型文件即可区分 SC/TSO 与 PSO，并由 herd 官方 TSO/PSO 方程交叉确认。
- Broad-validation harness 首次运行在 macOS Bash 3.2 下失败：`set -u` 不允许展开空数组，且 xargs 丢弃空 expected 参数造成 worker 参数错位。改为构造始终非空的完整命令数组，并以 `-` 编码缺失 expectation 后重跑；首轮输出不作为模型证据。

## Status
**Phase 1 complete** - 干净构建、100/100 单元/性质测试、4/4 CAT 集成测试、herd oracle 和 fast-driver 均通过；兼容性、性能与 Phase 2 输入已记录在 `doc/cat/phase-1-report.md`。下一目标是先评审 Phase 2 详细计划，不直接扩张实现范围。

**Broad validation complete** - 288 个程序全部形成有效证据；576 组 SC/TSO
内置 checker 与 CAT checker 结果完全一致，最终 mismatch 和 unsupported 均为 0。

**Phase 2 complete** - 递归语义、可接受性分析、离线 fixed point、解释、
GenMC 集成和 864 组广泛差分均已完成；最终报告为
`doc/cat/phase-2-report.md`。下一阶段仅在重新读取约束并制定 Phase 3
详细计划后开始，不把 incremental/backtracking 工作混入 Phase 2。

**Phase 3.0 ready to commit** - 已完成 CAAT/Kater/Dat3M 与 2023--2026
后续工作的调研，第三阶段合同冻结在 `doc/cat/phase-3-plan.md`；CAT/CAAT
基线 72/72 通过，Phase 2 广泛结果保持 864/864 match。提交并 push 后，
从可扩展 packed values 与独立 incremental evaluator 开始，不直接跳到
GenMC 热路径。

**Phase 3.1 implemented** - packed set/relation 已能跨 64 位边界保持内容
增长；独立 incremental state 的初始化/重建与 Phase 2 oracle 一致。正常
unit 125/125、并行 CAT/CAAT 77/77、ASan+UBSan focused 5/5 通过。当前差距
是尚未实现 insertion delta；这是 Phase 3.2 的唯一生产目标。

**Phase 3.2 implemented** - positive normalized operator 全部支持
insertion-only worklist 传播；删除、缩小和 difference 会在事务提交前要求
rebuild。unit 129/129、并行 CAT/CAAT 81/81、sanitizer incremental 6/6
通过，随机序列逐步匹配 Phase 2 oracle。下一目标是 checkpoint/rollback，
不能只用重新初始化模拟回滚。

**Phase 3.3 implemented** - 精确 snapshot checkpoint 支持嵌套 rollback、
后继/foreign/旧 epoch handle 拒绝、violation witness 恢复和当前 Reasoner
解释；随机 push/pop 树逐节点匹配 Phase 2。unit 132/132、incremental 9/9
通过。当前显式差距是 checkpoint 为全量复制且尚未接入 ExecutionGraph；
Phase 3.4 将实现稳定 event key、变更分类、回退原因与内存界限。

**Phase 3.4 implemented** - stable `EventPos`/`SAddr` ID、inactive `_`、
六类 graph transition、有界 checkpoint 与统计已实现；rf/co edge mutation、
RMW/lifecycle、cut/removeAfter、non-LIFO revisit matrix 已覆盖。unit 136/136、
parallel CAT/CAAT 98/98、ASan+UBSan focused 13/13 通过。下一目标是把该状态
真正接入每个 `BasicCATChecker` worker；standalone API 通过不能替代此目标。

**Phase 3.5 implemented** - 每个 recursive/forward-reference CAT checker worker
已持有 incremental evaluator/synchronizer；`--cat-stats` 可观测真实 transition。
SC/TSO/PSO 在 `fcombiner-async` 上均产生 insert 与 rollback-insert；unit
138/138、parallel focused 103/103、ASan+UBSan 15/15 通过。正单调违例可在
prefix 阶段持续拒绝，difference 因可能被后续插入修复而在执行前拒绝，不能
采用原计划中不安全的“offline prefix rejection”。下一目标是 3.6 的 mutation/
fallback stress、周期性 Phase 2 oracle cross-check 和确定性 mismatch dump。

**Phase 3.6 implemented** - 新增默认关闭的 `--cat-oracle`，逐 query 全量重算并
比较 error、每个 predicate、check 与 witness；差异会输出确定性 query/transition/
event/predicate 信息。39 组 exhaustive/randomized mutation rows 完成 5,396 次
oracle check，零 mismatch；unit 138/138、parallel focused 104/104、ASan+UBSan
15/15 通过。下一目标是 3.7 的 864+ broad validation、transition/performance/RSS
统计、最终文档与 requirement audit。

**Phase 3 complete** - 冻结 288-program ×
SC/TSO/PSO corpus 达到 864/864 match、0 mismatch、0 unsupported；结果逐行保存
transition/worklist counters。39-row mutation suite 有 5,396 次 oracle match。
独立 `196d370` Phase 2 binary 与当前 online binary 的 54-run time/RSS 对比已
记录，当前 full-snapshot checkpoint 导致平均时间 0.0700→0.2004 s，而 peak RSS
基本不变。最终报告、支持边界与逐条 audit 已完成；closure commit `93a5fdb`
已推送，提交后 local/remote 相等且工作区干净。

## SC RVF restored-flow gate (2026-07-17)

- [x] Reconcile the prototype with the frozen design and experiment protocol.
- [x] Remove the post-outage SmallVector micro-optimization drift.
- [x] Add a generated reachable-state oracle and validate its comparison rules.
- [x] Reproduce two native/RVF state losses in 324 one-worker state cells.
- [x] Reject and remove the insufficient prefix-witness coherence repair after 175/175
  unit tests and an unchanged four-shape failure result.
- [x] Repair the lost-state cases by refining own/other-thread sources and applying the
  witness-derived coherence order; four-shape oracle reaches 324/324.
- [x] Run all 20 canonical shapes with one and two workers on the server: 6,480 calls,
  zero timeout, late fail-open, worker mismatch, or state mismatch.
- [x] Run server Release/ASan+UBSan focused gates and the 185-case paired differential.
- [x] Run server mutation (39 rows / 5,441 checks) and broad differential (864/864).
- [x] Define and locally verify the quotient-disabled instrumentation control.
- [x] Run the formal V9/RVF/quotient-disabled/TSO/PSO performance matrix.

**Current decision:** every correctness gate is green. The explicit
`--sc-rvf-disable-quotient` control retains RVF setup and per-read instrumentation while
delegating every read to native RF-DPOR; its IRIWish smoke exactly preserves 27 complete
executions and all native candidate/work counters, records 31 disabled-control reads, and
queues zero representatives. Formal definitions and launch guards are the next step.

**Formal launch error:** the first full-matrix container exited before creating its output
directory because the disk guard queried unmounted `/data3` inside the container and saw
the 98 GiB overlay. The mounted experiment filesystem `/data3/sujie` has 270 GiB free.
The invalid launch ran zero tasks; the guard now queries the mounted path.

**Formal control correction:** the completed SC baseline/control/RVF cells are valid. The
TSO/PSO wrapper initially tried to replace `--svcomp-backend`, but the fair adapter had
already expanded that option to `--model-file=recursive-sc.cat`; TSO therefore repeated SC
and PSO-V9 rejected all rows at configuration validation. These 1,450 cells are excluded.
The corrected wrapper replaces the expanded model path and only TSO/PSO are rerun.

**Candidate decision:** revise; do not retain. The formal SC workload has zero RVF
activation and therefore zero candidate-space reduction. All correctness gates pass, and
the supported litmus cohort has a real 1,308→1,283 reduction, but that narrow coverage does
not satisfy the full-workload retain rule. Decision and next-direction rationale are in
`optimization-analysis/continuous/candidate-equivalence-quotient/decision-20260717.md`.

## V10 bounded positive conflict cores

- [x] Re-read the frozen V10 protocol and verify no prior core implementation remains.
- [x] Add the bounded exact positive-core database and deterministic lazy-edge derivation.
- [x] Connect learned cores to V9 RF/CO pre-enqueue decisions behind an explicit option.
- [x] Pass randomized replay, local/Release, sanitizer, mutation and broad differential gates.
- [x] Run the complete 725-task actual workload and decide retain/reject.

**Current status:** production RF/CO matching is opt-in through
`--cat-conflict-cores` and requires V9's structural preventive certificate. The
worker-local database admits at most 4,096 clauses of 64 sorted ground literals,
matches exact current base facts plus the proposed delta, and fails open on unsupported
provenance. Four activating PSO fixtures preserve complete executions and all
offered/queued/work counters. `CoRR2` learns six cores and records 36 hits, but none of
the four fixtures avoids a complete preventive root check; this is proof reuse, not yet
  candidate-subtree reduction. Local CTest finished with 194/195 passing, including the
  ten-iteration randomized driver; its only failure is the pre-existing missing
  `scripts/run-parallel.sh`. Server GCC 13 Release passes the expanded 185/185
  CAT/CAAT/unit/property gate, and GCC 13 ASan+UBSan passes the 14/14 focused gate.
  Mutation-oracle and broad-differential runs with V10 explicitly enabled pass, but the
  valid isolated-rewrite 725-task actual workload rejects V10: zero direct-root checks
  avoided and CPU V10/V9 1.049916 [1.025769, 1.077275]. Four V9-completed false tasks
  become timeouts. Do not use
  V10 as the next baseline; retain only the evidence and return to a real prefix/subtree
  or consistent-continuation reduction candidate.

## V11 focus-directed preventive reach

- [x] Audit RF/CO enumeration call paths and the proposed same-epoch invariant.
- [x] Reject the same-epoch invariant: unbounded final RF/CO choices use maximal
  extensibility without an `isExecutionValid()` call.
- [x] Define and property-test an analyzer-issued unassigned-focus sink certificate.
- [x] Implement and property-test production-independent exact forward/reverse lazy reach.
- [x] Implement exact forward/reverse lazy reach behind an explicit option, with
  whole-query fallback to V9 for every unsupported or ambiguous case.
- [x] Pass local/server focused correctness, sanitizer and 864 broad gates.
- [x] Record the mutation-oracle limitation: oracle mode deliberately disables candidate
  pruning, so V11 completeness evidence comes from the 864 broad candidate differential.
- [x] Run a direct complete 725-task V9/V11 experiment and retain or reject V11.

**Current decision:** reject V11. The valid same-certificate 725-task run preserves all
common-correct complete-execution and search counters, but CPU regresses by 10.85%
[6.72%, 15.49%] and 18 V9-completed tasks become resource failures. The A run is
excluded because it changed the selected certificate. V11 remains opt-in only while
the measured structural predecessor/intersection bottleneck is evaluated as V11.1;
the retained baseline and formal V9 wrapper do not enable it.

## V11.1 sparse base-intersection enumeration

- [x] Preserve the V9 certificate and V11 focus-reach semantics.
- [x] Select the lower-cardinality operand for exact base/base intersections.
- [x] Add base-relation cursor counters without changing candidate acceptance.
- [x] Pass server Release, ASan+UBSan and 864-pair broad correctness gates.
- [x] Complete the direct 725-task V9/V11.1 actual-workload comparison.

**Current decision:** reject V11.1 and pause this direction. Authoritative batch C uses
confirmed binary hash `01728a90...`, preserves common-correct complete/search counters,
but regresses CPU by 9.99% [5.68%, 14.81%] and loses 18 completed tasks. Base cursor
visits fall from 16.12B to 2.73B, so the selector activates, but it does not overcome
focus-reach synchronization/query overhead. Batch B is an old-V11 reproducibility run,
not V11.1 evidence; launch A ran zero tasks due to the missing cgroup mount.

**Paused recovery point:** the untried item from the preceding bottleneck summary is an
exact structural successor/predecessor cursor in `Relation`, not another intersection
selector. Today `nextSuccessor()` scans event IDs for ProgramOrder/Internal/External/
Location, while `nextPredecessor()` scans all sources for every structural kind except
indexed ExplicitEdges. Any resumed prototype must preserve the existing sorted-ID cursor
contract exactly, be property-tested against `contains()`, pass sanitizer and 864 broad
gates, and then go directly to the paired 725-task workload. No implementation or server
run has been started for this candidate.

## V12 exact structural relation cursors

- [x] Audit the existing structural relation membership and minimum-ID cursor contract.
- [x] Implement immutable exact-key/thread-group/Init-active indexes.
- [x] Prove successor and predecessor equality against exhaustive membership scans.
- [x] Pass server Release, ASan+UBSan and 864-pair broad correctness gates.
- [x] Run the direct paired 725-task actual workload and retain or reject V12.

**Current status:** implementation is restricted to `Relation` cursor enumeration. It
does not alter CAT membership, certificate selection, V9 pruning, or V11 focus reach.
ProgramOrder compares exact low-32-bit order indices within one real-thread group;
Internal and Location use exact group/key slices; External merges Init and active IDs
while excluding the source/target thread group. Exhaustive lower-bound cursor properties,
GCC 13 ASan+UBSan 8/8, and the 864-pair broad differential all pass with zero mismatch.
**Decision:** reject V12 and restore the previous production scans. Two unchanged full-
725 repetitions have zero status/category, complete-execution, common-correct search-
counter, or coverage differences. Per-task A/B medians give CPU 0.998008
[0.988150, 1.009684], wall 0.998314 [0.987935, 1.010209], and RSS 1.000173
[0.999945, 1.000453]. The indexed cursor therefore has no reproducible end-to-end gain.
Full evidence is in `v12-decision-20260718.md`; no intermediate performance matrix was
used.

## Core-direction evidence review (2026-07-18)

- [x] Freeze the review question: prioritize reductions in generated executions,
  equivalence classes, and decision subtrees before evaluator micro-optimizations.
- [x] Inventory retained/rejected decision artifacts and the 725-task candidate census.
- [x] Build a strict mechanism-level analysis bundle without pooling incompatible runs.
- [x] Write a ranked implementation roadmap with a separate completeness oracle for
  coarser equivalence classes.
- [x] Record explicit stop rules that prevent another cursor/cache/local-scan detour.

**Decision:** prioritize certified regional SC-RVF, followed by generation-time CAT
subtree blocking and exploration/history compression. No cursor/cache/local-scan branch
may advance unless it first changes actual candidate/class/work counts. The strict bundle
and direction report are under `optimization-analysis/core-direction-review-20260718/`.

**P0-A/P0-B entry audits:** generic regional fallback is not implementation-ready. The root
whole-program gate prevents merges before fallback; later `rvf.reset()` cannot restore
ancestor alternatives, and current `Frame`/`ThreadPool` state cannot revoke a region's
submitted descendants. V10 already matches cores before candidates reach the driver, and
its concrete-cycle clauses cannot prove a consistent prefix has no extensions. Entry
requirements are in `p0a-entry-audit.md` and `p0b-entry-audit.md`. The active core work is
sound whole-suffix SC-RVF scope expansion, beginning with property endpoint semantics.

- [x] Specify exact property-endpoint lowering and add four adapter unit tests.
- [x] Verify three real Docker tasks: abort is removed, verdicts agree, but all expose a
  deeper gate reason and RVF remains inactive.
- [ ] Enumerate post-normalization gate reasons across the actual runnable cohort.
- [ ] Admit one operation family only after a dedicated completeness oracle, then run the
  actual 725 tasks directly; do not insert an intermediate performance matrix.

**Gate census result:** server Docker completed 725/725 logs. A conservative zero-load-
annotation assume experiment changed the first-reason distribution but produced zero new
enabled quotient opportunities; it was rejected and reverted. The revealed dominant
blocker is modeled mutex lock (326 tasks). Mutex cannot be whitelisted because it lowers
to a conditional lock-CAS read/write pair and blocked attempt, while the independent SC
solver supports only plain reads/writes. Design requirements are in
`optimization-analysis/core-direction-review-20260718/mutex-entry-audit.md`.

**Mutex/RMW solver checkpoint:** successful atomic read/write pairs are now represented
and executed adjacently by the independent SC witness solver. Nine focused tests pass,
including exhaustive comparison with a separate direct linearization oracle over 21
good-write combinations; the full server Release unit suite passes 186/186. No adapter or
gate has been widened yet, so formal exploration behavior is unchanged.

**Launch/build errors:** one command accidentally used the server absolute path locally;
the corrected host launch then stopped before output creation because the result directory
is root-owned. The Docker retry completed. The first candidate build reconfigured and
failed while updating RapidCheck due a transient GitHub TLS error; configuring with the
already-populated dependency tree and `FETCHCONTENT_FULLY_DISCONNECTED=ON` then built
successfully.

**RMW adapter fixture error:** the first failed-lock adapter unit constructed an event on
thread 1 without a corresponding thread-start/thread graph and exited 139. The product
code had already passed the successful-pair test. Moving both fixture events into one
valid thread prefix fixes the test graph before any adapter result is accepted.

**Sanitizer findings:** the first sanitizer CTest pass failed three of seven tests. Both
mutex tests exposed pre-existing baseline UB in `callMutexLock/callMutexUnlock`, which
formed `&*specialDeps` from an empty `unique_ptr`; use `specialDeps.get()` instead. The
fallback fixture exceeded its Release-oriented 15-second timeout under Debug sanitizer;
its functional result was not reached, so the sanitizer timeout is raised to 60 seconds.

**Verification note:** invoking `unittest` from the repository root could not import the
sibling `rewrite_sources` module. Re-running from the adapter directory is the intended
test context and passes 4/4; this was a test-launch error, not a product failure.

**Error recorded:** the first figure build passed a color list to one Matplotlib
`errorbar` call, which rejects per-point `ecolor`. No valid figure was written; the script
now draws each interval separately.

## SV-COMP HTML task-name normalization (2026-07-15)

- [x] Confirm the cause: table-generator uses result-family-relative task names as row IDs.
- [x] Canonicalize full-table row IDs to `sv-benchmarks/c/<task>` and merge duplicates.
- [x] Regenerate HTML and verify row count, method coverage, links, and duplicate absence.

**Current status:** complete. Combined rows reduced from 2,463 path-fragmented rows to
725 unique tasks; all 5,118 non-empty method results were retained and offline links pass.

## Parallel SV-COMP scaling experiment (2026-07-15)

- [x] Audit the 56-core server, `genmc:sujie` image, binary, and shared 96-task set.
- [x] Generate 1/2/4/8-worker definitions with matching BenchExec `cpuCores` and five runs.
- [x] Pass 320 SC/TSO/PSO smoke runs; validate a 24-task CAAT oracle sample.
- [x] Complete 15,360 formal performance runs and 288 independent oracle-audit runs.
- [x] Pull XML/log archives, validate verdicts and safe execution counts, analyze metrics.
- [x] Generate the comparison HTML and deliver all artifacts locally.

**Current status:** the first LLVM 18 batch was quarantined after frontend regressions.
The formal matrix was restarted with a fresh LLVM 15.0.7 + GCC 13 Release build in the
isolated `sujie-parallel-formal` container. Raw output is isolated under
`parallel-scaling`; no server configuration is staged or committed. The full SC matrix
is complete (60 XML, 5,760 rows); the full TSO matrix is also complete (60 XML,
5,760 rows), and the full PSO matrix is complete (40 XML, 3,840 rows). Strict final
validation returned 0 for 160 XML and 15,360 rows: zero wrong verdicts, duplicates,
safe-task exploration-count mismatches, or missing safe counts. The 12-XML oracle
sample contains 288 rows and 2,444 full recomputation checks with zero verdict errors.
All XML/log archives, definitions, task snapshots, analysis documents, figures, and
12 methods-once HTML pages are local under `experiment-analysis/parallel-scaling/final`.
The HTML verifier checked 4,224 offline links; representative HTTP requests returned 200.

**Errors encountered:** local macOS rsync does not support `--info=stats1`; no remote
data was changed. Use the portable `--stats` option for checkpoint and final pulls.
The first analysis-bundle render applied a float format to a string baseline; fixed by
normalizing baseline metrics to floats. The in-app browser lacked required sandbox
metadata and local `curl` was absent; final HTTP regression used Python `urllib`.

## CAT / CAAT optimization study (2026-07-15)

- [x] Extract method-level hotspots, failure classes, timing, CPU, RSS, and scaling signals from the completed matrix.
- [x] Inspect the local concurrency-paper corpus and current primary-source literature/implementations.
- [x] Map evidence to concrete CAT and online/incremental CAAT optimizations.
- [x] Rank changes by expected benefit, semantic risk, implementation cost, and validation plan.
- [x] Deliver a reusable optimization analysis report with citations and experiment proposals.

**Current status:** complete. The reusable report is
`experiment-analysis/parallel-scaling/final/analysis/optimization-analysis-report.md`.

**Error recorded:** the first paired-ratio helper passed a generator to a helper that
required `len()`; it produced no data. The corrected read-only script materialized the
sequence and completed 10,000 task-bootstrap resamples with a fixed seed.

## Continuous CAT / CAAT optimization campaign

**Execution constraint (2026-07-15):** all container builds, tests, BenchExec runs, and
performance measurements use only `server@frp-arm.com:36722` under `/data3/sujie`.
The former lapulatos server is unavailable and must not be probed. macOS is restricted
to source editing and light compilation; server paths, configuration, logs, and results
remain outside Git commits.

### P0.7g: ordered compiled stream program

- [x] Freeze a model-independent semantic contract and pilot/formal gates.
- [x] Implement ordered union flattening and direct base/filter/identity emission with
  exact generic fallback.
- [x] Pass Release, sanitizer, mutation-oracle and broad-differential correctness gates.
- [x] Complete the balanced two-repetition pilot and audit exact mechanism counters.
- [x] Reject before formal because both PSO queue RSS observations exceed the frozen
  1.02 per-observation bound; archive evidence and restore production/test source.

**Decision:** reject. The candidate preserves all observed results and exact candidate
streams and improves affected CPU-heavy tasks, but PSO queue peak RSS rises by 6.385%
and 2.233%. The formal 2,304-cell matrix is intentionally skipped. Optimization 6.1 is
not used and is no longer considered a general/default optimization route.

**Audit command error:** macOS `head` rejects GNU-style negative line counts while
listing ZIP members. The read-only listing was rerun with `unzip -Z1`; no experiment
artifact or result was changed.

**Cleanup error:** the first generated reverse-patch helper contained a JavaScript
syntax error and made no file change. The corrected helper applied the reverse diff;
`git diff --exit-code -- genmc tests` and archived-patch `git apply --check` both pass.

### Retained optimization push

- [x] Audit the proven Optimization 01 source against the current worktree.
- [x] Exclude unproven Optimization 05b, rejected prototypes, experiment artifacts,
  server configuration, paths, and logs from the commit.
- [x] Verify the exact commit candidate in the server Docker environment (141/141 tests).
- [x] Review the staged diff, commit with a Conventional Commit message, push, and
  confirm the remote branch SHA.

**Push status:** complete at commit `550d550`; only the 14 retained source/test files
were committed. Planning notes and all experiment/server artifacts remain local.

**Push verification error:** the first Docker rebuild mounted `/data3/sujie` at
`/workspace`, but the existing CMake cache records the original `/data3/sujie` path.
CMake stopped before compilation. The retry mounts the directory at its recorded path.

### Optimization 05b: adaptive-offline history elision

- [x] Isolate history/checkpoint elision from the rejected by-value/move change.
- [x] Mirror exact before/after source trees on the server; diff contains two source/test
  files and no experiment-only production behavior.
- [x] Build and run server unit/property tests.
- [x] Run mutation and SV-COMP full-recomputation oracles.
- [x] Run balanced simultaneous before/after matrix and make a keep/reject decision.

**Decision:** reject and remove. Correctness gates passed, and profiling confirmed that
174/254 paired tasks reduced retained-history memory with no increases. However, the
fresh-build aggregate CPU ratio was 1.00250 with task-bootstrap 95% CI
[0.99779, 1.00734], so the optimization did not establish an aggregate benefit. The
model-specific split (SC/PSO slower, TSO faster) was observed post hoc and is not used
to select a TSO-only policy. Raw XML/log archives and the strict report are under
`optimization-analysis/continuous/adaptive-offline-history/`.

**Build error:** the first successful compile used generic `BUILD_TESTING=ON`, while
GenMC gates `unit_tests` with its own `BUILD_TESTS` option. `bin/genmc` built successfully;
the corrected configure adds `-DBUILD_TESTS=ON` before running the test binary.

**Optimization 05b final evidence:** a second, fresh-before/fresh-after matrix removed
the retained-binary build confound. It contains 3,456 cells, 36 XML files, and 36 log
archives, with measured combined overlap of 45--48 tasks. Common-solved verdict and safe
execution-count mismatches are both zero.

### Optimization 06: shared dependency adjacency

- [x] Identify repeated offline allocation/rebuild of an adjacency already produced by
  immutable model analysis.
- [x] Store the stable adjacency in `ModelAnalysis` and reuse it in both evaluators.
- [x] Add an analysis-level exact adjacency-order assertion; macOS compile only passed.
- [x] Run server unit/property and differential/oracle gates after the 05b fresh matrix.
- [x] Measure fresh before/after performance on the established matrix.

**Decision:** reject and remove. Server correctness gates passed: 141/141 unit/property tests,
39 mutation rows with 5,441 oracle checks, and 864 broad SC/TSO/PSO pairs with
852 comparable matches, 12 mutually unsupported cases, and zero mismatch. The complete
3,456-cell fresh-build matrix has zero verdict/safe-count mismatch, but aggregate CPU is
1.00879 [1.00416, 1.01357], TSO CPU is 1.02149 [1.01273, 1.03047], and RSS is unchanged.
The production source and test diff was restored exactly to HEAD. No macOS tests ran.

**Performance launch errors:** the first launch used copied task-set entries that still
contained `/workspace` paths; the second omitted the host cgroup mount required by
BenchExec. Both failed before executing any benchmark task and were discarded. The
authoritative retry uses rewritten `/data3/sujie` task paths plus a mounted cgroup tree;
all 3,456 cells completed successfully.

### Optimization 07: bounded offline ring worklist

- [x] Confirm the allocation scope: each complete offline evaluation creates a new
  deque and predicate-sized queued bitmap for every SCC stratum.
- [x] Implement one predicate-bounded circular FIFO and queued bitmap per evaluation.
- [x] Add recursive wraparound coverage; macOS compile-only gate passed.
- [x] Run server unit/property, mutation oracle, and broad differential gates.
- [x] Run the fresh paired performance matrix and make a keep/reject decision.

**Decision:** reject and remove. All correctness gates passed, and the 3,456-cell matrix
had zero status, verdict, or safe-count differences. Aggregate CPU was 1.00121
[0.99548, 1.00682], wall 1.00171 [0.98162, 1.02618], and RSS 0.99999
[0.99980, 1.00019]. Production/test source is exactly back to HEAD. No macOS tests ran.

### Optimization 08: direct from-read materialization

- [x] Identify generic `inverse(rf);co` construction inside the measured materializer.
- [x] Implement direct functional-rf construction and skip dense rf when not required.
- [x] Extend direct-vs-legacy mutation coverage with an fr-only adapter.
- [x] Run macOS compile-only gate; no local tests.
- [x] Run server unit/property, mutation oracle, and broad differential gates.
- [x] Run fresh paired performance matrix and make a keep/reject decision.

**Decision:** reject scalar direct construction. Correctness passed, but aggregate CPU
was 1.00835 [1.00164, 1.01510] and wall 1.01984 [1.00510, 1.03749]. The scalar
contains/insert loop loses packed row-union efficiency. Production/test source is back
to HEAD; a separate packed-row prototype may test the structural idea without this loop.

### Optimization 09: packed functional from-read

- [x] Add checked cross-relation packed row union and 130-event boundary coverage.
- [x] Reimplement functional fr construction with one packed row OR per rf edge.
- [x] Preserve fr-only legacy composition oracle across graph mutations.
- [x] Run macOS compile-only gate; no local tests.
- [x] Run server correctness/oracle gates.
- [x] Run fresh paired performance matrix and decide keep/reject.

**Decision:** reject and remove. Correctness passed, but aggregate CPU was 0.99996
[0.99557, 1.00444] and PSO was 1.00590 [0.99503, 1.01740]. SC improved to
0.98671 [0.97922, 0.99420], but this model split is post hoc and PSO exceeds the
0.5% point-regression tolerance. Production/test source is exactly back to HEAD.

### Optimization 10: certified host consistency

- [x] Freeze the fail-closed semantic boundary and performance retention rule.
- [x] Implement a generated-checker fast path for exact bundled recursive SC/TSO only.
- [x] Add activation and near-neighbor fallback coverage; compile locally without tests.
- [x] Run server unit/property, mutation-oracle, and broad differential gates.
- [x] Stop after one paired SC repetition, reject the method scope, and revert exactly.

**Decision:** reject by research-method scope. The prototype passed 142/142 tests,
5,441 mutation-oracle checks, and 864 broad pairs with zero mismatch, but it bypasses
CAT/CAAT consistency through GenMC's built-in SC/TSO checker. It therefore does not
optimize the generic evaluator or incremental/online integration. The formal matrix was
stopped after one paired SC repetition and is quarantined from performance claims;
production/test source was restored exactly to HEAD. Protocol:
`optimization-analysis/continuous/certified-host-consistency/experiment-protocol.md`.

### Optimization 11: generic closure-check lowering

- [x] Map expected time/space changes across normalization, fixed-point evaluation,
  online insertion, checkpoint/undo, explanation, and oracle paths.
- [x] Prove and encode a model-name-independent structural matcher for any admitted
  linear recursive relation, retaining its complete value for downstream predicates
  and all check kinds.
- [x] Canonicalize the exact linear least fixed point to the generic packed transitive
  closure operator inside CAT/CAAT, without invoking a built-in GenMC checker or
  changing candidate enumeration.
- [x] Preserve full predicate values and reasons whenever explanation/oracle requires
  them; add near-neighbor cases that must fail closed.
- [x] Complete the fresh paired matrix after passing server unit/property,
  mutation-oracle, and broad differential gates.

**Method constraint:** built-in generated checkers may inform algorithms, but CAT/CAAT
must execute the optimized consistency procedure itself. No delegation to SCChecker or
TSOChecker, no model-name dispatch, and no fixed-model bypass is admissible.

**Decision: retain.** The first server gate exposed missing provenance for a
non-reflexive closure diagonal pair: consistency values were correct, but two explanation
tests returned an empty reason. `Reasoner` now requires `(x,x)` in `R+` to replay a
non-empty seed cycle. The corrected prototype passes 144/144 tests, 39 mutation rows
with 5,441 full-oracle comparisons, and 864 broad pairs (852 matches, 12 mutually
unsupported, zero mismatch). The 3,456-cell fresh matrix has zero common-solved verdict
or safe-count mismatch and 15 additional correct cells. Aggregate CPU is 0.9901
[0.9832, 0.9959], wall 0.9896 [0.9824, 0.9956], and RSS 1.00002
[0.99985, 1.00019]. All model CPU point estimates improve. A separate paired profile
holds 842,811 queries and 689,269 offline evaluations exactly constant while reducing
offline time to 0.624 and snapshot-equivalent bytes to 0.976. Full report:
`optimization-analysis/continuous/linear-recursion-closure/report.md`.

**Commit:** local commit `b84309a` contains only the four verified production/test
files. Optimization 12 work, planning notes, server configuration, XML/log archives,
and analysis artifacts were excluded from that commit.

### Optimization 12: acyclic transitive-closure slicing

- [x] Prove the finite-relation equivalence `acyclic(R+) <=> acyclic(R)` and map
  normalization, analysis, evaluator, incremental, checkpoint, explanation, and oracle
  cost changes before implementation.
- [x] Freeze a fail-closed consumer boundary: only a generic transitive-closure value
  with no predicate consumers and exclusively `acyclic`/`irreflexive` check consumers
  may be sliced.
- [x] Redirect qualifying checks to the seed and remove the dead closure predicate with
  deterministic ID remapping; never use a built-in GenMC checker.
- [x] Add independent random-relation equivalence, activation, near-neighbor,
  certificate, and explanation tests.
- [x] Run the fresh 3,456-cell paired matrix after completing all server correctness
  gates, then make a
  retain/reject decision.

**Pre-implementation artifacts:**
`optimization-analysis/continuous/acyclic-closure-slicing/{experiment-protocol,
cost-model}.md`. Server Release tests are 147/147; mutation stress is 39 rows and
5,441 full-oracle checks; the broad differential is 852 matches, 12 mutually
unsupported, and zero mismatch across 864 pairs. Expected savings are one complete
closure evaluation/value per query, with unchanged candidates and complete seed-cycle
explanations. Downstream, `empty`, mixed observable, and reflexive-closure consumers
fail closed; terminal `irreflexive(R+)` is safely canonicalized to `acyclic(R)`.

**Decision: reject and remove.** The formal matrix has 3,456/3,456 rows, zero verdict
or safe-execution-count mismatch, and nine additional correct after cells. The paired
mechanism profile holds 1,261,169 queries and 1,054,517 offline evaluations constant
while reducing offline time to 0.905 and snapshot-equivalent bytes to 0.978. However,
the frozen 82-task cross-model aggregate CPU ratio is 0.99584 with 95% CI
[0.98957, 1.00131], so its upper bound is not below one. The favorable 261 model-task
sensitivity result is not substituted post hoc. Full report and pulled evidence:
`optimization-analysis/continuous/acyclic-closure-slicing/`.

### Large-program scalability research

- [x] Use the 725-task GenMC/Deagle experiment and Optimization 12 to separate
  per-candidate checker cost from execution/equivalence-class explosion.
- [x] Read the relevant local papers from the Cai Yan, Fei He, and Liangze Yin teams and
  verify current publication metadata against primary author/DOI pages.
- [x] Map EOG refinement, ordering-theory propagation, interference prioritization, and
  dependence reduction onto exact GenMC/CAT/CAAT stages.
- [x] Identify concrete current source insertion points and soundness/fallback boundaries.
- [x] Produce a research question card, evidence-labeled literature review, and staged
  optimization roadmap.

**Recommendation:** prioritize exact rejection-kernel learning and prefix subsumption.
The first implementation is instrumentation-only: canonicalize CAAT explanations,
measure duplicate/subsumed kernels and earliest possible firing, and do no pruning.
Only if the census shows at least 10% reusable conflicts should the next prototype filter
RF/CO alternatives and revisits. Dependency slicing begins as a search-order heuristic;
safe-result pruning requires a later CEGAR proof. Artifacts:
`optimization-analysis/large-program-scalability/`.

### P0: GenMC / Deagle fair-coverage repair

- [x] Establish the full-census failure baseline: 725 tasks, with GenMC reporting
  320 `ABORTED`, 159 compilation failures, 78 unsupported externals, 47 timeouts,
  2 OOMs, and 5 segmentation faults.
- [ ] Classify every compile/abort/external failure by directory and root-cause signature.
- [ ] Implement only semantics-preserving adapter/runtime/header fixes in an isolated
  server experiment tree; do not turn unsupported behavior into assumed success.
- [ ] Run a short 725-task census after each repair batch and retain fixes only with
  zero new wrong verdicts on previously correct tasks.
- [ ] Re-run the comparable GenMC/Deagle SC ILP32 table with identical limits and report
  coverage separately from common-solved time/RSS.

**User priority update:** continue reducing `ABORTED`, compilation, and unsupported-
external rows first, and give `wrong` rows explicit per-task diagnosis. Only after the
remaining errors are almost entirely TIMEOUT/OOM will the full 725-task corpus be rerun
with a 60-second limit. The 10-second and 60-second results remain separate.

**Priority change:** this compatibility repair is P0 because the current 96-task paired
performance subset hides 557 front-end/adapter failures. Optimization 11 remains an
uncommitted, compile-only prototype until the fair-coverage census is repaired.

**Fair-coverage probe error:** the first rewritten-source smoke completed its first
GenMC invocation, then the probe attempted to express the rewritten path relative to
the original benchmark root and raised `ValueError`. No benchmark result from that run
is retained. The probe now records the original relative source path before rewriting.
The retry also showed why GenMC's condvar declarations are commented out: exposing the
existing runtime calls compiled five candidates, but `40_barrier_vf` changed an expected
FALSE task into TRUE with zero completed executions. Condvar and TLS compatibility are
therefore excluded from the sound lane; only the independently supported rwlock surface
and exact abort-as-assume rewrites proceed.

**BenchExec topology error:** a single `-N48 --allowedCores 0-51` census was rejected
before any task ran because the host exposes asymmetric CPU/memory regions. The retry
uses the previously validated split layout: two simultaneous 24-worker shards on
cores `0-23` and `28-51`, for 48 task workers in total.

**Classification import error:** the first post-census classifier used the older probe
copy of `rewrite_sources.py`, which predated include-closure support. It failed before
reading any task. The same current module is now synchronized into both the probe and
BenchExec tool directories before rerunning classification.

- [x] Optimization 01: split PSO backend certification from candidate pruning; enable
  exact-model adaptive offline evaluation.
- [x] Optimization 01 local gate: 141 unit/property tests and 864 broad differential
  pairs with zero mismatch.
- [x] Optimization 01 server gate: 96 tasks × five repetitions before/after; raw XML and
  logs pulled back; zero wrong verdicts and zero safe exploration-count mismatches.
- [x] Optimization 01 decision: keep (wall ratio 0.937, task-bootstrap 95% CI
  [0.901, 0.968], CPU ratio 0.938, RSS ratio 1.000, five additional solved run cells).
- [x] Optimization 02a: add opt-in transition/dirty-density instrumentation.
- [x] Optimization 02b: prototype an `ExecutionGraph` mutation journal covering
  append, cut/retraction, rf replacement, and co placement/reorder, with legacy
  materialization as the oracle.
- [x] Optimization 02 local differential and server-wide evaluation; reject and remove
  the semantic snapshot cache (0.96% wall gain but 0.66% RSS increase and no median gain).
- [x] Optimization 03: incremental violation/cycle frontier; rejected and removed after
  oracle-correct prototypes regressed wall time by 11.0% and then 4.5%.
- [x] Optimization 04: measured online/offline cost selector; rejected and removed after
  simultaneous 48-worker held-out runs showed +6.3% wall and +3.5% CPU.
- [x] Optimization 05: profile the current production CAAT path over the full 96-task
  SC/TSO/PSO sample with one 48-worker run set per model, then select the next P1
  optimization from measured copy/history/rebuild/operator costs.
- [ ] Evaluate remaining P1/P2 candidates only after measurements identify their scope.

**Current status:** Optimization 01 is retained and documented at
`optimization-analysis/continuous/pso-adaptive-offline/report.md`. Optimization 02a shows
that append-only handling is insufficient: changed-query monotone/retracting counts on
fcombiner are SC 12/22, TSO 1/2, and PSO 1/6. Optimization 02b must cover retractions and
rf/co replacement before performance evaluation.

**Environment error recorded:** the first server BenchExec probe did not reproduce the
old binary's compile-time runtime-header mount and reported unsupported `pthread_create`
for 460/480 cells. It is quarantined and excluded. Re-running with the correct container
mount restored 395/480 baseline solved cells and produced the authoritative matrix.

**Optimization 02b result:** the exact semantic snapshot cache passed 141/141 tests,
864/864 local differential cells, and a 1,728-cell server before/after matrix with zero
common-solved verdict or safe execution-count mismatch. Its overall wall ratio was
0.9904, but median ratio was 1.0000 and RSS ratio was 1.0066; the prototype was removed.
Raw XML/logs, parsed TSV, bootstrap analysis, HTML, and the negative-result report are
under `optimization-analysis/continuous/semantic-snapshot-cache/`. Subsequent broad
screening defaults to 32 BenchExec task workers; 48 is allowed when host load is stable.

**Optimization 03 design evaluated:** insertion updates retained any still-valid
prior witness and inspect only newly added facts for `empty`/`irreflexive`; a previously
acyclic relation searches for a return path only from newly added edges. Replacement,
rebuild, and initialization retain the full checker. The offline oracle will continue
to compare every predicate and violated check, while accepting any witness that is
validated against the exact offline value because witnesses are not semantically unique.

**Optimization 03 result:** two prototypes passed the server correctness gates, including
39,879 full Phase 2 oracle comparisons with zero mismatch. The first 1,728-cell formal
matrix regressed wall/CPU by 11.0%/12.1%; a dense-frontier fallback still regressed wall
by 4.5% in screening. Both were removed and the reverted server suite passed 141/141.
Evidence and the negative-result report are under
`optimization-analysis/continuous/violation-frontier/`. Optimization 04 will use 48
configured BenchExec workers and record XML-derived peak overlap in addition to `-N`.

**Optimization 04 result:** 142/142 prototype tests and 39,879 oracle comparisons passed,
but the controlled 48-worker held-out matrix (48 tasks x SC/TSO/PSO x five reps x two
variants = 1,440 cells) regressed wall/CPU by 6.3%/3.5%. Static and selector queues ran
simultaneously on disjoint `0-23`/`28-51` core sets, swapped each repetition; XML measured
45--48 concurrent tasks. The earlier sequential apparent 8.8% gain was load/order bias.
Selector and experiment CLI were removed; server tests passed 141/141. Artifacts are
under `optimization-analysis/continuous/cost-selector/`.

**Optimization 05 in progress:** `caat-p1-profile-sujie` runs the reverted production
binary in `genmc15noble:sujie`; each model contains one 96-task run set with `-N48`,
one CPU and 4 GB per task, and `--nthreads=1`. SC completed 96 rows with 88 correct,
8 resource-limited unknown, and zero incorrect verdicts. Remote output is isolated at
`/data3/sujie/experiments/caat-optimization/p1-profile/results/`.

**Optimization 05 environment errors:** querying the container-built binary directly
on the host failed because the host lacks the image's glibc/libstdc++ versions; the
same binary and BenchExec 3.25 work inside `genmc15noble:sujie`. The first definition
upload also failed because `rsync` does not create a missing remote parent directory;
no experiment had started, the exact `p1-profile/{definitions,results}` directories
were created, and the retry succeeded. A later four-file source upload initially
flattened paths into the experimental `source/` root; those exact four files were
removed and resent with `rsync -R`, then verified in place. The first oracle container
exited 126 because copied test scripts were mode 0644; no oracle task had started, and
the retry invokes the same scripts explicitly with `bash`.

**Optimization 05 final:** 288 rows and 254 terminal profiles were collected. The
profile informed controlled Optimizations 05b--09. Every prototype passed its
correctness gates, but 05b--09 all failed their pre-registered performance retention
rules and were removed. Optimization 01 remains the only retained and pushed change.

## SV-COMP C.Concurrency fair-coverage repair (2026-07-16)

- [x] Run the 725-task 10-second v6 census and pull complete XML/log archives.
- [x] Reclassify five invalid-memory failures instead of reporting false reachability;
  v6 has 725 unique rows, 83 correct, 642 error, and zero wrong.
- [x] Reject the in-program thread-based `nondet_bool` prototype after server probes
  exposed compilation conflicts and a GenMC thread-create internal failure.
- [x] Freeze the accepted completeness scope: deterministic per-thread pseudo-input
  sequence with interpreter seed 1995, exhaustive only over concurrent executions.
- [x] Map bool/uint/long/ulong/longlong/char calls deterministically onto GenMC's
  native fixed-seed `__VERIFIER_nondet_int()` stream without adding program events.
- [x] Pass mixed true/false and mixed-type server probes before expanding coverage.
- [x] Re-run the 10-second full census after safe adapters stabilize.
- [x] Start a 60-second full repaired-corpus run only when remaining failures are
  overwhelmingly TIMEOUT/OOM rather than compile/ABORT/unsupported/wrong.

**Current status:** v9 is the repaired 10-second baseline and v10 is the completed
60-second fixed-seed census. V10 has 725 unique tasks and no duplicate rows: 408
benchmark-consistent, 31 expected-false tasks reported true under the fixed input, and
286 resource/unsupported errors. The fixed-seed lane does not add helper threads or
memory events. A `true` result means safe for deterministic seed-1995 per-thread input,
not for every possible SV-COMP nondeterministic input.

**Errors recorded:** the first v6 launch missed the server entry script and the second
missed the cgroup mount; both stopped before executing tasks. The formal retry used a
new output directory and completed both 24-worker shards. The first exact-bool prototype
compiled or ran 10 probes, but three manual-pthread sources conflicted with injected
GenMC typedefs and one task hit `GenMCDriver.cpp:1156` during helper-thread creation.
That prototype is rejected and will not enter the formal census.

## SV-COMP per-tool Python integration handbook (2026-07-16)

- [x] Clarify the deliverable: per-tool BenchExec integration and execution scripts,
  not a static corpus-rewrite guide.
- [x] Check the current official SV-COMP 2026 and BenchExec BaseTool2 boundaries.
- [x] Extract the reusable command-line, preprocessing, result-classification, metrics,
  and evidence patterns from the GenMC, CAT/CAAT, TruSt-family, Deagle, and CBMC work.
- [x] Write competition-ready ToolInfo and same-run wrapper templates with fail-closed
  behavior and per-tool extension points.
- [x] Review the handbook against the official container lifecycle, task metadata,
  witness, result, and anti-fingerprinting requirements.

**Current status:** handbook and two templates are complete under
`optimization-analysis/svcomp2026/`. Official BenchExec runs
`cmdline()` in a separate tool-info container, so source files created there are not
visible to the measured run. The competition-ready design therefore keeps
`benchexec/tools/<tool>.py` side-effect free and invokes an archive-bundled Python
runner that performs preprocessing and verification inside the same measured process
tree. The current research-only `genmc_svcomp_fair.py` writes from `cmdline()` and must
not be submitted unchanged.

**60-second census launch error:** the first container invocation tried to execute a
non-executable script and failed before producing XML. The failed log was retained and
the same two-shard job was relaunched explicitly through `bash`; no task parameters or
resource limits changed.

**Template verification:** both Python files pass `py_compile` locally and in a fresh
single-core server Docker container; the runner's `--version` entrypoint exits 0. A
root-owned `.pyc` prevented host-side removal of the temporary validation directory;
an ephemeral `--rm` container removed that exact directory, and the completed census
container was also removed.

## Adapted 725-task GenMC vs Deagle comparison (2026-07-16)

- [x] Confirm the existing task identities and old run limits.
- [x] Lock the fair unit: the same 725 YAML tasks, each through its own legal adapter,
  with unreach-call, ILP32, one core, 4 GB, and 60 seconds.
- [x] Run CAT, CAAT, and Deagle 4.1.0 at 60 seconds alongside the completed GenMC run.
- [x] Pull XML/log archives and validate 725 unique rows, task overlap, and wrong-result
  directions.
- [x] Produce a strict paired analysis bundle and one-method-each comparison HTML.

**Baseline audit:** the existing Deagle run covers exactly the same 725 tasks but used
10 seconds. It has 596 correct, 2 wrong, 20 unknown, and 107 errors; the GenMC fixed-seed
60-second run has 408 benchmark-consistent, 31 conditional-input misses, and 286 errors.
This establishes a large provisional coverage gap but is not yet a fair time comparison.

**Active run topology:** CAT uses cores 0--15 with 16 workers, CAAT uses cores 16--27
with 12 workers, and Deagle uses cores 36--51 with 16 workers: 44 simultaneous tasks,
one core and 4 GB per task. The first CAAT launch selected cores 18--33 across the
28-core NUMA boundary; BenchExec rejected the asymmetric topology before executing a
task. That launch directory is retained separately and the corrected CAAT run is live.

**Final comparison:** all four methods contain exactly 725 normalized task rows. Correct
counts are GenMC 408, CAT 350, CAAT 388, and Deagle 615; TIMEOUT+OOM counts are 260,
320, 281, and 26 respectively. The local table-generator page is
`optimization-analysis/svcomp2026/fair-coverage/comparison-60s/final/html/
adapted-725-genmc-cat-caat-deagle-60s.table.html`; 2,900 logs, 725 task YAML files, and
3,625 offline links were verified with zero missing targets.

## Optimization 12 and large-program scalability (2026-07-16)

- [x] Implement and validate cycle-check transitive-closure slicing without invoking a
  built-in memory-model checker.
- [x] Run correctness, broad differential, controlled six-repetition, and mechanism
  profile gates.
- [x] Reject and remove Optimization 12 because the frozen aggregate CPU confidence
  interval crosses one, despite lower offline work and snapshot-equivalent state.
- [x] Commit the previously retained Optimization 11 independently as `b84309a`.
- [x] Audit the local paper corpus and primary publication records for EOG/refinement,
  ordering-theory propagation, Deagle, and dependence-guided prediction.
- [x] Write the large-program research card, literature review, and source-level roadmap.
- [x] Separate and validate the check-kind certificate correctness fix exposed by the
  Optimization 12 audit: 144/144 unit tests, 5,441 oracle comparisons, and 864 broad
  pairs with zero mismatch.

**Current status:** Optimization 12 production/test changes are absent. The next
measurement-only implementation is a rejection-kernel census; pruning should begin only
if at least 10% of rejected candidates share a replayable kernel or a smaller kernel
subsumes later conflicts. The certificate check-kind fix remains an independent,
verified worktree change pending its own commit decision.

**Recorded error:** the first isolated certificate test configuration referenced a
RapidCheck source directory from a cleaned experiment and exited before compilation.
The retry used the existing server dependency source, passed every gate, and its exact
exited container was removed.

### P0.1: rejection-kernel census

- [x] Audit the exact CAAT rejection/explanation path and freeze a measurement-only
  protocol with replay, correctness, and retention gates.
- [x] Implement opt-in kernel census counters without changing consistency results or
  exploration decisions.
- [x] Pass 145/145 server tests, 5,441 oracle checks, and 864 broad differential pairs
  with zero mismatch.
- [x] Complete the 60-task historical TIMEOUT/OOM sample for SC, TSO, and PSO.
- [x] Complete the 96-task SC/TSO/PSO three-repetition paired overhead matrix.
- [x] Pull XML/log archives, generate the strict analysis bundle, and decide whether
  to remove the census or transform it into P0.2.
- [x] Remove the eager census implementation and implement the scoped P0.2 raw positive
  kernel fast-rejection prototype behind an experiment flag.
- [x] Pass P0.2 Release unit/property tests, hit-validating mutation oracle, and broad
  differential correctness gates.
- [x] Complete the balanced four-repetition 96-task performance matrix and reject/remove
  P0.2 under its frozen gate.

**Current status:** P0.1 is complete. The 1,728-cell formal matrix has zero safe
execution-count mismatch; all 27 status differences are measurement-overhead resource
perturbations. Aggregate CPU ratio is 1.20162 [1.12795, 1.29234]; PSO is 1.67688
[1.38861, 2.08157]. SC/TSO large tasks have only 0.0134%/0.0140% simulated hits per
query; PSO has 10,862 hits over 21,463 observed queries (50.6%) on 17/60 tasks. Decision:
remove the eager explanation/minimization census and prototype lazy raw-positive kernel
lookup only for the uncertified generic candidate path. Strict bundle and full pulled
XML/log archives are under
`optimization-analysis/continuous/rejection-kernel-census/`.

**P0.2 final evidence:** `--cat-kernel-cache` was automatically inert for certified
SC/TSO candidate profiles. On the uncertified generic path it materializes a stable
snapshot, rejects only a complete previously proved positive Reasoner conjunction, and
runs the original evaluator on every miss. It performs no deletion minimization and
stores at most 4,096 worker-local kernels. Server gates passed 145/145 tests, 39 mutation
rows with all 5,441 full-oracle checks (cache hits are explicitly recomputed under
`--cat-oracle`), and 864 broad pairs with 852 matches, 12 mutual unsupported, zero
mismatch. Broad offline evaluations fell from 869,028 to 643,107, and PSO cache logs had
6,568,688 hits in 7,368,584 queries. Nevertheless, the balanced 2,304-cell matrix failed
the retention gate: aggregate CPU 1.04343 [1.00788, 1.09055], PSO CPU 1.13116
[1.01913, 1.29104], and aggregate RSS 1.01808 [1.00038, 1.04728]. It traded 12 fewer PSO
TIMEOUT cells for eight additional OOM cells and only four additional correct FALSE
cells. The entire production/test cache diff was removed; only analysis/evidence remains.
All exact experiment containers and temporary server launchers were removed.

**Launch errors:** a long inline SSH command failed local zsh parsing before a container
was created. Three subsequent formal launches also stopped before benchmark execution:
BenchExec nested-overlay setup was unsupported, the first retry lacked the cgroup mount,
and a Docker cpuset exposed asymmetric NUMA topology. Each empty result tree and exited
own container was removed. The authoritative run uses the outer Docker for isolation,
BenchExec `--no-container`, an explicit host cgroup mount, and disjoint
`--allowedCores 0-23`/`28-51` groups with no outer cpuset restriction.

### P0.3: preventive-order opportunity census

- [x] Recover the exact OOPSLA 2022 and FM 2026 preventive conditions and their
  soundness boundary.
- [x] Map the conditions onto GenMC's explicit `rf`, total `co`, derived `fr`, RMW
  representation, and exact recursive PSO order.
- [x] Implement an opt-in measurement-only counterfactual census; it must not remove
  candidates or alter consistency results.
- [x] Pass server unit/property tests and full offline-oracle differential checks.
- [x] Measure the 96-task sample and the historical 60-task PSO TIMEOUT/OOM cohort.
- [x] Retain a pruning implementation only if direct-order reversal catches at least
  10% of rejected generic candidates and projected saved evaluator work exceeds census
  lookup work by at least 2x.

**Current decision boundary:** start with the exact bundled recursive PSO fingerprint.
SC/TSO already use independently certified candidate pruning, and the cited FM 2026
extension is SC-oriented. The census compares the rejected graph with a counterfactual
base in which only the current `rf` or current write's incident `co` edges are removed;
it recomputes `fr`, reruns the same CAAT fixed point, and counts a preventive opportunity
only when a newly introduced direct `order` edge reverses counterfactual `reach`.
RMW-Broadcast and RF-Join are recorded separately only where GenMC exposes the required
RMW/atomic-block structure; no missing block abstraction is guessed.

**Census result:** Release tests passed 144/144, the mutation oracle passed 39 rows and
5,441 checks, and the 864-pair broad run had 852 matches, 12 mutual unsupported, and
zero mismatch. On 79 normally terminated formal PSO tasks, full offline replay confirmed
all 12,624 production rejections with zero mismatch; 12,620 (99.97%) were direct-order
reversals. The fair historical hard cohort yielded partial checkpoints for six resource
tasks: 179,886 of 183,296 rejections (98.14%) matched, while 3,401 had an already
inconsistent counterfactual prefix. Among the 179,895 attributable candidates, only
nine lacked a direct reversal. The opportunity gate passes. The next prototype must
remove the expensive double replay and instead synchronize the no-choice prefix once,
then filter all offered RF/CO choices with stable-ID reachability lookups.

**Recorded launch errors:** the first formal build embedded `/workspace/...` as its
source path but the BenchExec container mounted only `/data3/sujie`; GenMC therefore
missed its pthread interception headers and 94/96 tasks failed before checking. That
result is preserved under `formal-96-invalid-source-path` and excluded. A fair-cohort
launcher first used a nonexistent `tools` module path, then an upload first targeted a
missing directory; both failed before benchmark execution and were preserved/logged.

### P0.3 pruning implementation and validation

- [x] Implement the exact recursive-PSO preventive filter behind
  `--cat-preventive-pruning`, without consulting a built-in checker verdict.
- [x] Fail closed for every model other than the fingerprinted recursive PSO model.
- [x] Pass Release unit tests and mutation-oracle validation.
- [x] Pass the 864-pair broad semantic and safe-execution-count differential.
- [x] Complete the balanced four-repetition 96-task PSO performance matrix.
- [x] Run the historical PSO TIMEOUT/OOM cohort only if the formal matrix does not show
  a clear mechanism-level regression.
- [x] Retain or remove the prototype under the frozen correctness and CPU/RSS gates.

**Current pruning evidence:** Release tests pass 144/144. Configuration smoke tests
accept the exact recursive PSO model and reject recursive SC and non-recursive PSO with
status 17. Mutation stress passes 39 rows and 6,440 offline-oracle checks. The broad
differential discovers 288 programs and reports 852 comparable matches, 12 mutual
unsupported pairs, and zero mismatch across 864 SC/TSO/PSO pairs. The balanced PSO
performance run is active on the server as `preventive-performance-sujie`; each
repetition runs 24 baseline and 24 pruning tasks concurrently on disjoint NUMA core
ranges and swaps the ranges on even repetitions.

**Smoke-test source error:** `tests/cooking/m5.c` includes a test-relative
`../stdatomic.h` that is not present in the copied experiment source tree, so the first
positive CLI smoke stopped in Clang before checking. The retry used the copied SB
litmus test, exited 0, explored four executions, and emitted preventive counters. This
is an experiment-source packaging issue, not a model-checking failure.

**Final P0.3 status:** the single-candidate gate removed one repeatable 4 GB regression;
the post-fix gates pass 145/145 tests, 39 mutation rows with 5,658 oracle checks, and the
864-pair broad differential with zero mismatch. The corrected 768-cell matrix has five
repeatable TIMEOUT-to-terminal gains per repetition, no OOM increase, zero safe
execution-count mismatch, CPU ratio 0.95369 [0.87527, 1.04274], and RSS ratio 1.00004
[0.99958, 1.00042]. The universal CPU gate is not met, but the implementation is retained
behind its opt-in exact-PSO flag as a coverage optimization. The adapted hard cohort
does not add terminal results: in both repetitions the same three OOMs become TIMEOUTs.
The full decision is in
`optimization-analysis/continuous/preventive-census/analysis-pruning-v2/decision.md`.

**Invalid fair-cohort run:** the first paired launcher let baseline and pruning write the
same rewrite root concurrently. Two task outcomes swapped between compilation error and
OOM, proving an experiment race. The active container was stopped, and the directory is
preserved on the server as `pruning-fair-timeout-oom-60-invalid-shared-rewrite`. The
authoritative retry used a distinct rewrite root for each variant and repetition.

**Final-sync error:** one multi-source rsync placed `CATChecker.cpp`, `CATChecker.hpp`,
and `ConfigTest.cpp` in the server experiment source root. No build had started. Those
three exact stray copies were removed, each file was uploaded to its proper source/test
subdirectory, and the final server rebuild plus 145/145 tests passed.

**Committed and pushed:** the independent certificate correction is `8a3a41e`, and the
opt-in preventive-pruning implementation is `05d7ff8`. Together with retained
Optimization 11 `b84309a`, all three commits are on `origin/genmc-caat`; local and remote
HEAD both resolve to `05d7ff8828f85ba8d4784a6254aee42881d2026e`. No experiment XML,
launcher, server path, pulled log/result archive, `task_plan.md`, or `notes.md` was
included in those commits.

### P0.4: preventive-history retention

- [x] Freeze a measurement-only provenance census and correctness/opportunity gates.
- [x] Tag preventive synchronizations and measure retained-checkpoint hits, discards,
  and attributable history bytes without changing evaluator behavior.
- [x] Pass Release, mutation-oracle, and 864-pair broad differential gates.
- [x] Measure the 96-task recursive-PSO sample and decide which retention class, if any,
  is safe and profitable to suppress.
- [x] Prototype the selected class and run four paired repetitions plus the adapted
  hard cohort before retaining or removing it.

**Current status:** source audit shows that a preventive prefix checkpoint can be the
exact parent needed to rollback after exploring an RF/CO child. Blindly disabling all
preventive retention would preserve semantic correctness but may replace cheap sibling
rollback with a full rebuild. The first implementation is therefore provenance-only;
the protocol is
`optimization-analysis/continuous/preventive-history/experiment-protocol.md`.

**Build error:** the first server build stopped in `GraphSynchronizer.cpp` because GCC
13's `std::count_if` implementation does not invoke a pointer-to-data-member predicate.
No tests or tasks started. The census now uses an explicit lambda; the failed build log
is retained under the experiment root.

**Test expectation error:** the new provenance test expected `Rollback`, but stable IDs
retain the removed event in the universe, so the exact transition is `RollbackInsert`.
All 145 pre-existing tests passed and the preventive checkpoint was restored. The test
now asserts the established transition semantics.

**Census scope correction:** the first 96-task census was stopped before completion
because aggregate provenance did not distinguish adaptive-offline retentions from
online insertion parents. Its partial directory is preserved as
`formal-96-incomplete-missing-retention-class`. The authoritative rerun adds separate
adaptive retention and unique-use counters.

**Authoritative census:** 87 completed CAT-stat logs contain 307,832 preventive
retentions and zero uniquely used checkpoints; all 307,753 adaptive-offline preventive
retentions are also unused. The exact lifetime identity holds: 307,832 retentions minus
zero uses equals 307,763 unused discards plus 69 unused live entries. Summed per-task
maximum preventive history is 73,732,376/92,023,544 base bytes (80.12%). The opportunity
gate passes, so the next build adds an opt-in no-retention purpose while retaining the
same evaluator state and all ordinary exploration checkpoints.

**Analysis error:** the first no-retention performance analysis attempted a geometric
mean for internal history ratios that may legitimately become exactly zero. The raw
768-cell matrix is unaffected. CPU/wall/RSS retain the frozen geometric-mean bootstrap;
mechanism counters now use total ratios, task medians, and explicit zero counts.

**Final P0.4 decision:** reject and remove the production/test prototype. Correctness
passed 148/148 Release tests, 39 mutation rows with 5,658 oracle checks, and the 864-pair
broad differential with zero mismatch. The four-repetition 768-cell matrix has zero
status or safe-execution-count differences and suppresses 1,231,328 retentions. Internal
maximum history and snapshot-equivalent totals fall to 20.61%, history-search time to
16.58%, and checkpoint time to 41.10%, but process RSS is 0.99992
[0.99949, 1.00030] and CPU is 0.99528 [0.98831, 1.00201]. The 240-cell adapted hard
cohort adds zero repeatable resource-to-terminal result. Because neither frozen retention
condition passes, the experimental CLI, provenance instrumentation, tests, and production
behavior were restored exactly to `05d7ff8`; only the protocol, XML/log evidence, and
analysis remain local.

### P0.5: structural-relation and predicate storage census

- [x] Reconcile P0.1--P0.4 failures with the 725-task GenMC/Deagle coverage gap.
- [x] Inspect Yin/He/Cai papers and Deagle's exact EOG/order solver source structures.
- [x] Map the literature mechanisms to current CAT/CAAT source and measured counters.
- [x] Freeze an implementation order and correctness/performance contract in
  `optimization-analysis/large-program-scalability/implementation-roadmap-v2.md`.
- [x] Add opt-in per-primitive/per-predicate byte, pair, growth, and time diagnostics.
- [x] Run the 96-task and large-event/historical resource census before changing storage.

**Current decision:** P0.5 measurement is next. On one completed 8,033-event task, a
single dense relation costs 8,097,264 bytes, current base values peak at 72,879,408
bytes, checkpoint-equivalent evaluator state at 587,519,208 bytes, worklist time at
21.841 seconds, and RSS at 1,060,139,008 bytes. Current `StableGraphAdapter` also builds
`po/loc/int/ext` by an active-event pair loop. This is materially stronger evidence for
structural relation views and a CAT-certified sparse EOG cycle plan than for another
checkpoint/cache optimization. No production storage change is authorized until the
per-relation census passes its opportunity gate.

**Test error:** the first P0.5 server run passed 144/145 tests. The added assertion
incorrectly required the last normalized predicate to have a nonzero evaluation count;
that auxiliary predicate is not necessarily scheduled. The production implementation
was unchanged, and the test now requires at least one recorded predicate attempt while
retaining exact vector-size and storage checks.

**Validation launch error:** SC/TSO/PSO profile smoke runs completed, but the first
combined correctness container exited 126 before mutation stress because the copied
test scripts lacked executable mode. No mutation or broad result was produced. The
retry invokes the same scripts explicitly with `bash`; the binary and arguments are
unchanged.

**Census schema correction:** the first 288-cell formal run completed, but base-value
records did not explicitly identify set versus relation. The raw run is retained as
`formal-96-schema-v1` and excluded from sparse-storage projection. A measurement-only
type field was added before the authoritative rerun; no result or exploration decision
changed.

**Resource-observability correction:** destructor-only profiles are absent when
BenchExec kills a TIMEOUT or OOM process. The authoritative census therefore emits a
snapshot at power-of-two consistency-query counts under `--cat-stats`. This adds only
O(log Q) diagnostic records, never changes a verdict or graph transition, and is not
used as a performance baseline.

**Smoke launch error:** one checkpoint smoke command ran the Noble/GCC 13 binary on
the older host userspace and failed before GenMC with missing GLIBC 2.38/GLIBCXX
symbols. It is excluded. The retry runs inside `genmc15noble:sujie`, matching every
build and formal experiment.

**Timing upload error:** the first rsync of per-predicate timing instrumentation was
closed by the FRP endpoint before a build container was created. The retry verifies the
uploaded `evaluation-ns` source marker before compiling; no partial build is evidence.

**Final P0.5 result:** all correctness gates pass: 145/145 Release tests, 39 mutation
rows with 12,784 full-oracle checks, and 864 broad pairs with zero mismatch. The final
and space-only 288-cell runs have zero status or safe-execution-count difference.
Profiles cover 273/288 formal cells and 12/60 hard cells, including 12 formal and 12 hard
resource failures through power-of-two checkpoints. Generic CSR fails on large graphs
(median packed/CSR 0.288x), while exact structural views plus direct-order EOG pass the
2x opportunity gate (2.04x median). Composition plus transitive closure account for
96.7%/97.4%/97.0% of SC/TSO/PSO predicate operation time. P0.6/P0.7 are justified; the
measurement code must be removed before production implementation.

### P0.6a: grouped packed primitive materialization

- [x] Freeze the exact construction-only scope, time/space effects and removal gates.
- [x] Implement packed row insertion and thread/location grouped construction.
- [x] Pass server unit/property, mutation-oracle and broad differential gates.
- [x] Run the balanced formal matrix and retain or remove under the frozen gate.

**Server build-option error:** the first paired Release builds completed, but the
configuration used CMake's generic `BUILD_TESTING=ON` while this repository gates its
test target with `BUILD_TESTS`. Both containers therefore exited 127 only when invoking
the absent `bin/unit_tests`; no correctness or benchmark task started. The retry keeps
the successful source build, reconfigures both trees with `-DBUILD_TESTS=ON`, and
rebuilds the test target with the same image and dependency source.

**Current correctness evidence:** the fresh baseline and candidate builds pass 145/145
and 146/146 Release tests. The candidate passes all 39 mutation rows and 5,441 full
offline-oracle checks. The broad differential discovers 288 programs and reports 852
matches, 12 mutual unsupported pairs, and zero mismatch across 864 SC/TSO/PSO pairs.

**Performance launch error:** the first six BenchExec processes stopped before any task
because the Docker invocation exposed the host cgroup namespace but did not mount
`/sys/fs/cgroup`. The empty run is retained as `formal-invalid-missing-cgroup`. The
authoritative retry adds that exact cgroup mount; definitions, binaries, limits, core
ranges, and repetition schedule are unchanged.

**Final P0.6a decision:** retain. All 24 balanced run sets exited zero and produced
2,304 cells. Large-event materialization is 0.2596 of baseline [0.2049, 0.3302], with
all five four-repetition model-task ratios below 0.70. Common-terminal CPU is 0.9954
[0.9871, 1.0036], so the universal CPU gate is not met. The alternative gate is met:
PSO `fib_unsafe-5` improves from 0/4 to 4/4 terminal results and TSO `fib_unsafe-7`
from 1/4 to 4/4; all seven status changes are TIMEOUT to correct false, OOM remains
60, and execution-count mismatches remain zero. Full evidence and limitations are in
`optimization-analysis/continuous/grouped-primitive-materialization/decision.md`.

### P0.6b: exact structural primitive views

- [x] Freeze supported expression shapes, representation ownership, byte counters, and
  fail-closed fallback before changing `Value` or evaluator semantics.
- [x] Prototype exact `po/int/ext/loc` views without materializing dense base matrices.
- [x] Add view-versus-dense algebra oracles and repeat all correctness gates.
- [x] Measure persistent base bytes, intermediate composition/closure bytes, CPU, RSS,
  and terminal coverage before retaining or removing the prototype.

**Current P0.6b evidence:** the design and frozen gates are in
`optimization-analysis/continuous/structural-primitive-views/experiment-protocol.md`.
The candidate passes 147/147 Release tests, all 39 mutation rows and 5,441 oracle
checks, and the 864-pair broad differential with 852 matches, 12 mutual unsupported,
and zero mismatch. The balanced four-repetition formal matrix is active as the sole
`views-formal-sujie` container, with 48 BenchExec workers and P0.6a as its baseline.

**P0.6b v1 performance pilot:** repetition 1 completed all six run sets, after which the
active container was stopped during repetition 2. SC `fib_unsafe-7` already regressed
from about 23 to 44 CPU seconds. The exact cause is generic row generation for
structural intersections: sparse `rf/fr & ext` and `po & loc` repeatedly scanned full
structural rows. The pilot is retained as `formal-pilot-v1-row-generation-regression`;
its three partial repetition-2 XMLs are excluded. V2 filters only set bits of a dense
operand, enumerates the smaller view for view/view intersection, and writes grouped
structural rows directly into the required dense result without materializing a second
base matrix.

**P0.6b V3 pilot:** the 512-event representation gate was active and its 148 tests plus
both semantic suites passed, but SC `fib_unsafe-7` remained about 33 versus 23.5 CPU
seconds. The regression came from a dense-path integration mistake: `baseSubset`
copied and grew every old dense relation before comparing it. V4 restores the original
read-only prefix loop for dense/dense and reserves grow plus structural subset for a
pair containing a view. The stopped V3 pilot is retained as
`formal-pilot-v3-dense-subset-copy` and is excluded.

**P0.6b V4 smoke:** correctness and 13,238 explored executions matched on SC
`fib_unsafe-7`, but candidate CPU was 32.14 s versus 23.06 s. The run never exceeded
319 stable events, so the `>512` view path was inactive. Candidate materialization was
10.77 s versus 6.09 s and offline evaluation was 10.66 s versus 6.36 s. The formal
matrix remains blocked while the dense-only path differs from P0.6a. The first targeted
V5 change restores direct final-size construction of the four dense primitives below
the threshold, while preserving zero-size construction before structural assignment
above it. Both completed V4 smoke containers were removed; their logs remain evidence.

**P0.6b V7 dense-fast-path smoke:** hoisting representation checks out of packed
construction, closure, and composition restored SC `fib_unsafe-7` to 23.44 s versus
23.15 s wall time with identical 13,238 executions. Materialization was 6.17 s versus
6.09 s and offline evaluation 6.43 s versus 6.32 s. Unit/property tests pass 148/148.
The first mutation/broad launch exited 126 before executing a test because the copied
scripts lacked executable mode; the retry invokes the same scripts explicitly through
`bash`. The failed launch is excluded and its two own containers are removed.

**Final P0.6b decision:** do not retain standalone. All correctness gates and the
2,304-cell formal matrix pass, common-terminal CPU is 1.00337 [0.99762, 1.00946], and
large-event RSS falls to 0.87544 [0.86020, 0.88975]. Total current base storage falls
only to 0.70217 [0.66986, 0.73603], not the frozen 0.5 gate, and no terminal result
changes. The uncommitted implementation is used only as the P0.6c experimental base;
no source is committed unless the combined sparse-base design passes a fresh full gate.
Decision: `optimization-analysis/continuous/structural-primitive-views/decision.md`.

### P0.6c: size-adaptive sparse edge primitives

- [x] Freeze exact sparse/dense selection, mutation fallback, byte accounting, and the
  minimum additional saving required on the five completed large-event tasks.
- [x] Represent sparse `rf/co/fr/rmw/tc/tj` bases without changing CAT verdict authority.
- [x] Add dense/sparse/view algebra and graph-transition oracles.
- [x] Repeat 148+ tests, mutation, broad differential, and balanced formal gates.

**Final P0.6c decision:** retain. The 2,304-cell matrix has zero opposite verdict and
zero common-terminal execution mismatch. Large-event total base falls to 0.14139
[0.10608, 0.16869], RSS to 0.61512 [0.53543, 0.70107], and materialization to 0.79688
[0.70568, 0.89969]. Common-terminal CPU is 1.00209 [0.99644, 1.00783]. Twenty cells
move from OOM to TIMEOUT; correct true/false totals are unchanged, so this is resource
relief rather than newly solved coverage. Final server tests pass 150/150; mutation is
39/5,441 and broad is 852/12/0. Decision and evidence:
`optimization-analysis/continuous/sparse-edge-primitives/decision.md`.

**Retained optimization push:** production and tests only were committed and pushed as
`7d405d7e71f95054a1cea44fcab2c774c6093d57` on `genmc-caat`. Local HEAD and
`origin/genmc-caat` match. Planning notes, analysis, XML/log archives, server paths,
launchers, and unrelated untracked files were not staged.

**Upload error:** one rsync command initially flattened four files into the private
server experiment root. They were detected before compilation and only the exact four
misplaced copies were removed; the files were then uploaded to their intended source
directories. No other server path, container, image, or user data was changed.

### Optimization 12 / P0.7a: cycle-only closure slicing on the CSR baseline

- [x] Reconcile the previously rejected closure-slicing result with retained P0.6c.
- [x] Freeze the exact semantic activation rule, fail-closed neighbours, performance
  matrix and retain/remove gates before changing production code.
- [x] Implement the normalizer rewrite and independent/random plus near-neighbour tests.
- [x] Pass server Release, mutation-oracle and broad differential gates.
- [x] Run the paired large-task pilot and balanced 2,304-cell formal matrix.
- [x] Retain and push, or restore production/test source, strictly under the frozen gate.

**Current status:** the server is available again (56 logical CPUs, about 718 GiB
available memory) and the required `genmc15noble:sujie` image is present. Baseline is
the pushed `7d405d7`. The earlier implementation was semantically correct but failed
its old aggregate-speed gate before sparse primitive storage existed. The new protocol
at `optimization-analysis/continuous/cycle-closure-slicing-csr/experiment-protocol.md`
freezes a current-baseline retest: closure slicing must eliminate `reach`, preserve all
CAT/CAAT verdicts and explanations, avoid new OOM, remain within a 2% CPU upper bound,
and produce a predeclared large-task space or repeatable coverage benefit.

**Correctness checkpoint:** current candidate passes 152/152 server Release tests,
including independent random DFS equivalence and fail-closed near neighbours. Mutation
stress passes 39 rows and 5,441 full-offline oracle checks. Broad differential discovers
288 programs and reports 852 comparable matches, 12 mutual unsupported pairs and zero
mismatch across 864 SC/TSO/PSO pairs. The fixed baseline passes its original 150/150
tests. Performance pilot is now authorized by the frozen protocol.

**Expected certificate/test adjustment:** the first candidate run failed three tests
because slicing intentionally changes normalized IDs/cycle check kind and one benchmark
test expected to inspect the now-dead closure value. Exact post-rewrite SC/TSO/PSO
semantic fingerprints were installed; the benchmark now adds an observable alias so it
continues testing closure construction. No evaluator or verdict logic was weakened.

**Formal result and decision:** retain. All 24 runsets and 2,304 cells complete. There
are zero opposite terminal verdicts, zero log-derived execution-count mismatches and no
new OOM. Aggregate common-terminal CPU is 0.99575 [0.98138, 1.00687]. Five SC tasks
change TIMEOUT to correct false in all four repetitions (20 cells), so frozen Gate B
passes; PSO `fib_unsafe-5` adds two more correct cells. Large offline time falls to
0.48197, snapshot-equivalent state to 0.95338 and large RSS to 0.95984. Gate A's 10%
space threshold does not pass and is not claimed. TSO CPU is 1.01364
[1.00205, 1.02493], recorded as the main limitation. Full decision:
`optimization-analysis/continuous/cycle-closure-slicing-csr/decision.md`.

**Analysis setup error:** the first core-analysis invocation redirected stdout into the
not-yet-created output directory and stopped before reading any result. Creating the
directory and rerunning produced the authoritative files; raw XML/logs were unchanged.

**Retained optimization push:** only `Normalized.cpp` and `CatEvaluatorTest.cpp` were
committed and pushed as `a081137860808c245f7e4f4b21da3d0ffe9fc828` on
`genmc-caat`. Local and `origin/genmc-caat` match. Planning files, server definitions,
XML/logs, analysis scripts/results and unrelated untracked files were not staged.

**Post-push compatibility finding:** P0.7b auditing exposed an opt-in regression:
`--cat-preventive-pruning` was an extra-model consumer of removed `reach` and its checker
constructor asserted. The default 2,304-cell matrix did not enable this option and is
unchanged. The exact correction reconstructs `order+` on demand only for the certified
preventive path. Server verification is 152/152, mutation 39/5,441, broad 852/12/0 and
a real smoke that prunes 3/10 RF candidates. Three paired repetitions preserve all
preventive decisions and execution outcomes relative to pre-slice `reach`. The focused
fix was pushed as `35a595ab5a706c7fbae52a0f4ccc6d29655fcdd8`; local and remote match.

### P0.7b: exact sparse EOG / topological cycle state

- [x] Audit the post-P0.7a `acyclic(order)` hot path and current relation representation.
- [x] Freeze a semantic contract for exact cycle detection without materializing reach.
- [x] Prototype the smallest representation/evaluator change justified by the census.
- [x] Repeat correctness, broad and balanced formal gates before retaining anything.

**Current status:** P0.7a exposes direct `acyclic(order)` and removes closure work, but
TSO common-terminal CPU is 1.01364 and 40 TIMEOUT plus 40 OOM cells remain. The retained
structural census attributes about 84% of TSO/PSO predicate-operation time to
composition, so a packed-row-only DFS change is insufficient. The frozen P0.7b1
prototype compiles an exact lazy cycle plan from an exclusive positive normalized CAT
cone and omits only that cone's derived values. Unsupported consumers/operators,
explanation, oracle and preventive modes use the existing evaluator. It remains a CAT
evaluation strategy and never consults a built-in checker verdict. Protocol:
`optimization-analysis/continuous/lazy-cycle-eog/experiment-protocol.md`.

**Prototype error:** the first compile reached the new production code but failed in the
new property test because `Violation` intentionally has no aggregate equality operator.
The test now compares its four observable fields individually; no production code was
changed to accommodate the test.

**Server setup errors:** the first fresh configure waited in FetchContent; it was stopped
and replaced with existing complete RapidCheck/GoogleTest source directories. The first
cleanup ran as the unprivileged host account against root-owned container output and was
rejected, so only this experiment's exact `build-after` path was removed inside the
owned image. Two stale-log/path mistakes were also rejected as evidence: the candidate
unit binary is `build-after/bin/unit_tests`, and both dependency source overrides are
required before configure. The authoritative rebuild exits 0 and passes 156/156.

**Correctness checkpoint:** the pre-counter prototype passed 39 mutation rows with 5,441
full oracle checks and the 864-pair broad differential with 852 matches, 12 mutual
unsupported cases and zero mismatch. The final counter-instrumented candidate is being
rerun before any performance claim.

**P0.7b1 formal diagnosis:** 2,304 cells complete with zero opposite verdict/execution
mismatch. Aggregate CPU is 0.98089 [0.95940, 1.00019], TSO 0.96124 and PSO 0.93532;
`queue_ok_longest` changes PSO TIMEOUT to correct true in 4/4 repetitions. Large snapshot
state is 0.34489 and all 40 OOM cells become TIMEOUT. However SC CPU is 1.04231
[1.02308, 1.06364] and large SC RSS is 1.25667. A frozen P0.7b2 structural cost selector
will disable the optimization for models whose admitted cycle cones contain no
composition, then repeat the full matrix; no post-hoc model-name dispatch is allowed.

**P0.7b2 retain decision:** all 156 unit/property tests, 5,441 mutation-oracle checks,
and 864 broad pairs pass. The final 2,304-cell matrix has zero opposite terminal verdict,
zero common-terminal execution mismatch, and no new OOM. Aggregate CPU is 0.97950 with
95% CI [0.95888, 0.99714], TSO/PSO CPU is 0.97303/0.94685, large peak snapshot is
0.57732, and large process RSS is 0.66807. SC activation is exactly zero; large SC RSS
returns to 1.00005, although SC CPU remains 1.01531 [1.00701, 1.02376] and is not claimed
as an improvement. The frozen gate passes; the durable decision is
`optimization-analysis/continuous/lazy-cycle-eog/decision.md`.

**P0.7b2 analysis error:** the first read-only v1/v2 comparison assumed that v1 analysis
already existed at `server-results/analysis-v1/`. Only the raw `formal-v1` archive was
present, so the comparison stopped with `FileNotFoundError`. Both v1 analyzers were then
rerun from the archived 24 XML/log pairs before making the decision; no experiment data
or production source was changed.

**P0.7b2 verification error:** the first final SHA-256 loop stored all 13 paths in one
zsh scalar. Because zsh does not perform bash-style implicit word splitting, both local
and remote hash files were empty and their comparison produced a misleading match line.
The result was discarded. The corrected command uses 13 explicit loop arguments with
`set -euo pipefail`; both hash files contain 13 lines and compare exactly.

**P0.7b2 push status:** retained production/test changes were committed and pushed as
`959f8697cfd3b20d47a4ca9d925e5e7e8ffdc508` on `genmc-caat`; local and remote match.
Only the 13 measured source/test files were committed. Planning files, XML/logs,
analysis artifacts, server paths, and unrelated files remain local. The exact completed
formal container `lazy-cycle-selector-formal-sujie` was removed; images and experiment
data were left intact.

### P0.7c: per-plan composition-cost selector

- [x] Audit the retained TSO/PSO plan shapes and identify the union-only coherence plan.
- [x] Freeze the structural rule, time/space effects, activation checks, and retain gate.
- [x] Implement the minimal analyzer/test change and pass server correctness gates.
- [x] Run the same 48-worker balanced performance matrix before retain.

**Current status:** P0.7b is pushed. Its selector is model-global: one composition-heavy
plan causes every admitted check in the model to become lazy. TSO/PSO therefore execute
both their composition-heavy main order and union-only coherence check lazily, while the
P0.7b1 SC result shows that union-only lazy evaluation loses to generic packed values.
P0.7c selects each plan independently. The frozen protocol is
`optimization-analysis/continuous/per-plan-lazy-cycle-selector/experiment-protocol.md`.

**Setup errors:** the first baseline archive command created the experiment root but not
its `source-before/` child, so `tar` exited before writing any source. After creating the
child, the first two-file rsync placed basename copies at `source-after/Analysis.cpp` and
`source-after/CatEvaluatorTest.cpp` rather than their repository paths. Those exact two
misplaced files were removed and resynced individually. A recursive diff now reports
only the intended `genmc/genmc/CAT/Analysis.cpp` and `tests/unit/CatEvaluatorTest.cpp`.

**Decision:** reject and restore. Correctness passed (157/157 tests, 5,441 oracle checks,
852/12/0 broad pairs, and zero formal verdict/execution mismatch). The 2,304-cell matrix
shows aggregate CPU 0.99326 [0.98708, 0.99937] and 33.4% fewer lazy candidates, but
large RSS is 1.11401 and peak snapshot state is 1.20292, both violating the frozen 1.02
bound. TSO/PSO large copy time rises to 2.56352/2.09731 because the generic coherence
cone reintroduces persistent derived relations and checkpoint copies. Full evidence is
in `optimization-analysis/continuous/per-plan-lazy-cycle-selector/decision.md`.

### P0.7d: composition-EOG traversal without macro-edge enumeration

- [x] Classify residual candidate edges using the retained-vs-P0.7c differential.
- [x] Freeze the first exact streaming-multigraph theorem and fail-closed boundary.
- [x] Prototype against both materialized and retained lazy evaluators.
- [x] Run rollback correctness and the balanced server matrix before retain.

**Current status:** P0.7c proves that the union-only coherence checks account for about
2.065B of 6.181B emitted candidates, but rematerializing them raises large RSS/snapshot
by 11.4%/20.3%. They must remain lazy. P0.7d1 therefore streams the exact multigraph
without per-source successor vectors and stops emission on the first cycle. The frozen
protocol is `optimization-analysis/continuous/streaming-lazy-cycle/experiment-protocol.md`.
This removes `E_path` transient state but does not yet avoid composition macro-edge
derivations on safe graphs; a later product-state EOG is still separate scope.

**P0.7d1 formal failure:** correctness gates before performance passed (157/157 Release,
ASan+UBSan focused 3/3, mutation 39/5,441, and broad 852/12/0). The full matrix then
changed 20 baseline TSO TIMEOUT cells into deterministic segmentation faults: five
Goblint race-reach tasks in all four repetitions, at about 9.2 seconds and 1.057 GB RSS.
Directly nesting event DFS inside expression callbacks makes C++ stack depth proportional
to a long event path. CPU 0.99258 and large RSS 0.93470 cannot justify a runtime error;
P0.7d1 fails and is not retainable.

**P0.7d2 refinement:** freeze a 2,048-event streaming recursion bound and an exact
heap-frame iterative buffered fallback. The abandoned streaming prefix is not a verdict;
the fallback reruns the whole check. An 8,193-event stress graph and the exact five crash
tasks are added before all gates are repeated. Protocol:
`optimization-analysis/continuous/streaming-lazy-cycle/depth-bounded-protocol.md`.

**P0.7d2 formal checkpoint:** Release 158/158, ASan+UBSan focused 4/4, mutation
39/5,441 and broad 852/12/0 all pass. The full 2,304-cell matrix has no status or
execution-count mismatch. Aggregate common-terminal CPU is 0.98593 with 95% CI
[0.97878, 0.99306], TSO/PSO CPU is 0.97871/0.97746, large RSS is 0.97676 and
snapshot-equivalent state is exactly 1.0. The exact depth fallback activates 80 times
across completed profiles and prevents the P0.7d1 stack crashes. This passes the frozen
retention gate; decision write-up and source-only push remain pending.

**P0.7d2 push:** the exact eight files measured on the server were committed and pushed
as `d963c49e45b82066ca9dcd036232c7384f6a95fe` (`perf(cat): stream lazy cycle
traversal`). Local HEAD and `origin/genmc-caat` match. Planning notes, experiment
adapters, server paths, raw XML/logs, analyses and HTML were not staged.

### Latest P0.7d2 versus Deagle on the adapted 725-task census

- [x] Freeze the comparison set and resource limits: same 725 tasks, 60 s, 4 GiB, one
  core per task and fixed-seed preprocessing.
- [x] Start fresh native GenMC, P0.7d2 CAAT-SC and Deagle runs.
- [x] Extend only the experiment adapter with CAAT-TSO/PSO model-file mappings and
  start both model runs.
- [x] Merge all XML/logs with one canonical task name and generate five-method HTML.
- [x] Produce paired coverage, verdict, CPU and RSS analysis with model-semantics caveats.
- [x] Reuse the previous corrected-census CAAT result and generate a direct old/new HTML.

**Current comparison status:** server execution root is
`/data3/sujie/experiments/fair-coverage/comparison-latest-vs-deagle-60s/`. CAAT-SC
finished as two NUMA-local shards; Deagle finished; native GenMC, CAAT-TSO and
CAAT-PSO are running. A discarded initial 48-worker SC launch executed zero tasks
because BenchExec rejects an allowed-core set spanning unequal portions of two memory
regions. The replacement uses 28+20 workers on exact NUMA-local core sets.

**Invalid deployment evidence:** the first completed five-method attempt is archived at
`comparison-latest-vs-deagle-60s/invalid-missing-runtime-include-v1/`. The P0.7d2
binary embeds its build-time `/data3/sujie/.../lli/runtime-include` path, but those
containers mounted the data only at `/workspace`; 619 SC tasks consequently failed to
find `genmc.h`. A direct transformed-program smoke passes after mounting the same data
read-only at `/data3/sujie`. The replacement v2 run uses that mount for all GenMC
methods. Deagle's discarded launch executed zero tasks because its image entrypoint was
not overridden; v2 uses `--entrypoint bash` and has completed all 725 tasks.

**Analysis setup error:** remote `py_compile` could not replace an old root-owned file
under `comparison-60s/__pycache__`. No script or result was changed by the failure.
Setting `PYTHONPYCACHEPREFIX=/tmp/sujie-pycache` verifies all three analysis scripts.

**Final comparison result:** all five v2 runsets exited zero with 725 rows each. Deagle
has 615 correct results, versus native GenMC 408 and latest CAAT-SC 398; Deagle has
16 TIMEOUT/10 OOM, CAAT-SC 240/31. On 367 tasks where CAAT-SC and Deagle both terminate
with the same verdict, CAAT-SC CPU is 0.159 [0.121, 0.210] of Deagle, showing that
GenMC is faster on their easy common subset but has much lower coverage. The latest
CAAT-SC improves the previous corrected-census CAAT from 388 to 398 correct and 418 to
428 terminal results. OOM falls 57 to 31: five old OOM and five old TIMEOUT tasks become
correct, while 21 old OOM tasks become TIMEOUT. Same-verdict paired CPU is 0.915
[0.895, 0.935] and RSS is 0.994 [0.987, 1.000]. No old terminal verdict changes.

**Final artifacts:** five-method HTML has 3,625 logs and 4,350 verified task/log links;
old/new CAAT HTML has 1,450 logs and 2,175 verified links. Both were pulled locally
with the original XML/ZIP/console logs. A NUL-safe SHA-256 audit compares 6,582 local
and remote files with no difference. The initial macOS rsync attempt used unsupported
`--info=stats2` and transferred zero bytes; the compatible `--stats` retry succeeded.
The in-app browser could not initialize because its environment metadata omitted a
required sandbox field, so both locally link-verified HTML files were opened with the
macOS default browser instead.

**Cleanup:** all 17 extant containers created by the P0.7d2 formal run and this census
comparison were confirmed stopped and removed. The first cleanup loop made no changes
because remote zsh did not split a scalar list; the explicit-name retry removed only
the listed `sujie` containers. Images and all experiment/result directories remain.

### Unified previous/current six-method HTML

- [x] Reuse the archived post-dataset-repair CAAT-SC XML/logs without rerunning it.
- [x] Place previous CAAT-SC, current native GenMC, current CAAT-SC/TSO/PSO, and Deagle
  under one common result root so `table-generator` uses one 725-task row namespace.
- [x] Generate full and difference-only HTML/CSV tables.
- [x] Verify 725 unique rows, six non-empty statuses per row, 4,350 log links, and 725
  task links with zero missing targets.

**Six-method artifact:**
`optimization-analysis/svcomp2026/fair-coverage/comparison-latest-vs-deagle-60s/all-six/html/adapted-725-previous-latest-sc-tso-pso-genmc-deagle-60s.table.html`.
The first generation attempt used XML files from two different raw roots and therefore
produced 1,450 path-fragmented rows; that newly generated invalid directory was removed
and regenerated from a common raw root. No original XML or log was changed.

### P0.7e: product-state EOG cycle checking

- [x] Re-read development constraints and the full Phase 3 plan at starting commit
  `d963c49e45b82066ca9dcd036232c7384f6a95fe`; production/test sources are clean.
- [x] Audit the retained lazy interpreter, analyzer selector, generated TSO visitor
  layout, and local ordering-consistency/EOG literature.
- [x] Freeze the exact automaton-product theorem, fail-closed cases, cost model,
  correctness gates, and balanced performance retain rule.
- [x] Implement immutable per-check automata, exact product-cycle witnesses, counters,
  and focused/property tests.
- [x] Reject P0.7e1 after its pilot showed repeatable TSO/PSO queue slowdown and
  1.365x TSO queue RSS despite improved TSO `fib_unsafe-7` time.
- [x] Implement P0.7e2 cursor-based heap traversal; Release 161/161 and focused
  ASan+UBSan 7/7 pass, with mutation/broad gates running.
- [x] Run server Docker correctness gates and the 2,304-cell before/after matrix.
- [x] Reject P0.7e1/e2/e3, restore P0.7d2, and pause before a new optimization round.

**Current status:** protocol is
`optimization-analysis/continuous/product-eog-cycle/experiment-protocol.md`. The
optimization targets repeated suffix traversal in composition without publishing a
derived relation. It adds `O(QN)` transient color state capped at 64 MiB; unsupported or
oversized plans use P0.7d2 exactly. Built-in memory-model checker verdicts remain outside
the CAT/CAAT path.

**P0.7e1 evidence:** Release 161/161, focused sanitizer 7/7, mutation 39/5,441 and
broad 852/12/0 pass. Pilot TSO `queue_ok_longer` CPU is 1.224/1.176 and RSS
1.366/1.365; PSO queue CPU is 1.032/1.029. TSO `fib_unsafe-7` improves to
0.911/0.917, but P0.7e1 violates the frozen deterministic-slowdown and memory rules.
P0.7e2 removes recursive product frames in favor of a resumable explicit stack; no
formal performance run starts until the same pilot is repeated.

**Local compile error:** `cmake --build build-tests -j4` stopped immediately because the
previous local build directory no longer exists. It compiled no source and ran no test.
Per the macOS/server execution boundary, verification continues in a fresh server Docker
build instead of recreating a local test environment.

**Server configure errors:** the first fresh configure waited in GoogleTest download;
it was stopped and the retry explicitly reused the existing server-owned GoogleTest and
RapidCheck source trees. Removing the 504 KiB incomplete build from the host then hit
root-owned container files; an ephemeral `--rm` container removed only
`product-eog-cycle/build-after`, after which configure completed in 1.5 seconds.

**P0.7e2 checkpoint:** the product DFS now uses resumable heap frames and direct base
relation cursors. The exact same automaton and witness projection remain in force;
atomic intersections alone retain source-local successor buffers. Server source hashes
match local, Release passes 161/161, and focused ASan+UBSan passes 7/7. Mutation and
broad differential gates are running; no pilot has started yet.

**P0.7e2 polling error:** a read-only remote zsh command tried to assign its reserved
`status` variable and exited without reading state. The corrected poll uses
`state_line`; no source, build, experiment, or container state was mutated.

### Latest comparison HTML column cleanup

- [x] Add an explicit result-column allowlist for the six-method table.
- [x] Regenerate full/difference HTML and CSV without per-core CPU columns.
- [x] Verify 725 rows, 36 method columns, zero `cputime-cpuNN` tokens, six XML files,
  4,350 logs and 725 tasks; leave raw evidence unchanged.

**HTML status:** the main six-method page now shows only status, category, total CPU,
wall time, memory and termination reason per method. The first regeneration attempt used
`table` instead of the required `/table` initial-state value and stopped during argument
validation; the corrected invocation completed. The other current comparison pages
were audited and already contain zero per-core CPU columns, so they were not rewritten.

### P0.7e2 / P0.7e3 refinement

- [x] Reject P0.7e2 after complete correctness gates and a two-repetition pilot.
- [x] Audit the apparent baseline-counter anomaly against source hashes and raw logs.
- [x] Freeze and implement the P0.7e3 32 KiB cache-resident product selector.
- [x] Pass P0.7e3 Release 162/162 and focused ASan+UBSan 8/8.
- [ ] Complete P0.7e3 mutation/broad gates and balanced pilot-v3.
- [ ] Run the formal matrix only if pilot-v3 satisfies the frozen gate.

**P0.7e2 decision:** reject. TSO fib CPU is 1.278/1.280 and one repetition loses a
terminal result; TSO queue is 1.330/1.333 and PSO queue 1.167/1.132. The formal matrix
was not run. Evidence is in `product-eog-cycle/e2-decision.md` and
`server-results/pilot-v2/`.

**Baseline audit correction:** `source-before` matches d963 SHA-256 and its raw logs
contain no product counters. The apparent counter was a display-only shift caused by
`column` collapsing empty TSV cells. The structured parser output is correct. Its first
full-pilot invocation also treated `r01.console.log` as a repetition directory; adding
an `is_dir()` guard fixed analysis without changing raw files.

**P0.7e3 status:** the frozen rule admits product traversal only when the one-byte
`Q*N` color array is at most 32 KiB; otherwise P0.7d2 runs exactly. It never reads a
model/task name or verdict. Server Release and sanitizer gates pass; mutation is complete
at 39/5,441 while broad differential continues.

**P0.7e3 final decision:** reject and restore P0.7d2. Correctness passes Release
162/162, ASan+UBSan 8/8, mutation 39/5,441, broad 852/12/0, and a 2,304-cell formal
matrix with zero status, verdict, or safe-execution mismatch. Aggregate CPU is 0.99930
with task-bootstrap 95% CI [0.99216, 1.00667], so the frozen upper-bound-below-one gate
fails; TSO/PSO point ratios are 1.00955/1.00780 and there is no coverage gain. Root
macro candidates fall 58.75%, but 18.83 billion product transitions make combined
interpreter work about 3.72 times the baseline macro-candidate count. No production or
test source is retained or committed. Evidence:
`optimization-analysis/continuous/product-eog-cycle/decision.md`.

**Optimization campaign pause:** the second EOG exploration round is complete. No
P0.7f experiment is launched. The current retained and pushed baseline remains
`d963c49e45b82066ca9dcd036232c7384f6a95fe`.

## GenMC TIMEOUT versus Deagle-correct root-cause study (2026-07-17)

- [x] Freeze the paired cohort from the existing 725-task, 60-second, 4-GiB results.
- [x] Extract task-family, verdict, CPU/RSS, source-shape and GenMC log features.
- [x] Cluster common TIMEOUT mechanisms and identify representative tasks.
- [x] Audit Deagle logs and implementation paths used on the same representatives.
- [x] Map observed differences to sound CAT/CAAT and exploration-level mechanisms.
- [x] Produce a strict analysis bundle and prioritized implementation proposals.

**Execution boundary:** this phase is read-only with respect to experiments and
production code. It reuses the pulled XML/log/source artifacts and local Deagle source;
no server job or P0.7f prototype starts during the analysis.

**Study status:** complete. The primary cohort contains 200 tasks, of which 192 also
TIMEOUT under native GenMC and 179 are loop-free `pthread-wmm` encodings. Semantic
audit separates 190 nondet-mismatched pairs and 11 incomplete fallback-bound TRUEs,
leaving nine strict finite tasks. The report, TSV/JSON, statistical appendix, figure
catalog and three SVG figures are under
`optimization-analysis/timeout-vs-deagle/analysis-output/`. No experiment or
production/test source was changed.

## P0-A cross-execution CAT learned nogoods (2026-07-17)

- [x] Re-read development/CAT constraints and record baseline/server state.
- [x] Freeze the positive-literal proof boundary, database limits, counters and
  correctness/performance gates before implementation.
- [x] Implement a worker-local stable-key nogood database and pure replay tests.
- [x] Intercept matching snapshots before incremental fixed-point evaluation.
- [x] Run server Release/sanitizer, mutation oracle and 864-row broad differential.
- [x] Run core/diagnostic pilots and remove rejected variants; formal matrices are
  correctly skipped when the frozen advancement gate fails.

**Current status:** protocol frozen at
`optimization-analysis/continuous/learned-cat-nogoods/experiment-protocol.md`.
The implementation may learn only positive explanations of acyclic/irreflexive
violations in online-admissible CAAT models. A hit skips only the evaluator; prospective
RF/CO candidate removal is explicitly deferred until snapshot-level replay is proven.
The baseline remains `d963c49`. V1 through V2f are rejected by their frozen pilot
gates, so no formal matrix or production commit is justified. The uncommitted tree is
temporarily left at V2f only to profile the now-isolated first-Reasoner fixed cost;
it is not a retained optimization.

### Optimization 6.1 scope decision (2026-07-17)

- [x] Separate the proposed WMM/program-shape-specific 6.1 route from the retained
  general CAT representation optimizations P0.6a/P0.6c.
- [x] Remove 6.1 from the general optimization main line.
- [ ] Reconsider it only as an opt-in specialized lane after an automatic equivalence
  certificate, cross-model differential tests, and an independent cost gate exist.

**Decision:** do not treat Optimization 6.1 as a generally good optimization. Its
benefit depends on recognizing a particular encoding or semantic pattern, so enabling
it by default would narrow applicability and enlarge the trusted transformation
surface. The active general route remains model-agnostic CAT/CAAT work reduction.
P0-A learned positive conflict explanations was subsequently rejected by its formal
CPU gate; the current P0.7f route compiles epsilon closure and transition layout without
recognizing a model or program family. No 6.1 production implementation or server
experiment is part of either route.

**P0-A correctness checkpoint:** the final Release suite passes 163/163. The GCC 13
ASan+UBSan build passes all five focused tests, the online mutation oracle passes 39
rows / 5,441 checks, and the 864-row recursive broad differential reports 852 exact
matches, 12 mutual unsupported pairs, and zero mismatch.

**P0-A execution error:** the first hit-census launch referenced the local artifact
layout `lazy-cycle-eog/server-config/` on the server, where the XMLs actually live under
`lazy-cycle-eog/definitions/`. The command stopped before creating either requested
container. It created only the previously absent `source-baseline/` from the frozen Git
commit; no result or existing source was overwritten. The corrected launch must use the
server-discovered `definitions/` path.

**P0-A census retry error:** the corrected-XML launch exposed two independent launch
configuration errors. SC/TSO ran 96 rows each as UNKNOWN because
`GENMC_EXPERIMENT_ROOT` was omitted and the adapter generated nonexistent
`/workspace/.../recursive-*.cat` paths. PSO did not start because its `24-35` core range
crossed the server's unequal 28-core NUMA boundary, which BenchExec rejects. These are
invalid diagnostic results and must be archived separately. The next retry uses the
pipeline root explicitly and three within-node eight-core ranges.

**P0-A pilot preflight error:** the first pilot command tried to execute the Noble-built
baseline binary on the older host before creating the Docker container. Host glibc and
libstdc++ do not provide GLIBC 2.38 / GLIBCXX 3.4.31--32, so `set -e` stopped the launch.
No pilot container or result was created. Validate and execute both binaries only inside
`genmc15noble:sujie` on retry.

**P0-A V2b test-contract failure:** the first full suite passes 164/165 after all five
focused learned-nogood tests pass. The sole failure is the old integration assertion
that a repeated snapshot hits on query two. V2b deliberately observes query one and
learns on query two, so the earliest hit is query three. Update that test to assert the
intermediate learned/no-hit state and rerun; no production behavior is changed by this
test correction.

**P0-A V2e launch error:** the first Release-gate container mounted `/data3/sujie` at
`/workspace`, but the preserved CMakeCache records absolute `/data3/sujie/...` source
and build paths. CMake stopped during regeneration before compiling or testing. Keep
`correctness-v2e-build.log` as launch diagnostics and rerun with an identical-path
mount; do not delete or regenerate the existing build tree.

**P0-A V2e decision:** reject after correctness passes (Release 165/165, sanitizer
5/5, mutation 39/5,441, broad 852/12/0). The 23-cell common-terminal CPU ratio is
1.0626; PSO fib_safe-5 gains a terminal result, but queue/circular regress 2.5814/1.8017.
Clause-length credit is too weak: PSO queue still admits seven clauses after 72 hits.
V2f raises successful-attempt credit to the generic 64-literal bound before repeating
the same gates. Evidence: `optimization-analysis/continuous/learned-cat-nogoods/v2e-decision.md`.

**P0-A V2f decision:** reject after the same correctness gates pass. The 23-cell CPU
ratio improves from 1.0626 to 1.0449, but PSO queue remains 2.1290 despite learning
only one clause and doing 3,018 literal checks. Do not continue threshold tuning.
Post-hoc counters correct the initial attribution: queue's Reasoner costs only 1.132 ms;
the dominant cost is globally disabling lazy cycles. V3 restores lazy queries and
materializes full provenance only on an admitted explanation. Evidence:
`optimization-analysis/continuous/learned-cat-nogoods/v2f-decision.md`.

**Reasoner instrumentation test correction:** the first focused run passed 8/9. The
only failure assumed a recursive-looking source model must perform nonzero normalized
derived-stratum work, but `RecursiveCycle` is simplified to a base/closure witness and
legitimately reports zero derived scans. Replace the three nonzero assertions with
universal counter invariants; production semantics and instrumentation are unchanged.

**P0-A V3 formal decision:** correctness passes 165/165, 17/17 sanitizer, 39/5,441
mutation, and 852/12/0 broad differential. The complete 2,304-cell formal matrix has
zero verdict/execution mismatch, six coverage gains, zero losses, and RSS P90 ratio
1.00242. However, task-model median CPU is 1.01032 with task-clustered 95% CI
[1.00151, 1.01866], so V3 is not retained standalone. V4 adds a generic 1,024-query
warm-up before the first explanation. Evidence:
`optimization-analysis/continuous/learned-cat-nogoods/v3-decision.md`.

**P0-A V4 progress:** the generic 1,024-query warm-up implementation and its updated
integration contract pass 11/11 focused, 166/166 full Release, and 11/11 focused
ASan+UBSan tests on the server. The mutation oracle passes 39 rows / 5,441 checks.
The broad differential passes 852 exact matches / 12 mutual unsupported / zero
mismatch. A short-task preflight performs 74 match queries with zero Reasoner calls,
materializations, clauses, or hits. The 23-cell hit-rich pilot CPU ratio is 0.99483,
with one coverage gain and zero losses, so V4 advances to the frozen four-repetition
formal matrix.

**P0-A V4 launch errors:** the first mutation/broad launch exited 126 before any task
because the copied shell scripts lacked executable bits; rerun with explicit `bash`
without changing repository modes. The second launch used two wrong assumptions:
`GENMC_REAL` was unset for the wrapper and CAT models were addressed under
`source/tests/models/cat` rather than `source/models/cat`; both gates failed before
valid comparisons. The third launch fixes both inputs, preserves the diagnostic logs,
and is authoritative.

**P0-A V4 formal decision:** reject and do not commit. The complete 2,304-cell matrix
has zero verdict/execution mismatch, four coverage gains, zero losses, and memory P90
ratio 1.00219. However, the primary task-model CPU ratio is 1.01162 with clustered
95% CI [1.00521, 1.01774], failing the frozen 1.01 gate. A fixed query warm-up reduces
Reasoner calls by 63.5% but moves them to larger graphs: total Reasoner time increases
21.8%, hits fall 59.3%, and two V3 coverage gains disappear. Evidence:
`optimization-analysis/continuous/learned-cat-nogoods/v4-decision.md`.

## P0.7f compiled epsilon closure / fused transition program

- [x] Restore rejected learned-nogood production/test files to retained `d963c49` and
  archive their exact source diff.
- [x] Freeze epsilon-elimination semantics, fail-closed boundary, time/space effects,
  correctness gates and pilot/formal retain rules.
- [x] Reconstruct P0.7e3 from its measured server source and implement the fused flat
  program with exact reset witness metadata.
- [x] Pass Release, sanitizer, mutation and 864-pair broad correctness gates.
- [x] Run the balanced product pilot and launch the formal matrix only if it advances.

**Current status:** active. P0.7e3 proved exact suffix sharing but spent 18.83B runtime
product transitions. P0.7f precomputes epsilon closure once, flattens transition storage
and removes pure epsilon states while keeping the 32 KiB generic selector and P0.7d2
fallback. Protocol:
`optimization-analysis/continuous/fused-lazy-cycle/experiment-protocol.md`.

**P0.7f reconstruction launch error:** two local non-login zsh attempts stopped before
transfer because first `rsync` and then its child `ssh` were absent from PATH. No source
or remote state changed. The authoritative retry uses explicit `/usr/bin/rsync` and
`/usr/bin/ssh` paths.

**P0.7f fresh-configure launch error:** the first new server configure supplied the
existing RapidCheck source but omitted the existing GoogleTest source. CMake waited in
its external download step and never reached compilation. The task-owned container
`fused-lazy-build-after-sujie` and only its incomplete `build-after` directory were
removed; the authoritative retry supplies both local dependency-source paths. This
attempt contributes no correctness or performance evidence.

**P0.7f correctness status:** the authoritative server build passes all 163 Release
unit/property tests and all 10 focused CAT/CAAT tests under GCC 13 ASan+UBSan. The
online mutation oracle passes 39 rows / 5,441 exact query comparisons. The 288-program
SC/TSO/PSO broad differential reports 852 exact matches, 12 mutually unsupported pairs,
and zero mismatch across 864 pairs. The implementation therefore advances to the
frozen two-repetition product pilot; these results do not yet establish a speedup.

**P0.7f pilot decision:** advance, not retain. All 12 paired cells have matching status
and category. Every task CPU median is at most 1.03970 and every RSS ratio is at most
1.01728. PSO queue product transitions fall 143 -> 28 (80.42%); TSO fib product-state
visits fall 215,828,825 -> 42,966,528 (80.09%) and its two CPU ratios are 0.8406 and
0.8435. The frozen four-repetition 2,304-cell matrix is running with 48 BenchExec task
workers. Evidence: `optimization-analysis/continuous/fused-lazy-cycle/pilot-decision.md`.

**P0.7f formal decision:** reject as a general default and do not commit. The complete
2,304-cell matrix has identical 716 TRUE / 360 FALSE / 76 TIMEOUT counts, zero status,
terminal-verdict, execution-count or coverage mismatch, and unchanged snapshot state.
The aggregate CPU point ratio is 0.99962 but its task-clustered 95% CI is
[0.99407, 1.00521], failing the frozen upper-below-1 gate. TSO improves to 0.98786,
while SC/PSO regress to 1.00556/1.00561; a TSO-name selector is explicitly disallowed
by the Optimization 6.1 generality decision. Fused compilation still reduces P0.7e3
state visits 82.69% and transitions 24.08%, showing that the remaining relation/product
work—not epsilon dispatch—is now the limiting cost. Evidence:
`optimization-analysis/continuous/fused-lazy-cycle/formal-decision.md`.

**P0.7f cleanup:** the exact 59,227-byte rejected production/test diff is archived as
`rejected-source.patch` (SHA-256
`90907f7182ef2309f25484517f8c979cd6d8f307390ec309e93a497952c3f6fb`). All ten
experimental production/test files were restored through `apply_patch`; `git diff
--exit-code -- genmc tests` returns 0. All task-owned `fused-lazy-*` containers were
removed after their successful exit. Planning, protocol, XML/log and analysis artifacts
remain intentionally uncommitted, and no server configuration enters Git.

## P0.7g ordered compiled stream program

- [x] Restore and verify the retained `d963c49` production/test baseline.
- [x] Freeze exact ordered-stream semantics, generic fallback, time/space effects and
  correctness/performance gates.
- [x] Implement immutable ordered terms and direct base/filtered-base emission.
- [x] Pass server Release, sanitizer, mutation and 864-pair broad gates.
- [x] Run the balanced pilot; reject before formal because the frozen RSS gate fails.

**Current status:** rejected and restored. P0.7g preserves P0.7d2's event DFS and exact
callback sequence, but PSO queue RSS ratios 1.06385 and 1.02233 exceed the frozen 1.02
per-observation gate. The formal matrix was not run. Production/test paths match the
retained baseline, and no model/task name, host profile, built-in checker verdict or
Optimization 6.1 route was used. Protocol:
`optimization-analysis/continuous/compiled-streamed-cycle/experiment-protocol.md`.

**P0.7g first-build error:** production and test objects compiled through the CAT
implementation, but the unit-test translation unit referenced the new direct
`findLazyCycle` comparison without including `LazyCycle.hpp`. The Release build stopped
before linking or running tests. Add the missing test-only include and rebuild the same
source tree; this attempt contributes no correctness or performance result.

**P0.7g correctness status:** the corrected server Release build passes 159/159 tests;
focused GCC 13 ASan+UBSan passes 18/18. The mutation oracle passes 39 rows / 5,441
full recomputations, and the broad differential reports 852 exact matches, 12 mutual
unsupported and zero mismatch over 864 pairs. A mechanism smoke preserves exactly four
lazy checks and 15,503,664 candidates while activating four compiled checks / 26 terms.

**P0.7g pilot/cleanup:** all 12 pairs preserve status/category and exact emitted
candidates. Affected CPU-heavy cases improve, but PSO queue RSS increases 6.385% and
2.233%, so the predeclared gate rejects the candidate before formal. The exact patch is
archived as `rejected-source.patch` (SHA-256
`0e183b001af8540a547780ca09e58c24e85f4a39a29f046561b8c10de4635294`). Production
and test paths are restored, the patch passes `git apply --check`, and no task-owned
experiment container remains.

## P0.7g2 minimal ordered stream program

- [x] Audit P0.7g object-layout growth and separate instrumentation from the mechanism.
- [x] Freeze a new protocol before implementation or performance measurement.
- [x] Replace the existing lazy-root vector in place with immutable plans; do not add
  production counters or enlarge lifetime statistics objects.
- [x] Pass Release, sanitizer, mutation-oracle and broad-differential gates on server.
- [x] Run a repetition-rich RSS/CPU pilot; run formal only if every frozen gate passes.
- [x] Complete and analyze the 48-worker formal matrix, archive the rejected patch,
  restore production/tests, pull all evidence and remove the task-owned container.

**Current status:** rejected by the frozen formal CPU gate and fully restored. The
48-worker 2,304-cell matrix completed with exit code 0 and 24/24 XML plus 24/24 log ZIP
archives were pulled locally. All semantic/state/coverage gates pass, but overall CPU is
0.999423 with 95% CI [0.994756, 1.004137], so a general speedup is not established.
Production/test files match HEAD, the exact rejected patch is archived, and the
task-owned formal container has been removed. The 36 pilot pairs had zero
correctness/state mismatch; PSO queue CPU median was
0.918100 and its RSS median/max-level ratios are 1.013865/0.994650. Optimization 6.1,
model/task names, host profiles, expected results and built-in checker verdicts remain
disallowed selectors.

**Layout-probe launch error:** the first rsync targeted a not-yet-created experiment
directory, so the helper was absent and compilation stopped before producing data. The
directory was created explicitly and the same read-only probe reran successfully; the
failed attempt contributes no measurement.

**Test-list launch error:** one read-only `--gtest_list_tests` command accidentally ran
the Noble/GCC 13 binary on the older server host and failed at dynamic loading due to
missing GLIBC/GLIBCXX versions. It executed no test. The command was rerun inside
`genmc15noble:sujie`, and all subsequent builds/tests remain containerized.

**Pilot-launch shell error:** the first launch command assigned to zsh's read-only
`status` variable before SSH execution and therefore created no remote container or
result. The retry uses a direct `docker inspect | grep` check and launched the
authoritative pilot successfully.

**Analysis-script compatibility error:** the first local smoke used evaluated PEP 604
annotations on the older macOS Python and stopped before reading XML or writing output.
Adding `from __future__ import annotations` made the same script portable; it then
reproduced P0.7g's rejection, including the 1.063852 PSO-queue RSS maximum ratio.

**Formal-definition generation error:** the first local transformation embedded literal
`\\n` text in three XML column lines. XML validation caught it before upload or launch;
the lines were split correctly, and all three definitions now parse successfully.

**Monitoring-command shell error:** a read-only SSH status query contained a glob and
`[b]` pattern that local zsh expanded before SSH, so it returned no server status and
did not affect the experiment. The retry used Bash plus non-glob `find | sed`; it
confirmed the container is healthy, 22/24 XMLs exist, and the two remaining instances
are the expected static-shard tail rather than a reduction of the 48-worker launch.

**Local evidence-query error:** an `rg` pattern beginning with `--numOfThreads` was
parsed as an option and produced no evidence. Re-running with `rg --` confirmed
`launch-formal.sh` passes `--numOfThreads 8` to each of its six concurrent instances.

**Reverse-restoration format error:** the first `apply_patch` restoration attempt used
the line-numbered hunk headers from `git diff -R`; the patch tool rejected the entire
change during context verification, so no source changed. Replacing only those hunk
headers with the tool's context form applied the same reverse diff. `git diff
--exit-code -- genmc tests` and `git apply --check rejected-source.patch` then passed.

**P0.7g2 formal decision:** reject as a generic default. All 2,304 cells / 1,152 pairs
preserve terminal verdict, execution count, lazy candidate/check counts, snapshot/base
state and coverage, with no new OOM. CPU is 0.999423 [0.994756, 1.004137]; SC/TSO/PSO
are 0.994100/1.004601/0.999926. RSS cell P90, P90-of-levels and large-task maximum are
1.002677/0.999844/1.003172. Evidence is in `formal-decision.md`, `analysis-output/` and
`server-results/formal/`. The 24,003-byte rejected patch SHA-256 is
`a3deb5a4cdb78846398fb458c6f0872b21250f955477b384f5130e51389af04a`.

## P0.8a rollback-safe incremental topological certificate

- [x] Re-read retained lazy-cycle, incremental evaluator and graph-synchronizer mutation
  lifecycles and identify a rollback-safe certificate boundary.
- [x] Freeze the exact theorem, fail-closed cases, `4*N*C` space cost and pilot/formal
  rejection gates before source implementation.
- [x] Implement certificate production/validation without adding experiment-only
  production counters or storing macro edges.
- [x] Pass server Release, sanitizer, mutation-oracle and broad-differential gates.
- [x] Run the 48-worker, 2,304-cell formal matrix directly; do not use a small selected
  program pilot as a performance advancement gate.

**Current status:** P0.7f/g/g2 show that epsilon/control/stream dispatch reductions do
not establish a generic end-to-end gain; remaining root-relation enumeration and event
DFS dominate. P0.8a reuses an exact topological rank only when every current streamed
root edge satisfies it. Failure to validate always falls back to the retained complete
lazy DFS. The rank is valid across insertion rollback because deleting edges preserves
all strict inequalities; cyclic or replacement epochs may lose reuse but cannot publish
an assumed verdict. Protocol:
`optimization-analysis/continuous/incremental-topological-certificate/experiment-protocol.md`.

**P0.8a opportunity decision:** reject the short-certificate behavior before formal.
The first complete paired repetition has zero semantic/state mismatch, but affected TSO
fib/queue and PSO queue all retain exactly 100% of baseline full lazy checks. Stable
event growth invalidates the rank vector before it can validate an edge, so the frozen
20% avoidance gate is structurally unreachable. Stop the remaining repetitions and
archive the partial run; do not interpret its invalid predecessor launch or single RSS
outlier as performance evidence. P0.8b freezes append-stable provisional ranks followed
by exact whole-edge validation in `p0.8b-protocol.md`.

**P0.8b correctness status:** append-stable provisional ranks are implemented without
new counters, edge storage, undo or checkpoint fields. The added universe-growth test
keeps full lazy-check count unchanged on an old-to-new insertion and forces exact
fallback on a non-forward edge. Server Release passes 159/159, GCC 13 ASan+UBSan 5/5,
mutation 39/5,441, and broad differential 852/12/0. The selected-program pilot was
stopped because it both underutilized the requested 48 cores and could misclassify a
local effect as a general performance result. Its partial artifacts are audit-only and
excluded. Performance now proceeds directly to the established 96-task, four-repetition,
SC/TSO/PSO before/after formal matrix (2,304 cells) with 48 actual BenchExec workers.

**P0.8b formal decision:** reject and restore. The complete 2,304-cell / 1,152-pair
matrix has zero status/category/verdict/execution/snapshot/base mismatch, zero coverage
gain/loss and zero new OOM. However, all 360 counted TSO pairs and 336 counted PSO pairs
retain exactly the baseline complete-check count: totals are 1,787,856 and 5,246,664 on
both sides. CPU ratio is 0.996569 with bootstrap 95% CI [0.992152, 1.000971], so no
speedup is established. Large-task maximum RSS is 1.051310 on PSO
`queue_ok_longest`. Exact rejected patch SHA-256 is
`9b8bdcf3856c7ee95448e08ce40e13fad6fad2055945fde85e70a2087bcfa779`.
All seven production/test files are restored; raw 24 XML and 24 log ZIP files are local.

**P0.8b execution-policy correction:** the initially selected fib/queue performance
pilot was stopped and excluded because it underutilized the requested cores and could
overgeneralize a local task effect. The formal matrix ran directly with six simultaneous
8-worker BenchExec instances. During each model queue's tail, active work naturally fell
below 48; this is queue exhaustion, not a reduced worker configuration.

**P0.8b audit-script error:** the first read-only remote row-count script used an escaped
list literal inside an f-string expression and raised `SyntaxError`. The corrected script
computed the boolean before formatting and confirmed 24 XML, 24 log archives, 2,304 rows,
12 zero-status model runs and a normal completion marker. No result changed.

**P0.8b restoration-tool errors:** a forward-patch applicability check was mistakenly
run while the candidate was still applied, so it correctly reported that the patch did
not apply twice. The first two reverse-patch conversions also retained a Git `a/` or
`b/` path prefix; a third retained line-number hunk headers unsupported by `apply_patch`.
All three were rejected before edits. Removing either prefix and using plain `@@` hunk
markers restored all seven files. Clean-path diff and archived forward-patch checks pass.

**Local compile-directory error:** the first macOS compile check targeted absent
`build/` and therefore compiled nothing. The authoritative compile uses the existing
`RelWithDebInfo/` directory and successfully links the library, unit-test binary and
GenMC. No benchmark test ran on macOS.

## P0-B V1 conflict-directed forward-RF worklist pruning

- [x] Complete the P0.8b formal-log opportunity census.
- [x] Freeze the exact RF-only pruning theorem, fail-closed cases, counters and direct
  formal/full-adapted experiment protocol.
- [ ] Reconstruct the validated P0-A V3 lazy-preserving nogood base without V4 warm-up.
- [ ] Implement two-stage exact RF prefilter/prefix matching and focused tests.
- [ ] Pass server Release, sanitizer, mutation-oracle and 864-pair broad differential.
- [ ] Run the direct 48-worker 2,304-cell formal matrix and, if semantically clean, the
  complete 725-task 60-second adapted experiment.
- [ ] Pull all XML/logs, analyze time/space/coverage/pruning counters, then retain or
  restore using the frozen gates.

**Current status:** active implementation. The P0.8b census covers all 1,152 baseline
formal cells and finds zero incremental insert, rollback, rollback-insert or replace
transitions; a dynamic rollback certificate cannot affect that path. P0-B instead
targets exploration work. It may remove only a forward read revisit whose exact
hypothetical cut-prefix matches a Reasoner-certified positive sufficient CAT clause.
All other revisits and all failed proof obligations remain untouched. Protocol:
`optimization-analysis/continuous/conflict-directed-rf-pruning/experiment-protocol.md`.

**Priority correction and decision:** stopped before server correctness/performance
experiments and restored. Dequeue-time filtering sees an RF choice only after it has
already been enumerated and queued, so it does not directly shrink GenMC's candidate
generation or consistent equivalence classes. The 77,394-byte compile-clean prototype
is archived as `rejected-dequeue-prototype.patch` (SHA-256
`29ea5633f5b001125bac00478b7b652d6ca2c082518ac22d50c096d48b8bd58d`), and
`git diff --exit-code -- genmc tests lli` plus `git apply --check` both pass. No server
test or performance claim is made. Next work starts with candidate-space measurement,
then generation-time propagation and decision-level backjump.

## Candidate-space census for generic CAT exploration

- [x] Establish that native GenMC already contains TruSt optimal RF-DPOR and default
  SPORE symmetry reduction; do not repeat those algorithms as an optimization.
- [x] Add opt-in, behavior-preserving RF/CO/backward/worklist/prefix counters.
- [x] Compile `genmc` and `unit_tests` locally without running them.
- [ ] Pass server Release, sanitizer, mutation and broad-differential gates.
- [ ] Run the 48-worker 96-task and full adapted 725-task SC/TSO/PSO census.
- [ ] Pull XML/logs and quantify the candidate-reduction upper bound by outcome class.
- [ ] Freeze and implement generic CAT-derived generation-time propagation based on
  measured opportunity, without model-name/fingerprint or built-in-checker routing.

**Current status:** instrumentation compiles and changes no candidate choice. Existing
P0.3 evidence shows why this direction matters: exact CAT-derived PSO reversal pruning
removed 78.56% of offered RF/CO choices, reduced offline evaluations 55%, and produced
five repeatable TIMEOUT-to-terminal gains. The new census determines how much of that
opportunity generalizes beyond its disallowed exact-model fingerprint. Protocol:
`optimization-analysis/continuous/candidate-space-census/experiment-protocol.md`.

**First server CMake option error:** the initial Release configure used the ignored
`GENMC_BUILD_TESTS=ON` variable; it built `bin/genmc` but not `bin/unit_tests`, then
exited 127 at the missing test executable. Reconfigure with the repository's actual
`BUILD_TESTS=ON` option produced and passed all 159 tests. The failed attempt is not
correctness evidence.

**P0.8a pilot mount error:** the first pilot container mounted data only at
`/data3/sujie`, while the candidate binary's internal Clang include path was compiled
under `/workspace`. It therefore used system pthread declarations and all candidate
cells reported unsupported `pthread_mutex_*` in about 0.07 seconds. The run was stopped
and archived as `pilot-invalid-config/`; it contributes no performance evidence. The
corrected container mounts the same task data at both paths and real candidate cells
then run for 20--60 seconds with matching verdicts.

**Invalid-pilot log-inspection error:** one read-only inline Python command lost quotes
around a glob through nested SSH shell parsing and raised `SyntaxError`. It changed no
files. `unzip -l/-p` then read the same log archive and established the missing runtime
include path.

**Root-owned pilot cleanup error:** the host account could not remove BenchExec files
written as container root. A one-shot task-owned `genmc15noble:sujie` container removed
only the exact `incremental-topological-certificate/pilot/` directory after its audit
copy had been retained; no other experiment path was touched.

**Pilot analyzer indentation error:** the first log-ZIP fallback patch mixed tabs with
the file's spaces and `py_compile` rejected it before analysis. Replacing that block
with spaces passed `py_compile`; the analyzer now uses XML pairing plus log-derived
execution, snapshot, base, lazy-check and candidate counters.
## Candidate-space census execution errors (2026-07-17)

- Initial server correctness command passed `online-mutation-stress.sh` through environment variables, but the script requires three positional arguments; Release build and all 158 unit tests completed, then the script failed before testing. Relaunched with explicit arguments.
- The synced CAT scripts are not executable in the server checkout, so direct execution returned status 126. Relaunched them explicitly with `bash`; no test result was lost or misclassified.
- The first 96x3 census binary was compiled with the container-only `/workspace` source path and then run by host BenchExec under `/data3`; its embedded runtime include path was invalid, producing 94/96 unknown-external results per model. The entire run was invalidated and the binary is being rebuilt with identical host/container `/data3/sujie` paths before rerun.
- The first census-analysis join used substring matching between BenchExec log names and task stems, which was ambiguous for `48_ticket_lock_low_contention_vs` versus its `-pthread` variant. It stopped without producing a summary; exact `.TASK.yml.log` suffix matching replaces it.
- The first rendered summary requested `maximum_retained_work`, while the normalized log key is `max_retained_work`; it displayed zeros despite correct raw counters. The summary field was corrected before using peak-work data in any decision.
- The first generic-preventive broad command applied the opt-in flag to SC and TSO as
  well as PSO. SC/TSO deliberately reject this flag because their generated candidate
  profile is not the claimed optimization, yielding exactly 576 configuration
  mismatches (288 programs x 2 models). The run is excluded; the harness now limits
  extra recursive arguments to an explicit model set and reruns all 864 pairs with
  preventive pruning only on PSO.
## Generic preventive V1/V2 (2026-07-17)

**V1 decision:** reject the dense full-closure implementation. Two complete 96-task PSO
repeats were identical: baseline 84 terminal / 12 TIMEOUT versus candidate 87 terminal /
4 TIMEOUT / 5 OOM. The search reduction is real but duplicates an O(V^2) closure for
both certified checks. V2 keeps the exact theorem and replaces each closure with
focus-specific forward/reverse reachability plus temporary O(V+E) inverse CSR. V2
correctness gates are running on the server.

## Generic preventive V3--V6 (2026-07-17)

- [x] V3: remove dense checked orders with lazy sparse materialization; reject its two
  certificate replays despite zero OOM and three stable coverage gains.
- [x] V4: structurally select one external-RF certificate; reject its 1.05457 CPU and
  1.03439 RSS ratios despite four stable coverage gains and 78.56% pruning.
- [x] V5: test one selected dense order/closure; reject 20 OOM cells and four repeated
  `queue_ok_longest` TRUE-to-TIMEOUT coverage losses.
- [x] Implement V6 single-pass lazy capture: the already required cycle traversal emits
  one exact sparse CSR root, and preventive queries retain only forward/reverse focus
  sets after a temporary O(V+E) inverse CSR.
- [x] Add random materialized-vs-captured relation equivalence and incremental
  insertion/rollback/oracle tests; macOS compile-only build succeeds.
- [ ] Complete server Release, sanitizer, 39/5,441 mutation-oracle and 864-pair broad
  differential gates for V6.
- [ ] Complete four balanced 24+24-worker formal repetitions, pull XML/logs, and decide
  V6 from coverage, CPU, wall, RSS and exact candidate counters.
- [ ] If V6 is retained, move generalized conflict certificates from query avoidance to
  RF/CO generation/enqueue pruning and require a measured drop in queued/popped work.

**Current status:** V6 correctness container
`generic-preventive-v6-correctness-sujie` is running on the only authorized server.
No macOS tests or benchmarks were run. The absent local `build-tests` path caused one
failed build invocation before the existing `RelWithDebInfo` compile-only tree was
found; it changed no source or results.

**V6 correctness result:** final-synchronized source passes Release 161/161, GCC 13
ASan+UBSan 161/161, mutation 39 rows / 5,441 offline-oracle checks, and broad
differential 852 comparable matches / 12 mutual unsupported / 0 mismatch over 864
SC/TSO/PSO pairs. The formal 4x96 PSO A/B is running as 24 baseline + 24 candidate
workers in `generic-preventive-v6-formal-sujie`.

**V6 execution errors:** the first correctness launch was invalidated after a final
implementation-only removal of repeated `vector::reserve`; its partial logs are under
server-only `logs/invalid-pre-final-sync/`. The final-synchronized mutation launch first
used `source/models` instead of `source/models/cat` and exited before its first oracle
row; that diagnostic is under `logs/invalid-model-root/`. Release and sanitizer had
already completed on the final source, so only mutation/broad were rerun. The first
formal-launch command stopped before creating output because `docker rm` named an
already-removed task container under `set -e`; the retry made cleanup idempotent.

**V6 formal decision:** reject the current representation, preserve the theorem and
continue to V7. Four repetitions show four stable coverage gains, zero OOM/losses,
81.74% fewer RF/CO choices queued, 73.97% less work popped, and peak retained work
51,601 -> 801. CPU remains 1.04146 and RSS 1.03698, so V6 fails the frozen default gate.
Raw XML/logs and strict analysis are local under
`optimization-analysis/continuous/candidate-space-census/{server-results/generic-preventive-formal-v6,analysis-generic-preventive-v6}`.

- [x] Implement V7 direct CSR rows: move each sorted/deduplicated successor row from its
  completed DFS frame into row storage, then build CSR without global edge sorting or a
  16-byte pair vector.
- [x] Extend sparse-relation and random captured-vs-materialized tests; macOS compile
  only succeeds.
- [ ] Run V7 server Release, sanitizer, mutation, broad differential, then the same
  no-pilot 4x96 / 48-worker formal protocol.

**V7 formal decision:** positive but not the final default representation. V7/V6
candidate CPU is 0.97601 [0.96429,0.98706] and wall is 0.97797
[0.96588,0.98941], while every candidate-space counter is exact. Against its own
baseline CPU remains 1.02416 and RSS 1.02792, so continue to V8 instead of retaining as
default. Raw results and analysis are local under the corresponding `formal-v7` paths.

- [x] Implement V8 optional reverse CSR and `nextPredecessor`; remove two full forward
  CSR scans and the temporary inverse allocation from every preventive preparation.
- [x] Extend dense, ordinary sparse and bidirectional sparse cursor tests; macOS
  compile-only build succeeds.
- [x] Run V8 full server correctness and no-pilot 4x96 / 48-worker formal protocol.

**V8 formal decision:** positive representation change, but reject as the final default.
V8/V7 candidate CPU is 0.98477 [0.97280, 0.99650] with exact search counters, while
candidate/baseline CPU remains 1.00796 [0.94325, 1.09232] and RSS is 1.02600
[0.99979, 1.06664]. The exact result and hotspot attribution are recorded in
`optimization-analysis/continuous/candidate-space-census/v8-decision.md`.

- [x] Implement V9 direct preventive root evaluation with an independent stable-ID
  adapter, without synchronizing or evaluating unrelated CAT predicates/checks.
- [x] Prove direct-root equality and fail-open behavior with unit/property tests, then
  run Release, sanitizer, mutation-oracle and 864-pair broad differential gates.
- [x] Run the same no-pilot 4x96 / 48-worker formal protocol and require both exact
  candidate-space preservation and removal of the early-error evaluator outlier.

**V9 correctness result:** macOS compile-only links `bin/genmc`; server Release and GCC
13 ASan+UBSan each pass 161/161 tests, mutation oracle passes 39 rows / 5,441 checks,
and broad differential reports 852 matches / 12 mutual unsupported / 0 mismatch over
864 pairs. The no-pilot formal matrix is running with 24 baseline + 24 V9 tasks.

**V9 execution errors:** the first multi-file rsync omitted `--relative` and created
three flat copies at the experiment source root without overwriting nested sources. Only
those exact copies were removed, then local/remote SHA-256 values matched. The first
formal container omitted the cgroup mount/privilege required by BenchExec; both runners
exited 1 before producing XML. It was removed and relaunched with the same cgroup setup
as the earlier formal container. A host-side attempt to move the root-owned empty output
directory was denied; the valid rerun safely truncates the manifest/console files in the
same V9-only directory and does not touch V8 or other results.

**V9 formal decision:** retain as the next opt-in working basis, but not as a default.
V9/V8 candidate CPU is 0.96105 [0.93854,0.98130] with zero status/search-counter
difference. Against simultaneous no-pruning baseline CPU is 0.97910
[0.91205,1.05786], so it misses the frozen confidence gate. Full evidence is in
`optimization-analysis/continuous/candidate-space-census/v9-decision.md`.

- [ ] V10: extract a bounded positive conflict core from an exact certified cycle and
  attach the proposed RF/CO delta as a sufficient nogood.
- [ ] Match learned cores prospectively during candidate generation so a repeated
  decision subtree is never enqueued; fail open on unsupported provenance.
- [ ] Run the same correctness and no-pilot formal protocol, requiring fewer preventive
  prefix queries or fewer offered/queued candidates than V9, not merely faster queries.

**Exploration-priority correction:** reducing one CAT query is not the primary objective.
Subsequent work is ranked by the amount of candidate exploration it soundly removes:
(1) evaluator-only speedups are supporting work; (2) certified pre-enqueue rejection of
an inconsistent RF/CO choice is useful candidate reduction; (3) a prefix conflict,
equivalence certificate, or abstraction that removes an entire descendant subtree or
equivalence class is the preferred target. V10 advances only if exact counters prove
level (2) or (3). A cache hit that merely bypasses an evaluator after the same candidate
has already been materialized does not qualify.

- [ ] P0: prototype a model-aware RF-value/EOG quotient with refinement before doing
  more evaluator-only or learned-query work. Require an explicit future-behavior
  equivalence argument, property preservation, and differential equality against
  exhaustive small instances before using it to merge consistent candidates. Measure
  reductions in offered, queued, popped and realized RF/CO/revisit work, not only CAT
  query count.
- [ ] P1: only after the quotient feasibility result, reconsider a rollback-scoped,
  monotone prefix blocker as a complementary inconsistent-subtree reduction: attach
  every positive conflict literal to its decision level, match the core at the earliest
  partial graph, and backjump before descendants are generated. Disable learning for
  non-monotone CAT provenance or mutable facts outside the rollback scope.

**Post-V10 audit and RF-equivalence pivot:** the partial V10 implementation was restored
exactly to the server-validated V9 source before experiments. A V10 hit occurs before
enqueue, but it only reproduces a candidate V9 already rejects, so it can lower direct
root checks but cannot reduce V9's surviving RF/CO work. Across 336 comparable V9 cells,
remaining queued work is 327,652 RF forward (76.3%), 98,148 backward (22.9%), and 3,336
CO (0.8%). The next behavior-preserving census therefore counts repeated
`(value, provenance)` RF classes as an optimistic RVF/value-centric upper bound; no
candidate is merged until causal-read order and CAT preservation are proved.

**Priority correction after review:** the next algorithmic optimization is candidate
equivalence reduction, not learned CAT query avoidance. Native TruSt already gives
RF-DPOR, so the target must be strictly coarser than reads-from equivalence while still
preserving local-safety verdicts and future extension behavior. The first proof target
is an SC RVF-style quotient (same event set, observed values and causal order on reads),
followed by explicit TSO/PSO/CAT refinement obligations. Same-value RF groups alone are
only an opportunity upper bound and are never merged. Learned conflicts remain P1
because they can remove inconsistent subtrees but cannot quotient multiple consistent
candidate executions into one representative.

The frozen P0 design and experiment contract are now under
`optimization-analysis/continuous/candidate-equivalence-quotient/`. It rejects local
same-value source deletion, requires `GoodW`/realizability/causal-map/backtrack machinery,
keeps the configured recursive-SC CAT checker authoritative, and treats all unsupported
operations and unproved TSO/PSO cases as fail-open. The historical GPLv3 Nidhugg branch is
architecture evidence only; no code may be copied into the dual-licensed GenMC tree.

- [x] Implement the independent base `VerifySC(X, GoodW)` witness-state search with
  exact `(executed event set, active-writer thread map)` state equality and explicit
  witness reconstruction. It does not call the built-in SC checker.
- [x] Add deterministic tests including exhaustive comparison against all SC
  linearizations for every nonempty good-write combination of a two-thread fixture.
- [x] Run the new solver tests in the server Docker after the active 48-worker census,
  including exhaustive proper-event-set differential checks, independent witness
  replay, and ASan+UBSan focused validation.
- [ ] Integrate `GoodW`, causal maps, backtrack signals and interpreter witness replay;
  the standalone solver by itself does not yet reduce GenMC candidates.

**RVF exploration-state checkpoint:** the clean-room SC path now contains three
independent components: exact `VisibleW_PO`, per-read/per-thread causal-map cutoffs plus
rollback-scoped ancestor signals, and a lightweight `ExecutionGraph` adapter. The adapter
uses structural `Event`/address identities, expands one virtual initial write per
location, preserves create/join prerequisites, rejects non-atomic and RMW accesses, and
converts a successful VerifySC witness back to a concrete RF map in two phases. Release
and GCC 13 ASan+UBSan each pass the 11 focused tests before concrete RF-map replay was
added; the replay extension compiles locally but still requires its server focused gate.
None of these components is counted as candidate reduction until the interpreter queues
one representative per value group and exact offered/queued counters fall.

**RVF compile-only errors:** the existing `.codex-build-v9` was configured with
`BUILD_TESTS=OFF`, so the first `unit_tests` target request had no rule. A fresh test
configuration with `BUILD_LLI=OFF` was also invalid because repository test definitions
refer to `$<TARGET_FILE:genmc>`. The valid full configuration compiled the new test
object but initially failed to link against a stale Homebrew `libhwloc.dylib` symlink;
reconfiguration with that unavailable optional library ignored linked `unit_tests`
successfully. No tests were executed on macOS.

**RVF solver correctness checkpoint:** after forcing CMake to rebuild the synchronized
test source, the server Release and GCC 13 ASan+UBSan binaries each pass all 5 focused
`SCGoodWritesSolver.*` tests. The exhaustive test covers all 15 nonempty four-event
read/write shape masks and every nonempty good-write subset for every read, compares
against direct enumeration of all SC thread interleavings, and independently replays
every returned witness. The focused suite takes 5 ms in both builds. The complete unit
binary still reproduces an existing RapidCheck `free(): invalid pointer` in
`IntervalMapPropertyTest` when the new suite is excluded, so it is recorded as test
infrastructure debt rather than attributed to this solver.

**RF opportunity correctness execution error:** the first Docker launch mounted the
user directory at `/workspace`, while the existing CMake cache names
`/data3/sujie/...`; it exited 2 during CMake's pre-build check and ran no tests. The exact
owned container was removed and relaunched with `/data3/sujie:/data3/sujie`.

**Full-725 census launch errors:** BenchExec first rejected cores `0-47` because the
selection was asymmetric across NUMA nodes; the replacement uses 24 cores from each
node (`0-23,28-51`). The next launch passed only relative names such as
`recursive-sc.cat`; all PSO tasks and the started SC tasks returned ERROR 17 because
BenchExec's working directory contains no CAT model. Docker reports `OOMKilled=false`,
so exit 137 was an external stop, not a tool OOM. Preserve both launches as invalid and
set `GENMC_MODEL_ROOT` to the source tree's absolute `models/cat` directory before the
fresh 48-worker run.

**Full-725 opportunity result:** the corrected 48-worker dynamic queues completed all
725 tasks for each of recursive SC, TSO and PSO with BenchExec exit 0. The joined 2,175
rows have zero accounting violations and 640/636/607 counter-bearing logs. Aggregate
same-value RF upper bounds are 18.93%/16.94%/12.12%; within counter-bearing TIMEOUT
rows they are 29.67%/29.55%/28.47%. Search-size sensitivity weakens the interpretation:
at `rf-offered >= 10,000`, TIMEOUT-minus-completed median fractions are only +0.62,
+0.37 and -0.77 percentage points. Therefore retain RVF as a hard-cohort experiment
because it exposes millions of removable candidates, but do not claim same-value
fraction independently causes timeout. Raw XML/logs, 2,175-row CSV, strict statistics
and SVG figures are under `optimization-analysis/continuous/candidate-space-census/`.

**RVF restart progress (2026-07-17):** local source passes `git diff --check` and the
existing macOS compile-only `unit_tests` target links. Exact source/test/build files were
synced to the dedicated server `sc-rvf-exploration/source` tree. The first fresh Release
gate was configured with Clang 15 plus GCC 13 libstdc++; it failed in pre-existing
C++23 ranges code (`AdaptiveView`/`EventLabel`) before compiling or testing RVF. This is
an invalid toolchain combination, not an RVF result. Reconfigure with the established
GCC 13 Release toolchain, then run Release and GCC 13 sanitizer focused gates.

**RVF restarted gate results:** GCC 13 Release builds both `unit_tests` and `genmc`;
11/11 solver/state/adapter focused tests pass. The first restarted sanitizer configure
incorrectly repeated the already documented `BUILD_LLI=OFF` combination; repository
tests reference `$<TARGET_FILE:genmc>`, so CMake stopped during generation and ran no
test. The sanitizer retry must retain the LLI/`genmc` target.

**RVF end-to-end root cause and gate (2026-07-17):** the leaked SB run repeatedly
scheduled the same load because scheduler actions name the pre-instruction event while
the resulting `ReadLabel` is stored at `action.event.next()`. Comparing processed reads
to `action.event` therefore never matched. The scheduler now compares to `.next()`;
temporary diagnostics are removed and the progress interval is restored to 100,000.
On the server, recursive-SC baseline, RVF `--nthreads=1`, and RVF `--nthreads=2` all
exit 0, report no errors, and explore exactly 3 complete SB executions. RVF counters
prove the optimized path ran: 4 attempted loads, 4 reduced loads, 5 representatives,
and zero fail-open events. GCC 13 Release focused tests pass 11/11.

**Sanitizer follow-up:** forcing the one-worker RVF path through the task pool exposed
a pre-existing nullable-pointer UB in `Interpreter::getDepTracker()`: `&*value_ptr`
dereferenced null before callers could test it. Returning `value_ptr::get()` is the
minimal semantics-preserving fix. After rebuilding, the sanitizer SB run exits 0 with
3 executions and the same RVF counters, with no ASan/UBSan diagnostic. A shell checker
mistakenly returned 3 because normal CAT statistics are written to stderr; that wrapper
result is invalid, while the saved GenMC output itself is passing evidence.

**Local fallback checkpoint while server is down (2026-07-17):** a 30-program paired
baseline/RVF × 1/2-worker matrix completed 120/120 runs with zero timeout and zero final
invariant violations. Seventeen programs enabled RVF and thirteen used native fallback;
all same-worker exit statuses and semantic verdicts match, successful RVF worker counts
agree, and every enabled run has zero late fail-open. The enabled cohort processes 173
loads but grows aggregate complete executions from 101 to 108 and has a diagnostic local
median elapsed ratio of 1.0084×, so no performance benefit is claimed. `fib_bench`
initially exposed a >100,000-load/20-second RVF expansion versus ~1.5 seconds native;
loop-bearing transformed programs now fall back before exploration. Fallback configuration
changes are delayed until after certification, restoring exact native one-worker behavior.

**RVF error/outcome checkpoint:** four explicit SC-SB outcome probes confirm `(0,0)` is
unreachable and the other three Boolean outcomes are reachable under baseline and RVF
with one/two workers. The probes exposed an intermittent error-replay assertion/segfault:
RF-DPOR replay restriction could remove the selected error event from an RVF witness
graph. Hard RVF errors now finalize directly on the certified witness graph. The suite
then passes 30 Release repetitions (360 invocations), plus 10 ASan+UBSan and 10 TSan
repetitions of the former two-worker crash. Focused unit tests remain 11/11.

**Local fallback regression checkpoint (2026-07-17):** while server execution remains
paused, register a dedicated CTest that runs loop-bearing `fib_bench` through baseline
and requested-RVF one-worker paths with a 15-second hard timeout. It must require the
loop-gate reason, identical exit/verdict/complete/blocked/exploration-stat output, and
zero RVF-specific work. This turns the previously manual 43,194-execution equality into
a repeatable zero-perturbation contract.

**Expanded local differential checkpoint (2026-07-17):** the runner now accepts an
optional line-oriented `RVF_CASE_FILE`. Forty additional litmus variants produced 160
baseline/RVF × one/two-worker rows: 160 completed, 35 enabled, 5 native fallback, zero
timeout and zero strict invariant violation. Enabled complete executions grow 294→298
despite 584 reduced loads; median local elapsed ratio is 1.0080×. `IRIWish` improves
27→16 while `LB3` regresses 7→18, so diagnose LB-family representative growth before
claiming benefit. Evidence is in `local-expanded-report-20260717.md` and
`local-results/differential-40-expanded-20260717a/`.

**Local merge-opportunity gate (2026-07-17):** `LB3` showed 18 singleton value groups,
18 representatives and 16 parent continuations, so RVF was adding search without
quotienting anything. First-visit reads now stay on native RF-DPOR unless a value group
has at least two sources. Later RVF problem construction derives singleton constraints
for native reads from the current graph rather than persisting an RF that revisits can
change. The repeated 40-case matrix remains violation-free and changes enabled totals
from 294→298 before the gate to 294→279 after it; all former growth disappears and only
11 genuinely reducible loads enter RVF. The repeated original 30-case matrix is exactly
101→101 with zero reduced loads. CTest was updated to use `IRIWish` (27→16) as the real
quotient fixture; integration/outcomes/fallback pass 3/3 and focused unit tests 11/11.
The local ASan+UBSan build also passes the updated `IRIWish` quotient integration and
all four SB outcome probes after recompiling the driver.

**IRIWish outcome-completeness checkpoint (2026-07-17):** a generated 32-outcome probe
encodes the five observable reads from the actual quotient fixture. Baseline identifies
16 reachable and 16 unreachable Boolean tuples; RVF one/two-worker runs agree on every
tuple (96 GenMC comparisons total), with the optimized gate enabled and zero fail-open.
The same 96-cell matrix passes under local ASan+UBSan. CTest now registers four SC-RVF
end-to-end tests, all passing in 9.35 seconds. The initial probe looped over joins and
therefore triggered native fallback; its result was discarded and the joins unrolled.

**Remaining reduced-cohort outcomes (2026-07-17):** exhaustive probes cover every
encoded observation of the other three programs whose complete-execution count falls:
`WRC+dep` has 5/8 reachable outcomes `[0,1,4,5,7]`; `MP+rels+acq` has `[3,4]` among six
conditional/sentinel encodings; `S+rels+acq` has `[2,5]` among six `(seen-y, final-x)`
encodings. Baseline, RVF/n1 and RVF/n2 agree cell-by-cell across 60 invocations, with
gate enabled and zero fail-open. Release CTest passes all five SC-RVF tests in 12.96s;
the new probes also pass under both ASan+UBSan and TSan.

**DOT error-view UB audit (2026-07-17):** Clang repeatedly diagnosed `if (&*errView)`
in `dotPrintToFile`: forming a reference through an empty `unique_ptr` is undefined
before the condition can test it. Replace it with the semantically intended ownership
test `if (errView)`, then exercise both hard-error DOT output and the SC-RVF error suites
under sanitizers. This is a generic error-reporting fix, not counted as RVF speedup.
The Release and ASan+UBSan hard-error runs both exit 42 and produce a valid 1,703-byte
DOT with no sanitizer report. Repeating the 40-case matrix preserves 294→279, 11 reduced
loads, zero timeout and zero invariant violation; five registered CTests pass 5/5 in
12.60s. The first test command was rejected before execution because it attempted to
delete its temporary file; the valid retry leaves explicit `/tmp` artifacts untouched.

**Automatic 185-litmus differential (2026-07-17):** automatic discovery records all
740 baseline/RVF × one/two-worker cells with hard timeouts. Of 185 fixtures, 184 run and
one (`wrw0.c`) consistently fails compilation because macro `N` is absent. There are
108 enabled, 76 native fallback, zero timeout and zero strict violation. Enabled search
falls 1,308→1,283 (25, 1.91%) at 17 genuinely reduced loads; diagnostic local median is
0.9940× and is not a performance claim. The first partial pass exposed that disabling
load annotations before the gate broke native IPR fallback for `WWR+2WR`. Source-level
assumes are now detected pre-transform, native annotation lowering is retained, and the
task falls back before exploration; baseline/RVF n1/n2 all exit 42. New exhaustive
outcome probes cover `cumul-release` (5/8) and `rel-B-cumul-acq` (10/16), and all outcome
suites pass under ASan+UBSan. Evidence: `local-auto-litmus-report-20260717.md`.
The extended Release CTest set passes 5/5 in 16.49s and focused units remain 11/11;
`WWR+2WR`, IRIWish and all reduced-outcome probes also pass under the rebuilt TSan binary.

**Raw value-group prefilter (2026-07-17):** completed locally while server testing is
paused. New counters expose 1,020 singleton/native decisions versus 17 genuinely reduced
loads. Moving the value/provenance duplicate check ahead of graph-adapter construction
reduces synthesized native-read constraints from 1,875 to 11 (99.41%) without changing
the 1,308→1,283 search result, fallback classification, timeout count, or any strict
invariant. Release CTest passes 5/5, focused units pass 11/11, and exhaustive reduced
outcomes pass under ASan+UBSan and TSan. The local median ratio is 1.0060× before and
1.0087× after, so no wall-clock improvement is claimed until server testing resumes.

**Current status:** local correctness and sanitizer verification for the prefilter are
complete. Server/Docker testing is suspended at the user's request and will not resume
without explicit confirmation that the server has recovered. The next local step is
source/diff hygiene only; the next performance step remains a fresh server-side paired
measurement after recovery.

**SmallVector prefilter follow-up (2026-07-17):** replace the raw duplicate check's
node-allocating map with an eight-key inline `SmallVector` and first-duplicate early exit.
The 740-row differential has zero violations and preserves every aggregate search/
mechanism counter. Focused units pass 11/11, Release end-to-end tests pass 5/5, and both
ASan+UBSan and TSan outcome suites print all six expected reachable sets without a
sanitizer report. Local median changes 1.0087×→1.0069×, which is too small/noisy to
claim speedup. The source change is uncommitted and provisional.

**Optimization campaign paused by user:** stop after documenting this candidate. Do not
start another optimization and do not resume server/Docker experiments. Before further
implementation, reassess whether SC RVF/value-centric exploration is still the desired
direction and what success metric should govern retain/reject decisions.

**Frozen-flow restoration audit (2026-07-17):** user approved restoring the original
process after the server recovered. The first contract audit is
`optimization-analysis/continuous/candidate-equivalence-quotient/contract-audit-20260717.md`.
Initial decision was request-changes before server execution. Primary-source rereading
then corrected one claim: ancestor signals prune redundant recursion but are not required
for completeness. Production conservatively submits every parent continuation, mapping
to an always-true signal; representative children clear processed reads so ancestors can
reappear on enlarged event sets. Exhaustive generated reachable-state coverage is still
absent, and parallel task decomposition remains unproved. Graph witnesses are replayed by
the normal interpreter
after `ThreadPool` task submission, and the recursive-SC CAT checker is invoked before
submission; those two integration boundaries are present. The provisional SmallVector
prefilter edit was removed because it changed no candidate-space metric. A stale config
unit expectation was updated to the intentional delayed-override rule required for exact
native fallback. No server container or test has been started during this audit.

## Mutex/RMW SC-RVF scope expansion (2026-07-18)

- [x] Implement and independently test atomic successful-lock read/write pairs.
- [x] Map lock CAS/failure/unlock labels and preserve RVF frames across native lock RF.
- [x] Pass Release, ASan+UBSan, contended mutex, and mutex+IRIWish outcome gates.
- [x] Run baseline/control/RVF directly on all 725 actual server/Docker tasks.
- [x] Repair formal analysis joining by full source identity instead of duplicate YAML basename.
- [x] Decide the branch from actual activation and downstream work counters.

**Decision:** reject mutex support as a standalone performance candidate, but retain its
correctness infrastructure. The 725-task run has 2 enabled tasks, zero RVF attempted or
reduced loads, zero representatives and zero fail-open. Production-safe gating stops 513
tasks first at assume/load-annotation IPR, so further isolated mutex/operation work is
deferred. The only next SC-RVF core question is a unified IPR-compatible quotient with a
finite class signature and explicit ancestor-completeness invariant. If that design audit
fails, pause SC-RVF as a full-workload optimization and prioritize exploration/history
memory compression. See `optimization-analysis/core-direction-review-20260718/core-roadmap-after-mutex-20260718.md`.

**Analysis errors recorded:** the inherited formal analyzer keyed rows by non-unique YAML
basename and failed on `wvr.yml`; the archive contains 407 duplicate-basename log entries.
The first replacement parser also failed on rewritten `.i`→`.c` sources and original
unrewritten paths. The final analyzer joins directory+source stem from each log command to
the XML source path and verifies all 725 rows per configuration.

## IPR-compatible SC-RVF audit (2026-07-18)

- [x] Trace load-annotation creation, runtime concretization, native IPR reconsideration,
  RVF parent continuations, `goodWrites`, scheduling, and backward revisits.
- [x] Identify the unsafe interaction caused by merely removing the static assume gate.
- [x] Define single ownership for annotated reads and future-write discovery.
- [x] Freeze conservative static admission and bounded-oracle requirements.
- [x] Implement the opt-in bounded experimental lane and invariant counters.
- [x] Pass generated Release/ASan+UBSan outcome coverage before any production admission.
- [x] Run the direct 725-task admission census and apply the activation retain gate.

**Audit decision:** proceed only with the bounded prototype in
`optimization-analysis/core-direction-review-20260718/ipr-rvf-entry-audit.md`. Annotated
reads must always be RVF-owned, failing value groups generate no child, and the parent
continuation exclusively owns future-write discovery. A native backward revisit of a read
already present in `goodWrites` is forbidden. The production gate remains until the load-
annotation pass records per-assume exact coverage; annotation-map size alone is unsound.

**Prototype build error:** `.codex-build-rvf-tests` was configured with `BUILD_LLI=OFF`;
CMake regeneration failed because registered tests reference the absent `genmc` target.
No source was compiled in that attempt. Continue with `.codex-build-rvf-tests2`, whose
cache has both `BUILD_LLI=ON` and `BUILD_TESTS=ON`.

**First annotated-read probe:** a two-same-value-writer fixture exposed a real semantic
mismatch before oracle registration. Native IPR upgrades unordered writes on an annotated
location to `VE_WWRace`, while RVF disabled `conf.ipr` and lost that error. Preserve the
user-requested IPR warning/error semantics separately in `scRvfNativeIpr`; only its revisit
algorithm is replaced by RVF ownership. The initial probe is diagnostic, not passing
completeness evidence.

**Assume CTest error:** the first registered shell gate expanded an empty Bash array under
macOS Bash 3.2 with `set -u`. The test executed zero GenMC baseline rows. Replace the
optional argument array with the repository's established empty-string pattern and rerun.

**Annotated-read prototype checkpoint:** the opt-in lane builds locally. The registered
`sc-rvf-assume-outcomes` gate passes all six baseline/RVF-one-worker/RVF-two-worker cells:
outcome 0 remains unreachable, outcome 1 remains reachable, the quotient changes complete
executions 2→1, and all RVF runs have nonzero reduced loads with zero fail-open. Full local
unit tests pass 190/190. Production behavior is unchanged unless
`--sc-rvf-annotated-reads` is explicit. Remaining oracle shapes and sanitizer validation
must pass before any server admission census.

**macOS sanitizer launch:** the first expanded matrix stopped after its first baseline
because LeakSanitizer reported 208 bytes from `libobjc`, `dyld`, and LLVM pass/TLS
initialization. No candidate/project allocation appears in the stacks, and no RVF row ran.
This is invalid matrix evidence. Rerun locally with `detect_leaks=0` while retaining ASan
and UBSan halting; require Linux/server sanitizer with leak detection before admission.

**Frozen annotated-read decision:** reject as a standalone optimization. Server GCC 13
Release passes 190/190. Linux ASan+UBSan with leak detection passes 16/16 focused tests
and the complete 35-invocation annotated outcome/error matrix. The full sanitizer unit
executable separately finds a pre-existing randomized `ViewPropertyTest` invalid free;
this is recorded rather than misreported as a candidate failure or full-suite pass. The
direct 725-task census completes with manifest status 0: the new per-assume gate admits
past 171 of the old assume/IPR blockers (513 to 342), but only two tasks are enabled and
all mechanism counters remain zero. The retain gate is false, so no performance matrix is
run. Evidence is under `optimization-analysis/core-direction-review-20260718/server-results/annotated-rvf-census-20260718b/`.

**Admission launch errors:** the first host launch ran zero tasks because the Docker-owned
result root was not writable by the SSH user. Container batch A also ran zero tasks because
`/workspace` was mapped to the wrong host directory. Batch B uses explicit benchmark and
experiment mounts and is the only valid census.

## Exploration/history memory track (active after annotated-RVF rejection)

- [x] Freeze the target: pre-CAT OOM and retained exploration state, not CAT relation bytes.
- [ ] Add opt-in accounting for retained labels/graph state, dynamic revisit views,
  worklist capacity, scheduler replay/history labels, and cross-worker graph clones.
- [ ] Run the accounting build on all 725 actual tasks and isolate the OOM cohort.
- [ ] Select one structural compression mechanism only after measured byte attribution.
- [ ] Require lower peak retained state or OOM/TIMEOUT-to-terminal improvement before
  considering timing work.

**Initial source attribution:** `WorkList` stores small polymorphic revisit objects, but
every `BackwardRevisit` owns a dynamically allocated `VectorClock`; current statistics
count items only. `ExecutionGraph::clone()` deep-clones every retained `EventLabel` and its
per-label views. `Scheduler` also retains cloned label sequences in a trie. Therefore the
first instrumentation must measure dynamic payloads and clone multiplicity; multiplying
`sizeof(Revisit)` or counting queue entries would miss the likely dominant allocations.
## 2026-07-18 decisive CAAT diagnosis

- [x] Add observation-only driver/CAT phase counters without changing exploration.
- [x] Pass server Docker Release (142/142) and GCC 13 ASan+UBSan (141/141) gates.
- [x] Run the frozen pthread-wmm 5-easy/5-medium/5-hard panel in Docker.
- [x] Reject RF-core/top-level-cycle/CDCL as the current main direction: 439,144/439,144
  completed-task CAT queries were accepted and every panel snapshot had zero inconsistent
  revisit prefixes.
- [x] Identify the dominant cost: CAT validity 79.02%, restore 20.34%, RF+CO 0.64%; the
  <=512 adaptive-offline policy caused zero true incremental transitions.
- [ ] Run paired A/B of adaptive-offline versus certified incremental synchronization,
  with identical verdict and exploration-count gates, before any new optimization code.
  - [x] Add one experimental CLI switch that only changes the adaptive-offline threshold.
  - [x] Pass local configuration/unit/differential gates with the switch off and on.
  - [x] Pass server Docker Release and focused ASan+UBSan gates.
  - [x] Run the fixed 15-case paired panel and compare terminal/search/resource metrics.
  - [x] Reject direct threshold removal: common-terminal CPU +32.73%, one terminal lost,
    209,527/233,553 changed queries still rebuilt and rollback reuse was zero.
  - [x] Add observation-only history-match/rejection counters before designing work-item
    checkpoint ownership.
  - [x] Measure history failure cause on three medium tasks: 51,486 entries examined,
    zero subset matches and therefore zero rollback attempts.
  - [x] Reject larger blind history and work-item checkpoint implementation as the immediate
    next prototype; the former cannot create a pre-choice state, while the latter requires
    persistent branching evaluator ownership.
  - [ ] Implement the proof-bearing primitive-delta adapter in independently oracle-checked
    substeps, starting with descriptor/cache boundary and set/single-edge deltas.
    - [x] Audit ownership/copy boundary: `initialize()` copies `BaseValues`, retained history
      copies it again, so a persistent adapter cache must not introduce a third retained
      snapshot or claim construction savings as end-to-end savings.
    - [x] Add phase-separated scan/delta/evaluator-copy/equality counters before changing
      primitive construction.
      - [x] Server Docker measurement: materialization 33.591 s (50.83% of CAT
        consistency), evaluator+history base copies 1.203 s (1.82%), equality 0.277 s
        (0.42%). All 10 common terminal search counters match exactly.
      - [x] Decision: primitive delta proceeds; history/base-copy micro-optimization does not.
    - [x] Implement exact unchanged descriptor/cache boundary behind
      `--cat-primitive-cache`, with full-materialization verification on every oracle hit.
    - [x] Pass 5,441 mutation oracle checks and 864 broad pairs with zero mismatch.
    - [x] Run simultaneous fixed-15 A/B: CPU -4.15%, materialization -17.31%, aggregate
      RSS +0.12%, identical status/search counters. Reject as standalone (<10% CPU gate),
      retain only as the next delta substep's experimental foundation.
    - [ ] Add set/id and single-edge RF/RMW/TC/TJ changed-query deltas with independent
      full-materialization oracle comparison.

**Instrumentation build error:** `.codex-build-rvf-tests2` has target `unit_tests`, not
`genmc-unit-tests`; the first build command stopped before compilation. Use the discovered
target names `unit_tests` and `genmc` without reconfiguring the build tree.

**Instrumentation sync error:** the first multi-source `rsync` flattened five files into
remote `genmc/genmc/` instead of their `CAT/` and `Execution/Consistency/` directories. No
build used them. The files were copied to the exact target directories, local/remote SHA-256
matched for all five, and only the five mistakenly created flat duplicates were deleted.

**Primitive-cache mutation launch error:** the first Docker command referenced
`tests/cat/online-mutation-stress.sh` relative to `/workspace`, so Bash exited before running any
row. The second command used the absolute path but attempted direct execution of a non-executable
test script and also exited before any row. The valid invocation explicitly uses `bash` with the
absolute path; neither failed launch is counted.

**Simple-delta boundary-test build error:** nine test expressions omitted the `cat::` namespace
on `StableEventKey`. Production sources compiled, no test ran, and the mechanical qualification
was applied before rerunning the same target.

**Primitive changed-query delta rejection checkpoint (2026-07-18):** the exact delta passed
local and server unit/sanitizer tests, but the server mutation oracle rejected it on
`recursive-tso`, two workers, `ms-queue-dynamic`: `fr` missed edges `79->82` and `81->82`.
The failure reproduces locally (second of 20 repetitions). Independently, normal-mode
`fcombiner` preserved all terminal and TruSt counters but increased materialization from
1.59 ms to 7.64 ms (4.8x). Do not run broad/paired-15 or tune this per-relation repair design;
restore the last exact unchanged-cache implementation and retain this attempt only as rejected
evidence. The subsequent server rerun was not evidence because SSH was closed by the remote host
(exit 255) after the build.

**Exact full-build checkpoint (2026-07-18):** small dense relations now optionally skip redundant
sorting and FR is derived from ordered per-address stores. Unit, sanitizer, 39-row mutation oracle,
and 864 broad gates pass with zero mismatch. On the paired fixed panel, materialization falls
48.85%, CAT consistency 42.47%, and completed-task CPU 22.49%. All-task CPU falls 7.90% because
the same five 61-second TIMEOUTs are unchanged. The earlier 2.92% figure was the concurrent suite
wall time, not CPU.
Retain `--cat-fast-primitive-build` only as an experimental foundation; do not run 283 yet. The
next core diagnostic is the offline fixed-point evaluator, now 26.897 s in the candidate.

## Fast-check recovery (2026-07-19)

- [x] Read the migration handoff and preserve the existing dirty research worktree.
- [x] Move `firstPair()` and `firstReflexive()` declarations from `EventSet` to `Relation`.
- [x] Restore a Linux GCC 13 / LLVM 15 build and link both `unit_tests` and `genmc`.
- [x] Run the full local unit binary: 221 passed, one expected Z3-availability skip.
- [x] Extend `online-mutation-stress.sh` to forward multiple options as a Bash array.
- [x] Complete the authoritative server mutation oracle and Release/sanitizer gates; retain the
  local early-error nondeterminism as a documented non-passing diagnostic.

**Recovery build errors:** migrated `.codex-build-rvf-tests2` names the old macOS source tree and
Homebrew `gmake`; it cannot be reused on Linux. The first fresh configure hit unavailable proxy
TLS, then a RapidCheck update TLS failure; the already complete sources are now reused with
`FETCHCONTENT_UPDATES_DISCONNECTED=ON`. GCC 14/LLVM 14 and GCC 13/LLVM 14 both fail in LLVM's old
`IntervalMap` headers, while Clang 14 cannot compile the required C++23 surface against the local
libstdc++ 14. The valid recovery toolchain is GCC 13 + LLVM 15.

**Fast-check local oracle status:** the first combined 39-row mutation launch stopped at TSO,
two workers, `malloc-not-hb0.c` because that nondeterministic error run reported zero oracle
checks. Focused repetitions show baseline 4/4/4 checks and candidate 8/0/0 checks, all exit 42 and
zero mismatch. This is not a passing oracle and not yet evidence of a semantic mismatch; diagnose
the parallel early-error path or use the server's established environment before certification.

**Fast-check final decision:** server Release passes 221 plus one expected skip; relevant
ASan+UBSan passes 31/31; mutation passes 39 rows / 5,416 checks; broad passes 852 matches / 12
mutually unsupported / zero mismatch. Fixed-15 all-task CPU is -8.20%, completed CPU -23.24%,
ordinary checks -5.12%, and RSS +0.06%, but all five hard TIMEOUTs remain. Relative to the prior
cache+fast-build candidate, CPU improves only 0.15%. Retain behind the experimental switch and do
not expand to 283. Report: `fast-check-recovery-report-20260719.md`.

## Exact successor-cursor composition (2026-07-19)

- [x] Audit the remaining offline profile and identify composition as the second-largest ordinary
  evaluator operator after primitive materialization was reduced.
- [x] Add independently switchable `--cat-fast-composition`: enumerate exact lhs successors and
  preserve the existing rhs row-union implementations.
- [x] Keep the incremental offline oracle on the old composition implementation.
- [x] Add dense/CSR, empty, and 64-bit word-boundary coverage plus randomized reference equality.
- [x] Pass the complete local unit binary: 223 passed, one expected Z3-availability skip.
- [x] Pass server Release (223 + one expected skip), ASan+UBSan (44/44), mutation oracle
  (39 rows / 5,416 checks), and 864 broad gates (852 match / 12 mutually unsupported / zero
  mismatch).
- [x] Run the fixed-15 paired A/B panel: all CPU -9.44%, completed CPU -26.77%, composition
  -88.61%, unchanged 10/5 terminal/TIMEOUT and exact search counts.
- [x] Retain behind the experimental switch, but do not expand to 283 because no hard cap moved
  and the strict all-task CPU gate remains below 10%.

## Exact successor-cursor cycle checks (2026-07-19)

- [x] Audit the now-dominant ordinary check path: acyclicity DFS scans every possible target even
  though relations expose exact sorted successor cursors.
- [x] Add independent `--cat-fast-cycle-checks`, preserving DFS color/parent/order/witness logic.
- [x] Add exact witness equality for cyclic CSR across 64-bit boundaries and acyclic coverage.
- [x] Pass local full unit: 225 passed plus one expected Z3-availability skip.
- [x] Pass server Release, sanitizer, mutation, and broad gates.
- [x] Run fixed-15 paired A/B: all CPU -12.74%, completed CPU -35.49%, check -77.08%,
  exact search counts, unchanged 10/5 terminal/TIMEOUT; retain the exact switch.
- [x] Run the simultaneous pthread-wmm 283 paired panel: eight TIMEOUTs become correct false
  results, with no reverse transition or terminal-verdict mismatch.
- [x] Run the authorized actual 725 paired panel: correct terminals 394 -> 402, TIMEOUT/OOM total
  275 -> 267, common-terminal CPU -27.87%, all-task CPU -4.49%, and aggregate RSS +0.92%.
- [x] Generate and pull the final 725-row/two-run-set HTML plus 28-row diff HTML; verify no
  `cputime-cpux` appears.
- [x] Audit and prototype exact ordered primitive coherence, preserving the old materializer and
  full evaluator as independent oracles; pass local/server Release, sanitizer, mutation, broad,
  and fixed-15 gates.
- [x] Reject expansion of ordered coherence: fixed-15 incremental CPU is only -0.08%, no hard cap
  moves, and suite wall is +0.11%, despite materialization -5.02%.
- [x] Audit and implement exact descriptor miss-build with a separate switch and unchanged graph
  rescan/full-snapshot oracle; pass Release, sanitizer, mutation, broad, and fixed-15 gates.
- [x] Reject descriptor expansion: scan -8.87% and materialization -2.68%, but fixed-15 CPU +0.02%,
  completed CPU +0.05%, and no TIMEOUT changes.
- [x] Implement and fully gate exact worker-local descriptor storage reuse.
- [x] Stop descriptor reuse before 283: materialization -11.01% and completed CPU -1.96%, but
  all-task CPU only -0.52%, suite wall -0.14%, and no TIMEOUT changes.
- [x] Re-rank remaining current-foundation costs; measured micro-phase ceilings cannot pass the
  expansion gate without a new work-removal mechanism.
- [x] Complete the final correctness/completeness, promotion, evidence, and deliverable audit in
  `caat-optimization-final-report-20260719.md`.

## Paper-facing CAAT optimization synthesis (2026-07-19)

**Goal:** consolidate the complete optimization history into one evidence-indexed internal report
that can later serve as the factual source for a paper, without promoting rejected or weak results.

- [x] Commit the retained exact CAAT implementation, tests, launchers, and final reports as
  `cd09f78b`.
- [x] Audit the strict analysis bundle and every optimization-stage report against raw paired
  artifacts.
- [x] Normalize methods, baselines, metrics, correctness gates, and decision status.
- [x] Write the unified paper-facing results report and reproducibility index.
- [x] Review numerical consistency, claim strength, links, and credential exclusion.

**Primary comparison:** baseline GenMC/CAAT versus the retained combined exact foundation
(`primitive cache + fast primitive build + packed checks + successor-cursor composition +
successor-cursor cycle checks`). Primary outcomes are terminal classifications under fixed limits,
common-terminal CPU, all-task CPU/wall, and correctness/completeness equivalence.

**Statistical scope:** paired deterministic benchmark observations and exact aggregate accounting;
no independent-seed sampling is available, so the report will not claim population-level
significance or confidence intervals.

**Errors encountered:** the first independent commit build exposed an accidentally staged
`ConflictCore.hpp` dependency in `LazyCycle.hpp`; the next build exposed conflict-core/SC-RVF tests
in the staged evaluator test file. Both came from mixed pre-existing research diffs. The commit was
amended to remove those rejected/unrelated hunks. One cleanup command targeted the temporary parent
instead of its registered `src` worktree; the exact registered worktree was then removed safely.

**Final verification:** clean detached snapshot of `cd09f78b`, GCC 13 + LLVM 15, independently
configured and built `genmc` and `unit_tests`; all 158 tests passed.

**Status:** complete. Unified report:
`optimization-analysis/core-direction-review-20260718/2026-07-19--genmc-caat-optimization--r00--paper-evidence-report.md`.

## Independent full-result reproduction (2026-07-19)

**Goal:** rebuild the exact retained commit `cd09f78b`, rerun the complete paired 725-task
experiment in a fresh server result directory, and compare it with the published internal claim.

- [x] Create a clean detached snapshot of `cd09f78b` and sync only through the approved source
  synchronization script.
- [x] Record server capacity/process preflight under `/data3/sujie`; rebuild and rerun correctness
  gates in the established LLVM 15 Docker environment.
- [x] Launch a fresh simultaneous baseline/candidate 725-task run without overwriting old results.
- [x] Pull raw artifacts, verify row/run-set completeness, and reproduce terminal/resource metrics.
- [x] Compare the rerun with the historical claimed run and write a strict reproduction report.

**Predeclared closeness gate:** zero terminal-verdict mismatch and zero resolved regression; the
candidate must preserve the optimization direction, reduce common-terminal CPU by at least 20%,
and keep all-task CPU and suite wall below baseline. Correct-terminal and hard-failure counts must
remain close enough that any deviation is attributable to boundary TIMEOUT/OOM behavior and is
reported task by task. The historical point estimates remain comparison targets, not exact
deterministic requirements, because simultaneous capped runs share machine resources.

**Primary historical target:** correct terminals 394 -> 402; hard failures 275 -> 267;
common-terminal CPU -27.87%; all-task CPU -4.49%; wall -3.78%; aggregate RSS +0.92%; eight
TIMEOUT-to-correct rescues and twenty TIMEOUT-to-OOM transitions.

**Statistical scope:** paired benchmark accounting under one fresh run per configuration; this
tests reproducibility of direction and magnitude but does not support population confidence
intervals or significance claims.

**Outcome:** pass.  Both 725-row status/category matrices exactly reproduce the historical run.
The rerun measures common-terminal CPU -27.44%, all-task CPU -4.43%, wall -3.74%, and aggregate
RSS +0.88%.  Report:
`optimization-analysis/core-direction-review-20260718/2026-07-19--genmc-caat-cd09f78b--r00--full-reproduction-report.md`.

**Publication status:** code commit `cd09f78b` and evidence commit `53c667d5` are locally complete.
HTTPS push is blocked because this environment has no GitHub credential; SSH is configured to an
unreachable port 9322.  The branch remains two commits ahead of `origin/genmc-caat`.

## Intermediate optimization development branch (2026-07-19)

**Goal:** preserve the remaining intermediate implementations, experiment infrastructure, useful
results, and research records on `genmc-caat-opt-dev`, publish that branch, and leave the complete
working tree clean without mixing them into the retained `genmc-caat` branch.

- [x] Audit all tracked/untracked changes by role, size, reproducibility value, and credential risk.
- [x] Create `genmc-caat-opt-dev` from the published CAAT evidence head while preserving the dirty
  worktree.
- [x] Commit intermediate source/tests separately from experiment scripts/reports where practical.
- [x] Ignore or safely remove build caches, generated binaries, OS metadata, and private local
  configuration.
- [x] Run proportional repository hygiene and object-integrity checks; prior experiment records
  retain their original validation status and intermediate prototypes are not promoted as passing.
- [x] Push `genmc-caat-opt-dev` and verify local/remote pointers plus a clean `git status`.

**Safety boundary:** do not commit plaintext credentials, `.claude`/local assistant state,
`.DS_Store`, compiler build trees, downloaded binaries, Python caches, or oversized regenerable
artifacts.  Preserve meaningful raw experiment summaries and compact evidence; use ignore rules for
large local archives that should remain available but do not belong in Git.

**Development policy:** all future optimization implementation and experiments begin on
`genmc-caat-opt-dev`.  Only independently gated effective commits are merged or cherry-picked into
`genmc-caat`.

**Archive commits:** `b8b008b9` preserves intermediate implementation/test prototypes;
`38222dca` preserves experiment infrastructure, compact results, reports, and ignore policy.

**Cleanup:** moved approximately 3.9 GiB of local build trees and generated binaries to the system
trash; they are recoverable from trash or by rebuilding.  Raw logs, ZIPs, generated HTML, caches,
local assistant state, and the credential-bearing local handoff remain ignored and untracked.

**Status:** complete and published to `origin/genmc-caat-opt-dev`.

## Core optimization campaign: P1 -> P0 -> P2 (2026-07-19)

**Goal:** execute every core direction from the frozen optimization brief on
`genmc-caat-opt-dev`, with correctness/completeness preserved by independent oracles and all broad
tests/experiments executed in the authorized server Docker environment.  Promote only independently
validated effective commits to `genmc-caat`.

### Phase A: frozen audit and measurement contract

- [x] Audit current exploration/history ownership, copying, worklist retention, and existing
  counters against the 20 Goblint TIMEOUT-to-OOM rows and pre-CAT OOM evidence. Archived missing
  phase markers are inconclusive because kill-time buffering can lose output; the correction and
  phase-separated evidence requirement are recorded in `notes.md`.
- [x] Audit existing SC-RVF region/fallback state and ConflictCore decision/provenance state. RVF
  lacks reversible region/descendant ownership; ConflictCore represents positive base facts rather
  than rollback-scoped RF/CO decision clauses.
- [x] Freeze per-direction correctness, completeness, resource, and early-stop gates before coding;
  see `notes.md` for the phase-separated P1, regional RVF, and certified-subtree contracts.

### Phase B: P1 exploration/history compression

- [x] Restore `genmc-caat-opt-dev` to an independently compiling interface-consistent baseline by
  recovering the evaluator/lazy-cycle implementation omitted from the archived intermediate commit.
- [x] Add exact attribution counters for retained labels, graph/history bytes, worklist bytes, CAT
  snapshot bytes, peak active work, and prefix sharing without changing scheduling semantics.
- [x] Run attribution census and identify the dominant retained allocation on the actual cohort.
- [x] Implement the highest-ceiling exact compression mechanism (prefix sharing, graph delta,
  revisit/class deduplication, compact worklist state, or bounded retained work as evidence directs).
- [ ] Pass Release, ASan+UBSan, mutation oracle, recursive broad differential, fixed panel, and
  progressively larger paired experiments.
- [ ] Retain, revise, or reject from OOM/TIMEOUT/terminal transitions plus CPU/wall/RSS and exact
  search/completeness counters.

### Phase C: P0 regional SC-RVF / generation-time quotienting

- [x] Freeze the transactional region state machine, result-isolation boundary, native entry
  snapshot, token/epoch ownership, nested revocation invariant, and implementation sequence in
  `optimization-analysis/core-direction-review-20260718/p0-regional-rvf-transaction-design.md`.
- [ ] Define and implement region ownership, entry/exit frontier, fail-open ledger, covered-class
  revocation, descendant withdrawal, and safe ancestor-alternative restoration.
- [ ] Extend exhaustive oracles for future writes, nested frontiers, loop iteration identity,
  own/non-own sources, pointer provenance, and one/two-worker class-set equality.
- [ ] Require nonzero actual-workload activation and reductions in offered/queued work, realized
  prefixes, or quotient representatives before any timing claim.

### Phase D: P2 certified subtree blocking

- [ ] Specify a sufficient `no-consistent-extension` certificate and stable RF/CO decision mapping.
- [ ] Validate earliest rollback-safe level, clause scope/lifetime, and work-item pre-enqueue block.
- [ ] Require an independent exhaustive oracle and actual reductions in direct checks,
  work-added/popped, or realized prefixes; cache hits alone are not evidence of success.

### Phase E: synthesis and promotion

- [ ] Maintain raw manifests, SHA-256 inputs, strict analysis bundles, figures, and decision reports
  for every accepted/rejected candidate.
- [ ] Run final broad regression and repeated paired experiments for retained candidates.
- [ ] Commit all research on `genmc-caat-opt-dev`; merge/cherry-pick only proven effective minimal
  commits into `genmc-caat`, then verify both remote branches and a clean worktree.

**Primary order:** P1 memory attribution/compression first; P0 completeness/activation second; P2
certificate third.  Evaluator-only micro-optimizations remain deferred unless measurements expose a
new end-to-end ceiling.

**Statistical scope:** deterministic paired task observations are the primary unit.  Single runs
support exact status/resource accounting and descriptive timing only; population-style claims
require at least three independent paired repetitions with clustered task analysis.

**Status:** Phase A complete. Phase B localized the representative Goblint OOM to first-query
derived-relation materialization: dense predicate values retain about 7.22 GB and a 32-GiB replay
samples a 16.44-GB process peak. Exact CSR plus copy-on-write union overlays move the identical
4-GiB task from OOM to TIMEOUT with a 456.4-MB sampled peak, but every tested policy is rejected:
the clean minimal fixed-15 comparison still regresses common-terminal CPU by 34.78%, and hoisting
COW mutation checks regresses it by 35.67%. Per the no-resource-tradeoff retention rule, the
candidate source was removed and will not be promoted. P1 now continues on the distinct small-graph
Weaver/libvsync exploration-history OOM cohort using enlarged-resource heap attribution before any
new compression mechanism is selected.
- [x] P1 empty EventDeps sharing: exact stable/candidate builds and fixed-15 resource gate pass;
  object sizes fall by 144 B per label and Weaver 4 GiB time-to-OOM improves 17.8%, with fixed-15
  CPU -0.11% and neutral memory. Focused dependency/clone tests pass. Still require complete-source
  ASan, mutation/oracle, and broad large-label gates before declaring effective or promoting.
  ASan+UBSan focused dependency tests and SC/TSO/recursive-CAAT differential tests pass 5/5. The
  283-task paired gate preserves exact task coverage, has no terminal regression, changes all-task
  CPU by -0.19% and common-terminal CPU by -2.66%, and moves one timeout to a correct terminal
  result. The cumulative mutation/oracle and complete-source gates below also pass.
- [x] P1 logical calculated-view deduplication: fixed-15 verdicts and resources remain neutral;
  retain on dev. Reject the subsequent prefix-view alias after a decisive +2.7% Weaver time result.
- [x] P1 same-worker history ViewBase sharing: cross-worker clones remain deep; the single fixed-15
  decision run preserves all statuses and changes aggregate/completed CPU by -0.78%/-2.29% with
  aggregate memory -0.05%. Retain on dev without another small-difference repetition.
- [x] P1 immutable calculated-relation storage: audited write-once/read-only use, reduced each
  cumulative label by another 8 B (152/240/248 B), and avoided deep relation copies in graph
  clones. A single Weaver 4-GiB run improved survival 1.7% but remained OOM; retain on dev and move
  on without repeating a small effect.
- [x] P1 inline backward-revisit clocks: replace the separate `VectorClock` allocation with typed
  inline View/DepView work items, saving about 24 B and one heap operation per backward revisit.
  Focused server tests pass 2/2; fixed-15 preserves all statuses with aggregate/completed CPU
  -0.36%/-1.03% and aggregate memory -0.06%. Retain without repetition.
- [x] Repair the test-suite ODR collision between two global `Oracle` helper types. Cumulative P1
  now passes 160/160 normal unit/property tests and 160/160 ASan+UBSan tests; SC, TSO, recursive
  CAAT differentials and the 39-row/5,441-check online mutation oracle pass in Release.

**P1 transition decision:** retain the five exact storage/allocation candidates on the development
branch. The complete current dev tree builds and runs 223 tests (222 pass, one expected backend
skip); the ASan integration container exits successfully. At the user's direction, stop adding
engineering-only P1 graph/history variants and move the research mainline to the more algorithmic
P0 regional SC-RVF direction. A final cumulative broad resource gate remains required before these
commits can be promoted to `genmc-caat`.

**P0 loop-equivalence correction (2026-07-19):** the initial regional-loop smoke compared native
and RVF complete-execution counts and treated 2 -> 1 as a completeness failure. That criterion is
invalid for a quotient: native RF-DPOR executions are finer than the implemented read-value/witness
classes. The loop blocker was historically a performance gate after `fib_bench` expansion, not a
proved semantic exclusion. Re-evaluate bounded loops by exhaustive observable outcomes, error
reachability, n1/n2 equality, and RVF class/work counters; do not require equality with native
complete-execution counts. The first local rebuild attempt did not reach candidate code because an
old CMake tree mixed Clang 14 with the updated GCC 14 `<format>/<ranges>` headers; authoritative
validation remains the established LLVM 15/GCC 13 server container.

**P0 loop decision:** the corrected minimal oracle proves that 2 -> 1 was a legitimate local
quotient, but the full two-iteration 20-shape oracle rejects loop admission for the correct reason:
12/1,620 observable error states are lost, consistently under n1/n2. The unsafe admission and CTest
registration are removed; the 6,480-call generator and raw counterexamples are retained as a future
repair gate. Do not launch workload timing for this candidate.

**P0 native-ancestor repair:** the loop counterexample remains real after manual unrolling and under
five explicit seeds. Dynamic tracking of native ancestor reads plus same-address future-write
revocation restores every lost observable error. A separate durable/speculative task-result split
then removes the scheduling-dependent n1/n2 count drift. The corrected unrolled oracle passes
6,480/6,480 server-Docker calls with 156 reduced cells retained, and five transaction unit tests
pass. Aggregate RVF process time remains +10.5%/+7.5% for n1/n2, so retain this as completeness
infrastructure on dev but do not promote it. Report:
`optimization-analysis/core-direction-review-20260718/p0-native-ancestor-frontier-report-20260719.md`.

**P0 repaired-loop gate:** regional admission now permits the statically certified combination of
bounded loops and non-atomic accesses, whose dynamic frontiers are owned by the repaired transaction.
The formerly decisive loop oracle passes 6,480/6,480 calls with zero violations and 156 reduced
cells, so the historical rejection is superseded for this implementation. The prior 16-task NA
archive accidentally ran the wrapper without `regional-rvf`; correct-mode spot checks reveal
secondary unsupported blockers. Next run a corrected loop+NA activation census, then pair only the
nonzero-activation actual tasks against native under identical enlarged limits.

**Errors recorded:** the server build target is `unit_tests`, not `unit`; its executable is
`build-dev-tests/bin/unit_tests`, not `tests/unit/unit_tests`; and the GoogleTest suite filter is
`RegionalRvfTransaction.*`, not `RegionalRvfTransactionTest.*`. Each command was corrected, and all
five intended tests ran and passed. The archived NA census omitted `GENMC_EXPERIMENT_MODE=regional-rvf`;
it is retained only as invalid activation evidence, not silently reused.

**Gate-census launch errors:** the first Docker census omitted the host cgroup mount and ran zero
tasks; the fixed host launcher now owns `--privileged` and `/sys/fs/cgroup`. The first gate-only
implementation read an LLVM option after `ResetAllOptionOccurrences()` and therefore continued into
exploration; the value is now persisted in `LLIConfig`. The first 51-task paired set used host
`/data3` paths where the tool adapter requires container `/workspace` paths; BenchExec returned zero
despite executing zero tasks. The analyzer now separates host validation paths from emitted
container paths, and the launcher requires exactly 51 XML `<run>` rows per lane.

**Source-sync error:** the first NA-prefix adapter rerun rebuilt no GenMC object because
`SCExecutionGraphAdapter.{hpp,cpp}` was absent from the explicit sync manifest. The remote hash
check could only validate listed files and therefore did not detect the omission. Both files are now
mandatory manifest entries; candidate tests from that unsynchronized binary are invalid and rerun.

**Rejected NA-prefix re-entry candidate:** allowing a new regional transaction after a native
non-atomic prefix passed the unrolled oracle but failed the full loop oracle. It produced widespread
n1/n2 count drift and lost a real error at shape `011100`, outcome `1112`, n1. The implementation,
counter, fixture, and adapter relaxation are removed. Do not repair or repeat this candidate; a
future re-entry design would first require a static/dynamic suffix-ownership certificate.

**Mandatory Docker/BenchExec preflight (applies to every future formal launch):** launch only via a
versioned host script; mount `/sys/fs/cgroup` read-write with the required privilege; verify every
explicit sync-manifest source is present and rebuilt; distinguish host `/data3/...` validation paths
from emitted container `/workspace/...` paths; validate the task-set row count before creating the
result directory; and accept a lane only when there is exactly one result XML whose `<run>` count
equals the declared task count. Docker/BenchExec exit code 0 and a log archive alone are never
evidence that tasks ran. Every new infrastructure failure must be appended here before retrying.

**P2 Gate-A1 unit-fixture error:** the first `CATDecisionState` focused run exited 139 because the
test inserted labels into thread IDs that had not been created. This was a test-graph construction
failure, not a candidate result. The fixture now explicitly calls `addNewThread()` and uses each
new thread's index 0 before the one permitted rerun.

**P2 Gate-A1 sync-manifest error:** the first integration build failed before linking because
`ConsistencyChecker.hpp` was modified locally but absent from the explicit server sync manifest;
the remote `GenMCDriver.cpp` therefore compiled against the old interface. No test or experiment
result was accepted. The header is now mandatory in the manifest and SHA-256 verification. Before
future builds, modified-path coverage must also be checked against the manifest rather than relying
only on hashes of the files already listed.

**P2 Gate-A1 first fixed-15 evidence limitation:** the strict paired run completed with 15 XML rows
per lane and identical 7 correct / 8 TIMEOUT classifications. All seven terminal candidate logs
report zero installed conflicts. The eight killed processes cannot emit checker-destructor counters,
so they do not prove zero opportunity. Backjump counters are therefore added to the existing
100k-activity progress record before the single timeout-evidence rerun; the first run remains valid
for terminal equivalence and launch validation, but not for the Gate-A1 opportunity decision.

**P2 Gate-A1 decision:** the timeout-resilient fixed-15 rerun has zero status differences and zero
comparable search-counter differences. Eleven tasks emit final/progress census records; all report
zero installed conflicts, cores, mapped decisions, recurrence, non-local depth, and potential
backjump distance. The other four stop after transformation and before this mechanism can learn or
block anything. Combined with the existing full-725 V9/V10 zero-subtree-reduction evidence, reject
Gate B/C, active CDCL, and another broad run. Preserve only opt-in observation infrastructure on
dev; do not promote it. Report: `p2-backjump-gate-a1-report-20260720.md`.

**P2 final CO-sidecar unit error:** the first new CO replacement test hit the invariant for an
"unplaced" write because `ExecutionGraph::co_imm_pred()` represents the address-specific Init
predecessor as null rather than as a `WriteLabel`. The implementation now separately verifies
`isInCo()` and maps a null immediate predecessor to the write address. No formal run used this
post-census test change; rerun only the focused unit gate.

**P0 frozen-prefix adapter compile error:** the first server build of the observation-backed
suffix-entry candidate referenced the dense `thread` identifier before its declaration in
`SCExecutionGraphAdapter.cpp`. Ninja stopped before linking, so no candidate test or experiment was
accepted. The identifier is now derived immediately after label validation, before prefix
classification; only the build and focused unit gate are rerun.

**P0 frozen-prefix oracle timeout-handler error:** the first one-shape native-prefix loop run hit
its 20-second per-call limit, then the Python oracle raised `TypeError` because `TimeoutExpired`
returned byte strings despite `text=True`. The partial directory contains no accepted oracle
result. The handler now normalizes bytes to UTF-8 replacement text, the per-call budget is enlarged
to 120 seconds, and the rerun writes a new `r2` directory.

**P0 frozen-prefix decisive targeted result:** with the timeout handler repaired and the per-call
limit raised to 120 seconds, native `shape=011100/outcome=1112` reaches the expected assertion in
about 0.08 seconds, while frozen-prefix RVF times out before one complete execution for both one and
two workers. This is a candidate failure, not a resource-limit ambiguity: the ratio exceeds three
orders of magnitude and reproduces across worker counts. Do not broaden this candidate; first use a
minimal NA-prefix/same-value-source fixture to distinguish adapter-state explosion from transaction
livelock, then reject and revert if it does not terminate promptly.

**P0 paired-725 progress-monitor error:** an ad-hoc console progress query counted both BenchExec
`starting` rows and completed-result rows, so interim chat updates overstated progress by roughly a
factor of two. This did not affect execution or result files. Subsequent monitoring computes
`timestamped rows - starting rows`; acceptance remains based solely on one XML with exactly 725
`<run>` entries per lane.

**P0 exact-NA full-725 run (2026-07-20):** the paired baseline/candidate run is active under
`p0-regional-rvf-paired-725-exact-na-20260720a`. A name-filtered `docker ps` check initially appeared
empty because the launcher uses unnamed `docker run --rm` containers; process inspection confirmed
both Docker/BenchExec lanes and their task processes remain live. Do not classify this as a stopped
run. The current run keeps its original 8+8 concurrency. Future full-725 launches default to 24+24
workers on disjoint CPU sets, with the unchanged 120-second/12-GB per-task limits and 300-GB
per-lane container ceilings; acceptance still requires exactly 725 result rows in both lanes.

**P0 exact-NA full-725 acceptance-tool errors:** the first post-run shell probe used Bash-style
array/glob handling through the server's default remote shell and consequently passed empty paths
to `bzcat`/`zipinfo`; it is invalid evidence. A POSIX `find`-based rerun proves exactly one archive
and one result XML per lane, with 725 logs and 725 `<run>` rows each. The first strict analyzer run
then rejected a compile-error task whose command retained its original `/sv-benchmarks/c/...`
source rather than a rewritten `/experiments/...-rewrite/c/...` source. Generalize identification
to the shared `/c/<task>.{c,i}` suffix and rerun the full set; do not skip the task.

**P0 exact-NA full-725 decision:** strict analysis accepts 725 XML/log rows per lane with zero
status differences and zero semantic-summary differences across 465 common-terminal pairs. All 51
regional tasks have `rvf-loads-reduced=0`; 50 tasks publish 377 exact NA constraints but no durable
RVF attempt/reduction. Regional common-terminal CPU changes 9.736 -> 21.434 seconds (+120.16%) with
neutral summed peak RSS (+0.0012%). Reject and remove exact-NA continuation. Restore the conservative
non-atomic reset/revocation frontier and validate it; do not interpret timeout-limited search-counter
declines as benefit. Full report: `p0-exact-na-full-725-report-20260720.md`.

**P0 exact-NA rollback gate:** the synchronized authoritative server build succeeds. Adapter and
transaction tests pass 12/12; `sc-rvf-regional-na-load` and `sc-rvf-regional-loop` pass 2/2. The
adapter again rejects non-atomic memory events, and the outer NA handlers reset a not-yet-open frame
or revoke an active transaction before native replay. Do not spend another 725-task run on this
rejected configuration; the prior valid 51-task zero-activation paired result already measures it.

**P0 closed-prefix census r1 infrastructure rejection:** the first 51-task launcher requested 48
parallel tasks capped at 12 GB each but gave the container only 300 GiB. BenchExec correctly rejected
the launch before running any task because 48 x 12 GB exceeds the cgroup allowance. The r1 directory
contains no accepted result. The versioned launcher now uses 36 tasks and a 500-GiB container
(432-GB declared task budget), and retries only in a fresh r2 directory. Future launch preflight must
check `threads * per-task-memory <= container-memory` explicitly rather than relying on BenchExec.

**P0 closed-prefix census decision:** authoritative r2 has exactly 51 XML rows and 51 logs. It
observes 63,899 native-only mergeable loads across 37 tasks, but zero supported ordinary-read
closed-prefix checks, zero admissions, zero RVF attempts, and zero reductions. The measured
post-frontier opportunities are special read kinds outside the current RVF proof. Reject and remove
the candidate without a paired resource run; retain the launcher and
`p0-closed-prefix-reentry-report-20260720.md` as negative evidence.

**P0 special-read causal audit:** r3 and r4 each validate exactly 51 XML rows and logs. Every one of
the 63,899 nominal mergeable loads (188,498 sources, 37 tasks) is CAS/lock; ordinary, pure-special,
and FAI buckets are zero. Refining CAS sources by exact HB view eliminates every class: zero loads,
sources, and tasks remain. Stop regional P0 for this workload. Same-valued unlock sources are not
RVF-equivalent because they create different synchronizes-with/HB histories. Remove all temporary
classification counters; retain the report and remote roots.

**P1 cumulative-725 launch error:** the first invocation referenced the server `/data3/...`
launcher path in a local shell instead of through `ssh`; it exited 127 before touching the server or
creating a result directory. The valid launch must explicitly execute the synchronized script on
`server@frp-arm.com` and pass all existing cardinality/cgroup checks.

**P1 cumulative-725 NUMA launch error:** valid remote run `r1` started the 24-worker baseline, but
BenchExec rejected candidate cores `24-47` because they cross the boundary of this host's two
28-core NUMA regions and expose asymmetric topology. The partial single-lane run was stopped and is
invalid. Preserve its directory, switch to disjoint within-node sets `0-23` and `28-51`, and add a
preflight that checks cardinality, disjointness, and single-NUMA membership before either lane starts.

**P1 clean-candidate sync error:** the first standalone rsync omitted `-e 'ssh -p 36722'` and used
the workstation SSH configuration's unrelated port 9322, which refused the connection. Only an
empty remote source directory was created; configuration/build never started. Retry with the server
port explicit in both rsync and ssh.

**P1 clean-candidate configure error:** standalone CMake used generic `BUILD_TESTING=ON`, while
GenMC gates unit targets with its own `BUILD_TESTS=ON`. Configuration succeeded but Ninja rejected
the nonexistent `unit_tests` target before compiling. Reconfigure the same source with
`-DBUILD_TESTS=ON`; no binary/result from the first configuration is evidence.

**P1 clean-candidate test scope:** clean stable-based commit `839ac8b9` builds successfully and
passes 162/162 unit/property tests plus the SC, TSO, PSO, recursive-CAAT and online-mutation CTests.
The aggregate CTest command reports 168/173 because five legacy driver/relinche targets reference
scripts, generated traces, or relinche input files absent from commit `53c667d5` itself; these are
unavailable-fixture failures, not accepted pass evidence and not candidate diagnostics. Formal
full-725 validation therefore still compares the clean binary directly against stable.

**P1 cumulative r2 validity:** r2 is complete (725 XML/log rows per lane), but its frozen candidate
contains older research-branch RVF/statistics code in addition to P1. The apparent search mismatches
come from stable not emitting the new counters, while candidate does; one `fib_safe-7` regression
and its resources cannot be attributed to P1. Preserve r2 as a contaminated-candidate diagnostic,
not a promotion gate. Rebuild exactly the P1 patch on stable and use that binary in r3.

**P1 clean cumulative r3 decision:** the strict clean stable-based run has 725 XML/log rows in each
lane and zero launcher failures. One threshold status improves (`fib_safe-7`, `TIMEOUT (true)` to
`true`) with identical 51,480 complete executions; 438 common-solved tasks have zero semantic or
execution-count differences. Their CPU geometric-mean ratio is 0.96532 and RSS ratio is 0.99645,
but all-task CPU rises 0.43%. The decisive swapped-NUMA 31-task cohort remains 31 OOM in both lanes
and takes 15.0% more candidate CPU at the same 12-GB cap. EventDeps alone reproduces +8.19%; inline
revisit alone is only +1.40% and is below the continuation threshold. Reject stable promotion of
the P1 five-item bundle and stop further engineering-only layout ablations. Full report:
`optimization-analysis/core-direction-review-20260718/p1-clean-full-725-report-20260720.md`.

**P1 standalone build infrastructure errors:** the first standalone EventDeps rsync targeted a
two-level remote directory that did not yet exist; rsync exited before copying. The corrected
procedure creates the exact directory first. Its first test-enabled CMake configure then failed
while FetchContent downloaded googletest from GitHub (HTTP/2 receive error). No candidate result
used that build. A fresh `build-release` with `BUILD_TESTS=OFF` compiled the production binary;
semantic test evidence remains the already completed cumulative GCC/ASan/differential gates.

### 2026-07-20 Deagle exact RVF lifecycle completion

- [x] Encode exactly resolved `pthread_join` create/finish/join lifecycle edges.
- [x] Pass the complete server C++ unit suite and a real two-join C oracle.
- [x] Reject and preserve zero-task r2 (missing Docker namespace capability).
- [x] Reject and preserve zero-task r3 (unsupported overlay directory mode).
- [x] Reject and preserve zero-task r4 (read-only workspace directory mode).
- [x] Run the 283-task exact SC-RVF regression with validated BenchExec setup.
- [x] Compare terminal classifications, witnesses, time, and memory with r1/control.

### 2026-07-20 Deagle SC-RVF direct-backend gate

- [x] Audit P0/P1/P2 and reject repetition of already exhausted candidates.
- [x] Define direct-backend hypothesis, support criteria, and falsification gate.
- [x] Implement diagnostic exhaustive error-constrained SC-RVF enumeration.
- [x] Pass generated finite and lifecycle verdict oracles without native safe shortcuts.
- [x] Run a server panel; reject before expansion because both resources regress.
- [x] Remove diagnostic code/CTest and retain only negative evidence on dev.

### 2026-07-20 P1 retained-state attribution

- [x] Audit graph/history/worklist ownership and existing counters.
- [x] Reject per-object byte estimates as incomplete for shared/dynamic storage.
- [x] Profile actual allocation stacks on a representative OOM task.
- [x] Identify eager quadratic CAT fixed-point bottoms as the dominant owner; no approximate
  component counters are needed for this allocation stack.
- [x] Implement exact implicit-empty relations and empty-operand algebra short circuits.
- [x] Pass focused relation tests, CAAT property tests, and the GCC 13 production build.
- [x] Complete the 31-task paired OOM cohort and evaluate terminal/time/RSS gates.
- [x] Reject and remove implicit-empty relations: 31/31 OOM remains and CPU rises 30.9%.

### 2026-07-20 P1 cross-family allocation attribution

- [x] Profile real allocation stacks for one Weaver, one LibVSync, and another Goblint OOM task.
- [x] Compare dominant owners and peak live bytes across the three families.
- [x] Prototype compact race-causality retention under `--disable-race-detection`.
- [x] Pass focused unit/property gates; skip sanitizer after the resource gate rejects the candidate.
- [x] Run representative and formal OOM31 paired resource experiments.
- [x] Reject and remove: 31/31 OOM remains and CPU rises 32.3% at the same 12-GB cap.

### 2026-07-20 P1 spin-loop PHI admission

- [x] Attribute four LibVSync OOMs to monotonically growing active polling loops rather than
  scheduler-retained work.
- [x] Implement the fail-closed candidate: permit PHI constants only from outside the loop;
  continue rejecting every loop-carried constant.
- [x] Pass positive preheader-seed and negative backedge-constant oracles.
- [x] Pass existing spin/saver/liveness coverage and one focused ASan+UBSan run.
- [x] Run a clean four-task stable/candidate paired resource gate.
- [x] Pass the joint terminal/time/RSS gate on OOM31 and the full 725 paired run.
- [x] Record the retain decision and remove all non-retained diagnostics.

**Spin-PHI sync infrastructure error:** the first multi-file `rsync` omitted `--relative`, so files
were flattened into the remote source root. Cleanup also removed the root `CMakeLists.txt`, causing
CMake regeneration to stop before compilation. Restore that file from the local authoritative
tree, resync with `rsync -R`, verify paths/hashes, and never accept the failed build as evidence.

**Spin-PHI first oracle-design failure:** the initial positive fixture returned the final polling
load, which the optimizer proves is zero on loop exit; its PHI disappeared and the run correctly
reported zero spin blocks. Preserve the failed run as a test-design diagnostic, change the fixture
to return the last nonzero body value through an observable atomic store, and rerun before judging
the candidate.

**Spin-PHI oracle-output corrections:** the pass lowers the generated spin end to an internal
assumption, so the positive runtime oracle is `0 complete / nonzero blocked`, not the diagnostic
`spin-loop-blocks` counter.  The negative assertion is printed as `Error: Safety violation!`, not
`Assertion violation`.  Both initial expectations failed before being corrected; neither indicates
a candidate semantic failure.

**Spin-PHI sanitizer invocation error:** the first ASan+UBSan command tried to execute the new test
script without an executable bit and stopped with permission denied before GenMC ran.  Invoke it
through `bash`; the single meaningful sanitizer run then passes both oracles.

**Spin-PHI ad-hoc log extraction error:** an initial post-four-task remote command allowed the local
zsh to expand remote archive globs, so it found no logs.  The XML/cardinality result was unaffected;
replace the probe with Python `zipfile` over explicit remote paths and use that for all accepted log
audits.

### Next P1 candidate after spin-PHI promotion

- [x] Inspect transformed IR for the two remaining LibVSync OOMs without modifying the validated
  candidate.
- [x] Identify the shared boundary: dynamic outside-loop PHI seed from ticket `atomicrmw`, with a
  polling-load backedge.
- [x] Add a non-foldable dynamic-seed control-flow oracle that must continue reaching its assertion.
- [ ] After committing the constant-seed optimization, prototype arbitrary outside-incoming
  admission as a separate dev change.
- [ ] Add dynamic-seed positive and finite/backedge negative oracles before any resource run.
- [ ] Require simultaneous terminal/time/RSS improvement on the same 4/OOM31/725 funnel.
