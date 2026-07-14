# Phase 3 progress: incremental and online CAAT

This append-only record tracks each Phase 3 substage required by
`doc/cat/phase-3-plan.md`.

## Phase 3.0: research, specification, and baseline

- Starting commit: `ed56a3cef374bdda5b2c2f8d6fd322a259dcd8ff`.
- Pre-check: re-read `doc/development.md`, `doc/cat/PROJECT_CONSTRAINTS.md`, the
  Phase 2 plan/report, `task_plan.md`, and `notes.md`; branch was `genmc-caat`,
  local and remote matched, and the worktree was clean.
- Research inputs: local CAAT and Kater papers; current Dat3M source at
  `a7e3e4843359dde3a0e29500a821030e2433e316`; CAAT author publication pages
  through July 2026; OOPSLA 2023 static memory-model analysis; OOPSLA 2026
  RAT-CAT-SAT; DRed/DRed-C, Differential Dataflow, DBSP and dynamic reachability
  literature.
- Reuse decision: retain GenMC's normalized IR, packed values, adapter, checker
  and Phase 2 oracle. Port Dat3M's MIT-licensed timestamp/delta/listener design
  idiomatically to dependency-free C++23. Use CAAT's insertion theorem and
  Kater's new-cycle argument. Do not import a Java/SMT/dataflow runtime.
- Important negative evidence: CAAT itself leaves deletion and non-monotonic
  difference open. Dat3M rejects a dynamic difference RHS and documents that
  difference in recursion is unsupported. RAT-CAT-SAT checks properties of
  memory models and is not an online program-execution backend.
- Contract: freeze positive insertion propagation, exact checkpoint rollback,
  stable graph identity, observable offline fallback, proof-gated pruning and
  predicate-by-predicate oracle comparison. No production code changes in 3.0.
- Error encountered: the first local CAAT PDF lookup assumed the paper was at
  the paper-library root; `find` located it under the nested consistency-checking
  directory. A shallow Dat3M clone did not expose historical blame and GitHub's
  unauthenticated commit API returned HTTP 403; current source semantics and
  published revisions are recorded, while unsupported authorship/date claims
  are deliberately omitted.
- Verification: the focused CAT/CAAT CTest selection passed 72/72 serially,
  including CLI, SC/TSO/PSO differential tests, recursive integration, unit,
  property and adapter tests. `fast-driver` passed in 143.01 seconds during a
  wider CTest run. The checked-in broad result tables still contain 576 Phase 1
  rows and 864 Phase 2 rows; Phase 2 classifications are 864/864 `match`.
- Wider-suite boundary: a parallel 130-test CTest run passed all completed CAT
  tests and `fast-driver`, but `run-parallel` failed because the configured
  repository script `scripts/run-parallel.sh` is absent. The long existing
  `randomize-driver` test was stopped after the other tests completed. A
  parallel focused run also exposed a pre-existing cross-process temporary-file
  collision in `CatEvaluatorTest.cpp`: its process-local counter produces the
  same fixture name in concurrently launched GoogleTest processes. The same
  focused selection passes 72/72 with `-j1`; Phase 3.1 will make fixture names
  process-unique before relying on parallel unit execution.
- Gap to Phase 3 after this substage: all production work 3.1--3.7 remains. The
  next target is extensible packed values and a standalone incremental state;
  GenMC integration cannot begin before its oracle equivalence is demonstrated.
- Delivery: committed as `28f63c6` (`docs(cat): plan incremental CAAT
  integration`) and pushed successfully to `origin/genmc-caat`.

## Phase 3.1: extensible values and standalone incremental state

- Starting commit: `679f206361a5a9a5971ecdf0054cc1390ed33cd6`.
- Pre-check: re-read `doc/development.md`, project constraints and the complete
  Phase 3 plan; branch was `genmc-caat`, local and remote matched, and the
  worktree was clean.
- Reuse survey: retained Phase 2 `CaatEvaluator` as the sole initialization and
  fallback oracle, `BaseValues` as the owned primitive snapshot, normalized
  model/analysis as borrowed immutable metadata, and existing packed words.
  Dat3M's mutable predicate hierarchy remains a design reference; no Java code,
  runtime, or dependency was copied.
- Contract: grow packed universes without changing existing memberships; add a
  worker-local incremental state that atomically publishes an exact Phase 2
  initialization/reinitialization; do not add delta propagation or GenMC
  checker integration yet. Fix the process-colliding CAAT fixture names exposed
  by Phase 3.0 so parallel tests are valid evidence.
- Implementation: `EventSet::grow` extends zero-initialized packed storage.
  `Relation::grow` preserves row offsets within a 64-event block and repacks
  every live row when the stride grows. `IncrementalCaatEvaluator` borrows the
  immutable model/analysis, owns its base snapshot and current fixed point, and
  records initialization/offline counters. Its only publication path computes
  a complete temporary Phase 2 result before replacing state.
- Test infrastructure fix: evaluator/model fixture paths now include the
  process ID as well as the atomic counter. This prevents separately launched
  GoogleTest processes from overwriting `...-0.cat` during parallel CTest.
- Verification: the normal RelWithDebInfo build passed; the complete unit
  executable passed 125/125; parallel focused CAT/CAAT/CLI/integration testing
  passed 77/77. Unit and RapidCheck tests cover growth through 0/1/63/64/65/130
  events, random old/new universes, relation row repacking, exact offline
  initialization and atomic fallback reinitialization.
- Sanitizers: a fresh Debug build with AddressSanitizer and
  UndefinedBehaviorSanitizer passed all five growth/incremental focused tests,
  including the randomized property. Only pre-existing generated-checker and
  driver warnings were emitted during that build.
- Error encountered: the first build used the later `toPairs` helper name and
  compared violation/error structs that intentionally have no equality
  operator. Tests now use the existing `toReference` oracle and compare exact
  predicate values plus consistency/violation/error counts. No production
  behavior was changed to accommodate the test.
- Gap to Phase 3: initialization and domain growth are complete, but every
  update after initialization still requires an offline evaluation. Phase 3.2
  must add positive insertion deltas for the complete normalized operator
  surface and compare every intermediate predicate with a fresh oracle.
- Delivery: committed as `33c6bfe` (`feat(cat): bootstrap incremental CAAT
  state`) and pushed successfully to `origin/genmc-caat`.

## Phase 3.2: insertion delta propagation

- Starting commit: `33c6bfe80c369dff7c8d1ff237fc5dfbdabed39e`.
- Pre-check: re-read repository development rules, project constraints and the
  complete Phase 3 plan; branch was `genmc-caat`, local and remote matched, and
  the worktree was clean.
- Reuse survey: reused Phase 2 values and operator semantics, normalized
  dependency edges, exact violations, and the Phase 3.1 transactional state.
  The scheduling follows CAAT's insertion worklist and Dat3M's predicate
  hierarchy concept, but is implemented independently in C++23. Full operator
  recomputation followed by strict-growth publication is deliberately chosen
  before operator-specific micro-optimizations.
- Contract: accept a complete snapshot only when the universe grows or stays
  fixed and every old base fact remains present; propagate affected positive
  equations to quiescence without invoking `CaatEvaluator`; reject shrinkage,
  deletion, missing/ill-typed bases, evaluation-error states, and any model
  containing difference before committing mutable values.
- Implementation: `tryInsert` grows a temporary copy of every predicate,
  validates each built-in/graph base by semantic subset, and drives a
  dependency worklist. An affected equation is fully evaluated for correctness,
  but dependents are queued only if its value strictly grows. Optional and
  reflexive-transitive closure receive explicit domain-growth tasks because
  their new identity edges are not announced by an operand delta. Online
  witnesses for empty, irreflexive and acyclic checks are recomputed from the
  maintained fixed point. Cumulative counters distinguish insertion updates,
  rejected updates, operation evaluations, changes and queue pushes.
- Transaction rule: all validation and propagation use local copies. A
  deletion or non-monotone derived result returns `RequiresRebuild` with a
  deterministic reason while preserving the previous event count, bases,
  predicate values and verdict. Difference is conservatively excluded even
  when Phase 2 semi-positivity accepts it.
- Verification: RelWithDebInfo built cleanly; the complete unit executable
  passed 129/129. Parallel CAT/CAAT/CLI/integration testing passed 81/81.
  ASan+UBSan passed all 6 incremental unit/property tests.
- Oracle coverage: one fixture exercises alias, identity restriction,
  domain/range, inverse, optional, product, composition, union/intersection,
  transitive/reflexive-transitive closure and recursive union/composition over
  three staged snapshots including universe growth. Separate tests cover mutual
  set recursion, duplicate support, transactional deletion/shrink rejection and
  difference fallback. RapidCheck compares every predicate and verdict with a
  fresh Phase 2 recursive reachability evaluation after every random edge.
- Error/gap analysis: domain growth initially depended only on operand changes,
  which would miss the implicit new diagonal of `r?` and `r*`; this was caught
  during design review before the complete-surface test and fixed with explicit
  tasks. Phase 3.2 still performs whole-operator recomputation and has no
  checkpoints. Phase 3.3 must add exact push/pop rollback and current violation
  state across recursive facts with multiple supports.
- Lint boundary: Homebrew LLVM `clang-tidy` could not parse the translation
  unit because its invocation could not locate the C++ standard header
  `<cstddef>`. The independently runnable formatter and compiler checks remain
  authoritative for this substage. The actionable anonymous-namespace
  redundancy warnings emitted before the fatal parse error were fixed.
- Verification command correction: the first focused rerun used the stale
  path `RelWithDebInfo/tests/unit/GenMCUnitTests`; this build emits
  `RelWithDebInfo/bin/unit_tests`. The corrected command is recorded by the
  successful result below and no source change was needed.
- Delivery: committed as `283bfa6` (`feat(cat): propagate incremental CAAT
  insertions`) and pushed successfully to `origin/genmc-caat`.

## Phase 3.3: checkpoints, rollback, and violation state

- Starting commit: `283bfa68ebb64e144e19e17398b848c96f8312dc`.
- Pre-check: re-read `doc/development.md`, project constraints and the complete
  Phase 3 plan; branch was `genmc-caat`, local and remote matched, and the
  worktree was clean.
- Reuse survey: retained Phase 2 value/result copies as the exact state format,
  Phase 3.2 transactional insertion, deterministic violation reconstruction,
  and the stateless Phase 2 `Reasoner`. Dat3M timestamp/backtrack remains the
  future memory optimization reference; this substage chose exact C++ value
  snapshots because multiple derivation support makes a premature fact-level
  undo trail easy to make unsound.
- Contract: create evaluator-local opaque checkpoint handles; restore universe,
  primitive bases, every derived predicate, evaluation counters, violations and
  witnesses atomically; retain the restored branch point, invalidate all later
  handles, and reject stale, foreign, or previous-initialization handles without
  state mutation.
- Implementation: every checkpoint owns a complete immutable-by-convention
  snapshot. Process-wide atomic IDs prevent a handle from one worker aliasing a
  handle in another. Rollback publishes the selected snapshot and erases only
  descendants. Reinitialization begins a new epoch by clearing all retained
  snapshots. Lifetime statistics count created, successful and rejected
  rollback operations.
- Correctness coverage: deterministic tests cross the packed 63-to-65 event
  boundary, restore recursive multi-edge reachability and cycle witnesses,
  exercise repeated restoration, and reject descendant, foreign and old-epoch
  handles. A restored cyclic state is passed directly to `Reasoner` and produces
  a current non-empty explanation. RapidCheck generates insertion/checkpoint/
  rollback trees and compares all values, verdicts and violation counts with a
  fresh Phase 2 evaluation at every node.
- Verification: RelWithDebInfo built cleanly; all incremental tests passed 9/9
  and the complete unit executable passed 132/132. `clang-format --dry-run
  --Werror` and `git diff --check` passed.
- Environment boundaries: the first sanitizer command referenced a nonexistent
  old `DebugSan` directory. A fresh `/tmp/genmc-caat-asan` configuration was
  created with ASan+UBSan, but both the full executable and unit target failed
  only at link time because this machine could not find `libhwloc`; all changed
  sources compiled with the sanitizer flags. Homebrew `clang-tidy` again failed
  at the missing `<cstddef>` toolchain boundary; its actionable warning about a
  non-const namespace atomic was removed by placing the counter inside the ID
  allocator.
- Gap to Phase 3: rollback is exact but each retained checkpoint costs a full
  state copy, O(checkpoints x predicate state), rather than Dat3M-style
  timestamps or a support-aware undo trail. Phase 3.4 must introduce stable
  graph synchronization, retain only useful semantic predecessors, bound
  checkpoint memory, and expose rebuild/classification reasons.
- Delivery: committed as `6d88425` (`feat(cat): add exact CAAT rollback
  checkpoints`) and pushed successfully to `origin/genmc-caat`.

## Phase 3.4: stable graph synchronization

- Starting commit: `6d88425e91043cae276c798b2291e46673f70bee`.
- Pre-check: re-read repository rules, project constraints and the complete
  Phase 3 plan; branch and remote were synchronized and the worktree was clean.
- Reuse/contract: reuse immutable `GraphAdapter` snapshots and `EventPos` rather
  than retaining graph pointers. Real events use `(thread,index)` keys, virtual
  initial writes use `SAddr`; IDs never move or get reused. Every primitive,
  including model-unreferenced bases, participates in mutation classification.
- Current implementation: `StableGraphAdapter` remaps dense snapshots into a
  persistent universe with inactive holes represented by `_`; the offline and
  incremental evaluators accept adapter-supplied `_`/`id`. `GraphSynchronizer`
  classifies unchanged, insert, rollback, rollback-plus-insert and rebuild,
  searches retained semantic predecessors newest-first, and bounds full-state
  checkpoints with explicit eviction.
- Evidence: 13/13 stable/synchronizer/incremental focused tests pass under both
  RelWithDebInfo and ASan+UBSan. The complete unit executable passed 136/136;
  parallel CAT/CAAT/CLI/integration selection passed 98/98.
  Real `removeLast` and address insertion prove virtual-IW reordering stability,
  inactive IDs, bounded history, rollback-plus-insert and mixed label-category
  rebuild. Edge-only `rf` replacement and `co` reorder rebuild; `cutToStamp`
  (which internally exercises `removeAfter`) restores a prefix, and same-position
  revisit reactivates the reserved ID. Existing full-surface graph fixtures
  cover RMW and create/join mappings. Transition and eviction counters are
  exposed for Phase 3.5 diagnostics.
- Errors fixed: testing exposed that normalized dead-code elimination allowed an
  unreferenced base deletion to evade `tryInsert`; classification now compares
  every primitive. A cut also resets GenMC stamps, confirming that stamps cannot
  serve as stable identity. The sanitizer build initially inherited a stale
  missing `hwloc` path; configuring `HWLOC` empty correctly exercised all new
  code. `clang-tidy` remains unable to find `<cstddef>` in this Homebrew/Xcode
  setup; formatter, compiler, unit, integration and sanitizers pass.
- Gap to Phase 3: graph synchronization is a tested standalone component but is
  not yet called by `BasicCATChecker`. Phase 3.5 must give each checker worker an
  incremental evaluator/synchronizer, retain offline fallback for non-monotone
  models, and prove real SC/TSO/PSO executions exercise insert/rollback paths.

## Phase 3.5: GenMC checker integration and early pruning

- Starting commit: `196d370e18621111cb5e8886ea26ba613bf4f043`.
- Pre-check: re-read `doc/development.md`, project constraints, the complete
  Phase 3 plan and `task_plan.md`; branch was `genmc-caat`, local and remote
  matched, and the worktree was clean.
- Reuse survey: retained `BasicCATChecker` as the worker ownership boundary,
  `ConsistencyChecker::create` as the only production factory, GenMC's existing
  candidate-prefix query points, Phase 2 `CaatEvaluator`/`Reasoner` as fallback
  and explanation oracles, and the Phase 3.4 synchronizer without duplicating
  graph mutation logic. No new runtime dependency was introduced.
- Contract correction: the initial acceptance text requested offline fallback
  for a non-monotone checked predicate. Inspection of `GenMCDriver` confirmed
  that `isConsistent` rejects candidate prefixes, not only complete executions.
  A difference RHS can grow later and repair such a prefix, so offline
  evaluation followed by rejection would be unsound. The plan now requires the
  existing source-located pre-execution rejection; only positive normalized
  models receive online state and early rejection.
- Implementation: each recursive/forward-reference `BasicCATChecker` owns one
  `IncrementalCaatEvaluator` and `GraphSynchronizer`; immutable model/analysis
  remain in shared `Config`. Every consistency query materializes the real
  graph, synchronizes it and reads the current incremental result. Explanations
  use the stable universe. Phase 1 non-recursive models retain their original
  evaluator. `--cat-stats` prints process-serialized per-worker initialize,
  unchanged, insert, rollback, rollback-insert, rebuild and eviction counters;
  it is rejected without `--model-file` and documented in the CLI manual.
- Pruning evidence: `empty F` over a positive recursive model accepts the empty
  graph, rejects after the first fence and remains rejected after another
  insertion with no rebuild. Recursive and acyclic difference fixtures remain
  rejected before exploration, proving the non-monotone boundary cannot prune
  a prefix. This is exactly the positive-growth proof recorded in Section 4.
- Real-program evidence: recursive SC/TSO/PSO all execute insertion and retained
  ancestor rollback-plus-insert on
  `correct/data-structures/fcombiner-async/variants/main0.c`. The observed
  single-worker counters were SC `insert=20, rollback-insert=176`, TSO
  `insert=1, rollback-insert=3`, and PSO `insert=1, rollback-insert=3`; the
  integration test requires both counters to remain nonzero for every model.
  The existing eight-program matrix compares SC/TSO/PSO baseline versus
  recursive models with one and two workers, including exact execution counts,
  errors and warnings.
- Verification: RelWithDebInfo built successfully; the complete unit executable
  passed 138/138. Parallel CAT/CAAT/config/CLI/integration testing passed
  103/103, including `cli-model-file` and the strengthened recursive
  differential suite. ASan+UBSan passed 15/15 checker, incremental, rollback,
  synchronizer and stable-adapter tests. `clang-format` and `git diff --check`
  passed.
- Errors encountered: libc++ on this machine exposes a `<syncstream>` header but
  no `std::osyncstream`; statistics output now builds the whole record first and
  emits it under one process-wide mutex. The first focused CTest regex did not
  select GoogleTest-discovered unit names; the corrected exact-name run passed
  4/4 before the complete 138-test run. Existing generated-checker override
  warnings and the recorded standalone `clang-tidy` standard-header failure are
  unchanged.
- Gap to Phase 3: the production hot path is active and semantically matched,
  but rebuild-heavy `rf`/`co` replacement and repeated revisit workloads need
  systematic oracle cross-checking and deterministic mismatch dumps. These are
  the sole production focus of Phase 3.6; broad performance claims wait for 3.7.
- Delivery: committed as `d890211` (`feat(cat): integrate online CAAT
  checking`) and pushed successfully to `origin/genmc-caat`.

## Phase 3.6: mutation/fallback hardening and differential stress

- Starting commit: `d890211eca1d3f478ebc85eb81061a32494edc7d`.
- Pre-check: re-read `doc/development.md`, project constraints, the complete
  Phase 3 plan and `task_plan.md`; branch was `genmc-caat`, local and remote
  matched, and the worktree was clean.
- Reuse survey: reused the Phase 2 `CaatEvaluator` as the only semantic oracle,
  the worker-local synchronizer as the query/classification boundary, existing
  CLI validation and CTest integration, and repository programs that already
  exercise RMW, join, dynamic allocation and lock-free data structures. No
  second relation implementation or external testing runtime was added.
- Contract: an explicitly enabled oracle recomputes the current stable universe
  and every normalized predicate from scratch after the selected graph
  transition. It compares errors, every optional value, checks and witnesses.
  The first mismatch emits a deterministic query number, named transition,
  universe size and first differing predicate/diagnostic before an invariant
  failure. Normal verification pays no oracle cost.
- Implementation: `IncrementalCaatEvaluator::offlineOracleMismatch()` owns the
  complete result comparison. `GraphSynchronizer` accepts an oracle interval,
  invokes it after initialize/unchanged/insert/rollback/rollback-insert/rebuild,
  and counts checks. `--cat-oracle` selects interval one independently of build
  mode; the option requires `--model-file`, is documented, and is visible in
  `--help`. `--cat-stats` now includes the oracle count.
- Mutation stress: `online-mutation-stress.sh` covers recursive SC/TSO/PSO,
  one/two workers, RMWFix, W+JW lifecycle, dynamic MS queue, dynamic Treiber
  stack, asynchronous flat combining and a malloc ordering error. Two fixed WFR
  seeds add reproducible randomized branch orders. The final 39 rows performed
  5,396 fresh Phase 2 comparisons with zero mismatch, unsupported status or
  stale rollback.
- Verification: complete unit testing passed 138/138; parallel CAT/CAAT/config/
  CLI/integration testing passed 104/104. ASan+UBSan passed the 15 checker,
  incremental property, checkpoint, synchronizer and stable-adapter tests. The
  focused oracle transition test checks all five transitions in its fixture,
  and real recursive SC/TSO/PSO integration runs with `--cat-oracle`.
- Errors/limits: the full ASan executable aborts in existing LLVM interpreter
  code (`value_ptr.hpp:137`, `Execution.cpp:1538`) before CAT checking. A plain
  Debug executable with `--validate-exec-graphs` also aborts before the checker,
  so the independent build-mode-neutral oracle option replaced that unusable
  hook. TSO-hosted fcombiner with two workers hits the existing replay-schedule
  assertion at `Scheduler.cpp:85`; fcombiner remains in every single-worker
  model row, while the other five fixtures provide two-worker coverage.
- Gap to Phase 3: correctness hardening is complete, but final closure still
  requires the frozen 864-row broad corpus, aggregate transition/performance/RSS
  measurements, documentation audit and local/remote equality. Those are the
  Phase 3.7 deliverables.
- Delivery: committed as `ab35de0` (`test(cat): harden online mutation
  checking`) and pushed successfully to `origin/genmc-caat`.

## Phase 3.7: broad validation, performance, and closure

- Starting commit: `ab35de08543ee70cacf68483521001ea5fba27b9`.
- Pre-check: re-read `doc/development.md`, project constraints, the complete
  Phase 3 plan and `task_plan.md`; branch was `genmc-caat`, local and remote
  matched, and the worktree was clean.
- Reuse survey: reused the exact Phase 2 288-program manifest and signature
  normalizer, current CTest/property/sanitizer suites, `/usr/bin/time -lp`, and
  an independently built detached `196d370` binary as the from-scratch
  baseline. No synthetic workload replaced the frozen real-program corpus.
- Broad evidence: 288 programs times recursive SC/TSO/PSO produced 864/864
  matching rows, zero mismatch and zero unsupported. Per-row counters record
  128,801 insertions, 620,887 rollback-insertions, 364,023 safe rebuilds and
  31,219,757 predicate evaluations. The complete 865-line TSV includes source
  hashes, exact arguments, semantic signatures and transition/worklist counts.
- Performance evidence: 54 end-to-end measurements compare the detached Phase
  2 checker and current online checker over three models, three programs and
  three repetitions. All paired execution counts match. Mean time is 0.0700 s
  offline versus 0.2004 s online; maximum RSS is 54,525,952 versus 54,493,184
  bytes. The 2.86x latency regression is honestly attributed to full-state
  checkpoint copies and SC fcombiner rather than hidden as noise.
- Documentation/audit: `phase-3-report.md` records the architecture, pruning
  proof, exact unsupported boundary, broad/mutation/performance evidence,
  environment limits and requirement-by-requirement audit. `supported-cat.md`
  now describes normalized recursion, stable online identity, transition
  fallback, difference rejection, `--cat-stats` and `--cat-oracle`.
- Verification: complete unit tests passed 138/138; parallel focused tests
  passed 104/104; ASan+UBSan passed 15/15; the 39-row mutation suite completed
  5,396 oracle comparisons; the final broad suite passed 864/864. Formatter,
  shell syntax and whitespace checks passed. Final clean-tree/remote equality is
  recorded after the closure commits are pushed.
- Gap analysis: no correctness or evidence item remains for the declared
  positive normalized fragment. Full CAT, support-aware checkpoint
  optimization and fewer rebuilds are explicitly future work, not silently
  claimed Phase 3 behavior.
- Delivery: closure evidence was committed as `93a5fdb` (`test(cat): close
  online CAAT validation`) and pushed successfully to `origin/genmc-caat`.
  Immediately after that push, local `HEAD` and the remote-tracking branch both
  resolved to `93a5fdbbe4fc5158d341ee43ee6c29e592aab635` with a clean worktree.
