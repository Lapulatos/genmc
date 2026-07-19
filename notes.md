# Notes: GenMC 通用 CAT 内存模型

## Optimization 06: shared immutable dependency adjacency (2026-07-15)

- Server: only `server@frp-arm.com:36722`; the old lapulatos host is unavailable and
  excluded from all commands.
- Correctness container exited 0 after 141/141 unit/property tests.
- Mutation stress: 39 rows, 5,441 full-oracle checks, zero mismatch.
- Broad differential: 288 programs x SC/TSO/PSO = 864 pairs; 852 comparable matches,
  12 mutually unsupported, zero mismatch.
- Fresh before/after builds use the same `genmc15noble:sujie` image and Release/Ninja
  configuration. A paired 24+24 BenchExec matrix with alternating disjoint core pools
  completed 3,456 cells with 44--48 measured overlap.
- Correct cells are identical at 1,554/1,554; verdict and safe execution-count
  mismatches are both zero.
- Strict task-clustered aggregate CPU is 1.00879 [1.00416, 1.01357]; TSO is 1.02149
  [1.01273, 1.03047]. Aggregate RSS is 0.99996 [0.99976, 1.00015].
- Decision: reject and remove. The production/test diff is exactly back to HEAD.

## Optimization 07: bounded offline ring worklist (2026-07-15)

- Correctness: 141/141 tests; 39 rows / 5,441 oracle checks; 864 broad pairs with
  852 matches, 12 mutual unsupported, and zero mismatch.
- Formal matrix: 3,456 rows, 36 XML, 36 log archives, 44--48 measured task overlap.
- Correct cells are 1,554 before and after; status changes, verdict mismatches, and safe
  execution-count mismatches are all zero.
- Strict aggregate CPU is 1.00121 [0.99548, 1.00682], wall 1.00171
  [0.98162, 1.02618], and RSS 0.99999 [0.99980, 1.00019].
- Decision: reject and remove; production/test source is exactly back to HEAD.

## Optimization 08: scalar direct from-read (2026-07-15)

- Correctness gates all passed; formal matrix has 3,456 rows and no status/verdict/count
  differences.
- Aggregate CPU is 1.00835 [1.00164, 1.01510], wall 1.01984
  [1.00510, 1.03749], RSS 0.99998 [0.99984, 1.00011].
- Decision: reject and remove. Scalar target scans lose generic compose's word-parallel
  row union. Only a packed-row version is worth one separate attempt.

## Optimization 09: packed functional from-read (2026-07-15)

- Correctness: 142/142, 5,441 oracle checks, 864 broad pairs, zero mismatch.
- Formal: 3,456 rows, 36 XML/log archives; correct cells 1,554 -> 1,556, with zero
  common verdict or safe-count mismatch.
- Aggregate CPU is 0.99996 [0.99557, 1.00444]; SC 0.98671
  [0.97922, 0.99420], TSO 0.99609 [0.99011, 1.00193], PSO 1.00590
  [0.99503, 1.01740].
- Decision: reject; do not select SC post hoc, and stop fr micro-specialization.

## Continuous optimization: P1 production-path profile (2026-07-15)

- Purpose: choose Optimization 05 from current production evidence rather than another
  speculative prototype.
- Full input is the established 96-task SV-COMP sample under SC, TSO, and PSO: 288 runs.
- Each model is one BenchExec run set with `-N48`; worker slots refill immediately.
  GenMC remains `--nthreads=1`, one CPU, 4 GB, and 60 seconds per task.
- Remote container: `caat-p1-profile-sujie`, image `genmc15noble:sujie`, cpuset
  `0-23,28-51`; output root `/data3/sujie/experiments/caat-optimization/p1-profile`.
- The already running host `bse-conc-simpor.xml` process belongs to another user and
  was not changed. Host load was high, so structural counters are authoritative while
  this one-pass profile's wall-clock nanoseconds are diagnostic, not a before/after
  performance claim.
- SC completed 96 rows: 88 correct, 8 unknown, 0 incorrect.
- Existing 24-task evidence already shows a 4,029-event query with 49.6 ms of copy time
  and 54.8 MB snapshot-equivalent state, plus a separate workload with 3,431 rebuild
  queries and 14.6 s total synchronization time. The 96-task profile determines how
  widespread these patterns are.
- Final profile: 288 rows, 254 terminal CAT-stat logs, 34 resource-limited logs, zero
  incorrect verdicts. Across terminal logs, 243,236 primitive-changing queries split
  into 195,430 retracting and 47,806 monotone-only changes; successful online insertion
  transitions were zero. Full details are in
  `optimization-analysis/continuous/p1-profile/report.md`.
- Optimization 05 prototype moves the just-materialized `BaseValues` into evaluator
  ownership and skips the unreachable duplicate history/checkpoint for certified
  adaptive-offline epochs at or below 512 events. Local compilation passed without
  tests; server Release build passed 142/142 unit/property tests.
- Server mutation stress passed 39 rows and 5,441 full Phase 2 oracle checks with zero
  mismatch. The 288-run SV-COMP oracle gate is in progress.

## Continuous optimization: measured cost selector (2026-07-15)

- Prototype gate: 142/142 server tests; 39,879 oracle checks, zero mismatch.
- Profile24: SC/TSO each 3,415 offline + 56 online probes; PSO 430 offline + 62 online,
  only 8 probes, so PSO learned an unhelpful online preference.
- Sequential 96-task run appeared 8.8% faster but was before-then-after under changing
  shared-server load; it is diagnostic only.
- Authoritative held-out: 48 tasks x three models x five reps x two variants = 1,440
  cells. Two 24-worker queues ran simultaneously on disjoint core sets and swapped
  sockets. XML peak concurrency was 45--48.
- Held-out selector/static: wall 1.0634 [1.0293,1.1001], CPU 1.0353
  [1.0274,1.0435], RSS 0.9861; completed 566 -> 565. Rejected and removed.
- After removal, server tests returned to 141/141.

## Environment
- Repository: `/Users/sujie/Documents/Codes/C++/GenMC/genmc`
- Branch: `genmc-caat`
- Initial worktree: clean（`git status --short` 无输出）
- Initial scope: feasibility analysis only; implementation began only after the
  three-phase sequence and detailed Phase 1 plan were reviewed.

## Sources

### GenMC source
- `genmc-caat` currently points to the same commit as `master`/`origin/master`: `29b03a6` (GenMC v0.17.0). There is no branch-specific CAAT implementation yet.
- `MemoryModel.hpp` hard-codes five models: SC, TSO, RA, RC11, IMM.
- CLI model selection is an enum in `lli/main.cpp`; IMM is currently commented out in the CLI choices even though an `IMMChecker` exists.
- `ConsistencyChecker` is not only a final consistency predicate. It requires incremental and exploration-facing operations:
  - consistency for a newly added label and for a full graph;
  - coherent read-from candidates and coherence placements;
  - revisit filtering;
  - per-label model-specific view updates and prefix/hb views;
  - model-specific error/warning checks and dependency-tracking choice.
- All five checker implementations are generated by Kater (`CAUTION: ... generated automatically by Kater`). Their `.cpp` files total 19,172 lines, showing that the generated backend specializes heavily rather than interpreting a small generic relation algebra.
- GenMC/TruSt requires a model to provide `corder`, `consistent_M`, and `IsErroneous_M`, and requires well-formedness/no-thin-air, corder-prefix-closedness, and extensibility.

### Local papers
- CAT semantics supports a broad language: sets/relations, composition/closure/inverse/difference, mutually recursive bindings, recursive functions, pattern matching, procedures, `forall`, flags, scopes, `classes`, `linearisations`, and `with ... from ...` candidate-extension enumeration.
- A CAT file is therefore more than a list of `acyclic` constraints; it may construct additional execution components such as coherence orders.
- CAAT handles recursive derived predicates by least fixed points and targets normalized, domain-independent, semi-positive consistency models.
- Semi-positivity requires the right operand of normalized difference `p1 \ p2` to be a base predicate. Cutting can handle other cases by eagerly encoding definitions, but may encode a large part of a model.
- CAAT explicitly says incrementality/online integration is difficult: additions can reuse prior fixed points, removals may require restart; non-monotonic differences and derivation tracking complicate online solving.
- The CAAT implementation is integrated into Dartagnan/Dat3M and uses JavaSMT; this is likely the source of the mistaken “herd is Java” premise.
- The 2024 GenMC paper reports Kater can fully automate porting SC, RA, RC11, and IMM using an extended DSL, including acyclicity, coherence, error/warning inclusion checks, views, and static assertions.

### herdtools7
- Official repository identifies herd7 as a generic simulator for `.cat` models.
- Repository language statistics show OCaml as the primary language (55.9%), not Java.
- herd is best treated as semantic reference/oracle plus model/litmus regression corpus. Direct source translation would inherit a broad interpreter and candidate-enumeration architecture that differs from GenMC's online DPOR architecture.
- Direct shallow clone failed due to a transient LibreSSL/GitHub TLS error; official GitHub pages and local CAT semantics paper were used instead.
- CAAT's official artifact is available on Zenodo, but is 5.4 GB; downloading it is unnecessary for this feasibility pass.

## Synthesized Findings
- A final/full-graph CAT interpreter is technically feasible and can validate executions produced by GenMC.
- A generic backend that preserves GenMC's completeness and performance is feasible only for a declared CAT subset with model admissibility checks and exploration metadata (`corder`, coherence policy, race/hb policy).
- “Any syntactically valid CAT file” is not a sound initial contract: CAT expressiveness exceeds both CAAT's efficient fragment and GenMC's model requirements.
- Recommended architecture: CAT frontend -> typed/normalized relational IR -> static admissibility analysis -> generic full-graph evaluator -> optional generated/specialized incremental checker -> GenMC adapter.
- herd should be used differentially: same litmus/model inputs, compare allowed outcomes/execution counts. Kater should be reused conceptually and, if available, as the closest implementation baseline for GenMC integration.
- Clarified MVP: load SC/TSO/PSO `.cat` files from the command line. These models fit a much smaller relational fragment and do not require full CAT or general CAAT-style recursion.
- Likely MVP operators: event sets/tests (`R`, `W`, `[S]`), `po/rf/co/fr/loc/ext`, union, intersection/difference, composition, inverse, optional, transitive/reflexive closure, non-recursive `let`, and `acyclic` (plus inexpensive support for `irreflexive`/`empty`).
- SC and TSO already have built-in generated checkers and CLI entries, so they are ideal differential baselines. PSO is the first genuinely new-model test and exposes whether the design is model-driven rather than merely dispatching to existing checkers.
- For a correctness-first generic adapter, candidate generation can conservatively enumerate all same-location `rf/co` choices and use CAT consistency to filter. Performance-specific coherent-store/placement and revisit pruning can follow after equivalence with built-in SC/TSO is established.
- Final sequencing decision:
  1. CAT file support: CLI, frontend, shared typed IR, full-graph evaluator, SC/TSO/PSO.
  2. Offline CAAT: normalized/stratified models, least fixed points, semi-positivity checks, violation explanations; recomputation is allowed.
  3. Incremental/online CAAT: event/edge deltas, push/pop/backtrack, deletion handling, trail-valid explanations, and early consistency pruning inside GenMC exploration.
- The full-graph evaluator from stage 1 remains the reference oracle for stages 2 and 3. Every incremental state should be periodically cross-checked against a from-scratch evaluation in debug/tests.
- Phase 1 planning constraints are recorded in `doc/cat/PROJECT_CONSTRAINTS.md`; every substage must first read GenMC's `doc/development.md`, then re-read the constraints and `doc/cat/phase-1-plan.md` before implementation.
- Phase 1 implementation is split into substages 1.0-1.9. Each coherent substage ends with verification, a plan gap analysis in `doc/cat/progress/phase-1.md`, an atomic Conventional Commit, and a push to `origin/genmc-caat`.
- Repository rules take precedence: comments use `/* ... */`, code follows the checked-in `.clang-format`/`.clang-tidy`, class declaration order and naming follow `doc/development.md`, and public docs remain under `doc/manual/`.
- Runtime implementation choice: C++23, matching `genmc_lib`. herdtools7 is an OCaml/CeCILL-B behavioral oracle rather than a source-copy dependency. Kater-generated SC/TSO code remains an internal differential/performance reference.
- Official herd `x86tso.cat` uses includes and helper constructs (e.g. `WW`, `RM`, `WR`, `MA`, `AM`) in addition to core relation algebra. Phase 1 therefore freezes selected model fixtures and their exact required surface in substage 1.0 rather than claiming compatibility from a simplified parser.
- Kater's public artifact is a roughly 3.6 GB Docker image plus instructions rather than a small source dependency, so Phase 1.0 must inventory it before deciding whether any implementation can be reused directly.
- Phase 1.0 build baseline succeeds on Apple arm64 with LLVM 20.1.7 after adding the Homebrew hwloc library directory to the executable linker flags; this is an environment workaround, not a source change.
- `fast-driver.sh` completed with exit 0 and reported 50.65s. Fixed memory-model smoke counts are SB SC=3/TSO=4 and LB+ctrl SC=3/TSO=3; WWR+2WR has 0 complete and 8 blocked executions in the driver.
- Reusing the already cloned RapidCheck source via `FETCHCONTENT_SOURCE_DIR_RAPIDCHECK` avoids its unused Catch submodule fetch. The complete unit target built; 40/40 focused unit/property tests passed, and CTest `fast-driver` passed 1/1 in 76.72s.
- herdtools7's current CAT frontend is `lib/modelLexer.mll` and `lib/modelParser.mly`, both explicitly CeCILL-B. The lexer recognizes `#`, `//`, and nested-style `(* *)` comments and the parser confirms relational precedence and the much broader unsupported language surface.
- The Phase 1 contract uses clean-room SC/TSO/PSO files under `models/cat/`. The models add GenMC lifecycle primitives `tc`/`tj`, RMW atomicity, coherence, and explicit TSO/PSO preserved-program-order equations without copying herd models.
- Phase 1 aliases `rfi/rfe/coi/coe/fri/fre/po-loc` are synthesized from `int`/`ext`/`loc`; `mo` is a deprecated alias for `co`. Relative includes resolve only beside the including file, so results never depend on a system herd installation.
- Phase 1.1 stores a canonical `std::filesystem::path` in `Config`, rejects explicit built-in model combinations and repeated `--model-file` occurrences, and disables the built-in memory-model detector for CAT input. A deliberate pre-execution diagnostic prevents accidental RC11 fallback until the parser/checker path is connected.
- LLVM's command-line scalar option accepts repeated occurrences and keeps the final value in this configuration; preserving `getNumOccurrences()` in `Config` is therefore required for the project's stable duplicate-option contract.
- The Phase 1.1 regression baseline is 46/46 unit tests, CLI model-file 1/1, and fast-driver 1/1 in 76.83 seconds. Local `clang-tidy` remains environment-blocked because its libc++ invocation cannot find the C++23 `<format>` header despite a valid compilation database.
- Phase 1.2 adds a dependency-free C++23 frontend under `genmc/genmc/CAT/`. It validates UTF-8, supports every frozen lexical/operator form, builds an owned syntax AST with canonical source spans, expands include fragments in place, and reports active-stack include cycles with path/line/column sites.
- Parser precedence is implemented as left-folded recursive-descent levels. Binary `*` is distinguished from postfix closure by whether the following token can start a primary expression; Phase 1.3 will confirm the resulting operand types.
- Top-level syntax recovery only resumes at explicit statement keywords, allowing multiple independent diagnostics without treating expression debris as a new statement. Known full-CAT keywords and complement/insertion/literal punctuation receive `unsupported`, while genuinely unknown bytes remain `lex` errors.
- Repeated parsing of the bundled PSO model measured about 20,631 files/s over 300 parses with 6.75 MiB peak RSS for the complete GoogleTest process. The frontend is not currently a performance bottleneck relative to verification.
- Phase 1.3 lowers syntax directly into an immutable topologically ordered `ModelIR`; every operand ID is smaller than its user, identifiers are fully resolved, and Config retains one `shared_ptr<const ModelIR>` for worker sharing.
- The compiler uses sequential definition visibility, rejects forward/undefined/duplicate/reserved names, types every set/relation operator and check, and publishes no partial model after errors. `empty` accepts either type; `acyclic` and `irreflexive` require relations.
- `M` lowers to `R | W`; internal/external aliases lower to generic intersections; `mo` shares the `co` node and emits a deprecation note. This prevents evaluator-side filename/model switches.
- Exact platform-independent summaries are checked in for SC (17 nodes), TSO (43), and PSO (45). RapidCheck additionally verifies source provenance and topological operand IDs for randomly selected valid expressions.
- Phase 1.4 represents an `N`-event set in `ceil(N/64)` words and a relation in `N * ceil(N/64)` contiguous row words. Composition unions complete rhs rows; transitive closure uses bitset Warshall.
- The pure evaluator owns one memo table per call, evaluates bindings and checks, and caches failed primitive attempts as well as values. It returns errors separately from violations, with event/pair/diagonal/closed-cycle witnesses carrying original check spans.
- RapidCheck compares set/relation Boolean algebra, composition, and closure against direct membership/`std::set` oracles. A generic synthetic-primitive test evaluates the SC model without checking its filename or model header.
- Sparse/dense relation work over 64, 128, 256, and 512 events took about 2.09 ms for the recorded unit benchmark; packed input storage was 85 KiB and full test-process peak RSS was about 6.67 MiB.
- Phase 1.5 maps one immutable `ExecutionGraph` snapshot to real-label IDs in insertion order, excludes `EmptyLabel`, and expands `InitLabel` by sorted address. Unclassified concrete labels remain in `_` and participate in `po/int/ext`; emitted NA reads/writes remain in `R/W`.
- GenMC's single address-polymorphic `InitLabel` cannot be one ordinary CAT event: composing `rf^-1 ; co` would otherwise create cross-address `fr` edges. The adapter expands it into one virtual `IW` per sorted tracked address and maps each virtual ID back to `InitLabel + SAddr`.
- `rf`, stored `co`, adjacent split-label RMWs, create/start pointers, and finish/join pointers are translated without mutating the graph. `fr` is constructed and debug-checked as `rf^-1 ; co`.
- No graph generation counter exists. The adapter therefore documents that every mutation invalidates it; `structurallyCurrent()` catches label changes but deliberately cannot certify edge-only stability.
- The 64/128/256/512-event adapter benchmark took about 4.93 ms and materialized about 511 KiB of packed primitive storage; the complete GoogleTest process peaked at about 7.47 MiB RSS.
- Phase 1.6 selects `CATChecker` whenever Config owns a typed CAT model; neither the path nor model header is inspected. Each consistency query rebuilds primitives and evaluates the typed DAG from scratch.
- `CATChecker` reuses the generated SC checker's host views, prefix calculation, language-level errors, and warnings, but overrides model-specific `rf/co` candidate pruning with complete current-graph enumeration and keeps all revisits. RMW writes retain their required rf-adjacent placement.
- SC differential coverage compares SB, LB+ctrl, WWR+2WR blocking/warning behavior, and a safety violation. CAT and built-in SC match exit status, complete-execution count, and verdict/error marker; a two-worker CAT run also returns SB=3.
- Ten isolated SB invocations measured 0.48 s and roughly 50.2 MiB peak RSS for both built-in SC and generic CAT SC on this machine; process/compiler startup dominates this smoke benchmark.
- Phase 1.7 stores a `HostProfile` enum in syntax and immutable IR. The optional declaration is an exact leading block comment, `(* @genmc host-profile sc|tso *)`; herd treats it as trivia, while GenMC rejects duplicates, unknown values, late declarations, and declarations in includes.
- The checker is now `BasicCATChecker<HostChecker>` with explicit SC/TSO instantiations. Generated host checkers supply causal/prefix/race/warning infrastructure only; generic conservative rf/co enumeration and from-scratch CAT consistency remain unchanged.
- TSO differential coverage matches built-in GenMC on seven C/LLVM fixtures: SB=4, LB+ctrl=3, MP=3, po-loc=3, RMWFix=4, WWR+2WR=0 with warning, and one safety-error execution.
- herdtools7 7.56+03 was installed into the existing opam 4.14 switch after a dry-run showed no dependency upgrades/removals. Official `x86tso.cat` reports X86 SB `Sometimes 1 3` over four states and the MP anomaly `Never 0 3` over three states.
- The herd oracle is intentionally assembly-aligned. GenMC's seven-case differential uses transformed C/LLVM events, so equal raw state counts between those C tests and herd assembly tests are not asserted.
- Ten warmed SB invocations measured 0.48 s for both built-in TSO and CAT TSO; peak RSS was 52,625,408 versus 52,641,792 bytes. Process/compiler startup dominates this smoke measurement.
- Phase 1.8 adds no production C++ and no PSO checker/profile enum. `pso.cat` declares the already justified TSO host and relaxes only the generic CAT preserved-program-order equation for cross-location W→W.
- The model-only `WW+RR.c` proof holds program/flags constant: SC and TSO are safe with three complete executions; PSO reaches `Ry=1, Rx=0`, reports a safety violation, and stops after two complete executions. Two PSO workers reproduce the violation/count.
- PSO retains TSO results for explicit same-location order (`po-loc`=3), RMW atomicity (`RMWFix`=4), and SC fences (`SB+scfs`=3).
- herd 7.56 official `x86tso.cat` classifies MP as `Never 0 3`; official `mips.cat`, whose source explicitly selects a PSO ppo, classifies the same plain-R/W event structure as `Sometimes 1 3` when architecture checking alone is disabled.
- Ten warmed WW+RR runs measured TSO CAT at 0.50 s/52,576,256-byte peak RSS and PSO CAT at 0.48 s/52,609,024 bytes. The measurement does not show a material model-load/evaluation penalty.
- Phase 1.9's soundness audit identified a non-monotonicity boundary: a CAT relation difference reachable from a check can shrink as an execution prefix grows. Config now rejects that online use while preserving difference in the parser, typed IR, and from-scratch evaluator.
- The same audit identified that Relinche requests a refinement-specific coherence predicate supplied by generated host checkers. `--model-file` now rejects Relinche collection/checking rather than silently applying SC/TSO host-model semantics to an arbitrary CAT model.
- A clean Phase1Audit build passed 100/100 unit/property tests, 4/4 focused CAT integrations, both herd oracles, and fast-driver in 76.90 seconds. `clang-tidy` remains environment-blocked because its Apple toolchain invocation cannot resolve the standard C++ header `cstddef`; normal LLVM 20.1.7 compilation succeeds.
- The final Phase 1 report freezes the supported subset and carries forward three principal inputs: Phase 2 recursive/fixed-point normalization and explanations, a future arbitrary-CAT Relinche contract, and Phase 3 incremental graph/evaluator state with proof-backed pruning.
- The post-Phase-1 broad-validation survey found 1,013 C tests in the repository: 265 litmus C files, 414 correct variant programs, and 120 wrong variants. The frozen first corpus uses 288 distinct sources across litmus, infrastructure, data structures, safety failures, races, and memory failures; SC and TSO each compare built-in and CAT paths.
- Broad-validation oracle policy compares status plus semantic summary, checks SC counts against repository expectations where available, and requires isolated/herd/manual analysis for every mismatch. Aggregate equality alone is not treated as proof of no false positives or negatives.
- Broad validation now has 288 valid programs, 576 matching SC/TSO rows, zero
  mismatch, and zero unsupported rows. The former `psc-base-notin-ar` exclusion
  was a fixture bug: it declared `__VERIFIER_assume` instead of including
  `<genmc.h>`.
- Fixed root causes: coherence-warning candidates, dynamic-address `Init` read-from candidates, and missing program-order context around TSO/PSO create/join edges.
- Phase 2 research froze Dat3M revision `a7e3e4843359dde3a0e29500a821030e2433e316`; the repository is MIT licensed. Its CAAT backend has a predicate dependency hierarchy, SCC handling via recursive graph placeholders, delta worklist propagation, and a derivation-based reasoner.
- CAAT canonical semantics evaluates signed-dependency SCC strata in topological order and takes a least fixed point within each stratum. A negative dependency inside an SCC is non-stratifiable and must be rejected.
- CAAT semi-positivity is checked after one-operator normalization: the right operand of every difference must be a base predicate. Dat3M's reasoner also requires this when emitting a negative base literal.
- Phase 2 deliberately implements offline full-graph CAAT in C++23. JavaSMT integration, automatic cutting into an outer SMT formula, backtracking, and online delta propagation are not silently imported.
- Phase 2.1 keeps the acyclic Phase 1 `ModelIR` intact and adds a separate cyclic `NormalizedModel`: named IDs are reserved before lowering, operands may point forward, and every derived predicate contains one operator. This avoids weakening Phase 1 evaluator invariants.
- Phase 2.2 signed dependencies mark only the normalized difference RHS negative. Tarjan SCCs are reversed into dependency-first strata; negative intra-SCC edges are rejected as non-stratifiable, and every derived difference RHS must be cut or rejected.
- Domain-independence over recursive equations needs the greatest Boolean solution of the syntactic rules: starting false incorrectly rejects productive guarded recursion such as `ob = base | ob;ob`.
- Phase 2.3 evaluates each SCC from bottom with a dependency worklist and no arbitrary iteration cap. Random reachability fixed points match an independently coded naive Kleene recurrence.
- Phase 2.4 reconstructs deterministic shortest-known derivations from a completed
  fixed point. Positive recursive membership is expanded to base facts; semi-positive
  difference emits an explicit negative base literal. Direct relation/set replay tests
  reproduce empty, irreflexive, and acyclic violations without consulting the reasoner.
- `--explain-cat` is deliberately opt-in. Phase 1 remains the verdict oracle for
  non-recursive models, while the normalized evaluator is run only for rejected
  candidates whose explanation was requested; default startup and output remain unchanged.
- Phase 2.5 selects the normalized backend for declared recursion and acyclic
  forward references. Ordinary SC/TSO/PSO files retain the Phase 1 evaluator;
  checker selection depends on immutable IR metadata rather than filenames.
- Full recomputation does not make negative literals prefix-monotone. The CLI
  therefore rejects difference when normalized CAAT is required, while the
  offline evaluator/reasoner continue to support semi-positive difference.
- Recursive SC/TSO/PSO clean-room fixtures replace final order acyclicity with
  recursive reachability. Across eight real programs and one/two workers, 48
  recursive results match the corresponding Phase 1 status and semantic summary.
- Phase 2 broad validation freezes the same 288 programs as Phase 1. The three
  recursive model pairs produce 864 rows: all 864 are exact matches, with zero
  mismatch and zero unsupported rows. Correct/wrong coverage is 696/168 rows.
- Dat3M revision `a7e3e4843359dde3a0e29500a821030e2433e316` builds successfully
  with Java 17/Maven: 699 production and 159 test sources compile and the jar is
  produced in 60 seconds with tests skipped. A direct verdict comparison is not
  claimed because GenMC and Dat3M do not expose an identical C/LLVM base-event
  mapping for the frozen corpus.
- The Phase 2 fixed-point benchmark over 32/64/128-event chains records 2.974 ms,
  442 operation evaluations, 439 value changes, 442 worklist pushes, and 2,816
  bytes of final packed relations. Twenty-run SB batches show ordinary/recursive
  totals of SC 1.00/0.96 s, TSO 0.97/0.97 s, and PSO 0.97/0.97 s, with peak RSS
  between 52.63 and 52.69 MB.
- Phase 3 research confirms CAAT's exact online boundary: insertion can seed
  iteration from the old least fixed point, while deletion and non-monotonic
  difference are explicitly unresolved. Its delta worklist is the normative
  insertion algorithm.
- Current Dat3M CAAT source has `addAndPropagate`, timestamped derivables,
  `backtrackTo(time)`, recursive predicate propagation and constraint
  listeners. It still rejects a dynamic difference RHS and documents that
  difference in recursion is unsupported. This is a reusable MIT-licensed
  design baseline, not proof of a complete GenMC-style online integration.
- The combined BenchExec HTML uses resolved task paths as row identity. The same YAML
  therefore appears as separate rows such as `sc/sv-benchmarks/c/...` and
  `new-server/cbmc/sv-benchmarks/c/...`, even though their task links already resolve
  to the shared offline snapshot. Normalize the embedded row IDs after generation,
  merge only non-overlapping method result cells, and leave raw XML/logs unchanged.
- Verification after normalization: 725 rows, 725 unique canonical IDs, 5,118
  non-empty result cells. Coverage is 96 rows for each of the eight GenMC-family
  SC/TSO/PSO columns and 725 rows for each TruSt-family/Deagle/CBMC column; 96 rows
  contain all 14 methods. The generator verified 8,015 offline task/log links, and
  HTTP requests for the combined table and a representative YAML both returned 200.
- Kater's incremental consistency argument starts a cycle search at the newly
  added event because a graph known consistent before the addition can only
  acquire a cycle containing that event. Kater restricts cached relations when
  their edges may later disappear; Phase 3 applies the same conservative rule.
- Thomas Haas, Roland Meyer and collaborators published OOPSLA 2023 static
  memory-model analysis and OOPSLA 2026 RAT-CAT-SAT. RAT-CAT-SAT supports the
  rational CAT fragment for checking properties of memory models, not DPOR
  program executions, so it is a future analysis reference rather than the
  Phase 3 runtime backend.
- DRed/DRed-C, Differential Dataflow and DBSP provide general recursive
  incremental view maintenance, including deletions in broader settings. Their
  runtime/data model is disproportionate for GenMC; Phase 3 implements the
  positive insertion and stack rollback case in C++23 and rebuilds on unknown
  mutations.
- Phase 3 uses stable event keys because Phase 1's snapshot-local dense IDs put
  sorted virtual initial writes after real labels. Adding a real label or new
  address can otherwise shift an existing virtual ID and be mistaken for an
  edge deletion.
- The formal 96-task parallel-scaling experiment completed all five SC runs at
  `nthreads=1` and `nthreads=2` (2,880 rows). There are no duplicate rows, wrong
  verdicts, or safe-task execution-count mismatches. At two workers the solved
  counts are GenMC 471/480, CAT 417/480, and CAAT 445/480; the corresponding
  one-worker counts are 480, 445, and 450. Common-solved geometric-mean wall
  speedups are 0.949, 1.006, and 0.969, respectively, so this corpus currently
  shows no useful two-worker speedup.
- All 16 SC two-worker `ABORTED` rows are unsafe tasks. Their logs identify
  GenMC parallel-runtime internal checks in `Scheduler.cpp:85` and
  `ExecutionState.cpp:167`, not CAT consistency disagreements: GenMC has 9,
  CAT 3, and CAAT 4 aborted rows. Treat these as unknown/coverage loss, not as
  false positives or false negatives. The experiment continues at four and
  eight workers to measure how this failure rate scales.
- The five SC four-worker runs add 1,440 rows with no wrong verdict and no
  safe-task execution-count mismatch. Completion is GenMC 460/480, CAT
  419/480, and CAAT 434/480. Relative to one worker, common-solved wall-time
  geometric speedups are 1.000, 0.991, and 1.046; CPU-efficiency ratios are
  0.783, 0.832, and 0.830. Thus only CAAT shows a small wall-time gain, bought
  with substantially more total CPU.
- SC four-worker failures consist of GenMC/CAT/CAAT ABORTED counts 17/14/12
  plus segmentation-fault counts 3/2/4. Every such row is unsafe. All nine
  segmentation faults occur on `pthread-demo-datarace-3`; nearby repeats log
  the same `ExecutionState.cpp:167 getAllocAccess` internal check, indicating
  a parallel GenMC runtime race rather than a CAT verdict disagreement.
- The complete SC scaling matrix contains 60 XML files and 5,760 task-runs.
  It has zero wrong verdicts, duplicate rows, or safe-task execution-count
  mismatches. At eight workers, GenMC/CAT/CAAT solve 456/414/428 of 480;
  ABORTED counts are 20/18/19 and other failures 4/4/3, all on unsafe tasks.
  Common-solved wall-speedup geometric means over one worker are
  1.015/0.952/1.035, while CPU-efficiency ratios fall to 0.733/0.766/0.753.
  Therefore eight workers provide at most a 3.5% wall gain on CAAT, slow CAT
  by 4.8%, consume substantially more CPU, and reduce unsafe-task coverage.
- The five-run TSO one-worker baseline contains 15 XML files and 1,440 rows,
  with zero wrong verdicts or duplicate rows. GenMC solves 480/480, CAT
  410/480, and CAAT 445/480. Every unsolved baseline row is a 60-second
  timeout; there are no ABORTED, segmentation-fault, or OOM rows.
- Adding the five TSO two-worker runs yields 2,880 rows with zero wrong
  verdicts and zero safe-task execution-count mismatches. GenMC/CAT/CAAT solve
  471/402/444 of 480, with ABORTED counts 9/3/1. Common-solved geometric wall
  speedups are 0.932/1.038/0.943 and CPU-efficiency ratios 0.811/0.921/0.826;
  only CAT shows a small wall gain, while all methods use more total CPU.
- The five TSO four-worker runs bring the checked subset to 4,320 rows, still
  with zero wrong verdicts and zero safe-task count mismatches. GenMC/CAT/CAAT
  solve 462/406/432 of 480. Four-worker wall-speedup geometric means over one
  worker are 1.013/1.003/1.005, effectively flat, while CPU-efficiency ratios
  fall to 0.798/0.840/0.809. Parallel internal failures increase to 18/14/18
  (ABORTED plus other), all treated as unknown rather than wrong verdicts.
- The complete TSO scaling matrix contains 60 XML files and 5,760 rows with
  zero wrong verdicts, duplicate rows, or safe-task count mismatches. At eight
  workers GenMC/CAT/CAAT solve 454/411/424 of 480. Common-solved wall-speedup
  geometric means are 1.010/0.933/0.985, while CPU-efficiency ratios are
  0.731/0.748/0.736. Thus eight workers are effectively flat for GenMC, slow
  CAT by 6.7% and CAAT by 1.5%, use substantially more total CPU, and increase
  parallel-runtime unknown results.
- The completed PSO one-worker checkpoint contains 10 XML files and 960 task-runs.
  CAT solves 410/480 and CAAT solves 395/480; all 155 unsolved rows are timeouts.
  There are zero duplicate rows, wrong verdicts, or safe-task exploration-count
  mismatches. A first pull made before the runset `.done` marker observed CAAT r05
  as a live XML; it was quarantined and replaced with the completed `.xml.bz2`.
- PSO t1/t2/t4 contains 30 XML files and 2,880 rows with zero wrong verdicts,
  duplicates, or safe-task exploration-count mismatches. At four workers CAT/CAAT
  solve 411/389 of 480. Common-solved wall-speedups are 0.983/0.969 and CPU-efficiency
  ratios are 0.833/0.822, so both methods are slower than one worker while consuming
  more aggregate CPU. Parallel failures remain unknown coverage loss, not verdict errors.
- The final formal matrix has 160 XML files and 15,360 rows. Strict validation finds
  zero missing/unexpected cells, duplicates, false alarms, missed bugs, safe-task count
  mismatches, or missing safe counts. The CAAT oracle has 12 XML/288 rows, 216 rows
  exposing oracle counts, 2,444 checks, and zero verdict errors.
- At eight workers, common-solved wall speedups are SC GenMC/CAT/CAAT
  1.015/0.952/1.035, TSO 1.010/0.933/0.985, and PSO CAT/CAAT 0.910/0.919.
  CPU-efficiency ratios are only 0.731--0.766. Task-level bootstrap intervals mostly
  cross 1.0; the robust direction is increased CPU cost and reduced unsafe coverage,
  not scalable wall-time improvement.
- Final offline HTML contains 12 memory-model/thread pages, fixes each page to r01 so
  every method appears once, and verifies 4,224 task/log links. Full five repetitions
  remain in the statistical analysis.
# CAT / CAAT optimization study (2026-07-15)

- Corrected local corpus root: `/Users/sujie/Documents/Papers/ConcurrencyPaper/paper`.
- Corpus inventory: 368 PDF files (about 1.2 GB).
- Formal matrix input: 15,360 rows, 96 tasks, five repetitions, SC/TSO/PSO, 1/2/4/8 workers; oracle input: 288 runs and 2,444 full-recomputation checks.
- Single-worker paired-task geometric ratios after taking each task's five-run median:
  - SC CAT/GenMC wall 1.520 [1.242, 1.918], CAAT/GenMC 1.260 [1.114, 1.462], CAAT/CAT 0.864 [0.765, 0.953].
  - TSO CAT/GenMC wall 1.316 [1.112, 1.620], CAAT/GenMC 1.288 [1.111, 1.541], CAAT/CAT 0.833 [0.730, 0.925].
  - PSO CAAT/CAT wall 1.091 [1.037, 1.159].
- Interpretation: SC/TSO CAAT currently benefits from exact-model certified pruning and adaptive offline evaluation; PSO/custom models deliberately remain on the generic online path.
- Main next optimizations: PSO certificate/fast path, ExecutionGraph mutation journal, incremental violation frontier, and a measured online/offline cost selector.
- Prior negative results constrain design: global CoW bitsets regressed suite time 8.2%; generic semi-naive union/composition regressed it 25.8%. Any delta maintenance must be density/cost/SCC gated.
- Literature synthesis: CAAT identifies non-monotonic difference/deletion as the hard part and proposes incremental on-demand cutting; Kater demonstrates generated sparse consistency checks; static memory-model analysis supports program-specific pruning; GenMC performance work supports cache/scratch allocation avoidance.
- Deliverable: `experiment-analysis/parallel-scaling/final/analysis/optimization-analysis-report.md`.

## Continuous campaign: Optimization 01

- Implemented a separate `certifiedAdaptiveOffline()` fingerprint. Exact recursive PSO
  may select offline evaluation below the existing 512-event limit, but remains
  ineligible for TSO candidate pruning.
- Local correctness: 141/141 unit/property tests and 864/864 broad differential pairs.
- Server matrix: 96 tasks × five repetitions before/after, 480 cells each. Correctly
  solved cells 395 -> 400; wrong verdicts 0 -> 0; common solved verdict mismatch 0;
  common safe execution-count mismatch 0.
- Server paired task result: wall ratio 0.937 [0.901, 0.968], CPU ratio 0.938, RSS ratio
  1.000. Decision: retain.
- Full record: `optimization-analysis/continuous/pso-adaptive-offline/report.md`.

## Continuous campaign: Optimization 02 instrumentation

- Added `--cat-stats`-only exact primitive delta counters: added/removed facts, changed
  primitive count, monotone changed queries, and retracting changed queries. Ordinary
  execution has no delta scan or counter overhead.
- fcombiner changed-query split:
  - SC: 12 monotone, 22 retracting; materialize 5.72 ms / sync 56.76 ms.
  - TSO: 1 monotone, 2 retracting; materialize 0.52 ms / sync 4.98 ms.
  - PSO: 1 monotone, 6 retracting; materialize 0.91 ms / sync 8.80 ms.
- Decision: do not implement an append-only-only fast path. It would cover only 1/7 PSO
  changed queries and has an overall gain ceiling well below the full materialization
  share. The mutation journal prototype must also support cut/retraction and rf/co
  replacement, or it should be rejected before server-wide benchmarking.
# Continuous optimization: semantic snapshot cache (2026-07-15)

- Wide paired gate: 96 tasks x SC/TSO/PSO x 3 reps x before/after = 1,728 cells.
- Correct cells: 777 before, 778 after; zero common-solved verdict mismatches and zero
  safe-task execution-count mismatches.
- Median-of-three model-task wall ratio: overall 0.9904, SC 0.9994, TSO 0.9901,
  PSO 0.9819. Overall task-bootstrap 95% CI [0.9828, 0.9979].
- RSS ratio overall 1.0066, 95% CI [1.0024, 1.0116]; median wall ratio 1.0000.
- Decision: remove cache. The small tail gain does not cover eight full snapshots,
  exact semantic-key maintenance, or the statistically visible memory regression.
- Keep direct materialization and opt-in transition/dirty-density instrumentation.
- Future server experiments: default 32 BenchExec task workers, optionally 48 after
  checking host load; paired before/after experiments must use identical worker counts.

# Continuous optimization: violation frontier (2026-07-15)

- Server Docker tests: 142/142 with prototype; 141/141 after clean removal.
- Oracle: 288 runs, 237 completed records, 39,879 full query comparisons, 0 mismatch.
- V1 formal: 1,728 cells; wall ratio 1.1104 [1.0909, 1.1305], CPU 1.1213,
  RSS 0.9811, completed 774 -> 771. Rejected.
- V2 dense-delta fallback: one-repetition wall ratio 1.0449. Stopped early and removed.
- Root cause: `irreflexive reach` is already a cheap diagonal scan; per-edge cycle
  reachability is worse than one DFS for dense coherence deltas.
- A configured `-N 32` produced XML-measured peak overlap 25--29, not 16. Very short
  tasks finish during serialized preparation/output. Future runs use `-N 48` and record
  both configured workers and measured overlap.

# Continuous optimization: rejected checker delegation and revised boundary

- The certified-host-consistency prototype passed 142/142 unit/property tests,
  5,441 mutation-oracle checks, and 864 broad SC/TSO/PSO differential pairs with zero
  mismatch. Its formal matrix was stopped after one paired SC repetition.
- Rejection is methodological, not a correctness or performance conclusion: exact
  SC/TSO queries were delegated to GenMC's built-in generated checker, so the measured
  effect would be a fixed-model bypass rather than a CAT/CAAT optimization.
- Production and test files were restored byte-for-byte to HEAD; the partial formal
  result is quarantined and must not support a performance claim. All own server
  containers were removed.
- Binding rule for subsequent work: generated checkers may supply algorithmic ideas
  such as sparse DFS and on-demand traversal, but the implementation must remain in the
  generic CAT/CAAT IR, analysis, evaluator, and incremental state; activation must be
  structural rather than model-name based, and explanation/oracle completeness remains
  mandatory.

## SV-COMP fixed-seed coverage repair (2026-07-16)

- GenMC's documented `__VERIFIER_nondet_int()` behavior is deterministic per thread
  across executions. `lli/Runtime/Interpreter.h` fixes the RNG seed to 1995 and
  `Interpreter::resetThread()` reseeds every execution; `--schedule-seed` controls
  scheduling only and is not an input seed.
- Accepted experiment contract: exhaustive concurrency exploration is claimed only for
  the deterministic per-thread seed-1995 pseudo-input policy. A completed `true` is not
  an unconditional SV-COMP safety proof over every nondeterministic input.
- Fixed-seed source adaptation maps bool, uint, long, ulong, long long, char, and unsigned
  char calls onto the native `__VERIFIER_nondet_int()` stream with a C cast. It adds no
  helper thread, memory access, synchronization edge, or execution choice.
- Static rewrite audit covers all 570 tasks previously marked data-nondeterministic with
  zero unsupported names and zero rewrite exceptions.
- Mixed server probe v1: 16 tasks produced 10 true, 1 false, 1 timeout, 3 compilation
  failures, and 1 segmentation fault. Follow-up fixes avoided `genmc.h` where a source
  defines its own `bool`, selected the task's preprocessed `.i` for driver-model sources,
  and rewrote preprocessed `malloc` to GenMC's modeled allocation API.
- Focused server probe v4: both pthread-driver-races tasks compile and verify true in
  0.182/0.165 seconds. The large spi LDV task moved from compilation failure to timeout;
  the cafe_ccic preprocessed task still segfaults after successful compilation.
- The rejected in-program bool-choice prototype remains evidence only: injected helper
  threads caused manual typedef conflicts and a GenMC thread-create internal failure.

## SV-COMP per-tool BenchExec integration findings (2026-07-16)

- SV-COMP 2026 requires a BenchExec tool-info module under `benchexec/tools`; its
  documented responsibilities are executable discovery, version reporting, command
  construction, and output-to-verdict translation.
- BenchExec explicitly states that tool-info code and commands it starts run in a
  separate container and file changes made there are not visible to the actual run.
  Therefore `cmdline()` cannot be the place where a competition adapter materializes a
  rewritten source file. A same-run wrapper bundled in the tool archive must perform
  preprocessing and then invoke the verifier.
- `BaseTool2.Task` exposes input files, property file, and task `options`; the latter
  carries `language` and `data_model`. `ResourceLimits` exposes CPU time, wall time,
  memory, and CPU-core counts. `Run` exposes the exact command, exit code/signal,
  output, and BenchExec termination reason.
- `determine_result()` must not depend on mutable state saved by `cmdline()`: BenchExec
  guarantees only that `cmdline()` happens first, not that results are parsed
  immediately or in the same task order. Classification must use only `run` and stable
  tool-level information.
- The official rule permits feature-based behavior using standard external calls such
  as `malloc` and `pthread_create`, but forbids tuning by task name, hash, category,
  comments, or identifiers. This makes structural feature detection acceptable and
  benchmark-path lookup tables unacceptable.
- Competition C tasks are a single `.i` or `.c` source plus a property and data model.
  A tool may preprocess `.c` using the selected `-m32` or `-m64`, but witnesses must
  still refer to the original source; preserving `#line` information is necessary.
- Submission archives must run from any path and may write only the current working
  directory and `/tmp`. The per-run wrapper must use `tempfile.TemporaryDirectory`
  under one of these locations and must not rely on research-server paths or
  environment variables.
- Current research adapter warning: `optimization-analysis/svcomp2026/fair-coverage/
  genmc_svcomp_fair.py` performs `rewrite_tree()` inside `cmdline()`. It works with
  `--no-container` experiments but violates the normal BenchExec container visibility
  model and is not a competition-ready ToolInfo module.
- Durable deliverables:
  - `optimization-analysis/svcomp2026/SVCOMP_TOOLINFO_AND_RUNNER_HANDBOOK.md`
  - `optimization-analysis/svcomp2026/toolinfo-template/benchexec_toolinfo_template.py`
  - `optimization-analysis/svcomp2026/toolinfo-template/svcomp_runner_template.py`
- The handbook separately covers GenMC, CAT, CAAT, TruSt, Awamoche, Mixer, Spore,
  Deagle, and CBMC; it includes transform/rejection tables, result-priority rules,
  metric/audit formats, correctness gates, and a ten-step new-tool checklist.
- Both templates pass Python syntax compilation locally and inside the server's
  `genmc15noble:sujie` image. The archive runner's `--version` entrypoint returned 0.

## SV-COMP fixed-seed 60-second census (2026-07-16)

- The first launch exited before tasks because `run_sharded_census.sh` was mode 0644
  and invoked directly. Its log was preserved as a launch-failure record; the retry
  explicitly invoked the unchanged script through `bash`.
- Both 24-worker shards completed with exit code 0. Pulled artifacts contain two XML
  files, two complete `logfiles.zip` archives, two text summaries, and two console logs.
- Strict analysis contains 725 rows, 725 unique tasks, and zero duplicates.
- V10 categories: 408 correct, 286 error, 31 wrong. All 31 wrong are true on an
  expected-false task and are interpreted as conditional fixed-input misses; there are
  zero false-on-expected-true fixed-seed soundness failures.
- V10 statuses: true 301, false(unreach-call) 138, TIMEOUT 210, OOM 50, compilation 13,
  unsupported external 2, invalid-memory property 7, uninitialized-memory property 1,
  ABORTED 2, and segmentation fault 1.
- The 286 non-verdict rows are all BenchExec `category=error`, not explicit
  `status=unknown`. Resource failures account for 260/286 (90.9%): 210 TIMEOUT and 50
  OOM. The 50 OOM rows were all 10-second TIMEOUT rows before the longer run let their
  execution graphs grow to the 4 GB task limit.
- The 13 compile failures divide into eight condition-variable tasks lacking
  `pthread_cond_t`, three pthread TLS tasks lacking `pthread_key_t`/key APIs, one
  system/GenMC pthread typedef collision, and one malformed large LDV preprocessed
  declaration. The two unsupported externals are `pthread_mutexattr_init`; the two
  ABORTED rows are unresolved LDV globals `current_task` and `irda_debug`; the remaining
  segmentation fault is the second `cafe_ccic` LDV task.
- Seven invalid-memory and one uninitialized-memory reports are deliberately not mapped
  to `false(unreach-call)`: they violate a different property and remain fail-closed
  adapter errors.
- Relative to v9/10 seconds: 25 error rows became benchmark-consistent, 4 timeout rows
  became conditional-input misses, 50 timeouts became OOM, 210 remained timeout, and
  no previously terminal correct/wrong row changed category.
- Local result root:
  `optimization-analysis/svcomp2026/fair-coverage/evidence/
  fair-census-run-v10-fixed-seed-60s/`.

## Adapted GenMC/CAT/CAAT/Deagle 60-second comparison (2026-07-16)

- Locked unit: the same 725 C.Concurrency YAML tasks, unreach-call, ILP32, SC, one
  core/task, 4 GB/task, 60 CPU seconds. Each method uses its own adapter; Deagle is not
  fed GenMC-specific rewritten source.
- Existing Deagle 10-second XML and GenMC 60-second XML have exactly 725 overlapping
  task IDs and no task-set difference. The old Deagle result is only provisional
  evidence because the time limits differ.
- Missing 60-second methods launched concurrently: CAT N16 on cores 0--15, CAAT N12 on
  cores 16--27, Deagle N16 on cores 36--51. Total task-level concurrency is 44.
- CAAT's first core set 18--33 crossed the two 28-core NUMA regions. BenchExec refused
  it with `Asymmetric machine architecture not supported` before task execution. The
  corrected 16--27 set stays within one region and runs normally.
- Final HTML builder:
  `optimization-analysis/svcomp2026/fair-coverage/comparison-60s/
  build_four_method_html.py`. It merges GenMC's two shards into one method, normalizes
  all task IDs, extracts and renames logs, copies 725 task YAML files, requires exact
  task/log counts, calls table-generator, checks the four column names, and verifies
  every YAML/log link.

## Optimization 11: generic linear-recursion closure (2026-07-16)

- Structural boundary: only `X = R | (X ; R)` and `X = R | (R ; X)`, including
  union/composition operand order, with the same recursion-free seed expression in both
  positions. Unequal seeds, other operators, extra terms, and mutual recursion retain
  the original SCC. Matching ignores source spans but not expression algebra.
- The rewrite is generic CAT IR canonicalization to `TransitiveClosure`; it never checks
  model names, host profiles, filenames, or invokes `SCChecker`/`TSOChecker`.
- Independent property oracle constructs the least fixed point by naive Kleene
  iteration for all four syntactic orientations and random finite relations.
- First server unit run: 142/144 passed. Two reason tests showed that `R+` provenance
  treated diagonal `(x,x)` as an empty path, which is valid only for `R*`. Consistency
  values were unaffected, but explanation replay was incomplete, so performance testing
  remained blocked.
- Corrected `Reasoner` obtains a non-empty seed cycle for diagonal membership in `R+`.
  Server Release gates then passed 144/144 unit/property tests, 39 mutation rows with
  5,441 full Phase-2 oracle comparisons, and 864 broad pairs with 852 matches, 12
  mutually unsupported rows, and zero mismatch.
- Fresh before/after binaries were independently configured and built inside
  `genmc15noble:sujie`. Running either on the Ubuntu host fails because the host lacks
  image GLIBC 2.38 and GLIBCXX 3.4.31/32; both execute normally inside the image. This
  repeats a known environment boundary and does not affect measured container runs.
- Authoritative server evidence root:
  `/data3/sujie/experiments/caat-optimization/linear-recursion-closure/`. The controlled
  96-task × SC/TSO/PSO × six-repetition × before/after matrix completed with alternating
  disjoint 24-core groups: 3,456 cells, 36 XML files, 36 log archives, and measured
  overlap 44--48.
- Formal decision: retain. Aggregate after/before CPU is 0.99010 with task-bootstrap
  95% CI [0.98316, 0.99593]; wall is 0.98956 [0.98240, 0.99555]; RSS is 1.00002
  [0.99985, 1.00019]. SC/TSO/PSO CPU ratios are 0.95525/0.98328/0.98137. There are zero
  common-solved verdict or safe execution-count mismatches and 15 additional correct
  run cells after canonicalization.
- Paired mechanism profile: all 259 common logs have identical 842,811 profiled queries,
  689,082 adaptive-offline queries, and 689,269 offline evaluations. Offline time falls
  to 0.62437, synchronization time to 0.75025, and snapshot-equivalent bytes to 0.97593;
  the latter decreases on 183 tasks and increases on none. This distinguishes reduced
  evaluator work/state from candidate pruning.
- Complete local report and pulled XML/log evidence:
  `optimization-analysis/continuous/linear-recursion-closure/report.md` and `server/`.
- The verified four-file change was committed locally as `b84309a`; the index was
  populated from the frozen, formally measured server source so unfinished
  Optimization 12 changes could not enter the commit.

## Optimization 12: cycle-check closure slicing (2026-07-16)

- Finite-relation law: `acyclic(R+)`, `irreflexive(R+)`, and `acyclic(R)` are
  equivalent. The normalizer redirects only terminal `acyclic`/`irreflexive` consumers
  to the seed and removes `R+` only when no predicate observes its complete value.
- `empty`, mixed check consumers, downstream predicate use, and reflexive closure retain
  the closure. Nested terminal closure chains are removed iteratively with deterministic
  predicate/check ID remapping.
- Adding check kinds to `NormalizedModel::summary()` fixed a fail-closed certificate
  gap: candidate/adaptive certificates now distinguish identical predicate DAGs with
  different consistency axioms. A TSO coherence-kind mutation explicitly tests this.
- Server correctness gates passed: 147/147 Release unit/property tests; 39 mutation
  rows with 5,441 full Phase-2 oracle checks; 864 broad SC/TSO/PSO pairs with 852 exact
  matches, 12 mutually unsupported, and zero mismatch.
- Fresh before/after binaries were configured and built independently. The formal
  3,456-cell matrix has zero verdict/safe-count mismatch and nine additional after
  completions. Paired common logs keep query/offline-evaluation counts identical while
  reducing offline time to 0.905, sync time to 0.938, and snapshot-equivalent bytes to
  0.978.
- Decision: reject and remove under the frozen primary unit. The 82-task cross-model
  aggregate CPU ratio is 0.99584 with 95% CI [0.98957, 1.00131], whose upper bound is
  not below one. The 261 model-task sensitivity estimate 0.99345
  [0.98740, 0.99877] is recorded but not used to change the analysis unit post hoc.

## Large-program scalability literature synthesis (2026-07-16)

- Yin et al.'s EOG/SCAR work weakens the global scheduling encoding, validates abstract
  counterexamples on an event-order graph, extracts small kernel reasons, and refines by
  blocking whole infeasible families; constraint fallback supplies bounded soundness and
  completeness. The reusable GenMC analogue is a sufficient base-literal blocker learned
  from exact CAAT explanations, not a replacement with CBMC.
- Their parallel-refinement work shares refinement constraints/learned clauses across
  diverse engines. This is a better model for GenMC parallelism than duplicating concrete
  exploration: partition unresolved abstract choices and share immutable blocker snapshots.
- He's Tord work eliminates exhaustive from-read encodings, derives relevant edges on
  demand, incrementally checks event-graph cycles, and learns compact reasons. The
  consistency-preserving propagation work identifies a fragile assignment before it is
  made and propagates its opposite, explaining a core part of Deagle's large-instance
  behavior.
- Cai's prediction work uses feasible partial orders, SSA/points-to information, and
  necessary-consistent reads to avoid unnecessary dependencies. These methods support
  GenMC choice ordering and a later CEGAR abstraction, but cannot directly justify pruning
  an exhaustive safety proof.
- Current insertion map: learn at `BasicCATChecker::isConsistent`; replay/minimize through
  `Reasoner`; fire blockers in `findConsistentRf`, `findConsistentCo`, `calcCoOrderings`,
  and `calcRevisits`; use `StableGraphAdapter` identities across cuts/revisits.
- Primary proposed experiment is deliberately read-only: measure kernel size, duplication,
  subsumption, and earliest firing on 96 tasks plus a stratified TIMEOUT/OOM cohort before
  any exploration result changes.

## Independent check-kind certificate fix (2026-07-16)

- The Optimization 12 rewrite was removed, but its audit exposed an independent
  correctness issue: `NormalizedModel::summary()` omitted `Statement::CheckKind` from
  the exact closed-world model certificate.
- The separate worktree fix adds `empty`/`acyclic`/`irreflexive` to the fingerprint and
  proves that changing only TSO's coherence check kind disables both specialized
  certifications.
- Fresh server verification passed 144/144 tests, 39 mutation rows, 5,441 oracle
  comparisons, and 864 broad pairs (852 match, 12 mutually unsupported, zero mismatch).
  Evidence is under
  `optimization-analysis/continuous/certificate-check-kind/server/`; the exited
  `caat-check-kind-sujie` container was removed.

## P0.1 rejection-kernel census (2026-07-16)

- The instrumentation is measurement-only: every query still runs the original generic
  evaluator and the simulated hit is never used as a consistency result or search-pruning
  decision. Positive explanations are replayed through the complete offline evaluator;
  accepted hits and replay failures remain zero in all completed cohorts.
- The 60-task historical TIMEOUT/OOM sample uses the same fair adapter, rewrite lane,
  model, one-core/4-GB/60-second limits, and task identities on both sides. An earlier
  simple-adapter run is quarantined and excluded.
- SC: baseline and census both have 30 OOM and 30 TIMEOUT, with zero status mismatches.
  Twenty-four logs reached a checkpoint: 1,541,386 queries, 258 rejections, 207 hits,
  and 51 unique kernels. Hits/rejections is 80.2%, but hits/all queries is only 0.0134%;
  only `safestack_relacy.yml` records a hit.
- TSO: baseline and census again have 30 OOM and 30 TIMEOUT with zero status mismatch.
  Twenty-four checkpointed logs contain 828,170 queries, 153 rejections, 116 hits, and
  37 unique kernels. Hits/rejections is 75.8%, hits/all queries is 0.0140%, and only
  `safestack_relacy.yml` records a hit.
- PSO: baseline has 35 OOM and 25 TIMEOUT; census has 34 OOM, 25 TIMEOUT, and one
  compilation-classified termination, so instrumentation perturbed one resource-limited
  task. Twenty-six logs reached a checkpoint: 21,463 queries, 11,862 rejected queries,
  24,772 explained violations, 3,412 unique kernels, 21,360 exact duplicate kernels,
  zero subsumption, and 10,862 simulated hits on 17 tasks. Hits/rejections is 91.6% and
  hits/all queries is 50.6%.
- PSO deletion minimization reduced 199,136 raw literals to 198,362 (0.39%), while
  explanation and minimization cost totaled 732.8 and 36.3 seconds. This rules out
  retaining eager Reasoner/minimizer work. The useful signal is exact duplicate reuse,
  not smaller antichains: subsumption remained zero.
- Thirty-four PSO large-task logs and thirty-six SC/TSO logs died before the first
  consistency-query checkpoint, mainly through OOM. A rejection cache cannot address
  those failures; they require frontend/local-state compression, symbolic loop/state
  abstraction, or dependence/property-directed exploration.
- The completed formal matrix contains 1,728 unique run cells (96 tasks × three models ×
  three repetitions × two variants). All 18 BenchExec run sets exited zero. There are
  27 paired status changes, all repeated resource-bound perturbations on seven tasks,
  and zero safe execution-count mismatch.
- Task-clustered formal overhead: aggregate CPU ratio 1.20162 with 95% bootstrap CI
  [1.12795, 1.29234], wall 1.19361 [1.12291, 1.28370], and RSS 1.00159
  [0.99998, 1.00415]. PSO CPU is 1.67688 [1.38861, 2.08157].
- The 96-task opportunity sample is intentionally not the large-task conclusion: SC/TSO
  get 3.21% hits/all-query almost entirely from two tasks, while the historical
  TIMEOUT/OOM cohort falls below 0.015%. PSO remains broad: 55 checkpointed formal tasks
  and 17 resource-limited tasks have hits.
- The results-analysis bundle has `analysis-report.md`, `stats-appendix.md`,
  `figure-catalog.md`, two visually checked SVG figures, exact TSVs, and JSON under
  `optimization-analysis/continuous/rejection-kernel-census/analysis-output/`.

## P0.2 exact positive rejection-kernel cache (2026-07-16)

- Prototype scope: opt-in, worker-local, maximum 4,096 kernels, inactive for certified
  SC/TSO candidate profiles. It matched a full positive Reasoner conjunction after stable
  materialization; misses used the unmodified evaluator. Negative and empty explanations
  failed closed. No deletion minimization or replay ran in performance mode.
- Correctness: 145/145 Release tests; 39 mutation rows and all 5,441 oracle checks with
  cache hits explicitly recomputed; broad 288-program × three-model differential had
  852 matches, 12 mutual unsupported, and zero mismatch.
- Broad mechanism: offline evaluations fell from 869,028 to 643,107 (26.0%). Across the
  four formal repetitions, PSO recorded 7,368,584 queries, 6,568,688 hits (89.1%), 3,092
  learned kernels, 0 duplicates/capacity drops/failures, 2.34 seconds lookup time, and
  106.10 seconds explanation time.
- The three-repetition launcher was amended before inspecting PSO outcomes: SC r01 was
  an inactive-cache negative control yet showed a 2--3% physical-core side difference.
  A fourth swapped repetition balanced each variant twice on each core group.
- Formal design: 2,304 cells, 24 run sets, all exit zero, 17 status differences, zero
  opposite terminal verdict mismatch, and zero safe execution-count mismatch.
- Decision: reject/remove. Aggregate CPU ratio 1.04343 [1.00788, 1.09055], wall 1.03929
  [1.00462, 1.08558], RSS 1.01808 [1.00038, 1.04728]. PSO CPU 1.13116
  [1.01913, 1.29104]. PSO TIMEOUT cells fell 36→24, but OOM rose 20→28 and correct
  FALSE rose only 104→108.
- Mechanistic diagnosis: eager explanations for kernels that may never pay back cost
  106 seconds, and skipping evaluator synchronization leaves no checkpoint at a hit,
  which can make a later miss rebuild from a less local state. The first can be tested
  by delayed-learning gates; the second requires a different incremental-state design.
- Local evidence/report roots:
  `optimization-analysis/continuous/rejection-kernel-census/server-cache/` and
  `optimization-analysis/continuous/rejection-kernel-census/analysis-cache/`.

## P0.3 preventive-order opportunity census (2026-07-16)

- Primary-source rule: for an exact positive order closure, a proposed direct edge
  `(u,v)` is a sufficient inconsistency witness when the no-choice prefix already has
  `reach(v,u)`. This is the model-local form of Self-Reversal/Stale-Read; it does not
  delegate consistency to the generated SC/TSO checker.
- Measurement implementation removed the candidate read's incoming `rf`, or all
  incident `co` edges of the candidate write, recomputed `fr = rf^-1 ; co`, and ran the
  same CAAT fixed point for full and counterfactual bases. It changed no returned verdict
  or candidate list.
- Server gates: 144/144 Release tests; 39 mutation rows and 5,441 oracle checks; broad
  differential 852 matches, 12 mutual unsupported, zero mismatch out of 864 pairs.
- Formal PSO normally terminated tasks: 12,624 rejections, 12,624 offline replay
  rejections, zero replay mismatch, 12,620 direct reversals (99.97%), four already
  inconsistent counterfactuals. Categories were 11,861 CO, 837 FR, zero direct RF, zero
  exposed RMW; 78 candidates matched both CO and FR.
- Fair hard-cohort partial checkpoints covered six killed resource tasks and 183,296
  rejections. Direct reversal matched 179,886 (98.14% total); 3,401 counterfactuals were
  already inconsistent and only nine of 179,895 attributable candidates lacked a
  match. Per-task checkpoint totals were: rwlock 1,024/1,024; ttaslock 1,024/1,024;
  spaghetti 6,144/6,144; exponential-4 123,627/126,976 with 3,349 pre-inconsistent;
  pthread-demo-datarace-1 19,456/19,456; safestack_relacy 28,611/28,672 with 52
  pre-inconsistent.
- Measurement cost was deliberately not retained: normally terminated tasks spent
  about 7.33 seconds in the double replay, while the hard-cohort checkpoints accumulated
  about 181.88 CPU-seconds. The pruning prototype must synchronize one no-choice prefix
  and use O(1) packed reach lookups per incident candidate edge.
- Build-path lesson: GenMC embeds source/include paths. A binary configured under
  `/workspace/...` cannot be moved into a container exposing only `/data3/sujie`; the
  missing interception headers manifest as unsupported pthread externals. Configure and
  execute under an identical absolute mounted path.

## P0.3 preventive pruning validation (2026-07-16)

- Implementation scope: opt-in `--cat-preventive-pruning`; validation accepts only the
  compiled fingerprint of the bundled positive-recursive PSO model. It does not invoke
  the host SC/TSO consistency result.
- Prefix work: synchronize the no-choice graph once and read the already-computed
  recursive `reach` relation. RF/CO candidates are removed only if a direct newly added
  `rfe`, `co`, or derived `fr` edge reverses that prefix reachability.
- Spatial effect expected: candidate vectors become smaller and rejected candidate
  graphs/checkpoints are never built; added persistent state is eight counters and one
  predicate ID per checker. The existing reach relation is borrowed, not copied.
- Temporal effect expected: save graph mutation, synchronization, fixed-point
  evaluation, rollback, and scheduling for each removed choice; pay one prefix
  synchronization per RF/CO choice point plus O(number of offered direct edges)
  reach-membership lookups.
- Release unit tests: 144/144.
- CLI model boundary: recursive PSO accepted; recursive SC and phase-1 PSO rejected
  with status 17. A copied SB case completed with four executions and zero pruning on
  its six RF plus two CO candidates, confirming the path is active on a trivial case.
- Mutation oracle: 39 rows, 6,440 offline checks, no mismatch.
- Broad differential: 288 programs, 864 model pairs; 852 matches, 12 mutual
  unsupported, zero mismatch. The signature includes complete and blocked execution
  counts, so no observable under- or over-exploration occurred.
- Authoritative server evidence root:
  `/data3/sujie/experiments/caat-optimization/preventive-census/`; broad result is
  `pruning-broad/results.tsv`, and the active paired matrix is `pruning-formal-96/`.
# 2026-07-16: P0.4 result and sparse-EOG direction

- P0.4 formal: 768 cells, zero status/safe-count mismatch, CPU 0.99528 with 95% CI
  [0.98831, 1.00201], RSS 0.99992 [0.99949, 1.00030]. Internal maximum history and
  snapshot-equivalent totals fell to 20.61%, but the 240-cell hard cohort had no
  repeatable terminal gain. Production/test prototype removed exactly.
- `queue_ok_longer` provides a completed large-event profile: 8,033 events, 72,879,408
  base bytes, 587,519,208 peak snapshot-equivalent bytes, 21.841 s worklist, 31.427 s
  synchronization, and 1,060,139,008 B RSS.
- Current dense relation storage at 8,033 events is 8,097,264 B per predicate. The
  adapter pairwise-materializes structural `po/loc/int/ext` relations.
- Deagle commit `980fe9e6757e967d12454c7d16e2ec4de1a1cae4` uses sparse in/out EOG
  edges, thread chains, decision trails, pooled reasons, on-demand FR, and preventive
  propagation in `Closure`, `ICD`, and `SegmentSolver`.
- Decision: measure per primitive/predicate first, then implement exact structural views
  and a CAT-certified sparse cycle plan. Generic CAT evaluation remains the fail-closed
  fallback; no built-in GenMC checker verdict is used.
# Optimization 12 final evidence (2026-07-16)

- P0.5 diagnostics passed 145/145 tests, 39 mutation rows / 12,784 oracle checks, and
  864 broad pairs with 852 matches, 12 mutual unsupported and zero mismatch.
- Formal census: 288 cells, 273 profiles, 27 TIMEOUT/OOM, 12 resource checkpoints, nine
  profiles with at least 512 stable events. Timing and space-only runs have zero status
  and safe-execution-count differences.
- `queue_ok_longer` PSO: 8,033 stable events, 72,879,408 current-base bytes,
  396,770,976 published-predicate bytes, about 1.078 GB RSS in the timing run.
- Generic CSR is counterproductive for dense relations (large-cell median packed/CSR
  0.288x). Model-derived structural views plus direct-order EOG project to 2.04x lower
  target payload on the same cohort.
- Predicate operation time: SC closure/composition 72.0%/24.7%; TSO
  composition/closure 84.3%/13.1%; PSO 84.4%/12.6%. This favors a certified sparse EOG
  plan that avoids dense compositions and closure materialization.
- Hard-60: 28 OOM, 11 TIMEOUT, 21 compilation errors; only 12 resource cells reached a
  CAT checkpoint. Three 4-GiB OOMs had 14--52 events and <=25 KiB measured CAT values,
  proving that structural CAT optimization cannot solve exploration-state OOM alone.
- Final artifacts: `optimization-analysis/continuous/structural-census/`.

# Optimization 13 final evidence (2026-07-16)

- Grouped thread/location construction and packed row insertion preserve the existing
  dense CAT `po/int/ext/loc` values; persistent space is unchanged.
- Correctness: baseline/candidate Release tests 145/145 and 146/146; mutation 39 rows
  and 5,441 oracle checks; broad differential 852 matches, 12 mutual unsupported, zero
  mismatch over 864 pairs.
- Formal performance: 4 repetitions × 3 models × 2 variants × 96 tasks = 2,304 cells;
  all 24 run sets exited zero and all 1,152 cells paired per variant.
- On five completed model-task pairs with at least 512 stable events, candidate/baseline
  `materialize-ns` is 0.2596 [0.2049, 0.3302]; individual ratios span 0.172--0.389.
- Common-terminal whole-process CPU is 0.9954 [0.9871, 1.0036], so no universal CPU
  significance claim is justified. RSS is 1.00008 [0.99989, 1.00025].
- Seven cells improve only from TIMEOUT to correct false: PSO `fib_unsafe-5` is 0/4 to
  4/4 terminal and TSO `fib_unsafe-7` is 1/4 to 4/4. OOM remains 60 and execution-count
  mismatch remains zero. This passes the predeclared repeatable-coverage retention gate.
- Full decision and evidence root:
  `optimization-analysis/continuous/grouped-primitive-materialization/`.
# P0.7a current-CSR baseline census (2026-07-16)

- Baseline: pushed commit `7d405d7`; source is unchanged in the retained P0.6c formal
  `after` logs.
- SC `queue_ok_longer`, 4,029 stable events: current base 2,830,196 B versus peak
  snapshot-equivalent state 25,966,696 B; offline 0.815 s, materialization 0.100 s.
- TSO `queue_ok_longer`, 4,029 stable events: current base 2,864,444 B versus peak
  snapshot-equivalent state 80,863,104 B; offline 2.657 s, materialization 0.106 s.
- PSO `queue_ok_longer`, 8,033 stable events: current base 5,872,240 B versus peak
  snapshot-equivalent state 335,636,480 B; offline 34.269 s, materialization 0.688 s.
- Thus sparse primitive storage is no longer the dominant retained evaluator state on
  these completed large cases: snapshot/base ratios are about 9.18x, 28.23x and 57.16x.
  This directly supports eliminating the dense derived `reach=order+` value before
  implementing a broader EOG representation.
- The exact raw evidence is in
  `optimization-analysis/continuous/sparse-edge-primitives/server-results/formal-sparse-v2/after/{sc,tso,pso}/r01/*.logfiles.zip`.

## P0.7a correctness checkpoint

- Isolated server root:
  `/data3/sujie/experiments/caat-optimization/cycle-closure-slicing-csr/`.
- Source-before is exact `git archive 7d405d7`; source-after differs only in
  `Normalized.cpp` and `CatEvaluatorTest.cpp`.
- Baseline Release tests: 150/150. Candidate v2 Release tests: 152/152.
- Mutation oracle: 39 rows, 5,441 full-offline comparisons, zero mismatch.
- Broad differential: 288 programs, 864 model-program pairs, 852 matches, 12 mutual
  unsupported, zero mismatch.
- Initial candidate test failure was fail-closed and expected: normalized model
  fingerprints still described the pre-slice DAG, and the fixed-point benchmark made
  its closure dead by observing it only through `acyclic`. The exact transformed
  fingerprints now certify the same bundled models; a downstream alias keeps the
  benchmark closure observable without disabling slicing globally.

## P0.7a formal result

- Full matrix: 24 XML + 24 compressed log archives; 1,152 baseline and 1,152 candidate
  cells, all runsets exit zero.
- Aggregate common-terminal CPU ratio 0.9957515, 95% CI
  [0.9813763, 1.0068731]; RSS 0.9990871 [0.9982416, 0.9997470].
- Correct results: baseline 712 true + 338 false; candidate 712 true + 360 false.
  OOM remains 40. Twenty SC cells are repeatable TIMEOUT→correct false; two PSO cells
  are partial-false TIMEOUT→correct false.
- Independent compressed-log execution audit finds zero mismatch among all pairs whose
  before/after XML categories are both correct.
- Large offline ratio 0.4819696 [0.2493146, 0.8260040]; snapshot ratio 0.9533792
  [0.9318953, 0.9752709]; large RSS 0.9598374 [0.9419968, 0.9749504].
- Gate B passes; Gate A does not. Retain based on 5 SC tasks solved in 4/4 repetitions,
  not on a 10% space claim.

## P0.7a preventive compatibility correction

- Audit found `CATChecker::preparePreventivePrefix()` still required the normalized
  predicate named `reach`; P0.7a removes it, so opt-in `--cat-preventive-pruning` would
  assert even though default CAAT remains correct.
- Correct fix: under the exact PSO certificate, use the retained `order` predicate and
  compute its finite transitive closure only when a preventive prefix query requests
  reachability. Default CAAT does not pay this cost.
- Server: 152/152; mutation 39/5,441; broad 852/12/0. Real smoke reports 4 prefix
  queries, 10 RF candidates, 3 pruned and 4 complete executions.
- Paired pre-slice/post-fix runs on RMWFix and fcombiner (3 repetitions each) preserve
  status, execution count, candidate counts and prune counts. Runtime is subsecond and
  does not establish a performance difference.
- Follow-up source-only push: `35a595ab5a706c7fbae52a0f4ccc6d29655fcdd8`.

## P0.7b1 lazy cycle-EOG audit (2026-07-16)

- Post-P0.7a structural profiles attribute 84.3%/84.4% of TSO/PSO predicate operation
  time to composition. A successor-bit iterator would reduce only the final DFS scan
  and cannot address the dominant allocations or checkpoint copies.
- Exact prototype boundary: compile only an exclusive, non-recursive, positive
  `acyclic` expression cone; enumerate its extensional edges lazily from normalized CAT
  operands and omit only the cone's derived values.
- Explanation, full oracle and preventive reachability need published values, so the
  prototype disables itself for those modes. Unsupported or shared cones also fall back.
- Expected savings occur at composition/union construction, fixed-point storage,
  transactional copy and undo/checkpoint state. Base materialization and exploration
  count are unchanged. Duplicate paths through composition are the primary CPU risk.
- Frozen protocol:
  `optimization-analysis/continuous/lazy-cycle-eog/experiment-protocol.md`.
- First active broad run: 288 programs x SC/TSO/PSO, 852 comparable matches, 12 mutual
  unsupported and zero mismatch. Fallback mutation run: 39 rows, 5,441 oracle checks.
- Final prototype exposes cumulative `lazy-cycle-checks`, `lazy-edge-candidates`, and
  `lazy-unique-edges` beside existing operation/copy/checkpoint counters. A 513-event
  unit test covers initialize, two monotone inserts forming a cycle, and exact rollback
  while the elided root remains unpublished.
- Space-bound correction during review: recursive DFS retains one deduplicated successor
  row per active frame, so transient auxiliary memory is `O(N + E_path)`, not strict
  `O(N)`. The optimization still removes persistent per-predicate matrices and their
  copies; formal RSS, rather than the original projection, decides retention.
- First full matrix: aggregate CPU 0.98089 [0.95940, 1.00019], TSO 0.96124, PSO
  0.93532; large snapshot 0.34489, large copy 0.10551. PSO `queue_ok_longest` is
  TIMEOUT->correct true in all four repetitions and all 40 OOM cells become TIMEOUT.
- Counter-signal: SC CPU 1.04231 [1.02308, 1.06364], large SC RSS 1.25667 and large SC
  offline time 1.41002. P0.7b2 therefore selects at model-analysis time only when an
  admitted cone contains composition. This keeps TSO/PSO and restores union-only SC to
  the generic evaluator without naming a memory model.
# P0.7b2 final evidence (2026-07-16)

- Formal selector matrix: 2,304 cells, 24 XML and 24 log archives, all 12 paired runsets
  exited zero. Local archive: `optimization-analysis/continuous/lazy-cycle-eog/server-results/formal-v2/`.
- Correctness: 156/156 unit/property, 39/5,441 mutation oracle, broad 852 matches plus
  12 mutually unsupported and zero mismatch; formal verdict/execution mismatches both zero.
- Retention gate: aggregate CPU 0.97950 [0.95888, 0.99714], TSO 0.97303, PSO 0.94685;
  large snapshot 0.57732 and RSS 0.66807; no new OOM; PSO `queue_ok_longest` solves
  correctly in 4/4 repetitions.
- Structural selector effect: SC lazy checks 893,948 -> 0 and large RSS 1.25667 ->
  1.00005 relative to baseline. TSO/PSO retain 1,787,856/5,246,664 lazy checks and their
  large-state reductions. SC CPU is still 1.01531 [1.00701, 1.02376], so no SC speedup
  claim is justified.
- Remaining cost: 6.181B candidate root edges and 4.739B unique edges. Next work should
  reduce exact composition enumeration with rollback-safe EOG/topological state.

# P0.7c per-plan selector rejection (2026-07-16)

- Per-check selection halves lazy checks and reduces candidate root edges by 33.4%.
- Formal CPU improves slightly to 0.99326 [0.98708, 0.99937], but large RSS/snapshot
  regress to 1.11401/1.20292. TSO is 1.24384/1.59045 and PSO 1.45558/1.73694.
- The missing coherence DFS saves time, but materialized coherence values roughly double
  TSO/PSO large copy time. Keep coherence lazy and target macro-edge enumeration inside
  the lazy path instead.

# P0.7d1 streaming failure and P0.7d2 direction (2026-07-16)

- Streaming removes successor-row storage and pilot improves TSO CPU/RSS materially, but
  recursive event DFS inside the emitter crashes five deep TSO graphs in 4/4 repetitions.
- The 20 failures are baseline TIMEOUT -> SEGFAULT, roughly 9.2 s and 1.057 GB each.
  Treating them as resource failures would hide a real runtime bug and is forbidden.
- P0.7d2 retains streaming below depth 2,048, then reruns exact cycle detection with a
  heap-based iterative buffered DFS. It must pass an 8,193-event chain and all old gates.

# P0.7d2 versus Deagle census run (2026-07-16)

- Exact methods requested: native GenMC, P0.7d2 CAAT under SC/TSO/PSO, and Deagle.
- Common input is the adapted 725-task `C.Concurrency` census; per-task limits are
  60 seconds, 4 GiB and one core. GenMC-family methods use the same fixed-seed,
  fail-closed preprocessing adapter.
- CAAT uses the measured P0.7d2 binary at
  `/data3/sujie/experiments/caat-optimization/streaming-lazy-cycle/build-after/bin/genmc`.
  Deagle is rerun from `deagle:sujie`; it is not copied from an old environment.
- The experiment-only adapter adds only two dispatch mappings:
  `caat-tso -> recursive-tso.cat` and `caat-pso -> recursive-pso.cat`. It does not use
  GenMC's built-in TSO checker and is excluded from Git commits.
- A single 48-worker BenchExec SC launch failed before task execution because cores
  0--47 cover 28 cores from NUMA node 0 and 20 from node 1. Two independent shards on
  cores 0--27 and 28--47 completed instead, preserving 48-way task parallelism.
- TSO/PSO verdict disagreements with SC task labels must be reported as cross-model
  semantic differences unless a model-specific oracle is available; only SC supports
  ordinary SV-COMP correct/wrong scoring on this task set.
- Final exact coverage: Deagle 615 correct/3 wrong/16 TIMEOUT/10 OOM; native GenMC
  408/31/210/50; CAAT-SC 398/30/240/31; CAAT-TSO 385/30/253/31; CAAT-PSO
  355/29/274/41. All GenMC-family wrong results are `true` under the fixed seed; they
  are conditional-input misses, not evidence that CAT consistency accepts a forbidden
  graph. TSO/PSO correct/wrong labels remain SC-label descriptions only.
- Deagle versus CAAT-SC has 367 common same-verdict terminal tasks. CAAT-SC/Deagle
  CPU geomean is 0.159 [0.121, 0.210]; this easy-common-subset speed does not compensate
  for the 217-result correct-coverage gap. Median solved RSS over each method's own
  set is about 25.9 MiB for CAAT-SC and 12.9 MiB for Deagle.
- Previous-to-latest CAAT-SC: correct 388->398, terminal 418->428, OOM 57->31,
  TIMEOUT 224->240. Exact transitions include OOM->false 5, OOM->TIMEOUT 21,
  TIMEOUT->false 4 and TIMEOUT->true 1; there are no terminal verdict changes and no
  solved-to-resource regressions. CPU ratio on 418 same-verdict terminal pairs is
  0.915 [0.895, 0.935]; RSS 0.994 [0.987, 1.000].

# Unified previous/current six-method HTML (2026-07-16)

- Combined the archived 2026-07-15 post-repair CAAT-SC result with current native
  GenMC, P0.7d2 CAAT-SC/TSO/PSO, and Deagle. No historical run was repeated.
- All six XML files must reside under one common `raw/` root for table-generator row
  identity. Supplying XMLs from the separate `final-five/raw` and
  `previous-vs-latest/raw` roots produced an invalid 1,450-row table with 4,350 empty
  cells; it was removed and regenerated correctly.
- Final full table has 725 unique tasks and six non-empty status cells per task. The
  self-contained result root contains 4,350 logs; all 4,350 log links and all 725 task
  links resolve locally.
- Current versus previous CAAT-SC on 418 same-verdict terminal pairs: CPU 0.915446
  [0.894591, 0.934561], RSS 0.994343 [0.986828, 1.00024]. Ten additional tasks become
  terminal, 26 fewer hit OOM, and 21 old OOM cases shift to TIMEOUT; no old terminal
  result regresses or changes verdict.
- The archived previous XML reports `commit #unknown`. The comparison therefore binds
  the old column to its XML/log archive and run timestamp, not to an inferred Git SHA.

# P0.7e product-state EOG design evidence (2026-07-16)

- P0.7d2 still performs 5,694,936,384 root-relation candidate callbacks. For a
  composition `A ; B`, different sources that reach the same middle event rescan the
  same `B(m,_)` suffix; streaming removed row storage but not this repeated work.
- Generated TSO code represents the accepted path language with multiple visitor arrays
  indexed by event and an accepting-depth counter. The reusable idea is the product
  state layout only; P0.7e compiles generic normalized CAT expressions and never calls
  or trusts the generated checker.
- Local PLDI 2021/TOPLAS 2023 ordering-consistency papers reduce consistency to cycles
  on event graphs and maintain state incrementally. OOPSLA 2023 further supports
  removing derived relations and SCC/transitive-reduction preprocessing. P0.7e uses
  these as algorithmic motivation, not copied implementation or model-specific clauses.
- Exact construction: compile the nonrecursive expression into an acyclic epsilon-NFA,
  add root-exit-to-entry reset, and search `(control,event)`. Because the only control
  back edge is reset, every product cycle splits into one or more exact root-relation
  paths; reset events form the event witness.
- Whole-relation intersections remain atomic because factoring a guard such as
  `(A ; B) & loc` would otherwise need to remember the macro-edge source. This preserves
  correctness and makes the limitation observable instead of introducing a spurious
  shared path.
- Expected time reduction is suffix reuse; expected space increase is one product-color
  byte per `(control,event)`. Automata are immutable analysis metadata, product state is
  query-local, recursive depth remains bounded, and any unsupported/oversized plan
  falls back to P0.7d2.

## P0.7e1 pilot diagnosis

- Correctness passes: Release 161/161; ASan+UBSan focused 7/7; mutation 39 rows and
  5,441 full oracle checks; broad 288 programs, 852 matches, 12 mutually unsupported,
  zero mismatch.
- Two-repetition pilot ratios (after/before): TSO `fib_unsafe-7` CPU
  0.9107/0.9166; TSO `queue_ok_longer` CPU 1.2243/1.1761 and RSS 1.3662/1.3651;
  PSO queue CPU 1.0316/1.0293 and RSS 1.0180/1.0178.
- TSO queue retained 35,572 checks. Root macro candidates fell from 455.4M to 101.5M,
  while the product processed 1.777B control transitions. Its offline time fell from
  29.92 s to 25.68 s, so suffix reuse works, but recursive control frames raise total
  CPU overhead and peak stack/RSS.
- P0.7e2 uses an explicit DFS stack. Base relation rows are resumed directly with
  `nextSuccessor`; epsilon/guard edges carry scalar cursors. Only atomic whole-edge
  intersections need a successor vector because their endpoint-dependent filter cannot
  be split without source context.

## P0.7e2 implementation checkpoint

- Product traversal no longer uses native recursion, callback enumeration for base
  relations, buffered whole-product rows, or a 2,048-depth restart. The retained
  P0.7d2 non-product fallback is unchanged.
- Local/remote SHA-256 matched for the two P0.7e2 files. Server Release build and all
  161 tests pass; focused ASan+UBSan passes 7/7, including the 8,193-event heap-frame
  path and incremental rollback test.
- Mutation and 288-program broad differential run in a separate `p07e2-oracles-sujie`
  container and write only `*-e2` evidence files.
- A read-only remote poll first assigned zsh's reserved `status` variable and exited
  before inspecting the container. Renaming it to `state_line` fixed the poll; no build,
  source, container, or result state was changed by the failed command.

## Six-method HTML column correction

- The first six-method page inherited per-core BenchExec columns from CPU-affinity
  metadata, producing `cputime-cpu0` through `cputime-cpu55` across the merged shards.
- `all-six/table-definition.xml` is now an explicit allowlist: status, category, total
  cputime, walltime, memory, and termination reason. Regeneration reports 725 rows and
  36 method columns; both HTML pages contain zero `cputime-cpuNN` tokens and retain all
  4,350 log references. Raw XML/logs are unchanged.
- The first regeneration command passed `table` instead of `/table` to
  `--initial-table-state`; argument validation stopped before writing output. The
  corrected command completed normally.

## P0.7e2 rejection and P0.7e3 selector

- P0.7e2 correctness passed: Release 161/161, sanitizer 7/7, mutation 39/5,441,
  broad 852/12/0. Its two-repetition pilot nevertheless has TSO fib CPU
  1.2778/1.2802, TSO queue 1.3298/1.3331 and PSO queue 1.1666/1.1316. One TSO fib
  baseline correct result becomes a timeout. No formal matrix was started.
- The explicit frame traversal removes P0.7e1's RSS regression but makes every product
  transition substantially more expensive. The same 1,776,716,915-transition TSO fib
  offline work rises from about 25.65 s in e1 to 42.60 s in e2.
- A baseline-audit alarm was false: server `source-before` hashes match d963 exactly and
  raw before logs have no product counters. GNU/BSD `column` collapsed empty TSV fields
  and visually shifted later values under product headers. Structured CSV parsing is
  authoritative; formatted terminal columns are not used as evidence.
- P0.7e3 restores e1 traversal and freezes a model/task-independent 32 KiB product-color
  cap. Evidence: the beneficial TSO fib has at most 319 stable events and roughly 12.1k
  visited product states/check; bad TSO/PSO queue cases reach 4,029/8,033 events.
- P0.7e3 Release passes 162/162 and focused ASan+UBSan 8/8. A 2,049-event product still
  takes the exact product depth fallback; an 8,193-event product fails the cache selector
  and takes the exact retained P0.7d2 event fallback with a complete witness.
- P0.7e3 mutation/broad pass 39/5,441 and 852/12/0. Pilot-v3 has zero status/category
  mismatch; TSO fib CPU is 0.9110/0.9161, TSO queue 1.0133/1.0052, PSO queue
  0.9827/0.9829, and every RSS ratio is within 1.003.
- Formal r01 has aggregate CPU 1.0180, while the r01+r02 task-median result is 0.9966;
  the core swap removes much of the single-repetition bias. Two-repetition PSO remains
  1.0173, so all four frozen repetitions are required before deciding.
- One intermediate command removed only the agent-owned
  `/tmp/p07e-analysis-r02` directory before recreating a read-only analysis. No repo,
  server, or raw evidence path was touched. Further analyses use new output paths rather
  than destructive temporary cleanup.
# GenMC TIMEOUT versus Deagle-correct study (2026-07-17)

- Primary cohort: latest GenMC+CAAT-SC is TIMEOUT while Deagle returns the benchmark-
  correct verdict under the same adapted 725-task, 60-second, 4-GiB comparison.
- Secondary cohorts: native GenMC TIMEOUT/Deagle-correct and CAAT-SC OOM/Deagle-correct,
  used to distinguish exploration explosion from CAT/CAAT-only overhead and memory
  pressure.
- Unit of analysis: one canonical SV-COMP task, not individual log lines or repeated
  counters.
- This study will not treat TSO/PSO verdicts as competition correctness because the
  task labels are SC properties.
- Frozen primary cohort: 200/725 tasks; it covers 200/240 (83.3%) current CAAT-SC
  TIMEOUTs for which Deagle is correct. Native GenMC also TIMEOUTs on 192/200, so only
  eight cases are CAAT-specific threshold regressions.
- The cohort is not uniformly fair: 190 tasks use symbolic nondet in Deagle but fixed
  seed 1995 in GenMC; 11 Deagle TRUEs use fallback unwind=3 and are not conclusive.
  Their overlap leaves nine strict, complete, deterministic pairs.
- On the nine strict tasks, native GenMC TIMEOUTs on eight; Deagle completes all in
  0.266--54.803 CPU seconds. CAAT RSS remains 25.82--25.96 MB.
- `pthread-wmm` contributes 179/200. Every such task is loop-free, contains 3--4
  creates, 15--22 atomic blocks, nondet and explicit buffer/flush encoding. Deagle
  median CPU is 0.277 seconds, but this is descriptive because nondet semantics differ.
- Official Deagle V4.1 defaults to `deagle-closure`/`ClosureSolver`. Its ordering
  theory participates in Boolean propagation, cycle conflicts become learned clauses,
  conflict analysis backjumps non-chronologically, and graph changes use decision-level
  trails. Current GenMC+CAAT does not reuse a CAAT conflict across future candidate
  execution graphs.
- Highest-leverage new directions: cross-execution learned nogoods, conflict-level
  backjumping, and a finite symbolic event-skeleton lane with CAT/CAAT as the theory
  authority. Assertion-cone search, certified WMM encoding collapse and exact finite
  loop summaries are secondary.
- Analysis artifacts:
  `optimization-analysis/timeout-vs-deagle/analysis-output/analysis-report.md`,
  `stats-appendix.md`, `primary-cohort.tsv`, `summary.json`, and three SVG figures.

## P0-A learned CAT nogoods pre-implementation evidence (2026-07-17)

- The existing `Reasoner` already reconstructs deterministic sufficient conjunctions
  over base predicates, including recursive positive derivations. Reuse it rather than
  maintaining provenance inside every CAAT operator in V1.
- `StableGraphAdapter` assigns worker-lifetime IDs keyed by `EventPos` or initial-write
  `SAddr`; it needs a checked ID-to-key accessor for explanations to survive graph cuts.
- `GraphSynchronizer` materializes stable `BaseValues` immediately before calling the
  incremental evaluator. This is the earliest existing point where an exact ground
  nogood can be matched without rebuilding primitive relations.
- A skip must not leave callers reading the evaluator's result for the previous graph.
  The synchronization result therefore needs an explicit `knownInconsistent` outcome,
  and `CATChecker` must return false directly.
- V1 is sound only for positive base-literal explanations of cycle/self-loop checks.
  Negative/difference explanations, empty checks, unmapped endpoints, oversized clauses,
  and oracle-mode queries fail closed to normal evaluation.
- Unlike the rejected snapshot cache, the retained object is a bounded conjunction of
  ground choices/facts, with subset elimination. It does not own `BaseValues`, relation
  matrices, checkpoints, or evaluator state.

## Optimization 6.1 applicability boundary (2026-07-17)

- User judgment: the proposed WMM/program-shape-specific 6.1 route is not a generally
  good optimization.
- Resolution: remove it from the default/general roadmap. It must not be used to claim
  a general GenMC+CAT/CAAT improvement or to explain results outside the recognized
  family.
- A future specialized variant is admissible only if the frontend can emit a checked
  equivalence certificate, unsupported shapes fail closed to the original program,
  and SC/TSO/PSO plus original-model differential tests show unchanged verdict and
  exploration coverage.
- This decision does not invalidate retained P0.6a/P0.6c. Those optimize generic CAT
  base-relation construction/storage and were measured across SC/TSO/PSO; they do not
  rewrite a benchmark into a WMM-specific semantic shortcut.

## P0-A correctness checkpoint and launch audit (2026-07-17)

- Final GCC 13 Release suite: 163/163 passed.
- GCC 13 ASan+UBSan focused learned-nogood suite: 5/5 passed.
- Online mutation oracle: 39 rows and 5,441 full comparisons passed.
- Recursive broad differential: 288 programs x SC/TSO/PSO = 864 pairs; 852 exact
  matches, 12 mutual unsupported pairs, and zero mismatch.
- Complete correctness outputs are under
  `optimization-analysis/continuous/learned-cat-nogoods/correctness/`.
- A census launch failed closed because a local `server-config/` path was used for a
  server-side `definitions/` directory. No container started; only the isolated frozen
  baseline source directory was created. The corrected input path was discovered with
  `find` before retrying.
- The next launch was also invalid: SC/TSO produced 96 UNKNOWN rows each because the
  tool adapter lacked `GENMC_EXPERIMENT_ROOT`, while PSO rejected a core range crossing
  the 28-core NUMA split. Preserve those XML/logs as launch diagnostics, do not include
  them in hit or performance analysis, and retry with an explicit root plus within-node
  ranges.

## P0-A 96-task hit census (2026-07-17)

- Valid retry completed 96 rows per recursive model. Terminal logs available for
  SC/TSO/PSO: 95/90/83; timeout/OOM processes do not emit final CAT counters, so all
  totals below are lower bounds.
- SC: 303,325 match queries, 38,792 hits, 4 learning tasks and 2 hit tasks. Butterfly
  contributes 38,784 hits; Szymanski contributes 8. Stored maximum bytes summed across
  terminal tasks: 15,451 bytes.
- TSO: 303,320 match queries, 38,792 hits, again 2 hit tasks. Butterfly alone performs
  24,280,918 literal checks, showing that a high hit count may still lose time to a
  linear clause/literal scan. Summed terminal maximum bytes: 32,836 bytes.
- PSO: 916,961 match queries, 744,058 hits across 50/83 terminal tasks, with 584 learned
  clauses and 53,439,881 literal checks. Largest hit contributors are triangular-2
  (241,807), butterfly (234,163), fib_safe-5 (208,546), and fib_unsafe-5 (47,313).
  Summed terminal maximum bytes: 695,176 bytes.
- Interpretation: there is enough reuse for a performance pilot, especially under PSO,
  but literal-check volume is the primary regression risk. Do not infer speedup from
  hit counts alone.
- The first paired-pilot preflight incorrectly executed a Noble-built baseline on the
  older server host and failed on GLIBC/GLIBCXX symbol versions before container
  creation. This is an environment check error, not a binary build failure; all actual
  runs remain inside `genmc15noble:sujie`.
- V2b's first full test run passes 164/165. The only failure is an intentionally changed
  timing contract: the integration test expected learning on query one and a hit on
  query two, while repeated-witness admission learns on query two and can first hit on
  query three. The focused database tests all pass; amend the integration sequence and
  rerun the suite.

## P0-A V1/V2a/V2b pilot decisions (2026-07-17)

- V1 rejected: all-model hit-rich CPU ratio 1.2090 and zero new core terminals. Full
  Reasoner reconstruction plus linear matching dominate despite large PSO hit counts.
- V2a rejected standalone but retained as a safety backstop: limiting explanation to 16
  attempts improves the ratio to 1.0957 and preserves PSO fib_safe-5 TIMEOUT-to-TRUE,
  but queue/circular/szymanski still regress.
- V2b exact repeated-witness admission rejected and removed: ratio 1.1705, PSO 1.4898,
  and fib_safe-5 loses its terminal result. Derived-witness recurrence does not predict
  reusable base clauses.
- V2c target is now measured rather than speculative: compile predicate/stable-event
  identities so each snapshot resolves predicates once and the 0.56M--33.6M literal
  checks in the pilot use integer hot paths.

## P0-A V2e pay-as-you-go result (2026-07-17)

- All correctness gates pass: Release 165/165, ASan+UBSan 5/5, mutation 39/5,441,
  broad 852/12/0.
- The 23 common-terminal pilot cells have CPU ratio 1.0626; SC/TSO/PSO are
  1.0192/1.0583/1.1195. The frozen 1.01 advancement gate fails.
- PSO fib_safe-5 changes TIMEOUT to TRUE at 43.402 seconds, and triangular-2 is 0.6252
  of baseline. This confirms reusable explanations can remove meaningful evaluation.
- Clause-length credit does not amortize low-yield clauses: PSO queue learns seven
  clauses after 72 hits, performs 54,849 literal checks, and costs 2.5814 of baseline;
  circular_buffer_bad costs 1.8017.
- V2f therefore uses the model-independent 64-literal limit as the minimum number of
  new hits required after every explanation attempt. This changes optional work only;
  it does not use Optimization 6.1 or built-in checker verdicts.

## P0-A V2f minimum-credit result (2026-07-17)

- Correctness again passes 165/165, 5/5, 39/5,441, and 852/12/0.
- Common-terminal CPU improves relative to V2e but remains a regression: SC 1.0039,
  TSO 1.0610, PSO 1.0748, aggregate 1.0449.
- PSO queue proves that later admission is no longer the main issue: V2f learns one
  clause, performs 3,018 literal checks, records 20 hits and 2,918 credit skips, yet
  costs 2.1290 of baseline.
- Beneficial cases remain: PSO fib_safe-5 terminates in 44.092 seconds versus baseline
  TIMEOUT; triangular-2/fib_unsafe-5/reorder_2 ratios are 0.6280/0.8391/0.7831.
- Independent counters correct the initial mechanism hypothesis: queue's only Reasoner
  call costs 1.132 ms. Learned nogoods instead disable lazy-cycle evaluation globally
  so that every query carries full predicate values. V3 restores lazy evaluation and
  performs exact full materialization only for admitted explanations. A larger
  hit-credit constant would not fix the global evaluator-mode regression.

## P0-A Reasoner work census and V3 preflight (2026-07-17)

- Candidate-only hit-rich instrumentation records exact Reasoner time and dense work in
  `optimization-analysis/continuous/learned-cat-nogoods/reasoner-profile/reasoner-work.tsv`.
- PSO queue: one Reasoner call, 1.132 ms, 19,300 reason slots, 26,420 derived scans,
  1,238 derive attempts, 619 updates, and 4,560 composition-middle probes. This is far
  below its roughly 1.34-second V2f CPU increase.
- High-yield PSO cases pay more Reasoner work but amortize it: fib_safe-5 uses 12 calls
  and 2.032 s for 208,167 hits; triangular-2 uses 11 calls and 94.9 ms for 241,471 hits;
  reorder_2 uses seven calls and 90.4 ms for 5,933 hits.
- The actual V2f mode difference is constructor-level: `catLearnedNogoods` disabled
  lazy-cycle evaluation for every incremental query so all values remained available.
- V3 keeps lazy evaluation and materializes a non-lazy snapshot only when an admitted
  explanation finds missing values. PSO queue preflight reports 5,878 lazy checks, one
  full materialization in 39.34 us, one Reasoner call, one learned clause, and 20 hits.
- V3 Release tests pass 165/165, focused tests 17/17, mutation 39/5,441, and broad
  differential 852/12/0. Focused ASan+UBSan passes 17/17. A full sanitized queue run
  exposes two interpreter-null UBSan reports in `lli/Runtime`; a no-nogood differential
  run is in progress to classify whether they predate V3.

## P0-A V3 formal result (2026-07-17)

- Four balanced repetitions produce all 2,304 cells and 1,152 pairs. There are zero
  terminal-verdict or complete-execution mismatches, six PSO coverage-gain cells, and
  zero coverage losses.
- Formal task-model median CPU ratio is 1.01032 with task-clustered 95% CI
  [1.00151, 1.01866]. SC/TSO/PSO are 1.00533/0.99909/1.02823. The frozen CPU gate
  fails even though the cell-level ratio is 1.00757.
- RSS passes: candidate/baseline P90 is 1.00242 and memory-ratio P90 is 1.00316.
- Across terminal candidate logs, 624 Reasoner calls cost 19.506 s; 556 on-demand full
  materializations cost only 0.112 s. There are 6.144M hits and 78.751M literal checks.
- Large PSO tasks improve (triangular-2 0.6232, reorder 0.7308, fib_unsafe-5 0.8091,
  butterfly 0.8268), while the worst regressions are sub-0.12-second tasks paying one
  explanation with little remaining exploration.
- V4 requires 1,024 match queries before first learning, derived from the existing
  64-literal and 16-attempt bounds. It delays optional pruning only and preserves the
  V3 lazy/full-provenance correctness boundary.
- V4 server checkpoint: focused Release 11/11, full Release 166/166, focused
  ASan+UBSan 11/11, and mutation oracle 39/5,441 pass. The old integration test was
  updated to assert no learning before query 1,024, learning at query 1,024, and the
  first hit on query 1,025. This changes only the test contract.
- V4 broad differential passes 852/12/0. PSO `twostage_3` observes 74 match queries,
  54 warm-up skips, and zero Reasoner calls/materializations/learned clauses while
  retaining the correct false verdict.
- V4's 23 common-terminal hit-rich pilot CPU geometric-mean ratio is 0.99483;
  SC/TSO/PSO are 1.01067/1.02872/0.93638. PSO `fib_safe-5` changes TIMEOUT to a
  correct TRUE in 46.93 s, with zero coverage losses. No task above one baseline CPU
  second exceeds the 1.20 regression guard. This satisfies the formal-matrix
  advancement gate; it is not yet a retain decision.
- V4 formal result: all 2,304 cells / 1,152 pairs are complete, with zero verdict or
  execution mismatch, four PSO `fib_safe-5` coverage gains, and zero losses. The
  task-model CPU ratio is 1.01162 [1.00521, 1.01774], so the frozen 1.01 gate fails.
  SC/TSO/PSO are 1.00719/1.02252/1.00504 and memory P90 candidate/baseline is
  1.00219. Compared with V3, calls/materializations/literal-checks fall, but delayed
  explanations occur on larger graphs and total Reasoner time rises 19.506 s to
  23.754 s. Reject the fixed 1,024-query rule and do not commit it.

## P0.7f fused lazy-cycle pre-implementation evidence

- The rejected learned-nogood source/test diff is archived at
  `optimization-analysis/continuous/learned-cat-nogoods/rejected-v4-source.patch`;
  production/test paths now match retained `d963c49` exactly.
- P0.7e3's exact product-state construction reduced macro candidates 58.75% but ran
  18,831,879,352 product transitions and had aggregate CPU 0.99930. The next experiment
  must remove NFA interpretation work rather than tune the 32 KiB selector.
- P0.7f compiles epsilon closure excluding reset, deduplicates fused actions, compacts
  reachable action states and stores flat offsets/actions. Runtime retains relation,
  set-guard and reset actions only. Reset-source metadata preserves exact event witness
  projection after exit-state epsilon paths are fused.
- Expected reductions: runtime transition dispatch, product vertices/color bytes, DFS
  depth and immutable vector-object overhead. Expected added cost is bounded one-time
  `O(Q(Q+E))` analysis with `Q <= 256`; no query-persistent or rollback state is added.
- Per-transition Relation/Guard/Reset subtype counters were removed before measurement.
  The evaluator always passes a statistics object, so retaining those diagnostics would
  add an extra `switch` to every product transition and bias the candidate. Program
  state/action totals remain constant-cost per check; the existing aggregate product
  transition counter is sufficient to test the frozen reduction gate.
- Server correctness evidence: Release 163/163; focused GCC 13 ASan+UBSan 10/10;
  mutation oracle 39 rows / 5,441 full-recomputation checks; broad differential
  852 match / 12 mutual unsupported / 0 mismatch over 864 SC/TSO/PSO pairs.
- P0.7f formal result rejects the general default: CPU all 0.99962 with 95% CI
  [0.99407, 1.00521], SC/TSO/PSO 1.00556/0.98786/1.00561. Exactness and coverage are
  unchanged. Relative to P0.7e3, state visits fall 82.69% and transitions 24.08%, but
  the end-to-end aggregate speedup is not statistically established. Do not convert
  the isolated TSO benefit into a model-name selector.
- Cleanup restored all production/test paths to retained `d963c49`; the rejected patch
  SHA-256 is `90907f7182ef2309f25484517f8c979cd6d8f307390ec309e93a497952c3f6fb`.
  No P0.7f source is staged or committed; raw evidence and reports remain local only.

## P0.7g pre-implementation boundary

- The retained lazy evaluator repeatedly interprets nested unions and builds a
  type-erased callback for atomic intersections while streaming billions of exact root
  derivations. P0.7g flattens ordered unions and compiles base/filtered-base terms but
  deliberately keeps the same event-level DFS and candidate sequence.
- Composition, optional and any non-atomic intersection remain exact `Generic` terms;
  unsupported compilation loses only the optimization. This is an operator compiler,
  not a TSO/PSO or benchmark-shape selector.
- Expected reduction is dispatch/callback time only. Candidate counts, query-local DFS
  storage, snapshot-equivalent bytes and rollback state must remain exact.
- P0.7g correctness passes Release 159/159, focused ASan+UBSan 18/18, mutation
  39/5,441, and broad 852/12/0. A TSO queue smoke has identical four lazy checks and
  15,503,664 candidates; candidate activation is four compiled checks / 26 terms.

## P0.7g pilot decision (2026-07-17)

- Reject before formal under the frozen protocol. All 12 paired cells preserve status,
  category and exact candidate counts; SC has zero compiled checks while TSO/PSO
  exercise the intended path.
- Time is promising on affected cases: PSO queue median CPU ratio 0.89724, TSO fib
  0.92975 and TSO queue 0.93435. SC queue remains within the 1.03 guard at 1.02348.
- The independent RSS gate fails twice on PSO queue: 1.06385 (+7,495,680 B) and
  1.02233 (+2,621,440 B), both above the per-observation 1.02 limit.
- Snapshot-equivalent bytes and maximum current-base bytes are exact; the binary grows
  only 21,136 bytes on disk / about 23,844 text bytes. This narrows the regression to
  process/layout/allocation effects rather than graph-state growth, but it cannot be
  waived after seeing the result.
- Optimization 6.1 remains excluded from the generic default route. Any reuse requires
  a separately frozen, fail-closed and equivalence-certified specialization experiment.
- The rejected source/test patch is archived with SHA-256
  `0e183b001af8540a547780ca09e58c24e85f4a39a29f046561b8c10de4635294`.
  Production/test paths were restored exactly; the patch passes `git apply --check`.

## P0.7g2 layout audit and frozen boundary (2026-07-17)

- A server-side GCC 13 header probe against exact P0.7g before/after sources reports:
  `ModelAnalysis` 176→200 B, `FixedPointStatistics` 48→64 B,
  `CaatEvaluationResult` 144→160 B, `IncrementalStatistics` 184→200 B,
  `IncrementalCaatEvaluator` 496→528 B and `LazyCycleStatistics` 24→40 B.
- The two new counters flowed through fixed-point results, incremental lifetime stats
  and evaluator aggregation. These changes do not by themselves prove the 2.6–7.5 MiB
  RSS regression, but they are avoidable layout/instrumentation confounds.
- P0.7g2 replaces the existing lazy-root vector with a lazy-plan vector instead of
  adding another vector. No production statistics fields or CAT output are added; all
  six probed public type sizes must equal baseline before performance testing.
- The independent protocol is frozen at
  `optimization-analysis/continuous/compiled-streamed-cycle-v2/experiment-protocol.md`.
  Six balanced pilot repetitions use both paired medians and candidate-vs-baseline
  maxima for RSS. This does not alter P0.7g's rejection.
- Candidate-only layout is `LazyStreamTerm=12 B`, `LazyCyclePlan=32 B`, and
  `optional<LazyCyclePlan>=40 B` versus `optional<PredicateId>=8 B`. The added fixed
  metadata is therefore 32 B per model check plus 12 B per ordered term and allocator
  bookkeeping; it is independent of event count and never enters graph snapshots.
- The final correctness gates pass Release 161/161, focused ASan+UBSan 22/22,
  mutation 39/5,441 and broad differential 852/12/0.
- Before any P0.7g2 formal result existed, the formal analyzer was regression-tested on
  archived P0.7f data. Requiring four terminal repetitions and taking the ratio of
  before/after medians reproduces CPU 0.999616 and SC/TSO/PSO
  1.005556/0.987858/1.005609. The protocol now names this exact model-task bootstrap
  unit; the upper-CI-below-1 retain threshold is unchanged.
- P0.7g2's six-repetition pilot completes 36 paired cells with zero status/category,
  execution, candidate, lazy-check, snapshot or base mismatch and zero coverage loss.
  PSO queue CPU median is 0.918100 with RSS median 1.013865 and max-level ratio
  0.994650; TSO fib/queue CPU medians are 0.942600/0.923781. All frozen pilot gates pass,
  so the 48-worker 2,304-cell formal matrix is authorized.
- P0.7g2 formal scheduling is 6 BenchExec instances x 8 workers, with disjoint core
  ranges 0--47. `progress.tsv` confirms repetitions 1--3 completed successfully for
  all SC/TSO/PSO before/after pairs. A live checkpoint found 22/24 XMLs and two active
  BenchExec instances: this is static-shard tail imbalance after four instances have
  finished, not a three-worker or reduced-parallelism run. Preserve the frozen paired
  affinity for this matrix; use a shared queue or finer shards only in a later protocol.
- P0.7g2 formal completes 24/24 XMLs and 24/24 log ZIPs with 2,304 cells / 1,152
  pairs. Exactness and coverage mismatches are zero. CPU is 0.999423 with clustered
  95% CI [0.994756, 1.004137], failing the frozen upper-below-1 gate; SC/TSO/PSO are
  0.994100/1.004601/0.999926. Memory gates pass at 1.002677 cell P90, 0.999844
  P90-of-levels and 1.003172 large-task maximum. Reject the generic mechanism rather
  than introducing a model-name selector.
- P0.7g2 production/test sources are restored exactly to HEAD. The rejected patch is
  24,003 bytes with SHA-256
  `a3deb5a4cdb78846398fb458c6f0872b21250f955477b384f5130e51389af04a`; `git apply
  --check` passes. The server formal container was removed only after local artifact
  counts and analysis outputs were verified.

## P0.8a design boundary (2026-07-17)

- The retained incremental evaluator reruns `findLazyCycle` after every applied insert
  and replacement. Checkpoints restore values/violations through insertion deltas, but
  the lazy checker retains no cross-query acyclicity certificate.
- A cached rank is an exact negative-cycle certificate only after streaming every edge
  of the current extensional lazy root and checking `rank[u] < rank[v]`. Any failed
  inequality triggers the existing full DFS; it never proves a cycle or inconsistency.
- A topological order valid for an insertion descendant remains valid when rollback
  deletes edges. If a descendant is cyclic the certificate is cleared; rollback may
  pay one full DFS to recreate it. Replacement clears synchronizer history and is still
  validated/fallback checked exactly.
- Persistent cost is one `uint32_t` per stable event per lazy check, outside evaluator
  undo and snapshot-equivalent state. This is a deliberate `4*N*C` RSS risk and must
  pass the frozen 1.02 memory gates; storing macro-edge adjacency is excluded.
- Protocol is frozen at
  `optimization-analysis/continuous/incremental-topological-certificate/experiment-protocol.md`.
- P0.8a passes all correctness gates but has zero certificate hits on the first complete
  pilot repetition: affected TSO/PSO full-check ratios are all exactly 1.0. Almost every
  useful query grows the stable event universe, and the initial short-vector rule falls
  back before validation. The deterministic opportunity gate cannot pass, so the run is
  stopped early and archived.
- P0.8b assigns newly appended stable IDs provisional ranks after every existing rank,
  then validates every current extensional edge. This favors common old-to-new program
  order without assuming it: any new-to-old or otherwise non-forward edge triggers the
  original full DFS. Space remains `4*N*C`; rollback and reactivated IDs are revalidated.
- P0.8b server correctness repeats after the semantic refinement: Release 159/159,
  GCC 13 ASan+UBSan 5/5, mutation 39/5,441 and broad 852 matches / 12 mutual
  unsupported / 0 mismatch. Layout remains 504-byte evaluator versus 496-byte baseline;
  statistics and result objects are unchanged.
- BenchExec does not preserve the custom regex columns in these pilot XMLs. The strict
  P0.8 analyzer therefore pairs on XML task/model/repetition but extracts execution,
  snapshot/base and lazy work counters from the matching `logfiles.zip` member. This
  fixes a weakness in the older pilot-only analyzer rather than changing experiment data.
- P0.8b no longer uses the two selected programs as a performance advancement gate.
  Such a gate can miss large-program TIMEOUT/OOM behavior and overgeneralize a local
  model/task effect. Correctness remains guarded by Release 159/159, sanitizer 5/5,
  mutation 39/5,441 and broad differential 852/12/0. Performance is decided directly
  by the established 96-task x 3-model x before/after x 4-repetition formal matrix.
  The stopped pilot data is retained only for audit and is excluded from conclusions.
- P0.8b formal result rejects the static certificate. The 2,304 cells / 1,152 pairs have
  zero semantic, status, category, execution, snapshot/base or coverage mismatch, but
  TSO and PSO complete lazy checks are exactly unchanged in every counted cell
  (1,787,856 and 5,246,664 total on each side). CPU ratio 0.996569 has bootstrap 95% CI
  [0.992152, 1.000971], and PSO `queue_ok_longest` raises the large-task maximum RSS
  ratio to 1.051310. The append-stable order does not match real edge mutations, so it
  stores ranks without replacing DFS work. Production/test source is restored; exact
  rejected patch SHA-256 is
  `9b8bdcf3856c7ee95448e08ce40e13fad6fad2055945fde85e70a2087bcfa779`.
- Full P0.8b baseline-log census: 1,152 cells, 1,076 with final CAT statistics and 76
  TIMEOUT cells without final statistics. SC/TSO/PSO record zero incremental inserts,
  rollbacks, rollback-inserts, replacements and candidate transitions. Rebuild counts
  are only 8/8/128; unchanged transitions are 319,352/319,352/189,768. A rollback-EOG
  implementation has a measured upper-bound opportunity of 0% on the established
  formal path, so it is deferred rather than implemented speculatively. Evidence:
  `optimization-analysis/continuous/rollback-eog-delta/opportunity/`.
- P0-A V3/V4 only skip repeated evaluator queries; they do not reduce GenMC's queued
  RF/CO revisits or suffix replay. P0-B V1 will reuse V3's certified positive clauses
  but only prefilter forward RF revisits. It first requires an exact
  `rf(source,read)` literal, then materializes the exact hypothetical cut-prefix and
  matches the complete clause. This avoids hand-coded preservation assumptions for
  `po/co/fr/loc/int/ext` and fails closed for every unsupported revisit.
- The dequeue-time P0-B prototype was stopped before server experiments after the
  objective was sharpened to candidate-space reduction. It could avoid cut/replay of
  an already queued inconsistent RF choice, but could not reduce initial alternative
  enumeration or merge consistent execution classes. Its exact 77,394-byte patch is
  archived under `conflict-directed-rf-pruning/`; all production/test/CLI paths are
  restored. The next evidence must quantify generated RF/CO choices, queued revisits,
  realized prefixes, decision-subtree conflict coverage and duplicate EOG projections.
- Native GenMC is already the implementation line of TruSt's sound/complete/optimal
  RF-DPOR, and symmetry reduction is enabled by default through SPORE utilities. The
  likely extension-specific exploration gap is `BasicCATChecker`'s conservative generic
  candidate enumeration when no automatically proved CAT constraint is available.
  Therefore the next census counts offered/queued RF, CO and backward revisits plus
  worklist peaks and immediately inconsistent prefixes; it does not search for ordinary
  schedule duplicates already excluded by the base algorithm.
## Candidate-space census result and next theorem (2026-07-17)

- Corrected formal-96 run: SC 95/96 terminal, TSO 90/96, PSO 84/96; no incorrect
  verdict. All 288 accounting rows satisfy queued/offered, popped/added, and
  rejected/realized invariants.
- Post-generation inconsistency is 161/180,482 (0.09%) under SC,
  161/180,472 (0.09%) under TSO, but 301,191/398,054 (75.67%) under PSO.
- Maximum retained work is 801 for SC/TSO and 51,601 for PSO among terminal logs.
- Decision: implement a normalized-CAT structural certificate for an irreflexive
  linear-recursive closure over a union of choice-independent edges plus structurally
  recognized rfe/fr/co. Use exact current reach to reject only edge reversals before
  RF/CO candidate enqueueing. Fail closed for every other grammar.

## Generic preventive V3--V6 evidence (2026-07-17)

- V3 used two certified lazy sparse orders: 13,465,988/16,175,796 choices pruned,
  three stable TIMEOUT-to-TRUE gains and zero OOM, but CPU 1.06106 and RSS 1.03378.
- V4 selected one certificate structurally (external RF before none/all):
  14,187,204/18,058,008 choices pruned, three TRUE plus one FALSE stable gain, zero OOM,
  CPU 1.05457 and RSS 1.03439. Replaying the selected lazy edge expression solely for
  preventive reach remained the dominant avoidable work.
- V5 selected the same certificate but published a dense order and dense closure. Four
  repeats totaled 20 OOM, and `queue_ok_longest` lost a baseline TRUE in every repeat.
  CPU 1.02375 and RSS 1.02362 do not offset the deterministic coverage loss. Exact
  decision: `optimization-analysis/continuous/candidate-space-census/v5-decision.md`.
- V6 retains the same sufficient reversal theorem and certificate selection. When the
  selected lazy check is acyclic, its existing DFS buffers each source's exact unique
  successors and publishes a CSR relation. A cycle publishes no partial relation. The
  checker derives forward and reverse reach only around the current read/write, using
  a temporary inverse CSR, then discards all O(E) auxiliary storage except the captured
  root already needed for those queries and two O(V) bitsets.
- Incremental insertion replaces the captured CSR after the exact lazy check. Rollback
  deliberately reconstructs it once after all undo deltas instead of mutating sparse
  CSR edges through generic removal, which would densify to O(V^2). Random finite-input
  tests compare captured and fully materialized relations; a dedicated test checks
  insertion, rollback and offline-oracle equality.
- Local `RelWithDebInfo` compilation of `genmc` and `unit_tests` passed. This is compile
  evidence only; per the execution constraint, all tests and experiments run in the
  server Docker container.
- Final V6 server gates: Release 161/161; GCC 13 ASan+UBSan 161/161; mutation oracle
  39/5,441; broad differential 852/12/0 over 864 pairs. The formal protocol uses four
  balanced repetitions with 24 baseline and 24 V6 BenchExec workers simultaneously;
  odd/even repetitions exchange core ranges 0--23 and 28--51.
- V6 formal confirms real exploration reduction on 336 common-accounting cells:
  RF+CO offered -67.82%, queued -81.74%, work added -77.53%, work popped -73.97%,
  realized prefixes -75.67%, and max retained work 51,601 -> 801. Four tasks become
  terminal per repetition, with no OOM, loss or safe-count mismatch.
- V6 still has CPU 1.04146 [0.97512,1.13572] and RSS 1.03698
  [1.00010,1.09524]. The theorem is retained, but the representation advances to V7.
  Direct CSR rows remove global sorting/deduplication of 16-byte edge pairs. Each DFS
  frame already owns a sorted unique successor row; on completion V7 moves it into its
  source slot and later flattens 32-bit rows linearly into CSR.
- V7 improves candidate CPU by 2.40% [1.29%,3.57%] and wall by 2.20%
  [1.06%,3.41%] versus V6, with 0/384 cell-level differences across 12 search counters.
  It still sits at CPU 1.02416 and RSS 1.02792 versus simultaneous baseline.
- V8 builds reverse offsets/targets only for the retained preventive sparse root.
  Forward targets and reverse counts share the first row pass; a second row pass fills
  sorted predecessors. This replaces the prior post-evaluation two-pass scan and
  temporary inverse CSR. Ordinary sparse/structural/dense relations retain an exact
  membership-scan fallback for `nextPredecessor`, so the API is semantic rather than
  model-dispatched.
- V8 formal preserves the four stable coverage gains and all search counters. Directly
  against V7, candidate CPU is 0.98477 [0.97280,0.99650], but against simultaneous
  baseline CPU is 1.00796 [0.94325,1.09232] and RSS 1.02600 [0.99979,1.06664]. It is a
  positive representation change but not yet a default optimization.
- The largest remaining V8 outlier is `pthread-demo-datarace-3.yml`: about 0.18 s
  baseline versus 2.14 s candidate with the same 41 popped work items. Preventive mode
  executes 1,719 prefix queries and 1,759 offline evaluations to remove a queue tail
  that early error termination never consumes. V9 must retain pre-enqueue pruning while
  evaluating only the analyzer-certified structural root, leaving authoritative CAT
  consistency state untouched.
- V9 uses a separate stable-ID adapter restricted to base predicates reachable from the
  selected lazy root. It calls `findLazyCycle` directly and retains the exact
  bidirectional CSR only for an acyclic root. Cyclic, missing-base, unsupported,
  explanation and oracle paths fail open or retain the complete evaluator path. The
  authoritative synchronizer/evaluator is never updated by this pre-enqueue query.
- V9 correctness evidence is Release 161/161, GCC 13 ASan+UBSan 161/161, mutation
  39/5,441, and broad 852/12/0. The random lazy property now also fills base leaves only,
  invokes the direct interpreter, and compares the exact relation/cycle witness with the
  complete materialized evaluator.
- V9 formal has the same four stable coverage gains and exact search-space counters as
  V8. Against baseline CPU/wall/RSS are 0.97910/0.97302/1.00787, but CPU CI crosses 1.
  Direct V9/V8 candidate CPU is 0.96105 [0.93854,0.98130] and wall 0.95698
  [0.93535,0.97650], with baseline CPU drift 0.99848.
- V9 removes 66.89% of V8's full offline evaluations and 76.88% of offline evaluator
  time. The remaining 1,231,356 direct prefix queries consume 468.29 s in preparation.
  CO/RF lookup is only 7.84 s, so interval-only lookup optimization is deferred. V10
  must reuse a proof across descendants/siblings and measurably reduce query or candidate
  counts.
- Candidate-reduction priority was clarified on 2026-07-17. There are three distinct
  effects: evaluator acceleration, rejection before enqueue, and subtree/equivalence-
  class elimination. V9 already reaches the second level: relative to no preventive
  pruning it reduces RF+CO offered 67.82%, queued 81.74%, work-added 77.53%, popped
  73.97%, and peak queued work from 51,601 to 801. V10 is worthwhile only if a learned
  positive core is matched early enough to reduce direct-root queries or offered/queued
  work; merely skipping a full evaluator on an already materialized candidate is not an
  exploration result. The higher-priority successor is rollback-scoped prefix blocking
  with decision levels/backjumping, followed by a model-aware RF-equivalence/EOG
  quotient refined when future behavior cannot yet be proved equal.
- Source audit corrected two prospective claims. Native `getSymmetricTidSR()` already
  auto-certifies ordinary thread creates with the same parent, function and argument
  when no memory access lies between the creates; explicit `spawn_symmetric` is not the
  only path. Native GenMC is also the TruSt RF-DPOR implementation. Consequently the
  next coarser search-space question is reads-value-from/value-centric equivalence, not
  reimplementing ordinary RF-DPOR or automatic same-argument symmetry. Instrumentation
  counts post-filter RF candidates sharing both raw value and provenance, but treats
  that only as an optimistic bound: event-set equality, causal read order and CAT-model
  behavior remain mandatory before any merge.
- Optimization ordering was corrected after user review: candidate-execution quotienting
  is P0; learned CAT query avoidance is not. The concrete first target is an SC
  RVF/value-centric representative construction with three mandatory keys/proofs:
  identical event set, identical observed values, and identical causal order restricted
  to reads. TSO/PSO and generic CAT are separate proof stages because preserved values
  do not by themselves preserve buffer visibility, coherence/from-read constraints,
  CAT acceptance, or future enabledness. A reduction is credited only when GenMC offers,
  queues, pops or realizes fewer RF/CO/revisit candidates with unchanged exhaustive
  verdicts; evaluator-call savings are reported separately.
- The first balanced-NUMA full-725 opportunity census is invalid for a launch-only
  reason: the fair adapter defaults `GENMC_MODEL_ROOT` to `.`, so generated commands
  used `--model-file=recursive-{sc,tso,pso}.cat` outside the model directory. Logs show
  ERROR 17 with “CAT model file does not exist”; Docker inspect reports
  `OOMKilled=false` and the host had about 734 GiB available. The corrected launcher
  exports the absolute server-side source `models/cat` path. No verdict or performance
  row from the invalid launch is used.
- Primary-source audit found the historical RVF-SMC implementation at
  `ViToSVK/nidhugg`, branch `reads_value_from`, commit
  `3f20b4f169a1bf26d50f478d2681ba23a605be8b` (2021-04-20). It confirms that the
  implementation is a distinct recursive exploration engine with annotated traces,
  partial-order closure/linearization, ancestor backtrack signals and interpreter replay;
  it is not an RF-list filter. The branch is GPLv3, so it is not a code source for
  Apache-2.0/MIT GenMC. The published CAV 2021 algorithm/proof is the implementation
  specification. Its conclusion explicitly leaves relaxed-memory extension as future
  work, so the initial proof scope is SC only.
- The first implementation component is
  `genmc/Verification/SCGoodWritesSolver.{hpp,cpp}`. It implements the published base
  VerifySC witness-state search rather than concrete-RF filtering. State equality is
  structural: the exact executed-event bitset plus the active writer's thread for every
  variable; hashes are lookup accelerators only. Executability enforces thread/extra
  predecessors, active-good-write reads, and held-variable write blocking. The new unit
  test exhaustively compares all nine nonempty pairs of good-write subsets in a
  two-thread/four-event instance against direct enumeration of SC linearizations.
  macOS compiled `genmc` and linked the complete `unit_tests` binary but did not execute
  it, per the experiment-location constraint.
- Corrected full opportunity census: PSO ran 03:22:36--03:29:37 UTC, SC
  03:29:37--03:35:52, and TSO 03:35:52--03:42:13, each as one 48-worker queue over
  725 tasks. All run sets exited 0. Local raw evidence is
  `candidate-space-census/server-results/rvf-opportunity-full-725/`; analysis reports
  2,175 rows and zero counter invariants violated. Same-value upper-bound totals are
  SC 12,868,426/67,978,223 (18.93%), TSO 9,775,191/57,718,147 (16.94%), PSO
  2,787,706/23,005,964 (12.12%). Counter-bearing TIMEOUT aggregates are about 28--30%,
  but a strict task-level sensitivity analysis shows this is substantially confounded
  by search size: among rows with at least 10,000 RF candidates, the median TIMEOUT
  differences are SC +0.62 pp, TSO +0.37 pp and PSO -0.77 pp. The allowed conclusion is
  opportunity magnitude in the hard cohort, not a causal prediction of timeout removal.
  The strict bundle is `candidate-space-census/analysis-rvf-timeout-effect/` with report,
  statistical appendix, figure catalog and two SVG figures.
- The server Release and GCC 13 ASan+UBSan focused gates both pass all 5
  `SCGoodWritesSolver.*` tests in 5 ms. The expanded exhaustive oracle covers all 15
  nonempty read/write shape masks for two threads and four events, every nonempty
  `GoodW` subset assigned to each read, and direct replay of every returned witness.
  The full unit binary's `free(): invalid pointer` is independently reproducible in the
  pre-existing RapidCheck `IntervalMapPropertyTest` with the new suite excluded; it is
  not evidence against the solver and remains separate infrastructure debt.
- The first exploration-state layer implements the formal `VisibleW_PO` condition with
  both negated order tests intact, causal-map thread-prefix cutoffs, and strict-LIFO
  ancestor signals. A lightweight graph adapter maps real labels by `Event` and virtual
  initial writes by `SAddr`, orders initial writes before real thread prefixes, carries
  create/join predecessors, and rejects RMW/non-atomic prefixes by returning an error so
  the eventual driver can fail open. Its initial server Release and sanitizer gates pass
  11/11. VerifySC now also returns the concrete active write selected for every read, and
  the adapter can apply that RF map to a graph clone only after validating every endpoint.
- 2026-07-17 SB end-to-end diagnosis: `Action.event` is the position before executing a
  load, while its `ReadLabel` is at `Action.event.next()`. The RVF scheduler compared
  these different coordinates and repeatedly selected the same load. After switching
  to `.next()`, recursive-SC baseline and RVF with one/two workers all exit 0 and explore
  3 executions. Counters report 4 attempted and 4 reduced loads, 5 representatives, and
  zero fail-open events.
- The sanitizer gate then exposed null-dereference UB in the generic
  `Interpreter::getDepTracker()` accessor (`&*dynState.depTracker`). Returning
  `dynState.depTracker.get()` preserves non-null behavior and makes existing guards
  effective. The rebuilt GCC 13 ASan+UBSan SB run completes with no sanitizer diagnostic;
  focused solver/state/adapter tests remain 11/11.
- During the server outage, the final local paired differential bundle contains 120 runs
  across 30 programs. All semantic verdicts match and all enabled runs have zero late
  fail-open, but the enabled cohort grows from 101 RF-DPOR executions to 108 RVF
  executions and the short-run median elapsed ratio is 1.0084×. This is a negative
  performance checkpoint, not an optimization claim.
- Prefix-local dense adapter IDs were invalid causal-map persistence keys. Stable GenMC
  event keys now own causal cutoffs. A loop performance gate prevents the observed
  `fib_bench` explosion, and unsupported programs retain native configuration and the
  direct one-worker path.
- Four SC-SB reachability probes cover all Boolean observations. After hard RVF errors
  were finalized directly on their witness graph, the probes passed 30 Release loops,
  10 ASan+UBSan loops and 10 TSan loops without the former error-replay crash.
- The local CTest registration exposes both SC-RVF integration suites. A dedicated
  `sc-rvf-fallback` regression compares native and requested-RVF `fib_bench` output,
  requires the explicit loop-gate reason and zero RVF work, and gives each process a
  15-second hard timeout. A preliminary interactive probe failed only because zsh
  reserves `status`; rerunning with `exit_code` confirmed both paths exit 42 and report
  exactly 43,194 complete executions.
- An additional 40-case local manifest exercised 160 baseline/RVF worker cells. The
  strict analyzer reports 35 enabled, 5 fallback, no timeout and no violation. Enabled
  totals are 294 baseline versus 298 RVF complete executions, with 584 reduced loads
  and a 1.0080× diagnostic median elapsed ratio. Reductions (`IRIWish` 27→16) coexist
  with growth (`LB3` 7→18; `TC2` 4→7), making LB representative generation the next
  algorithmic target rather than a reason to broaden fallback.
- The first post-gate CTest run correctly failed the old integration assertion: SB has
  no multi-source same-value class, so it no longer increments `rvf-loads-reduced`.
  The integration fixture now uses `IRIWish` and checks equal verdict, deterministic
  RVF worker count, a strict 27→16 reduction, positive reduced-load count, and zero
  fail-open. Outcome and fallback tests were already passing.
- Root cause of the LB growth: RVF treated singleton value classes as reductions.
  `LB3` recorded 18 visible sources, 18 groups, 18 representatives and 16 parent
  continuations. A first-visit local gate now uses native RF-DPOR unless some group has
  at least two sources. Native reads are synthesized from the current graph into later
  RVF problems, avoiding stale persisted RF constraints. Repeating 160 cells yields
  294→279 complete executions, 11 actually reduced loads, no former growth and no
  invariant failure; the original 120 cells become exactly 101→101 with zero reductions.
- The post-gate local ASan+UBSan binary passes `sc-rvf-integration.sh` on `IRIWish` and
  `sc-rvf-outcomes.sh` on all four observations. Linking prints pre-existing malformed
  DWARF warnings, but the build succeeds and neither run emits sanitizer diagnostics.
- The first exhaustive IRIWish outcome-script attempt stopped at outcome 0 because its
  compact `for` loop around `pthread_join` activated the whole-program loop fallback;
  it compared native against native and therefore was invalid RVF evidence. The joins
  were unrolled before rerunning the outcome matrix.
- With unrolled joins, exhaustive IRIWish outcome testing compares all 32 five-bit
  observations under baseline, RVF/n1 and RVF/n2. Exactly 16 are reachable, and all 32
  statuses agree across modes/workers (96 invocations), with gate enabled and zero
  fail-open. The same matrix passes with local ASan+UBSan. Four registered SC-RVF CTests
  pass 4/4 in 9.35 seconds.
- Full outcome probes now cover every program with a measured post-gate reduction.
  `WRC+dep` reachable encodings are `[0,1,4,5,7]`; `MP+rels+acq` `[3,4]`; and
  `S+rels+acq` `[2,5]`. Across 20 outcomes and three modes, all 60 statuses agree and
  every RVF cell is enabled with zero fail-open. The probes pass with Release,
  ASan+UBSan and TSan; the registered end-to-end set passes 5/5 in 12.96 seconds.
- Replace `if (&*errView)` in DOT error reporting with `if (errView)`. The former forms
  an invalid reference when empty and Clang diagnosed it in every sanitizer build. The
  warning disappears after the change. Release and ASan+UBSan RVF error runs both exit
  42 and emit a 1,703-byte DOT; no sanitizer diagnostic appears. The repeated 160-cell
  differential remains 294→279 with 11 reduced loads and no violation; CTest is 5/5.
  A preliminary cleanup-bearing command was rejected by local safety policy and ran no
  test; the accepted retry deliberately leaves its two `/tmp` DOT/log pairs.
- Automatic local discovery covers 185 C litmus/racy variants and 740 paired cells.
  Final result: 184 runnable, one paired compile failure (`wrw0.c` needs macro `N`), 108
  enabled, 76 fallback, no timeout or invariant violation, and enabled search 1,308→1,283
  at 17 reduced loads. Fallback reasons are 47 non-atomic store, 22 RMW, 6 assume/IPR,
  and 1 malloc. Median local elapsed ratio is 0.9940× and remains diagnostic only.
- The first broad pass caught a false native fallback: RVF disabled `loadAnnot` before
  discovering an assume, so `WWR+2WR` lost its IPR-upgraded warning (42→0). Pre-transform
  assume detection plus native load-annotation lowering restores exact 42/42 behavior;
  the rejected partial IPR-semantics split was removed. Runner parsing was also changed
  so empty verdicts remain recorded instead of ending the batch.
- Additional exhaustive outcomes pass for `cumul-release` (`[0,1,4,5,7]`) and
  `rel-B-cumul-acq` (`[0,1,4,5,7,8,9,12,13,15]`) under Release and ASan+UBSan.
- After rebuilding TSan for the pre-transform gate/config changes, `WWR+2WR` fallback,
  IRIWish 32-outcome enumeration, and all five reduced structures pass without a TSan
  report. The reduced TSan command appeared to return after `WRC+dep`, but process/log
  inspection showed it was still running; its 120-second guarded run completed every
  structure successfully. Release CTest is 5/5 in 16.49 seconds.
- RVF overhead counters show 1,020 singleton/native bypasses and only 17 actually reduced
  loads across the 185-case automatic corpus. Before the raw-value prefilter, later RVF
  problem construction synthesized 1,875 native reads; checking the raw store superset
  for repeated `(value, provenance)` classes first reduces that work to 11 (99.41%). A
  visible-write subset cannot introduce a duplicate absent from its superset, so the
  early native continuation is exact. Both 740-row matrices have zero timeout/violation
  and the identical 1,308→1,283 complete-execution result.
- Post-prefilter local gates: Release CTest 5/5 in 16.66 seconds, focused RVF unit tests
  11/11, and exhaustive IRIWish plus five reduced-structure outcomes pass under both
  ASan+UBSan and TSan. The final TSan log is
  `/tmp/genmc-rvf-prefilter-tsan-tests.log`. Two initial rerun attempts were invalid and
  launched no GenMC test: one omitted required script arguments; one named a nonexistent
  build path. A third used relative include paths and stopped at compilation status 5.
  The authoritative run used absolute binary/model/template paths and completed all six
  printed reachable-outcome sets without a timeout or TSan report.
- Local median RVF/n1 ratios are 1.0060× before and 1.0087× after the prefilter. These
  short compilation-dominated measurements are noise-level diagnostics, not evidence of
  a wall-clock benefit. Server testing remains paused after the outage.
- Final local hygiene rerun: shell syntax, analyzer `py_compile`, and `git diff --check`
  pass; the five registered SC-RVF CTests pass 5/5 in 16.31 seconds; the correctly
  filtered RVF unit set passes 11/11. An initial unit command supplied a Catch2-style
  positional filter to a GoogleTest binary, so it unintentionally ran all 174 tests.
  The 11 RVF tests passed, while the unrelated
  `ConfigModelFileTest.RequiresStructuralCertificateForSCValueExploration` retained old
  expectations for six native configuration fields and failed (173/174 overall). This
  result is recorded rather than reported as a full-suite pass; it does not invalidate
  the subsequent authoritative 11/11 focused run.
- The raw prefilter's `std::map<ValueKey,size_t>` performed node allocation and scanned
  all sources even though only the first repeated class matters. A provisional follow-up
  uses `SmallVector<ValueKey,8>` and returns from the scan at the first duplicate. Its
  740-row matrix preserves all aggregate values exactly (1,308→1,283 complete, 17
  reduced, 1,020 bypass, 11 synthesized), with zero timeout/violation. Median local
  ratio is 1.0069× versus 1.0087× for the map version, so no timing benefit is claimed.
- Post-candidate gates pass: focused unit 11/11, Release CTest 5/5 in 17.03 seconds,
  ASan+UBSan and TSan IRIWish plus five reduced-structure outcome suites. Logs are
  `/tmp/genmc-rvf-smallvector-{asan,tsan}-tests.log`.
- User requested a pause after this round because the current optimization direction
  feels questionable. No subsequent candidate or server experiment should start until
  the direction and acceptance metric are reviewed. The SmallVector edit remains
  uncommitted/provisional in the worktree.
- Frozen-contract review after direction reset: the RVF witness graph is submitted as a
  `ThreadPool` task and therefore does go through the normal graph-driven interpreter
  replay; recursive-SC CAT consistency is checked on the rebuilt graph before submission.
  Primary-source rereading corrected the initial interpretation of dead
  `AncestorSignals`: the signal is a redundancy-pruning heuristic. Production
  conservatively always queues a parent continuation; representative children clear
  `processedReads`, corresponding to always backtracking and re-enabling ancestor reads on
  enlarged event sets. This may over-explore but is not by itself a completeness loss.
  Hand-written finite outcome probes still cannot prove that every bounded maximal SC
  execution is represented, and n1/n2 equality does not prove task decomposition.
- Removed the provisional SmallVector raw-prefilter follow-up. It preserved every search
  counter and showed no reliable timing benefit, so retaining it as a milestone would
  repeat the post-outage objective drift. The map-based raw prefilter remains as part of
  the current prototype, not as a separately accepted optimization.
- Updated the config unit expectation: `Config::validate` must preserve native options;
  RVF overrides are applied only after transformed-program certification in `adjustConfig`.
  This keeps unsupported whole programs on the exact native path.
- The first generated-state oracle smoke reported one verdict-hash difference, but log
  inspection proved it was a harness defect: non-atomic observation globals forced native
  fallback, and RVF/n2 was compared to baseline/n1, which differed only by an unordered-
  writes warning. That 243-invocation run is quarantined. The corrected generator uses
  atomic observation slots and baseline/RVF x n1/n2 same-worker pairing.
- The valid one-worker generated oracle passes 81/81 states for one canonical shape, but
  the four-shape expansion finds two real losses: `000001/1112` and `000011/1012`.
  Native exits 42 with the requested final/read state; RVF exits 0. Applying the current
  VerifySC prefix witness's write order to the clone passed 175/175 units but left both
  failures unchanged, so that attempted repair was removed. The missing invariant is now
  localized beyond RF selection: the online prefix task decomposition must account for
  future coherence placements/events after the quotient frontier. Formal server timing is
  blocked until this correctness failure is repaired.
- The loss is repaired by refining each value/provenance group with whether its source is
  from the read's own thread, and by applying VerifySC's witness-derived per-location
  write order before RF on the representative clone. The first condition prevents the
  unsound merge exposed by `000001/1112`; the second prevents witness/clone mismatch and
  162 late model fail-opens. Local 20-shape n1 is 3,240/3,240 calls with zero violations.
  Server Release n1/n2 is 6,480/6,480 with zero timeout, late fail-open, worker mismatch,
  or state mismatch; server ASan+UBSan focused is 13/13 and four-shape oracle is 648/648.
  The server 185-case differential is 740 rows with zero invariant violation and retains
  the enabled-cohort reduction 1,308→1,283 at 17 reduced loads. Formal timing remains
  gated on mutation and broad correctness.
- The remaining server correctness gates now pass. The mutation oracle reports 39 rows
  and 5,441 full recomputations. The broad recursive-SC/TSO/PSO differential reports
  864/864 matches, zero mismatch and zero unsupported pair. Every Docker container used
  a unique name, `--rm`, and an outer timeout.
- The frozen protocol's quotient-disabled instrumentation control is now explicit rather
  than approximated by whole-program fail-open. `--sc-rvf-disable-quotient` requires
  `--sc-rvf-exploration`, retains the certified program gate, RVF thread-pool/scheduler
  setup and per-read handling, but routes each read through the complete native RF-DPOR
  branch before any value grouping, adapter, VerifySC, or representative submission.
  A separate `rvf-quotient-disabled-loads` counter prevents those reads from being
  mislabeled as singleton bypasses. On IRIWish the local control and baseline both explore
  27 executions with identical RF/CO/work/validity counters; the control records 31 reads,
  zero VerifySC calls and zero representatives. The two focused config tests pass and the
  local `genmc` binary links successfully.
- The first formal-full Docker launch was invalid and ran zero tasks: its preflight disk
  check queried `/data3`, which is the container's 98 GiB overlay because only
  `/data3/sujie` is bind-mounted. Host and container both report 270 GiB free for the
  actual experiment filesystem at `/data3/sujie`. The retained diagnostic container was
  removed, there was no result directory or GenMC/BenchExec process, and the launch guard
  now checks the mounted path.
- The first complete 5-configuration process produced valid SC baseline/control/RVF rows,
  but its TSO/PSO controls exposed a wrapper-contract error. The fair adapter expands
  `--svcomp-backend` into `--model-file` before invoking `GENMC_BINARY`; replacing the
  backend token in the wrapper was therefore a no-op. TSO repeated recursive SC, and all
  PSO-V9 rows returned configuration error 17 because preventive pruning saw recursive SC.
  Those 1,450 cells are excluded. The wrapper now replaces the expanded recursive-SC model
  path with recursive-TSO/PSO, and a separate launcher reruns only the two invalid controls.
- The valid formal result contains 2,175 SC cells plus 1,450 corrected TSO/PSO controls.
  SC baseline/control/RVF have identical status/category, 120,187 complete executions and
  every recorded candidate/work counter. RVF activates on zero of 114 executed formal
  tasks: 53 abort, 40 modeled mutex lock, eight non-atomic store, four loop and one
  non-atomic load fallback; eight compile/property rows record no gate. On 90 common solved
  rows RVF/baseline CPU is 1.00281 [0.99650, 1.00864], wall 0.99556
  [0.96314, 1.01884], and RSS 0.99951 [0.99856, 1.00021]. These are zero-activation drift,
  not optimization benefit. The candidate is therefore revised/not retained; the next
  optimization returns to V9's full-workload candidate-subtree direction rather than
  widening RVF support without a proof.
- V10 conflict-core implementation begins from the frozen protocol, not from a verdict
  cache. `ConflictCoreDatabase` stores only sorted positive ground base facts, rejects
  empty/oversized clauses, removes supersets, bounds itself to 4,096 clauses x 64 literals,
  and matches every literal exactly against the current normalized base values plus a
  proposed candidate delta. `deriveLazyEdge` recursively produces deterministic positive
  provenance through base/alias/union/intersection/composition/identity/optional nodes and
  fails closed on unsupported forms. The existing randomized lazy-relation property now
  verifies that every materialized edge has a derivation whose literals are actually
  present. Focused property/database tests pass 3/3.
- Production integration is guarded by `--cat-conflict-cores`, which is rejected unless
  `--cat-preventive-pruning` is also enabled and the V9 structural certificate exists.
  RF/CO candidates are matched against learned positive cores before the V9 direct root
  test; every V9-rejected candidate with supported provenance attempts to learn a core.
- An 80-fixture local PSO scan found real RF and CO preventive-pruning paths. Strict V9/V10
  comparisons on `CoRR2/corr20.c`, `IRIWish0.c`, `LB+fr-fr-data+data0.c`, and
  `assume-ctrl0.c` preserve complete executions, RF/CO offered and queued counts, and
  work added/popped. V10 core hits are respectively 36, 3, 4, and 2; `CoRR2` learns six
  clauses containing 32 literals (512 literal bytes). All four record zero complete
  direct-root checks avoided, so this evidence demonstrates proof reuse but no search-space
  reduction or subtree elimination.
- A full local CTest run finished with 194/195 passing tests. `run-parallel` fails immediately because
  the already-configured test refers to absent `scripts/run-parallel.sh`; it is unrelated
  to V10. The ten-iteration randomized driver passed in 842.50 seconds. The first scan
  attempt produced no data because zsh reserves `status` as a
  read-only variable; the corrected script uses `rc`.
- The exact V10 source/test list was synchronized to the dedicated server copy at
  `/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/source`; local and
  remote SHA-256 for `ConflictCore.cpp` match. No user-owned broad script was synced.
- Server GCC 13 Release rebuilt successfully and passed an expanded 185/185 gate covering
  all unit/property tests plus CLI, SC/TSO/PSO CAT differential, recursive differential,
  and mutation stress. GCC 13 ASan+UBSan passed 14/14 focused CAT/V10 tests with leak and
  undefined-behavior halting enabled.
- The first Release container mount used `/work`, conflicting with absolute paths in the
  existing CMake cache; it exited before compilation and was rerun at the recorded path.
  The first sanitizer configure attempted a RapidCheck GitHub update and timed out before
  compilation. Reconfiguration points FetchContent at the complete Release-build
  RapidCheck source copy; sanitizer build and tests then passed without network access.
- The complete actual workload, not the 96-task diagnostic set, rejects V10. The valid
  isolated-rewrite run uses concurrent V9/V10 725-task PSO units with 60 s/4 GB per task
  and disjoint 24-core pools. Both units exit 0. Four V9-completed false tasks become V10
  timeouts and one V10 false result crosses the time limit. Among 368 common terminal
  correct tasks, status, complete executions and search counters match; CPU V10/V9 is
  1.049916 [1.025769, 1.077275], wall 1.048947 [1.024901, 1.076183], RSS
  1.000341 [0.999941, 1.000849]. V10 hits 14,193,035 learned cores in 158 tasks but avoids
  zero direct roots; its literal probes total 8,600,686,460. This is an evaluator/proof
  cache cost with no candidate-subtree elimination, so V10 is rejected. Full decision:
  `optimization-analysis/continuous/candidate-space-census/v10-decision-20260718.md`.
# V11 correctness audit (2026-07-18)

- The original same-epoch premise is false in the current driver. In unbounded mode,
  `GenMCDriver::findConsistentRf()` accepts its final RF candidate and
  `GenMCDriver::findConsistentCo()` accepts its maximal CO placement without calling
  `isExecutionValid()`; both rely on maximal extensibility.
- `getCoherentStores()` runs after a fresh read label is appended but before RF is set.
  `getCoherentPlacings()` runs after a fresh write is appended but before CO is added.
- Consequently a checker-local "last checked epoch" cannot justify skipping V9's global
  selected-root cycle scan. Doing so would silently strengthen the generic CAT contract.
- Safe revised direction: require an analyzer-produced unassigned-focus sink certificate
  for the selected positive root. Only a proven root sink can use focus-only forward and
  reverse lazy reach; every unsupported or ambiguous case delegates the whole query to V9.
- The production-independent `findLazyReach` primitive now interprets exact forward and
  transposed-root reach. Reverse base enumeration uses `nextPredecessor`; composition
  reverses operand traversal. A RapidCheck property compares both directions for every
  focus against the fully evaluated root relation and passes (34 ms in the focused run).
- The first oracle draft incorrectly treated `findLazyCycle(..., materialized)` as a full
  materializer. It may stop at the first cycle, so that relation can be partial. The test
  now uses `CaatEvaluator`'s complete root value; the saved failing case no longer fails.
- V11 is opt-in as `--cat-focus-reach` and requires `--cat-preventive-pruning`. The
  analyzer proves fresh thread-suffix sink status through alias/union/intersection/
  difference/composition/transitive-closure structure and fails closed for optional,
  identity, lifecycle/unknown bases and recursive ambiguity. The runtime rechecks suffix,
  missing RF/CO and absence of write readers before using the certificate.
- Server GCC 13 Release focused certificate/lazy tests pass 10/10. The broader quick gate
  passes 192/194; only the pre-existing relinche generators fail because the dedicated
  source tree lacks `queue_spec_*.in` and `stack_spec_*.in` artifacts.
- V11-enabled broad differential passes 288 programs x SC/TSO/PSO = 864/864 match,
  zero mismatch and zero unsupported.
- GCC 13 ASan+UBSan passes 14/14 focused tests. A V11 CoRR1 smoke has 36 complete
  executions, 21/21 final focus-reach successes, zero fallback, 20 emitted reach
  candidates and no sanitizer diagnostic. The matching V9 final run has the same RF/CO
  offered/pruned counters but performs 21 full-root checks and emits 1,169 candidates.
- The complete paired 725-task V9/V11 run is active in container
  `v11-full-725-sujie`, output
  `/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/formal-results/v11-full-725-20260718a`.
- The A run is excluded: V11 selected a different preventive certificate and caused
  31 common-correct search-counter differences. It was useful for detecting the
  protocol violation but is not performance evidence.
- The corrected same-certificate B run is authoritative: 725 tasks per variant, both
  status 0, zero common-correct complete/search mismatch. V11 activates on 216 tasks,
  replaces 1,205,405 full-root checks / 2,943,514,438 root candidates with 571,899
  successful focus queries / 489,083,393 emitted candidates, but CPU is 1.108462
  [1.067238, 1.154924], wall 1.111303 [1.069816, 1.156717], and RSS 0.999895
  [0.999670, 1.000116]. Eighteen V9-completed tasks become resource failures.
- Decision: reject V11. On the 355 common-correct tasks, `sync-ns` has a 1.2945
  geometric-mean ratio. The next evidence-backed attempt is a structural predecessor/
  sparse-intersection cursor, not another certificate or candidate-set change.

# V11.1 sparse base-intersection enumeration (2026-07-18)

- Exact lazy intersection now counts each materialized base relation once per query and
  enumerates the lower-cardinality base when both operands are base leaves. Non-base and
  identity selection retains the previous plan. This changes enumeration cost only, not
  the relation, certificate, focus condition, or accepted RF/CO candidates.
- New `preventive-direct-base-candidates` and `focus-reach-base-candidates` counters record
  base cursor entries before derived filtering and deduplication.
- The first remote build invocation was ineffective because `rsync -a` preserved older
  source mtimes. Touching only the four synchronized files in the dedicated server source
  copy forced the intended GCC 13/Noble rebuild. A host-side test attempt is invalid
  because the Noble binary requires newer GLIBC/GLIBCXX; all valid tests run in the fixed
  `genmc15noble:sujie` image.
- Server Release focused tests pass 7/7. GCC 13 ASan+UBSan focused tests pass 8/8 with
  leak/UB halting. The 288-program SC/TSO/PSO differential passes 864/864, with zero
  mismatch and zero unsupported pair; output is `formal-results/v111-broad-20260718a.tsv`.
- Full-launch A ran zero tasks because its Docker invocation omitted the host cgroup
  mount required by BenchExec; both manifest statuses are 1. It remains invalid launch
  evidence. B uses a privileged container with `/sys/fs/cgroup`, separate 24-core queues,
  and rewrite roots derived from the unique output batch name. It is active at
  `formal-results/v111-full-725-20260718b`.
- Batch B completed 725 tasks per side but its frozen input hash was the previous V11
  hash `6c1078d1...`; the incremental object rebuild had not changed the linked binary.
  It is valid only as a V11 reproducibility run and is excluded from the V11.1 decision.
- A containerized `--clean-first` build produced confirmed V11.1 hash `01728a90...` and
  both new counter strings. The rebuilt candidate again passes Release 8/8, sanitizer
  8/8 and broad 864/864 correctness gates.
- Authoritative batch C completes 725 tasks per side with both statuses 0. Common-correct
  complete-execution and search-counter differences are zero. CPU V11.1/V9 is 1.099922
  [1.056791, 1.148091], wall 1.102284 [1.058770, 1.150656], and RSS 1.000070
  [0.999849, 1.000291]. Eighteen V9-completed tasks become resource failures and none
  improve in reverse. Base cursor visits fall 16,119,624,922 to 2,726,541,308, proving
  activation but not net benefit. Reject V11.1 and pause this optimization direction.
- Pause audit confirms one separate candidate remains from the prior bottleneck summary:
  exact structural cursors in `Relation::nextSuccessor/nextPredecessor`. Structural
  ProgramOrder/Internal/External/Location successors currently scan target IDs, and all
  non-indexed structural predecessors scan source IDs. This candidate is not the rejected
  base/base intersection selector. It remains unimplemented; resumption must first prove
  cursor/`contains()` equivalence for every lower bound and then follow sanitizer, 864
  broad, and direct paired-725 gates without an intermediate performance matrix.
- The final pause-time server status check could not complete because the remote closed
  SSH immediately. No remote command or new workload was started; the last authoritative
  completed manifest remains batch C with both unit statuses 0.

# V12 exact structural relation cursors (2026-07-18)

- `Relation::StructuralData` now builds immutable sorted indexes for exact location keys,
  real-thread groups, initial events and active events. Structural successor/predecessor
  cursors enumerate these slices instead of probing the entire event-ID universe.
- A compile-time `GENMC_DISABLE_EXACT_STRUCTURAL_CURSORS` lane restores the old scans,
  allowing a same-source before/after comparison. The candidate and baseline binary
  hashes are `b5f6132b...` and `88d5fbfd...` respectively.
- The randomized property checks all four structural relation kinds, both cursor
  directions, every event, and every lower bound against exhaustive `contains()` scans.
  Candidate and macro-disabled builds pass the focused property set.
- Server GCC 13 ASan+UBSan passes 8/8. The 288-program recursive PSO broad differential
  produces 864/864 matches, zero mismatch and zero unsupported.
- The formal paired 725-task experiment is active in container
  `v12-full-725a-sujie`, output `formal-results/v12-full-725-20260718a`, with disjoint
  24-core pools and a unique rewrite root per side.
- V12 is search-preserving, so equal verdicts, completed-execution counts, and search
  counters are required. A future coarser-equivalence/quotient candidate may legitimately
  reduce those counts and must instead use outcome coverage, representative completeness,
  and fail-open checks; the V12 equal-count oracle must not be reused blindly.
- Full run A completed 725 tasks per side and both launch units exited zero. There are
  zero status/category, common-correct complete-execution, search-counter, or coverage
  differences. CPU after/before is 0.988710 [0.977375, 1.001604], wall is 0.989603
  [0.978159, 1.002127], and RSS is 1.000179 [0.999909, 1.000487]. The small apparent
  speedup is inconclusive because both timing intervals cross 1.
- An unchanged full-725 repetition B was launched at
  `formal-results/v12-full-725-20260718b` with the same before/after hashes. The final
  decision will use per-task medians across the repetitions rather than selecting run A's
  favorable point estimate.
- Full run B also completes 725 tasks per side with both units exiting zero and zero
  status/category, complete-execution, common-correct search-counter, or coverage
  differences. Its CPU ratio reverses A's point estimate: 1.006793
  [0.995481, 1.019709]. Across A/B, per-task medians on 373 common-correct tasks give CPU
  0.998008 [0.988150, 1.009684], wall 0.998314 [0.987935, 1.010209], and RSS 1.000173
  [0.999945, 1.000453]. Reject V12 and remove its production indexes/cursor branches;
  retain the property, analysis code, and complete raw evidence.

# Core optimization direction review (2026-07-18)

- Cross-experiment evidence must not be pooled statistically because baselines and
  cohorts differ. Each source CI is retained; the synthesis is mechanism-level.
- V9 is the strongest positive pattern because it reduces generated RF/CO/work before
  descendants. V10--V12 preserve the actual search; even large internal-work reductions
  fail to produce an end-to-end benefit.
- The 725 census exposes 12.87M/9.78M/2.79M optimistic same-value alternatives under
  SC/TSO/PSO, but same-value alone is not a complete class key.
- The repaired SC-RVF finite oracles preserve all tested observations and reduce bounded
  executions, while the formal workload activates on zero tasks due to whole-program
  abort/mutex/non-atomic/loop gates. The next implementation target is a certified region
  boundary with future-event completeness and an auditable fail-open ledger.
- Generation-time CAT subtree blocking is second priority. It must prevent work creation;
  hits after queue/replay are not credited.
- Adaptive sparse CAT bases already reduce large base bytes to 14.1% and convert 20 OOM
  cells to TIMEOUT without solving them. Pre-CAT OOMs require exploration/history
  compression, not more CAT relation micro-optimization.
- Full report: `optimization-analysis/core-direction-review-20260718/2026-07-18--genmc-core-optimization--r00--direction-review.md`.
- Post-report source audit finds arbitrary regional fallback unsound in the current task
  decomposition: submitted representative tasks erase ancestor RF alternatives, while a
  later `rvf.reset()` only falls back from the current prefix. `Frame` and `ThreadPool`
  have no region revocation protocol. External `abort` also raises SIGABRT directly and
  cannot be whitelisted without property-endpoint semantics. P0-A is design-blocked;
  P0-B generation-time subtree blocking is the active implementable core direction.
- A second source audit finds that V10 already matches conflict cores before RF/CO lists
  return to the driver. Its positive clause proves one proposed candidate inconsistent,
  not that a consistent parent prefix has no extension; therefore “earlier backjumping”
  cannot soundly remove consistent classes. P0-B is also design-blocked absent a stronger
  no-consistent-extension certificate. Active core work returns to whole-suffix SC-RVF
  scope expansion, starting with property endpoint normalization.
- Property endpoint normalization passes 4/4 adapter unit tests and removes the abort
  reason on three real server/Docker examples without changing their verdicts. Each still
  falls back on a deeper reason: non-atomic store, `__VERIFIER_malloc`, or mutex lock.
  This is necessary enabling work, not RVF activation or speedup. Evidence is under
  `optimization-analysis/core-direction-review-20260718/server-results/endpoint-smoke-20260718a/`.
- The endpoint-normalized server gate census has 725/725 logs and manifest status 0. A
  diagnostic zero-annotation-assume admission exposed 326 mutex-first tasks but enabled
  only the same two zero-opportunity tasks; the production experiment was rejected and
  reverted. Mutex support requires RMW-pair adjacency/source/conditional-write semantics
  in the independent SC solver, not a whitelist edit. Evidence and design are in
  `core-direction-review-20260718/server-results/gate-census-20260718{a,b}` and
  `core-direction-review-20260718/mutex-entry-audit.md`.
- The first mutex enabler is isolated in the independent SC solver: a validated successful
  RMW read/write pair executes atomically and cannot be scheduled as separate events.
  Focused solver tests pass 9/9, including 21 exhaustive oracle combinations, and the
  full server Release suite passes 186/186. The adapter and static gate remain unchanged.

# Mutex/RMW actual 725 result (2026-07-18)

- Frozen result: `optimization-analysis/core-direction-review-20260718/server-results/mutex-full-20260718a/`.
- Baseline/control/RVF each contain 725 XML rows and 725 logs. Category totals are
  identical: 398 correct, 297 error/resource, 30 wrong. Control/RVF status differences: 0.
- RVF gates: assume/load-annotation IPR 513, non-atomic store 57, non-atomic load 32,
  loop 32, malloc 4, generic RMW 1, enabled 2, not recorded 84.
- RVF mechanism totals are all zero, including attempted/reduced loads, verify calls,
  representatives, parent continuations, and fail-open. Mutex support therefore receives
  no actual quotient traffic.
- Common-correct RVF/baseline CPU ratio 1.00056 [0.99665, 1.00461]; RVF/control 1.00246
  [0.99883, 1.00642]. These are zero-activation drift, not optimization performance.
- The inherited analyzer failed because BenchExec log names have 407 duplicate YAML
  basenames. `analyze_mutex_full.py` uses the full directory+source stem from command lines,
  handles `.i`→`.c` rewriting, and writes `analysis.json`.
- Decision: retain mutex correctness infrastructure, reject isolated mutex optimization,
  audit unified IPR-compatible quotient next; otherwise pause SC-RVF and move to
  exploration/history memory compression.

# IPR/RVF source audit (2026-07-18)

- RVF forces native IPR off. Source assumes remain executable, but future-write in-place
  reconsideration is absent, which is why the static gate is necessary.
- Annotation expressions are concretized predicates of the current load value. Existing
  `(value, provenance)` groups therefore have uniform assume truth values.
- Safe unified rule: annotated reads never take singleton/native bypass; false groups have
  no representative; the always-submitted parent continuation discovers future writes.
- Merely removing the gate is unsafe because a false RVF child can retain old
  `goodWrites[r]` while a native backward revisit changes `rf(r)`.
- Production admission needs per-assume coverage metadata. `annotMap.size()` cannot prove
  one-to-one coverage because assumptions and loads are many-to-many and annotations may
  overwrite.
- Bounded prototype/oracle contract is frozen in `core-direction-review-20260718/ipr-rvf-entry-audit.md`.

## First prototype evidence

- Added explicit experimental option `--sc-rvf-annotated-reads`; it requires
  `--sc-rvf-exploration`, and the default source-assume fallback remains unchanged.
- Annotated reads cannot use singleton/native bypass; false value groups are rejected;
  native backward revisits of RVF-owned annotated reads are suppressed and counted.
- The first unordered-writer probe exposed loss of native IPR's `VE_WWRace` upgrade.
  `scRvfNativeIpr` now preserves warning/error semantics while RVF replaces only revisit
  behavior. Baseline and RVF again report the same warning/hard status.
- The ordered two-same-value-source outcome fixture has outcome 0 unreachable and outcome
  1 reachable under baseline and RVF with one/two workers. RVF reduces complete executions
  2→1, records one reduced load, and has zero fail-open.
- Registered CTest `sc-rvf-assume-outcomes` passes in 0.50 s; full local units pass 190/190.

## Annotated-read server gate and actual-workload decision

- Server GCC 13 Release passes 190/190 after synchronizing the previously omitted
  `ConfigTest.cpp`; the earlier 189 count is not final evidence.
- Linux GCC 13 ASan+UBSan with leak detection passes 16/16 focused tests. The complete
  35-invocation baseline/RVF matrix passes outcome/error, worker, negative-admission,
  reduction, rejected-group, and fail-open checks without a sanitizer diagnostic.
- The full sanitizer unit executable is not green: randomized
  `ViewPropertyTest.MatchesOracleBehavior` hits an invalid free while destroying its
  `Oracle`. This is outside the annotated-read change but remains a repository-wide issue.
- Actual census batch B runs all 725 tasks with status 0. Gate counts: 342 per-assume
  coverage fallbacks, 275 not recorded, 54 non-atomic stores, 31 non-atomic loads, 16
  loops, 4 malloc, 1 generic RMW, and 2 enabled.
- Across all logs, attempted loads, reduced loads, rejected annotation groups, suppressed
  owned revisits, and fail-open are all zero. The metadata admits 171 tasks past the old
  513-task assume/IPR blocker, but none reaches a quotient operation.
- Decision: reject this standalone branch and skip the performance matrix. Raw logs, XML,
  hashes, manifest, and summary are at
  `optimization-analysis/core-direction-review-20260718/server-results/annotated-rvf-census-20260718b/`.
- Invalid launches: direct host launch failed on Docker-owned permissions; container A
  expanded zero inputs due the wrong `/workspace` mount. Only batch B is authoritative.

## Memory-track source audit kickoff

- `WorkList` is `vector<unique_ptr<Revisit>>`; forward RF/CO items are small, but each
  `BackwardRevisit` owns a heap `VectorClock`. Existing `max-retained-work` counts objects,
  not vector capacity or dynamic clock bytes.
- `ExecutionGraph::clone()` calls `getCopyUpTo()` and clones each retained `EventLabel`.
  Clone sites include cross-worker `extractState()` and the experimental RVF branch.
- `Scheduler` retains cloned label sequences in its execution trie, a distinct source of
  history memory that is not represented by worklist counts.
- Instrument byte attribution before choosing persistent prefixes, graph deltas, or revisit
  deduplication. The 725 OOM cohort, not a synthetic middle subset, is the decision set.

## pthread-wmm timeout structure (2026-07-18)

- In the frozen 725-task memory-attribution run, 179 of 239 TIMEOUT rows (74.9%) are
  `pthread-wmm`; the cohort has 283 `pthread-wmm` rows in total, so 179/283 (63.3%) time out.
- The 179 timeout sources have zero syntactic `for`/`while`/`do` loops. Loop unrolling or an
  inferred loop-depth bound therefore cannot address this dominant TIMEOUT cohort.
- Static source medians for TIMEOUT are three `pthread_create` calls, 19
  `__VERIFIER_atomic_begin` occurrences (one declaration plus about 18 regions), 11 integer
  shared-state declarations, three explicit `if` statements, 46 ternary operators, and three
  nondeterministic-Boolean symbol occurrences (one declaration plus about two calls). The
  corresponding solved-false medians are 3, 16, 10.5, 3, 46, and 5.
- The low `if` count is misleading: generated weak-memory buffer/flush semantics are encoded
  mainly as nested ternaries and nondeterministic choices inside many atomic regions. These
  are straight-line litmus-derived programs with combinatorial scheduling, RF/value, and
  buffer-state choices, not loop-heavy application code.
- Of the 179 timeout rows, 149 have expected verdict false and 30 true. Thus timeout is not
  explained only by exhaustive proof of safe programs; many bugs are buried behind a large
  encoded choice space.
- Optimization implication: keep explicit/proved loop bounds as a separate bounded-verification
  feature, but target generation-time class reduction, property-directed ordering, and exact
  state/history sharing for `pthread-wmm`. Treat the four libvsync OOM cases as a separate
  repeated-CAS graph-growth defect.
- Measurement source: `optimization-analysis/core-direction-review-20260718/server-results/
  memory-attribution-20260718a/` plus the matching server corpus under
  `optimization-analysis/svcomp2026/remote-results/sv-benchmarks/c/pthread-wmm`.

## Deagle transfer decision (2026-07-18)

- Deagle's loop-free `pthread-wmm` advantage comes from one finite SSA/event skeleton,
  symbolic guard/nondet/RF/order choices, ordering-theory propagation, learned conflict
  clauses, and non-chronological backtracking—not from loop-bound inference.
- The current GenMC adapter fixes nondeterminism while Deagle keeps it symbolic, so the
  0.217--0.560 second Deagle times are mechanism evidence rather than a fair speedup ratio.
- V10 already matches cores before RF/CO candidate lists return, but learns only from choices
  that V9 has already rejected. The next prototype learns exact positive cycle cores from
  ordinary CAT-rejected complete graphs, then matches them at the same pre-generation boundary.
- Do not copy the published preventive rules blindly: the 2025 stability audit reports an SC
  soundness issue in the published Basic algorithm and a compensating implementation deviation
  in Deagle. Every learned GenMC core must be independently derivable and fail open.
# Local Z3 installation and solver verification (2026-07-18)

- Homebrew reports `z3 4.15.2` installed locally.
- `/opt/homebrew/bin/z3` initially remained a stale standalone 4.14.1 binary even though
  the Homebrew formula, headers, and library were 4.15.2. `brew link --overwrite z3`
  replaced the stale command link; both `/opt/homebrew/bin/z3 --version` and the formula
  binary now report 4.15.2.
- Reconfiguring `.codex-build-rvf-tests2` resolves
  `Z3_INCLUDE_DIR=/opt/homebrew/opt/z3/include` and
  `Z3_LIBRARY=/opt/homebrew/opt/z3/lib/libz3.dylib`, with the configure diagnostic
  `Finite symbolic solver: Z3 enabled`.
- The focused local solver/encoder/CAT suite after relinking ran 10 tests: 9 passed and
  the unavailable-backend test was skipped by design because Z3 is present.
- Existing toolchain remained unchanged: `llvm 20.1.7`, `rust 1.88.0`, and
  `ccls 0.20241108_1`.
- Local CMake, with `CMAKE_PREFIX_PATH=$(brew --prefix z3)`, reports
  `Finite symbolic solver: Z3 enabled`.
- Focused local test: 4 passed, 1 skipped by design; the exhaustive 4-bit
  arithmetic/bitwise oracle covered 256 input pairs.
- Focused server/Docker test: the same 4 passed and 1 skipped.
- Command error: the first local invocation used the nonexistent path
  `.codex-build-rvf-tests2/tests/unit/unit_tests`; the built executable is
  `.codex-build-rvf-tests2/bin/unit_tests`. No test had run before correction.

## Finite skeleton IR checkpoint

- Added pointer-free function, basic-block, SSA-value, and event-site IR to
  `passes/passes/FiniteEventSkeleton.{hpp,cpp}`.
- Actual transformed `pthread-wmm/mix000.opt` builds as 3 functions, 216 blocks, 560
  values, and 313 event sites with zero blocker.
- Server rerun C: 283/283 structural and IR records, manifest status 0; 225 built and 58
  failed open only for the already-known dynamic address.
- Built-task totals: 105,309 values and 60,414 event sites.
- Server launch errors retained as non-evidence: missing Docker `/workspace` mount;
  relative SSH path; root-owned output parent; container BenchExec without host cgroups;
  absent `setfacl`; zsh readonly variable `status`; and hard-coded `/workspace` under a
  host launch. The valid run used Docker with `/data3/sujie:/workspace` and an explicit
  writable `/sys/fs/cgroup` bind. Parent permissions were restored to `root:root/755`.

## Finite CAAT explanation-clause experiment

- The first actual `mix000` run reported 100/100 theory-core translation fallbacks with
  `non-ground-predicate:fr`. The cause was lifecycle ordering inside the encoder, not an
  `fr` semantic mismatch: `next()` installed the full-assignment blocker before returning
  and `Solver::constrain()` invalidated the Z3 model needed to replay each explanation
  literal.
- Full-assignment blocking is now deferred until the following `next()` call. A graph or
  explanation blocker supersedes that pending clause. This retains exact enumeration and
  keeps the returned model available for literal-by-literal validation.
- A recursive CAAT -> Reasoner -> finite Z3 clause integration test now passes. The focused
  finite encoder/CAT suite is 6/6, and the broader solver/encoder/CAT suite is 10 tests with
  9 pass plus the expected unavailable-backend skip.
- On the same transformed actual `pthread-wmm/mix000` IR and the first 100 CAT-rejected
  assignments, all 100 explanations translated successfully (1,329 literals total, maximum
  48). However, wall/RSS were worse than both controls: theory core 3.83 s / 279,838,720 B;
  full-graph blocking 2.89 s / 204,324,864 B; exact assignment blocking 2.92 s /
  170,917,888 B. All three examined 100 assignments and rejected all 100.
- Decision: retain the verified translation only as an opt-in diagnostic experiment; do
  not make accumulated Reasoner clauses the production P1 mechanism. At this prefix it
  reduced neither assignments nor CAT calls and added 32.5% wall time versus graph blocking
  and 64.3% peak RSS. A future theory propagator needs an incremental/direct ordering
  encoding or an admission rule based on demonstrated multi-graph pruning.

## Finite lane strict-admission hardening

- The pointer-free builder now fails open for integer function arguments, ordinary direct
  calls, thread joins, unresolved thread entries, reused thread-entry functions, and mutex
  operations without a static address. These cases are not admitted until call binding,
  lifecycle `tj`, per-instance thread identity, and address semantics are encoded exactly.
- This closes a potential TRUE unsoundness: integer arguments were previously independent
  Z3 inputs, and direct calls activated callee entry blocks without binding parameters,
  returns, or interprocedural program order.
- Actual transformed `mix000` remains admitted after hardening (3 functions, 216 blocks,
  560 values, 313 events), and all six finite encoder/CAT focused tests pass.
- Existing GenMC replay is graph-prefix based: the scheduler replays a materialized
  `ExecutionGraph`, and the RVF path applies witnesses to an already interpreted graph.
  It cannot directly replay finite skeleton site/RF/CO IDs. A verdict-changing symbolic
  FALSE therefore still requires an explicit skeleton-to-ExecutionGraph/interpreter bridge;
  ordinary re-verification without forcing the witness is not accepted as replay evidence.

## First real-interpreter error replay bridge

- `ValueNode` and `EventSite` now retain a stable function-local LLVM instruction ordinal.
  This maps a pointer-free solver assignment back to the cloned transformed module without
  retaining LLVM pointers across contexts.
- `constrainReplayModule()` replaces each admitted nondeterministic call with its exact Z3
  model value and inserts a user-assume equality after every active load. It does not claim
  a verdict: the ordinary GenMC interpreter and the selected generic CAT/CAAT checker must
  independently find the safety violation.
- The diagnostic option `--finite-skeleton-replay-output=PATH` writes the first
  CAT-consistent error candidate as constrained LLVM IR. On
  `tests/cat/programs/finite-replay-smoke.ll`, the solver examined two assignments, rejected
  one by CAT, found one consistent error candidate, and wrote the replay artifact. Running
  ordinary GenMC with `recursive-sc.cat` on that artifact produced a real `ERROR` event and
  `Error: Safety violation!`.
- Registered CTest `finite-skeleton-error-replay` performs the complete symbolic candidate
  -> constrained LLVM -> ordinary interpreter/checker chain and passes locally in 0.61 s.
  This validates FALSE without trusting the symbolic error bit or finite CAT adapter alone.
- Current boundary: the artifact constrains nondet and observed load values, not the exact
  RF/CO identity. That is sufficient only when ordinary GenMC actually finds the error; a
  failed replay is inconclusive and must continue/fall back. Production in-process replay,
  exhaustive TRUE certification, and actual-package tests remain incomplete.

## Finite CAT primitive correction

- Differential source review against `cat::GraphAdapter` found that the finite adapter
  incorrectly placed every R/W and virtual initial write in `SC`. The real adapter inserts
  only labels whose memory ordering is sequentially consistent. This was latent under the
  recursive SC model but can change TSO/PSO results.
- Event IR now records sequential-consistency, initial writes are excluded from `SC`, and
  LLVM `FenceInst` is materialized as a CAT `F` event with its ordering. The explanation
  translator uses the same classification.
- A focused regression proves ordinary/IW events satisfy `empty SC`, an explicitly SC
  store violates it, and a fence violates `empty F`. Finite CAT tests are 4/4; the full
  finite encoder/CAT focused set and the real-interpreter replay CTest remain green.
- Direct primitive differential tests now construct matching finite and production
  `ExecutionGraph` snapshots and compare all 18 base predicates exactly. Both the
  memory-only RF/CO case and split mutex lock RMW case match `_`, `R`, `W`, `F`, `IW`,
  `SC`, `0`, `id`, `po`, `rf`, `co`, `fr`, `rmw`, `loc`, `int`, `ext`, `tc`, and `tj`.

## Strict-admission server rerun D

- The first exact lane now rejects unsupported CFG terminators (including `switch`), GEP
  and alloca addresses, and non-integer memory operations. This prevents simultaneous
  activation of all switch successors and prevents distinct byte offsets/lifetimes from
  collapsing to one symbolic location.
- Registered `finite-skeleton-strict-admission` confirms switch and GEP inputs fail open;
  together with `finite-skeleton-error-replay`, both focused CTests pass locally.
- Authoritative Docker/BenchExec census D completed with manifest status zero and 283/283
  structural plus IR records. Coverage remains exactly 225 built and 58 fail-open only for
  dynamic addresses; built totals remain 932 functions, 38,099 blocks, 105,309 values, and
  60,414 events. Evidence:
  `optimization-analysis/core-direction-review-20260718/server-results/finite-skeleton-pthread-wmm-20260718d/`.

## Finite symbolic production gate and full-725 launch

- A conservative opt-in production mode, `--finite-symbolic-errors`, now searches finite
  CAT-consistent error assignments and replays each through the ordinary interpreter and
  recursive CAT verifier. It can return FALSE only after native GenMC confirms the error.
  Every other outcome falls back to the unchanged native search; it never proves TRUE.
- Server Release gate: 204 unit tests ran, 203 passed and the unavailable-Z3 test skipped
  because Z3 is enabled. `finite-skeleton-error-replay`, `strict-admission`, and
  `verdict-oracle` all passed. The first Release launch failed only at CMake generation
  because three newly referenced unit sources had not yet been synchronized; no test or
  benchmark ran in that attempt. Synchronizing the exact sources resolved it.
- Server ASan+UBSan gate: 14 focused tests ran, 13 passed plus the same expected skip; all
  three finite CTests passed. No ASan or UBSan finding was reported.
- Experiment definition:
  `optimization-analysis/core-direction-review-20260718/finite-symbolic-full-725.xml`.
  Launcher:
  `optimization-analysis/core-direction-review-20260718/launch-finite-symbolic-full-725.sh`.
  Both configurations use the same Release binary, recursive SC CAT model, 60 CPU seconds,
  4 GiB and one core per task. Baseline uses cores 0--23; candidate uses 28--51 and adds
  `--finite-symbolic-errors --finite-symbolic-max=10000`. The two 725-task queues run
  concurrently to reduce time-of-day bias.
- Active server container: `finite-symbolic-full-725a-sujie`. Output:
  `/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/formal-results/finite-symbolic-full-725-20260718a`.
- Final result rejects the candidate: correct 398 -> 318, TIMEOUT 239 -> 302, ABORTED
  2 -> 21, and total CPU 16,131.35 -> 19,530.80 s. It confirmed two errors, both already
  fast baseline FALSE results, and produced zero new correct terminals. One confirmed task
  regressed from 0.060 s to 44.05 s. Archived candidate logs expose an uncaught
  `std::out_of_range` in the new path.
- Raw artifacts were pulled locally and `table-generator` produced a 725-row, two-run-set
  HTML plus a 96-row diff HTML under `server-results/finite-symbolic-full-725-20260718a/html/`.
  The first rsync attempt used GNU-only `--info=progress2`, unsupported by macOS rsync; it
  transferred nothing. The retry with `--progress` completed successfully.

## Finite crash repair and property-directed pthread-wmm experiment

- The 21 candidate ABORTED rows traced to a dangling `ValueNode` reference in
  `FiniteEventSkeleton.cpp`: `setOperands()` and PHI lowering held a reference into
  `program.values` while `valueRef()` could append constants and reallocate the vector.
  The builder now collects IDs first and writes by stable index afterward. The encoder
  additionally validates operand arity/ranges and fails open on malformed IR.
- The exact archived `36-apron...neg` input previously aborted before assignment 1. After
  repair it produces one CAT-consistent assignment, native replay confirms the error, and
  GenMC exits 42. All 21 archived ABORTED inputs were rechecked with no `out_of_range`.
- Release gates passed 5/5 encoder tests plus 3/3 finite CTests. ASan+UBSan passed 11/11
  focused tests plus 3/3 finite CTests with no finding.
- A hard property-directed `error-active` disjunction was then tested on all 283 actual
  `pthread-wmm` tasks under unchanged 60 s/4 GiB limits. It was decisively rejected:
  correct 104 -> 35, TIMEOUT 179 -> 248, CPU 11,635.69 -> 15,119.27 s, peak RSS
  27.1 -> 630.6 MB, zero new correct terminals.
- The 225 admitted tasks time out before producing assignment 1; only the 58 fail-open
  cohort writes final counters. The cause is the first monolithic Z3 query over full
  control/value/RF/CO constraints, not a high assignment cap. The hard objective has been
  removed. Evidence and decision:
  `optimization-analysis/core-direction-review-20260718/finite-symbolic-pthread-wmm-report.md`.
- Per user direction, implementation is paused pending design review. The PPoPP 2022 and
  OOPSLA 2022 papers show that Deagle does not solve the same monolithic formulation: RF
  is a prioritized Boolean interference decision, only po-loc/ppo/RF are explicit, and
  WS/FR plus transitivity are derived on demand by an ordering theory with conflict
  explanations. Our eager per-store BV CO ranks are the opposite design.
- The recommended path is documented in
  `optimization-analysis/core-direction-review-20260718/deagle-smt-encoding-design-review.md`:
  first perform a no-solver encoding-size census, then test Boolean/bit-blasted SAT with
  complete-assignment CAT oracle, then add an ordering theory only for structurally
  certified SC/TSO/PSO. Preventive propagation remains deferred.

## Expanded concurrent-SMT/EOG literature map

- The previous review was too Deagle-centric. There are two distinct top-level strategies:
  (1) Yogar-CBMC removes the scheduling constraint from the initial abstraction and validates
  abstract counterexamples with an Event Order Graph; (2) the Fei He ordering-theory line
  keeps a DPLL(T) architecture but replaces generic timestamp solving and eager FR constraints
  with a dedicated incremental theory.
- Yogar-CBMC's graph validator derives implicit event orders. A cycle proves an abstract
  counterexample infeasible. Kernel reasons are tracked through derivations and minimized into
  refinement clauses. A constraint-based validator/refiner is the completeness fallback when
  graph rules cannot decide feasibility. Its proof is bounded by the chosen loop unwinding.
- The Yogar weak-memory extension covers SC/TSO/PSO with a unified EOG construction, but this
  is still model-specific and does not justify arbitrary-CAT support.
- PLDI 2021 defines ordering consistency theory for SC with incremental consistency checking,
  minimal conflict generation and specialized theory propagation. It explicitly targets the
  waste of concrete event clocks and pre-encoding all FR constraints.
- TOPLAS 2023 extends the exact theory/encoding to SC, TSO and PSO; RF and WS are ordering
  variables and FR is derived online only when the corresponding RF/WS choices are active.
- PPoPP 2022 adds domain-guided DPLL decisions: interference variables such as RF/WS receive
  priority. This is complementary to, not a replacement for, scheduling abstraction.
- OOPSLA 2022 preventive propagation tries to preserve consistency so cycle checking/conflict
  generation become unnecessary. It remains the last candidate due to independent correctness
  concerns and the need for exact generic-CAT fallback.
- Control-flow-guided SMT solving may later improve SSA/branch decisions, but it does not remove
  the measured eager RF/CO scheduling burden and is not the current core candidate.
- Revised sequence: three-representation no-solver census -> Yogar-style FALSE-only CEGAR ->
  exact PLDI/TOPLAS ordering validator -> PPoPP decision priority -> only then preventive rules.
- Local direct PDF downloads failed on 2026-07-18 with OpenSSL `SSL_ERROR_SYSCALL`; no PDF or
  repository file was created. Official indexed paper pages and author publication metadata
  were used for this review, and the failure does not justify implementation assumptions.
- Deagle source evidence was recovered through official raw GitHub despite clone failures.
  `ClosureSolver` subclasses MiniSAT, receives ordering edges/guards via `addOC`/`addGuard`,
  activates them while consuming the Boolean trail, turns graph cycles into conflict clauses,
  queues theory literals with reasons, and mirrors SAT decision scopes into graph scopes.
- Its `one_more_time` branch recursively invokes propagation when theory adds literals, reaching
  a fixpoint. The 2025 stability audit identifies exactly this source behavior as a material
  difference from the published prospective algorithm. We may copy the architectural contract,
  not assume the paper's preventive rule is correct.

## Actual finite representation census

- Server Docker Release and local gates both passed FiniteSkeletonEncoder 5/5 and the three
  finite CTests 3/3. The census is solver-free and default behavior is unchanged.
- Actual 283 pthread-wmm stats-only run completed in 5.4 s wall at the batch level, 21.255 s
  total CPU, max per-task wall 0.113 s and max RSS 27,099,136 B. Coverage is unchanged at 225
  built / 58 fail-open. All 225 built tasks have zero reads without an RF source.
- Across 225 built tasks: RF pairs total 1,059,950 (p50 4,355), CO pairs total 141,179
  (p50 590), CO rank bits total 62,366. RF pairwise constraints are about 7.5x CO pairs.
- In the 156 admitted baseline TIMEOUT tasks: RF pairs p50 5,436 and sum 824,229; CO pairs
  p50 735 and sum 111,979; value bits p50 4,890. The 69 non-TIMEOUT built tasks have RF pairs
  p50 3,088, CO pairs p50 403, and value bits p50 4,728.
- Decision: reject a CO-only abstraction prototype. The next diagnostic must combine removal
  of first-query CO/scheduling with linear/native RF at-most-one, then measure only first
  abstract error model time. No EOG completion means no replay and no verdict.
- Per user direction, repeated implementation experiments use a fixed 15-task panel instead
  of all 283 tasks: easy 5 (3 FALSE/2 TRUE under 0.25 s), medium 5 (3 FALSE/2 TRUE at 10--50 s),
  and hard 5 (TIMEOUT with the largest RF+CO pair counts). Exact tasks and frozen baseline
  metrics are in `pthread-wmm-15-panel.tsv`; full-package runs remain the promotion gate.
- 2026-07-18 user confirmation: during tool-completion and truncation testing, the fixed
  easy/medium/hard 5+5+5 panel is sufficient and should be preferred to a full pthread-wmm run
  to save time. It reduces only iterative performance workload; it does not waive correctness
  gates. Promote to all 283 pthread-wmm tasks only after the panel shows a meaningful reduction
  in timeout/OOM or a similarly material core metric without regressions.
- P1a fixed-panel evidence: eager CO/pairwise RF found 9/15 first models, no-CO/pairwise found
  10/15, and no-CO/native-cardinality found 15/15. The last mode cut panel CPU to 102.368 s and
  peak RSS to 69.9 MB, so both CO abstraction and native cardinality are required.
- P1b completion maps from the CAT materializer's exact primitives and passes 24 focused tests,
  including all nine fixed-RF shapes against exhaustive CO permutations. All 15 first abstract
  models are SC-infeasible; one fixed completion costs only 0.16--3.31 ms.
- Whole-graph refinement rejected 1000 candidates on three small TRUE tasks and timed out the
  other 12. Relaxed-RMW RF cores prove two TRUE error abstractions UNSAT quickly, but greedy and
  QuickXplain extraction still leave 13/15 timeouts and about 275 s total CPU. Repeated graph
  checks are therefore rejected; use an active-candidate rank/order query with RF assumptions
  and native UNSAT-core extraction next.
- The active-candidate rank/order query was implemented and independently matched the exhaustive
  RF/CO oracle, but failed the actual panel decisively: 15/15 timed out before the first ordering
  result and CPU rose to 315.58 s. The state counts previously read as candidate size were early
  dead-end depths, not event counts. Reject global ranks; the remaining P1 mechanism must be a
  graph-native incremental ordering theory with on-demand edges and direct conflict reasons.
- Repeated infrastructure error: a panel launcher was first called on the server host despite
  the established container-only protocol and failed before tasks due to root-owned results.
  The launcher now requires `/.dockerenv`; valid runs create and write results inside the same
  `genmc15noble:sujie` container.
# 2026-07-18 graph-native SC theory design freeze

- Audited `CAT/LazyCycle`, `CAT/IncrementalEvaluator`, `CAT/Analysis`,
  `CAT/ConflictCore`, `SCGoodWritesSolver`, and the recursive SC model.
- Existing lazy/incremental CAT code operates on already fixed positive base facts.  It
  cannot prove inconsistency across both orientations of SC visibility constraints.
- `SCGoodWritesSolver` is fast for one fixed RF shape but returns no all-prefix proof; a
  conflict from one dead-end search prefix is not a safe RF core.
- Froze a dedicated disjunctive ordering theory design in
  `optimization-analysis/core-direction-review-20260718/graph-native-sc-theory-design.md`.
- Core safety rule: only return refinement-safe UNSAT after resolving every internal
  orientation branch literal.  SAT still requires production materialization and exact
  recursive-SC/RMW checking.  Unsupported or budget-limited cases fail open.
- No server process was started and no result directory was created during this audit.

## Architecture correction: integrate at the real GenMC choice boundary

- The standalone graph-native SC design above was superseded before implementation.  It
  would still put a second search/completion engine outside GenMC.
- Source audit found that `BasicCATChecker` already integrates CAAT at actual RF-source
  and CO-placement enumeration and owns stable identities, preventive checked-order
  certificates, focus reach, lazy top-cycle explanations, plus incremental graph
  insert/rollback/replace synchronization.
- The missing mechanism is not another relation graph: it is a persistent decision/clause
  layer that can reuse mixed RF/CO conflicts across compatible revisits.
- `ConflictCoreDatabase` is not CDCL; it linearly matches positive fact conjunctions and
  has no decision levels, watched propagation, resolution, or backjumping.  Archived V3/V4
  regressions therefore warn against more core matching rather than ruling out CDCL.
- Froze the unified architecture and a census-first gate in
  `optimization-analysis/core-direction-review-20260718/genmc-caat-cdcl-integration-design.md`.
- No source implementation or server process was started in this correction.

### Existing V9/V10 data rules out clause-only replacement

- Read-only aggregation of the archived V9 full-725 log archive found:
  `preventive-rf-pruned=5,814,007`, `preventive-co-pruned=12,899,587`,
  `preventive-all-pruned-fallbacks=0`, and `preventive-prefix-inconsistent=0`.
- V10 already saw 14,193,035 repeated positive-core hits but no search-counter reduction
  and a 4.99% CPU regression.
- Therefore watched clauses for the same immediate candidate cycles would not provide a
  new search-space mechanism.  A CDCL prototype is justified only by non-local complete-
  CAAT conflicts whose explanation omits irrelevant recent decisions and enables a real
  backjump/subtree skip.
- Next implementation gate: observation-only mapping of installed-candidate CAAT
  violations to stable RF/CO decision depths.  It must not change candidate order or
  verdicts.

### User-requested plan-before-code hold

- Stopped the just-started server broad diagnostic before completion; no result from that
  partial run is used as evidence.  The container had already exited when the exact stop
  check ran, and no GenMC diagnostic container remained.
- Froze the proposed unified algorithm, ownership split, decision/revisit flow,
  soundness/completeness obligations, time/space cost sources, safety budgets, and staged
  implementation gates in `genmc-caat-cdcl-algorithm-plan.md`.
- Critical architectural result: GenMC's `WorkList` contains independent `Revisit`
  objects, not a chronological SAT trail.  A real CDCL backjump therefore requires a
  compact immutable choice-trail ID on retained executions/revisits and invalidation at
  restore/pop time.  Merely calling solver push/pop at `CATChecker` cannot skip GenMC work.
- No active CDCL method, worklist mutation, candidate filtering, or backjump code has been
  implemented.

### Event-stamp proxy rejected

- A self-review found that the initial observation-only `--cat-backjump-census` prototype
  incorrectly treated `EventLabel::Stamp` as a decision level.  Stamps order inserted
  labels, while RF/CO revisits can change choices on an old label without changing its
  stamp.
- The prototype and CLI/config surface were removed before any panel/full result was
  accepted.  Its local/server compilation and sanitizer results prove only code hygiene,
  not a valid measurement method.
- A valid census first needs an explicit observation-only `DecisionTrace`: normal choices
  append nodes, forward/backward revisits restore the appropriate parent/prefix and append
  their alternative, graph cuts discard removed suffix decisions, and forced RMW CO is
  attributed to RF rather than counted as an independent level.
- The interrupted server broad run remains excluded and no stamp-based statistic will be
  reported.

### Decision-state design correction after revisit source audit

- `restrictAndRevisit()` cuts the current execution by a relevant stamp, but a backward
  revisit then calls `getCopyUpTo(VectorClock)`.  The selected view may contain holes and
  is not necessarily a chronological ancestor prefix.
- `ExecutionGraph` resets stamps after structural copy/cleanup.  Stamps therefore cannot
  identify stable choice levels across backward revisits.
- A valid observation/solver bridge needs one active RF/CO entry per real choice subject,
  filtered with the same graph view, plus an immutable base snapshot on each queued revisit.
  Decision ordinals are scheduling hints only; exact snapshot clause satisfaction is the
  pruning authority.
- Archived V9 bounds for cost planning: worst worker/task `work-added=409,678`; maximum
  simultaneously retained worklist `16,388`.  A 16/24/32-byte unique node envelope is
  roughly 0.25/0.375/0.50 MiB at peak retained work, versus 6.3/9.4/12.5 MiB if all added
  nodes leaked.  Backward filtering, live executions, allocator, clauses, and watches are
  additional measured categories.

### Search-ownership and resource-budget correction

- “Solver is advisory only” is also wrong for the intended architecture: it repeats V10's
  peripheral core matching and gives up CDCL's complete RF/CO combination search.
- Ownership is partitioned.  GenMC completely explores control/schedules/dynamic events;
  the solver completely explores each RF/CO subspace delegated to it.  A joint frontier
  invariant records every feasible delegated assignment as current, GenMC-queued, or
  solver-unexplored.
- Deleting an unlocked redundant learned clause is standard and completeness-preserving;
  deleting base constraints, active reasons, activation definitions, or unexplored frontier
  state is not.
- The earlier numeric budgets are now observation thresholds.  If an active solver cannot
  continue after safe clause GC, the outcome is resource exhaustion/inconclusive, never
  TRUE.  Native fallback is exact only after exporting all remaining assignments or
  restarting from a completeness checkpoint, neither of which belongs in P0.

### TruSt optimality preservation boundary

- TruSt optimality means one representative interleaving per extendible DPOR execution-
  graph equivalence class; it is not merely terminal verdict equality.
- The extension must leave native revisit construction, vector-clock prefix selection,
  maximal-extension checks, revisitability, `ChoiceMap`, and class ownership unchanged.
- A queued revisit can be deleted only with a complete checked-root proof that no CAT-
  consistent extension in the entire represented scope exists.  UNSAT for the currently
  installed concrete suffix does not meet this obligation.
- Uniqueness is then inherited because no new representative is generated.  Completeness
  follows because only non-extendible classes are removed.  This reduces inconsistent
  failed prefixes without changing TruSt's equivalence relation.
- Required evidence includes bounded representative-signature equality and a revisit ledger
  with exactly one explored/proof-pruned terminal state per native work-item ID.

### Prefix-nogood hypothesis and likely sparsity

- Candidate mechanism: extension-closed prefix nogoods from stable already-fixed RF/CO literals whose
  sparse CAAT derivation forms a cycle in an analyzer-certified prefix-monotone checked root.
- This is the mixed-choice generalization of V9 preventive pruning.  It can reject every
  suffix containing the same cause, but cannot merge or regenerate consistent TruSt classes.
- A native queued revisit is pruned only when its exact DecisionSnapshot entails the entire
  cause and activation scope; unresolved choices and concrete-suffix-only conflicts do not
  justify deletion.
- Memory is tied to the live TruSt working set: O(1) snapshot root per work item, persistent
  deltas for live items/depth, compact clauses proportional to live events, and reclaimable
  proof scratch.  No assignment frontier or dense reach/rank matrix is retained.
- Existing evidence makes this a gate, not a selected core implementation: V9 reported zero
  all-sibling-pruned fallbacks and V10's 14,193,035 core hits reduced no common-correct
  search counter while regressing CPU 4.99%.
- A stronger opportunity is CDCL resolution proving the parent UNSAT after all alternatives
  conflict, but RF/CO domains are not automatically closed: future writes/backward revisits
  can add alternatives.  Such pruning requires an explicit domain-closure certificate.
- The census must measure clauses unit before candidate installation, resolution-derived
  closed-domain parent UNSAT, and native work items skipped before restore.  Mere recurring
  cycle/core counts are insufficient.
## 2026-07-18 CAAT decisive diagnosis

- Fixed 15-case pthread-wmm observation-only panel completed in the server Docker: 10
  frozen terminals match and five hard cases remain TIMEOUT; no incorrect, ABORTED, or
  unsupported result.
- Release correctness is 142/142; GCC 13 ASan+UBSan is 141/141 with no sanitizer finding.
- Across the ten completed tasks, CAT validity consumes 66.207 s (79.02% of the three
  measured core phases), revisit restore 17.038 s (20.34%), and RF+CO enumeration only
  0.540 s (0.64%). Materialization plus offline evaluation explains 60.787 s of CAT time.
- All 439,144 completed-task CAT queries are accepted; all 15 exploration snapshots report
  zero inconsistent revisit prefixes. Conflict learning/top-level cycle SMT is therefore
  not the primary direction for this dataset.
- The actual structural finding is `adaptive-offline=292,774`, `unchanged=146,360`, and
  insert/rollback/replace all zero: the certified <=512-event heuristic disables the
  incremental path on this panel. Next experiment is a paired evaluator/synchronization A/B,
  not a new searcher. Full report:
  `optimization-analysis/core-direction-review-20260718/decisive-diagnostic-report-20260718.md`.

### Adaptive-offline versus incremental A/B implementation

- Added opt-in `--cat-disable-adaptive-offline`. Validation requires a CAT file, the CAAT
  backend, and the exact adaptive-offline structural certificate. Default behavior is
  unchanged; the switch changes only GraphSynchronizer's small-graph threshold from 512
  to 0.
- Local unit executable passes 215 plus one expected Z3-availability skip. Focused CAT/CAAT
  differential, mutation, configuration, and transition tests pass 6/6.
- A `WW+RR` smoke has the same status, three complete executions, two work items, four
  validity queries, and two realized prefixes in both modes. Baseline uses 12 adaptive
  offline selections; the candidate exercises insert=5, rollback=4, rollback-insert=1 in
  its main checker, proving the switch reaches the intended path without changing TruSt
  exploration.
- The first smoke launcher used zsh's reserved read-only `status` variable and stopped after
  its baseline command. It was invalid evidence. The rerun uses `rc` and retained complete
  logs at `/tmp/genmc-incremental-ab.RunYor`.

### Adaptive-offline versus incremental A/B result

- Server Docker gates pass: Release 144/144 and GCC 13 ASan+UBSan 143/143, with no
  sanitizer report.
- Reject direct threshold removal. Baseline has 10 terminals; candidate loses `mix035` to
  TIMEOUT. Across nine common terminals, CPU is 96.359 -> 127.896 s (+32.73%) and CAT
  validity is 43.898 -> 75.086 s (+71.05%), while all search counters match exactly.
- Materialization is unchanged (23.543 -> 23.643 s). Offline work saves only 1.743 s but
  history search adds 25.196 s and insertion attempts add 7.560 s.
- Candidate transitions are insert=24,026, rebuild=209,527, rollback=0,
  rollback-insert=0, replace=0. The existing 32-query history has no actual reuse on these
  runs; snapshot-equivalent peak doubles. Next step is reason counters, not a larger blind
  history. Report: `adaptive-offline-incremental-report-20260718.md`.

### Incremental history rejection diagnosis

- Observation-only counters pass four focused GraphSynchronizer/config tests under server
  Release and ASan+UBSan. Three medium tasks completed with unchanged verdicts.
- Across those tasks, insertion rejection=46,520 and history entries examined=51,486, but
  subset matches, rollback attempts, rollback-insert attempts, and rollback successes are
  all zero. The limit of 32 checkpoints is not the issue: saved states already include the
  sibling's fixed RF/CO fact and are not subsets of the next sibling; rebuild then clears the
  epoch.
- Do not increase history capacity. A genuine work-item checkpoint would need a pre-choice
  evaluator state and persistent branching ownership, not query LRU tuning.
- The next generic-CAT prototype targets the independent 23.5 s materialization cost with
  an exact worker-local primitive cache/delta adapter. The complete staged algorithm,
  complexity, oracle, retention, and stop gates are in `primitive-delta-adapter-plan.md`.
# 2026-07-18 primitive-delta ownership audit

- `StableGraphAdapter::materialize(const ExecutionGraph&)` currently returns an owning
  `StableGraphSnapshot` by value.
- `GraphSynchronizer::synchronize(StableGraphSnapshot)` passes its base by const reference, but
  `IncrementalCaatEvaluator::initialize()` copies the full map into evaluator `base_`.
- `GraphSynchronizer::retainCurrent()` copies evaluator `base_` once more into `history_` even in
  the `<=512` adaptive-offline branch whose next changed query clears that history.
- Decision: do not implement a persistent `BaseValues` cache blindly. First split timings into
  descriptor scan/delta work/evaluator copy/equality and ensure the small-graph path never retains
  adapter cache + evaluator base + history base simultaneously in the performance candidate.
- Server cost breakdown refines that decision: materialization is 33.591 s / 50.83% of CAT
  consistency, while evaluator and history base copies total 1.203 s / 1.82%. A first isolated
  delta prototype may keep its own packed base and cross the existing API by copy, because this
  preserves evaluator correctness and the copy has a low time ceiling. It remains conditional on
  the fixed-panel RSS gate (no more than +5%) and stays restricted to <=512 stable events.
- Exact unchanged cache result: 146,360 hits, fixed-panel CPU 142.274 -> 136.369 s (-4.15%),
  materialization 33.526 -> 27.723 s (-17.31%), aggregate RSS +0.12%, worst task +0.42%,
  with identical status and search counters. It fails the 10% CPU standalone gate but validates
  the descriptor/cache boundary. Continue into changed-query primitive deltas rather than enabling
  this switch by default.

### Changed-query primitive delta result

- Local unit: 218 passed plus one expected Z3-availability skip; focused server Release matched,
  and server ASan+UBSan focused tests passed 18/18.
- Local `fcombiner` normal mode exactly matched verdict and every exploration counter, with 34
  delta updates and no hidden oracle rebuild. Materialization nevertheless regressed from
  1.591 ms to 7.640 ms (4.80x).
- The mutation oracle found an exact correctness failure on TSO, two workers,
  `ms-queue-dynamic`: stable event and mapping counts matched, but `fr` omitted edges 79->82 and
  81->82. It reproduces locally under repeated scheduling.
- Decision: reject the per-relation changed-query repair algorithm before broad or paired-panel
  testing. Its constant cost is already worse than full packed rebuild, and its `fr` dependency
  surface is not locally closed under dynamic graph/revisit changes. Preserve the exact unchanged
  cache result; do not repair this prototype through relation-specific special cases.

### Exact primitive full-build result

- Added an independently switchable `--cat-fast-primitive-build` experiment: small dense
  relations skip semantically redundant edge sorting, and FR is derived directly from ordered
  per-address store vectors rather than a CO-edge hash expansion.
- Correctness passes local/server unit, server ASan+UBSan 18/18, mutation oracle 39 rows / 5,466
  checks, and 864 broad with zero mismatch.
- Corrected paired fixed-15 result: all-task CPU 470.52 -> 433.35 s (-7.90%), while the ten
  completed tasks improve 165.56 -> 128.33 s (-22.49%). The four-job suite wall time is
  138.37 -> 134.33 s (-2.92%); an earlier reading mislabeled this column as CPU. CAT consistency
  is 89.571 -> 51.528 s
  (-42.47%), materialization 37.747 -> 19.308 s (-48.85%), offline 49.106 -> 26.897 s
  (-45.23%). RSS is effectively unchanged; terminal/TIMEOUT counts remain 10/5.
- Decision: retain behind the experiment switch as a combined core foundation. Completed-task
  CPU clearly passes the 10% gate, but do not expand to 283 yet because no TIMEOUT is removed and
  the five fixed caps dilute all-task CPU improvement to 7.90%. Next
  diagnose the complete offline fixed-point evaluator. Report:
  `optimization-analysis/core-direction-review-20260718/primitive-full-build-report-20260718.md`.

### Fast-check recovery checkpoint (2026-07-19)

- The handoff compile failure was exactly the misplaced public declarations: implementations and
  call sites consistently target `Relation`. Moving declarations is sufficient for production
  compilation.
- A fresh GCC 13 + LLVM 15 RelWithDebInfo tree at `.codex-build-linux-gcc13-recovery` builds
  `genmc` and the complete unit binary. Result: 221 passed and one intentional skip because Z3 is
  available. The new dense/CSR witness test passes.
- The mutation runner now receives every option after its three required paths as a quoted Bash
  array, so the three experimental switches are distinct argv entries.
- Local combined mutation did not complete: TSO/two-worker `malloc-not-hb0.c` can terminate with
  exit 42 before recording an oracle check. Candidate repetitions produced 8/0/0 checks versus
  baseline 4/4/4, with no mismatch. Keep the gate failed/incomplete until a reliable environment
  exercises every intended row.
- The established server Docker does exercise all 39 mutation rows: 5,416 complete oracle checks
  and zero mismatch. Broad is 852/12/0 over 864 pairs. The paired fixed-15 candidate preserves
  exact complete/blocked/bound counts and 10/5 terminal/TIMEOUT classification.
- Fast-check reduces ordinary witness checks 17.577 to 16.678 s (-5.12%), but cache + fast-build +
  fast-checks reaches only -8.20% all-task CPU and leaves all hard caps unchanged. Its 432.72 s
  candidate CPU is only 0.15% below the previous 433.35 s candidate. Reject expansion to 283.

### Successor-cursor composition implementation checkpoint (2026-07-19)

- `composeFast()` is an exact alternate implementation: each lhs row is traversed through
  `nextSuccessor()`, then the same explicit-CSR, structural, or packed-dense rhs row is unioned.
- The feature is independently gated by `--cat-fast-composition` and is plumbed through both
  offline and incremental CAAT evaluators. `offlineOracleMismatch()` deliberately omits the flag,
  preserving the previous nested-scan implementation as an independent oracle.
- Deterministic tests cover dense/dense, CSR/dense, dense/CSR, CSR/CSR, empty lhs, and event IDs
  crossing 63/64/129. The randomized relation-algebra property compares fast composition, old
  composition, and the set-of-pairs reference.
- Local GCC 13 / LLVM 15 full unit result after implementation: 224 total, 223 passed, one
  intentional Z3-availability skip.
- Safe server sync verified SHA-256 for all 21 manifest files and found no flattened duplicates.
  The established Docker Release tree rebuilt successfully and repeated the same 223-pass/one-skip
  full unit result.
- Server GCC 13 ASan+UBSan passes 44/44 focused relation/evaluator/adapter/config tests with leak
  detection and halt-on-error enabled. Mutation needed one retry because the known parallel
  early-error case recorded zero oracle checks; the retained rerun passes all 39 rows and 5,416
  old-implementation oracle comparisons. Broad differential passes 852 comparable pairs plus 12
  mutually unsupported pairs with zero mismatch over 864 total pairs.
- The first paired-15 launch directory `primitive-fast-compose-paired-15-20260719a` contains only
  a pre-execution BenchExec cgroup error and is not performance evidence. The corrected `b` launch
  uses the host cgroup namespace and explicit cgroup mount; both lanes are executing concurrently.
- The corrected paired run completed both lanes. Baseline/candidate all-task CPU is
  471.234/426.753 s (-9.44%), completed CPU 166.275/121.763 s (-26.77%), suite wall
  138.39/133.80 s (-3.32%), CAT consistency 89.888/45.972 s (-48.86%), and aggregate RSS is
  effectively unchanged. Composition falls 5.631/0.641 s (-88.61%). Search counts are exactly
  18,693 complete, 197,703 blocked, zero bound in both lanes, with the same five TIMEOUTs.
- Relative to the prior fast-check foundation, composition alone adds -1.37% all-task CPU,
  -4.64% completed CPU, and -19.89% offline-evaluator time. Retain it, but do not expand to 283:
  no hard cap moved and the strict overall gate is 9.44%. Report: `fast-composition-report-20260719.md`.

### Successor-cursor cycle-check implementation checkpoint (2026-07-19)

- After fast composition, ordinary check evaluation is 16.601 s and dominates the 20.504 s
  offline evaluator. The acyclic DFS still scans every possible target and calls `contains()`.
- `--cat-fast-cycle-checks` independently switches this loop to ascending `nextSuccessor()`
  enumeration; DFS state, root order, parent construction, and cycle witness encoding are unchanged.
  The incremental offline oracle omits the switch and remains an independent old-path comparison.
- Local focused tests preserve the exact closed witness on a CSR cycle crossing events 0/64/129;
  the complete unit result is 225 passed and one intentional Z3-availability skip.
- Server Release repeats 225 plus one expected skip; ASan+UBSan passes 45/45; mutation passes
  39 rows / 5,441 old-path oracle checks; broad passes 852/12/0 over 864 pairs.
- Fixed-15 baseline/candidate CPU is 475.920/415.295 s (-12.74%), completed CPU
  170.957/110.293 s (-35.49%), suite wall 138.93/132.45 s (-4.66%), check
  22.090/5.063 s (-77.08%), and offline evaluation 53.453/8.953 s (-83.25%). RSS changes +0.04%
  aggregate and -0.20% peak. Search counts remain exactly 18,693 / 197,703 / 0 and terminal/TIMEOUT
  remains 10/5.
- Because overall CPU exceeds the strict 10% gate, the simultaneous actual pthread-wmm 283 panel
  was launched after a clean server audit: 24 baseline jobs on CPUs 0-23 and 24 candidate jobs on
  CPUs 28-51, with unchanged 60-second/4-GB per-task limits. Result directory:
  `primitive-fast-compose-cycle-paired-283-20260719a`.

### Actual 725 successor-cursor result (2026-07-19)

- The simultaneous paired run completed under the unchanged 60-second/4-GB limits. Baseline has
  394 correct terminals, 244 TIMEOUTs, and 31 OOMs; candidate has 402 correct terminals, 216
  TIMEOUTs, and 51 OOMs. Total unresolved hard-resource outcomes therefore fall 275 -> 267.
- All eight newly correct results are the same pthread-wmm TIMEOUT rescues observed on the 283
  panel. The other 20 changes are Goblint `28-race_reach_*` TIMEOUT -> OOM transitions. No
  resolved terminal becomes a resource failure and no terminal verdict changes.
- Common-terminal CPU is 1,033.224 -> 745.272 s (-27.87%); all-task CPU is 16,366.595 ->
  15,632.316 s (-4.49%); suite wall is 717.88 -> 690.74 s (-3.78%). Aggregate RSS rises 0.92%
  because the 20 faster Goblint tasks reach the same 4-GB cap; maximum RSS is unchanged.
- Candidate CAT consistency is 317.789 s: materialization 167.090 s and offline evaluation
  96.623 s remain the largest phases. Within materialization, primitive coherence is now the
  largest measured subphase at 57.518 s, followed by scan 24.115 s, packing 19.807 s, FR 19.440
  s, co-edge construction 18.271 s, structural 17.689 s, and label construction 15.656 s.
- Retain the combined exact foundation behind experimental flags. The next candidate must target
  coherence exactly and keep the old full materializer as oracle; the 20 TIMEOUT -> OOM changes
  are an explicit operational cost, not a solved-result improvement. Raw evidence:
  `server-results/primitive-fast-compose-cycle-paired-725-20260719a/`.

### Exact ordered coherence-build result (2026-07-19)

- `--cat-fast-coherence-build` directly fills small dense `co` and `fr` rows from GenMC's ordered
  per-location writes and exact RF-source buckets, avoiding intermediate quadratic edge vectors.
  It is restricted to the certified <=512 path; old behavior remains for larger graphs.
- Correctness: local/server Release 226 plus one expected skip, ASan+UBSan 53/53, mutation 39 rows
  / 5,441 oracle checks, and broad 852/12/0 over 864 pairs. Full stable snapshots match the old
  dense-remap oracle across RF/CO/event/location mutations.
- Valid fixed-15 delta against the prior cycle foundation: CPU 415.186 -> 414.842 s (-0.08%),
  completed CPU 110.216 -> 109.834 s (-0.35%), suite wall 132.44 -> 132.59 s (+0.11%), aggregate
  RSS +0.07%, peak RSS +0.24%, with exact 18,693 / 197,703 / 0 search counts and unchanged 10/5
  terminal/TIMEOUT.
- Internal savings are real but too small: coherence -17.55%, packing -55.74%, materialization
  -5.02%, consistency -2.81%. Do not run 283/725 or enable by default. Report:
  `fast-coherence-build-report-20260719.md`; authoritative raw result ends in `20260719b`.
- Invalid evidence retained transparently: the first broad command expanded an unset shell variable
  and stopped before output; fixed-panel `a` used the stale main Release binary and candidate rows
  all rejected the unknown option. Neither is used for performance conclusions.

### Next primitive scan audit

- Every changed `materializeCached()` query first builds a complete `GraphDescriptor` for exact
  cache comparison, then `materialize(graph)` scans the same labels and locations again and repeats
  type/address/RF/lifecycle extraction. On the actual 725 candidate there are 2,000,267 cache misses,
  so this duplicate pass is a more credible remaining target than further coherence tuning.
- `GraphDescriptor` already contains every primitive determinant needed for exact reconstruction:
  stable event position, thread/index, R/W/F/SC classes, address, RF source, RMW target, create/join
  endpoints, and ordered per-location writes. The next candidate should materialize cache misses
  from this frozen descriptor under a separate flag, while the existing graph-rescan materializer
  remains the oracle. No delta repair or inferred edge is permitted.

### Exact descriptor miss-build result (2026-07-19)

- Implemented `--cat-fast-descriptor-build`, requiring the primitive cache. It reuses only the
  descriptor constructed on the current call to avoid the second graph scan on a miss; stale
  pointers are excluded from descriptor equality and never dereferenced.
- Correctness passes local/server Release 228+1 skip, ASan+UBSan 55/55, mutation 39 rows / 5,424
  old-evaluator checks, and broad 852/12/0. The mutation shell emitted one ignored-NUL warning but
  completed every row with nonzero oracle coverage and zero mismatch.
- Fixed-15 delta: CPU 416.013 -> 416.091 s (+0.02%), completed CPU +0.05%, suite wall -0.03%,
  aggregate RSS -0.03%, peak RSS +0.08%, exact search counts, and unchanged 10/5 terminal/TIMEOUT.
  Scan improves 8.87% and materialization 2.68%, but the absolute saving is only 0.535 s.
- Reject 283/725 expansion and default enablement. Report: `fast-descriptor-build-report-20260719.md`;
  raw evidence: `server-results/primitive-fast-descriptor-delta-paired-15-20260719a/`.

### Exact descriptor storage-reuse result (2026-07-19)

- `--cat-fast-descriptor-reuse` keeps complete descriptor equality but swaps cached/current worker
  descriptors after misses so vector capacities are reused. It uses no hash, epoch, approximate
  equality, inferred edge, or pruning.
- Correctness passes local/server Release 229+1 skip, ASan+UBSan 56/56, authoritative mutation
  rerun 39 rows / 5,441 oracle checks, and broad 852/12/0. The first mutation run is invalid because
  the known nondeterministic TSO/2-worker early-error row produced zero oracle checks.
- Fixed-15: all CPU 416.958 -> 414.796 s (-0.52%), completed CPU -1.96%, suite wall -0.14%,
  aggregate RSS -0.14%, peak RSS -0.47%, exact search counts, unchanged 10/5 terminal/TIMEOUT.
  Materialization improves 11.01% and CAT consistency 6.06%, but no hard cap moves.
- Stop before 283/725 and keep experimental. Report: `fast-descriptor-reuse-report-20260719.md`;
  raw evidence: `server-results/primitive-fast-descriptor-reuse-delta-paired-15-20260719a/`.

### Best-foundation 725 HTML delivery (2026-07-19)

- Generated the cycle-foundation baseline/candidate HTML from the already authoritative paired 725
  XML files using BenchExec 3.25 `table-generator` with the endpoint-pipeline module available.
- The main table has exactly 725 rows and two run sets; the difference table has 28 rows. Both HTML
  files and CSV companions are under
  `server-results/primitive-fast-compose-cycle-paired-725-20260719a/html/`.
- Server and pulled-local scans confirm that no generated artifact contains `cputime-cpux`.

### Final CAAT optimization decision (2026-07-19)

- The requested recovery funnel is complete through actual 725 and final HTML. The retained exact
  cycle foundation produces eight additional correct results and eight fewer total hard-resource
  failures, with no resolved-case or terminal-verdict regression.
- Do not enable it by default yet: 20 Goblint rows move TIMEOUT -> OOM under the unchanged limits.
  Preserve that operational tradeoff and require workload-specific opt-in until an explicit default
  policy is chosen.
- Ordered coherence, descriptor miss-build, and descriptor storage reuse are exact but fail the
  expansion gate; do not spend another 283/725 run combining sub-percent effects.
- Consolidated evidence and direction decisions are in
  `caat-optimization-final-report-20260719.md`.

## Paper-facing synthesis audit (2026-07-19)

- The retained history contains five distinguishable contribution classes: exact normalization
  (linear recursion and cycle-only closure slicing), grouped/structural primitive construction and
  representation, analyzer-certified lazy cycle evaluation, preventive search-space pruning, and
  the final small-graph exact evaluator foundation.
- The old 2026-07-18 direction report's blanket deferral of relation cursors is superseded by the
  2026-07-19 successor-cursor evidence: cycle checks rescue eight tasks at 283 and the actual 725
  gate. Preserve this as an explicit belief update rather than silently rewriting history.
- Repeated formal matrices support population-style task-bootstrap intervals for the earlier
  normalization/lazy/preventive families. The final 15/283/725 foundation uses one simultaneous
  paired run per cohort; report exact paired aggregates and status transitions, not p-values or
  confidence intervals.
- Strongest final workload result: 394 -> 402 correct terminals, 275 -> 267 TIMEOUT+OOM,
  common-terminal CPU -27.87%, all-task CPU -4.49%, wall -3.78%; 20 Goblint TIMEOUT -> OOM is an
  unresolved operational tradeoff, not a regression to a wrong verdict.
- The paper should separate solved coverage from resource-class movement, and should not call the
  opt-in foundation a universal/default speedup.
# 2026-07-19 independent `cd09f78b` full reproduction

- Built an isolated server source tree from clean commit `cd09f78b`; SHA-256 verified 6,210
  relative-path files.  Fresh GCC 13.3/LLVM 15.0.7 Release build passed 158/158 unit/property
  tests.
- Correctness gates passed: mutation 39 rows / 5,441 oracle checks / zero mismatch; broad 852
  match + 12 mutually unsupported / zero mismatch over 864 pairs.
- Fresh simultaneous 725-task lanes completed successfully.  Historical and rerun status/category
  matrices are identical for both lanes.  Correct terminals remain 394 -> 402; hard failures
  275 -> 267; transitions remain eight TIMEOUT-to-correct and twenty TIMEOUT-to-OOM.
- Rerun effects: common-terminal CPU 1,015.699 -> 736.993 s (-27.44%); all CPU 16,340.049 ->
  15,615.592 s (-4.43%); wall 716.699 -> 689.878 s (-3.74%); aggregate RSS +0.88%; maximum RSS
  unchanged at 3,999,997,952 bytes.  These differ from historical effects by only 0.43, 0.05,
  0.04, and -0.05 percentage points respectively.
- Raw result:
  `optimization-analysis/core-direction-review-20260718/server-results/primitive-fast-compose-cycle-paired-725-cd09f78b-rerun-20260719a/`.
- Evidence commit `53c667d5` follows code commit `cd09f78b`.  GitHub push is pending authentication;
  HTTPS has no stored token and configured SSH port 9322 is unreachable.
# Core optimization campaign evidence (2026-07-19)

## P1 phase attribution audit

- Exact evidence source: independent paired 725 rerun under
  `optimization-analysis/core-direction-review-20260718/server-results/primitive-fast-compose-cycle-paired-725-cd09f78b-rerun-20260719a/`.
- Parsed both compressed BenchExec XML files and the candidate log archive. All 20 retained
  `TIMEOUT -> OUT OF MEMORY` transitions are Goblint `28-race_reach_*` tasks.
- Every candidate reaches exactly `3,999,997,952 B`, uses about 39--47 CPU seconds, and its archived
  log contains only the command banner: 20/20 lack `Compilation complete`,
  `Transformation complete`, and every exploration/CAT statistic.
- **Correction:** absence of those lines does not locate the OOM phase. BenchExec redirects output;
  normal buffered output can be lost when the cgroup kills the process. These 20 failures remain
  phase-unknown until an unbuffered, sampled, or uncapped replay establishes the last completed
  phase. They must not yet be classified as pre-compilation failures.
- A clean `cd09f78b` probe with explicitly flushed markers disproves the pre-compilation
  hypothesis for representative task `28-race_reach_03-munge_racing`: configured peak is
  50,679,808 B, compiled peak 54,403,072 B, transformed peak 56,844,288 B, and Docker then kills
  the process at the 4-GiB cap (`exit=137`, `OOMKilled=true`). The task unquestionably reaches
  exploration.
- A second observation-only probe samples each 10,000 labels. At 40,000 labels the graph has
  20,001 threads, max stamp 60,002, and process peak only 128,638,976 B. The task then jumps to the
  cgroup limit before the next sample. This rules out linear label/worklist retention as the main
  4-GiB source for this representative task and localizes the peak to work immediately after the
  first approximately 40k-event graph is formed, with first CAT evaluation the leading hypothesis.
- A 64-GiB replay exceeds 13 GiB and later falls below 9 GiB while still running, supporting a
  large transient peak rather than monotone retained-history growth. Final/query-level evidence is
  still required before assigning the allocation to a specific evaluator operation.
- A separate inspected task (`28-race_reach_01-simple_racing`) does complete transformation and
  reaches its first CAT evaluation with 40,017 stable events. Its first snapshot-equivalent CAT
  state is about 1.807 GB while the current sparse base is about 1.681 MB; this is a distinct
  first-query CAT/evaluator case, not retained exploration history.
- Existing `--cat-stats` exploration instrumentation already counts work added/popped, maximum
  retained revisits, current/stack graph-label cardinality, and scheduler cached-label clones. It
  does not yet measure owned bytes or frontend/LLVM phase RSS, and its 100,000-activity progress
  cadence cannot attribute pre-compilation failures.
- `WorkList` owns only `unique_ptr<Revisit>` objects. Backward revisits additionally own one
  `VectorClock`; work items do not own graph snapshots. Deep graph copies are retained instead in
  `GenMCDriver::execStack` during backward revisits and in submitted worker `Execution` states.
- `Scheduler::seenPrefixes` independently owns cloned, reset `EventLabel` sequences and grows
  monotonically for the driver lifetime when instruction caching is enabled.

### Consequence

P1 must use phase-separated cohorts and claims:

1. pre-compilation/LLVM frontend peak memory,
2. transformation peak memory,
3. first-CAT-query construction memory,
4. exploration-retained memory after at least one query.

The exact 20 Goblint transitions are not assigned to any cohort until stronger phase evidence is
collected. No direction may claim them based only on missing buffered log markers.

### Representative first-query attribution and adaptive-representation experiment

- A 128-GiB/1,800-s clean `cd09f78b` replay of
  `28-race_reach_03-munge_racing` reached the same 40,000-label/20,001-thread prefix and did not
  OOM. A live debugger backtrace placed the running process in
  `relationIntersection -> CaatEvaluator::evaluateStratum -> IncrementalCaatEvaluator::initialize
  -> GraphSynchronizer -> BasicCATChecker::isConsistent`. This confirms that the representative
  task enters its first CAAT fixed point; it is not a pre-exploration or retained-worklist OOM.
- Predicate-boundary instrumentation on the coherent dev build shows an approximately 80,000-event
  stable universe. The dense evaluator retains 7,211,282,876--7,271,979,372 bytes of predicate
  values; each derived relation occupies about 800--808 MB. A 32-GiB run peaks at
  16,444,674,048 bytes and times out after 600 s. The primitive RF/FR/CO/lifecycle operands are only
  about 0.3--0.64 MB each. The dominant allocation is therefore exact derived-relation
  materialization and repeated union copies, not labels or sparse primitives.
- Sparse-derived v1 changes empty fixed-point values to exact CSR and keeps sparse Boolean/
  composition results as CSR when their calculated upper bound is smaller than packed storage.
  Release unit/property tests pass, but the representative still OOMs: `po | tc` is genuinely
  large and two successive union temporaries retain about 1.607 GB before later fixed-point values.
- Overlay v2 adds copy-on-write packed words and immutable exact union overlays. On the identical
  4-GiB/300-s task, it changes `OOMKilled=true/exit 137` to `TIMEOUT/exit 124`, keeps current memory
  below the cap, and reduces the observed predicate-value footprint from about 7.22 GB to
  5.6--6.3 MB. Sampled process peak is 456,400,896 bytes, at least 8.7x below the 4-GiB failure
  boundary and about 36x below the dense 32-GiB sampled peak. No verdict is claimed because the task
  remains a timeout.
- V2 is rejected as a general policy despite the memory result. Its simultaneous fixed-15 panel has
  identical 10 terminal / 5 TIMEOUT classifications, but common-terminal CPU regresses 39.28%
  (median task ratio 1.2449), all-task CPU regresses 10.37%, and aggregate RSS rises 0.67%.
- V3 admits overlays only when one packed relation would occupy at least 64 MiB; smaller graphs keep
  the original packed path. This threshold is representation-only and cannot change membership.
  A clean minimal comparison against `cd09f78b` showed that the representation candidate itself,
  rather than the restored dev statistics implementation, regressed common-terminal CPU by 34.78%
  with identical 10 terminal / 5 TIMEOUT statuses and essentially unchanged aggregate RSS.
  Moving COW uniqueness checks out of per-word loops did not repair the result (35.67% regression),
  identifying the representation/algorithm path rather than the check as the cost. The user froze
  a stronger no-time-for-memory/no-memory-for-time rule; all adaptive CSR/overlay candidate source
  was therefore removed. The 456.4-MB result remains negative mechanism evidence, not a retained
  optimization or paper performance claim. Raw evidence root:
  `/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/`.

## Development baseline build audit

- A clean archive of `genmc-caat-opt-dev` head `4701c580` does not compile independently.
  `CATChecker.cpp` references incremental-statistics fields, lazy-cycle helpers, and evaluator
  signatures absent from the archived `CaatEvaluator`, `IncrementalEvaluator`, and `LazyCycle`
  interfaces. This is pre-existing branch inconsistency, not caused by the phase-memory probe.
- The exact `cd09f78b` snapshot plus the phase probe builds successfully in the prescribed server
  Docker image. New optimization implementation cannot safely proceed on dev until its archived
  intermediate source is restored to a coherent build baseline.
- Build setup errors encountered: `BUILD_TESTING=ON` is not this project’s option (the option is
  `BUILD_TESTS=ON`); enabling it attempted a slow RapidCheck network clone, so the diagnostic binary
  was built separately with tests off. Full tests remain mandatory after dev consistency repair.
- The omitted implementation was recovered from repository commit `69bdedbd`, which is the complete
  source-side counterpart of the already retained `cd09f78b` optimization. Restoring its six CAAT
  evaluator/lazy-cycle files makes the dev interfaces coherent without removing later ConflictCore,
  finite-symbolic, or CATChecker experiments. The repaired Release build passes 218 tests with one
  expected Z3-availability skip; SC/TSO differentials, PSO proof, recursive CAAT differential, and
  online mutation stress all pass. The isolated `fast-driver` failure is a fixture-path/build-tree
  setup error (missing hard-coded `RelWithDebInfo` artifacts), not a verdict mismatch.

## Frozen direction gates

### P1 phase-separated memory work

- Correctness: identical terminal status/verdict and identical complete/blocked/work-added/
  work-popped counters on all terminal paired cells; zero new crash, unsupported result, or
  resolved-case resource regression.
- Attribution: every measured peak is labeled pre-compilation, transformation, first CAT query,
  or post-query exploration. Missing progress records are not treated as zeros.
- Instrumentation: observation-only builds must preserve all search counters and stay within 3%
  CPU and 3% RSS on the fixed terminal panel before their measurements are accepted.
- Expansion: a candidate must reduce the targeted cohort's median and maximum peak RSS by at least
  10%, or move at least one fixed-limit OOM to TIMEOUT/terminal without losing a terminal result.
- Retention: broad paired runs must not increase aggregate hard failures; an OOM-to-TIMEOUT move is
  recorded as resource improvement but not as solved coverage.

### P0 regional SC-RVF

- No global static-gate relaxation without reversible region ownership. A region needs identity,
  entry snapshot, exit frontier, covered-class ledger, ownership epoch/revocation token, and
  descendant withdrawal before native fallback.
- Generated exhaustive oracles must cover future writes, nested regions, loop-iteration identity,
  own/non-own sources, equal values with distinct provenance, pointer provenance, assume/error
  endpoints, mutex/RMW boundaries, and exact one/two-worker observation/class equality.
- Expansion requires nonzero production-workload activation and a reduction in offered/queued work,
  realized prefixes, or representatives. Oracle-only activation is not a performance result.

### P2 certified CAT subtree blocking

- Immediate positive-core matching is frozen as rejected evidence and will not be reimplemented.
- A candidate must provide a sufficient no-consistent-extension certificate, translate every
  required CAT fact to stable active RF/CO literals, select an earliest rollback-safe scope, and
  block before enqueue. Incomplete translations fail open.
- Learned state is worker-local and scope-guarded; it may not own a second execution graph.
- Exhaustive baseline/candidate oracle equality is mandatory. Expansion additionally requires a
  measured reduction in work added/popped, realized prefixes, or direct full CAT checks; hit counts
  alone are invalid evidence.
## 2026-07-19 P1 empty EventDeps sharing candidate

- Heap attribution on `weaver/mult-dist.wvr.yml` showed millions of retained non-atomic labels.
  The clean stable ABI sizes were `EventDeps=160`, `EventLabel=304`, `ReadLabel=392`, and
  `WriteLabel=400` bytes. Representing empty dependencies by a null immutable shared handle reduces
  them to 160, 248, and 256 bytes respectively: 144 bytes saved per event label without changing
  dependency semantics.
- The exact isolated source comparison differs only in
  `genmc/genmc/Execution/EventLabel.hpp`. Both Release builds succeed. Under the identical Weaver
  4 GiB hard limit, stable OOMed after 10.94 s and the candidate after 12.89 s (+17.8% survival at
  the same memory cap). This is targeted memory-density evidence, not a completion claim.
- Simultaneous fixed-15 A/B (`labeldeps-fixed15-20260719b`) preserved all outcomes and the 10
  terminal / 5 TIMEOUT split. Aggregate CPU was 409.070 -> 408.600 s (-0.11%); completed-task CPU
  was 104.257 -> 103.652 s (-0.58%); aggregate measured memory was -0.04% (small graphs, effectively
  neutral). Thus no time-for-memory or memory-for-time regression was observed in this gate.
- Added focused tests for canonical empty storage and immutable non-empty dependency sharing across
  clones. Both pass. The complete dev-tree run had 233 passes, one expected solver skip, and nine
  infrastructure failures caused by files omitted from the isolated server source tree (explicit
  missing script/test paths); these are not counted as candidate correctness failures. ASan and a
  complete-tree broad gate remain required before promotion.
- The dedicated GCC 13 ASan+UBSan build succeeded. With leak detection and halt-on-error enabled,
  both focused `EventLabelTest`s plus SC, TSO, and recursive-CAAT differential tests passed (5/5;
  recursive differential 161.50 s) with no sanitizer report. The remaining promotion gate is the
  broad large-label/oracle panel on a complete server source tree.
- Raw server evidence:
  `/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/labeldeps-weaver-4g/`
  and `.../labeldeps-fixed15-20260719b/`.

## 2026-07-19 P1 calculated-view and history-copy compression

- Event labels now retain one physical calculated `View` for equal logical checker views and a
  four-entry logical-to-physical map. The generated SC/TSO/RA/RC11/IMM calculation order remains
  explicit. Against the retained empty-dependency candidate, fixed-15 preserved the exact
  6-false/4-true/5-TIMEOUT split; aggregate CPU changed by -0.16%, completed CPU by -0.49%, and
  memory by -0.02% (all effectively neutral). A prefix-view alias follow-up was rejected after its
  single decisive Weaver run regressed time by 2.7%; that source was removed.
- Backward-revisit graph copies now share immutable copy-on-write `ViewBase` storage only within the
  same worker. The cross-worker `clone()` path remains a deep copy. With `--cat-stats`, exact unique
  bases and a conservative byte lower bound are reported; without attribution enabled, the clone's
  already-shared bases require no hash-set insertion.
- The one permitted simultaneous fixed-15 decision run compared the retained label/view candidate
  with and without history sharing. Statuses were identical (10 terminal, 5 TIMEOUT). Aggregate CPU
  was 466.174 -> 462.520 s (-0.78%); common-terminal CPU was 161.209 -> 157.519 s (-2.29%);
  aggregate measured memory was 396,210,176 -> 396,009,472 bytes (-0.05%). This is descriptive
  single-run evidence and is not treated as a repeated statistical claim. Per the instruction to
  stop repeating small differences, retain on dev and move to the next P1 mechanism.
- Raw paired evidence is under
  `optimization-analysis/core-direction-review-20260718/server-results/history-share-fixed15-20260719b/`.

### Immutable calculated-relation storage

- A complete use-site audit found that `calculatedRels` is published only by `setCalculated`, read
  only through the const `calculated()` range, and reset as a unit. It is now an optional immutable
  shared object. Normal labels carry no allocation, and graph/history clones cannot deep-copy the
  relation sets.
- The cumulative object sizes are now `EventLabel=152`, `ReadLabel=240`, and `WriteLabel=248`
  bytes, another 8 bytes per label below the retained empty-dependency/view candidate (and 152
  bytes per label below the original 304-byte `EventLabel`). The local core library and exact
  server Release binary both build successfully.
- One same-cap Weaver decision run remained OOM in both lanes, as expected for an 8-byte incremental
  change, but time to the identical 4-GiB kill moved from 14.438 to 14.686 seconds (+1.7%). This is
  only direction/activation evidence; no completion or timing-speedup claim is made. In accordance
  with the no-repeat-small-differences instruction, retain on dev and do not run another panel now.

### Inline backward-revisit clocks

- `BackwardRevisit` previously owned a separately allocated polymorphic `VectorClock`: the outer
  object was 40 B and the clock was 32 B (`View`) or 104 B (`DepView`), with two allocator headers
  and two allocation/free pairs. It is now an abstract base with typed `View`/`DepView` storage in
  the same allocation. Concrete sizes are 64 B and 136 B, saving approximately 24 B and one heap
  operation per retained backward revisit for either model family.
- The conversion does not alter a work-item key, ordering, prefix contents, or scheduling. Focused
  GCC 13 server tests preserve plain-view indices and dependency-view holes (2/2 pass); the local
  core library and exact server Release executable build successfully.
- A single simultaneous fixed-15 comparison against the cumulative calculated-relation baseline
  preserves all 6 false, 4 true, and 5 TIMEOUT statuses. Aggregate CPU is
  463.283 -> 461.616 s (-0.36%); common-terminal CPU is 158.279 -> 156.645 s (-1.03%);
  aggregate measured memory is 396,242,944 -> 395,993,088 bytes (-0.06%). This is descriptive
  evidence from one decision run. Both resources move in the intended direction, so retain on dev
  and do not repeat the small difference.
- Raw paired evidence is under
  `optimization-analysis/core-direction-review-20260718/server-results/worklist-inline-fixed15-20260719a/`.

### Cumulative P1 correctness-gate repair and results

- The first full unit run crashed in `ViewPropertyTest` with `free(): invalid pointer`. ASan showed
  `Oracle::~Oracle` destroying a `std::vector<int>` although the View test's oracle owns a
  `std::map<int,int>`. Root cause was a pre-existing ODR violation: both `IntervalMapTest.cpp` and
  `ViewTest.cpp` defined distinct global `class Oracle` types, allowing weak inline destructor
  symbols to collide. Renaming them to `IntervalMapOracle` and `ViewOracle` changes test code only.
- After the repair, the cumulative exact GCC 13 build passes all 160 unit/property tests. The same
  160 tests pass under ASan+UBSan with halt-on-error enabled and leak checking disabled; there is no
  sanitizer diagnostic.
- Release SC differential, TSO differential, recursive CAAT differential, and the full online
  mutation oracle all pass in one fail-fast container. The mutation gate covers 39 rows and 5,441
  independent oracle checks. This establishes cumulative semantic coverage for empty dependencies,
  logical view deduplication, same-worker ViewBase sharing, immutable calculated relations, and
  inline View/DepView revisit clocks before larger resource experiments.
- The complete synchronized development tree subsequently builds successfully in the GCC 13 server
  Docker environment and runs 223 tests: 222 pass and the unavailable-backend case is skipped as
  designed. The integration sanitizer container also exits 0 and writes `complete.txt`; strict
  ASan+UBSan unit/property coverage remains 160/160. A strict integration-only UBSan warning in the
  native SC interpreter's pre-existing null `DepTracker` reset path is recorded separately and is
  not attributed to the P1 changes.
- The 283-task EventDeps comparison preserves the exact 283-row task set and introduces no terminal
  regression. Correct terminal rows change from 108 to 109 because one prior timeout completes
  correctly at 59.69 seconds; aggregate CPU changes by -0.19%, common-terminal CPU by -2.66%, and
  measured RSS is neutral. Raw evidence is under
  `optimization-analysis/core-direction-review-20260718/server-results/p1-labeldeps-paired-283-r1-20260719/`.
- P1 retention decision: keep the five exact layout/sharing/allocation improvements on
  `genmc-caat-opt-dev`, but stop adding engineering-only graph/history variants. The main research
  line now moves to P0 regional SC-RVF; P1 still requires a final cumulative broad resource gate
  before promotion to the stable branch.

## P0 bounded-loop SC-RVF correction and rejection (2026-07-19)

- The initial `complete executions 2 -> 1` rejection used the wrong oracle. Native RF-DPOR and
  SC-RVF enumerate different equivalence partitions; the controlled safe fixture confirms that this
  particular reduction is legitimate and also reduces RF offered 4 -> 1, work popped 1 -> 0, and
  realized prefixes 1 -> 0 with identical n1/n2 results.
- The corrected two-iteration generated oracle is decisive: four shapes pass 1,296 invocations,
  but the full 20-shape/6,480-invocation gate loses 12 reachable error states across six shapes,
  identically under one and two workers. Native/RVF error cells are 154/130 and RVF reports 16,988
  reduced loads; those reductions are invalid because they remove observable errors.
- Bounded-loop admission was removed. The generator, counterexamples, exact metrics, and raw server
  paths are recorded in `p0-regional-loop-decision-20260719.md`. Future loop work must repair
  repeated dynamic-read/future-write class handling and pass that oracle before workload timing.

## P0 native-ancestor frontier and result split (2026-07-19)

- The first loop counterexample remains incomplete after manual two-copy unrolling, so LLVM loop
  detection and random scheduling are not the cause. Five explicit seeds reproduce native error /
  RVF safe. The first same-value merge fails to own future backward revisits of native ancestor
  reads; disabling that merge alone restores the error.
- Frames now track those exact native ancestor read positions. A future same-address write revokes
  the speculative region and replays its untouched native entry. Representative revisitable lists
  cannot drive this frontier because the missing native branch is absent from that graph.
- The first full unrolled run restored all errors but exposed n1/n2 result-accounting drift. Splitting
  task results at region open into durable pre-region and speculative post-open portions fixes it.
- The corrected 20-shape oracle passes all 6,480 server-Docker calls with zero violations and retains
  156 reduced RVF cells. Aggregate RVF process time is nevertheless +10.5% (n1) and +7.5% (n2), so
  this remains correctness infrastructure on dev, not an effective optimization for promotion.
- Full evidence and rejected attempts are in
  `p0-native-ancestor-frontier-report-20260719.md`.
- After this repair, the formerly failing two-iteration loop oracle passes all 6,480 calls and
  retains 156 reduced cells. Regional loop admission is restored on dev, with its CTest oracle, but
  still requires an actual-workload resource gate.
- The archived `regional-na-load-16` BenchExec logs did not actually pass `regional-rvf` through the
  wrapper and therefore cannot prove zero activation. Correct-mode spot checks still fall back
  because those programs have secondary blockers beyond NA accesses; a corrected loop+NA census is
  required before selecting the production cohort.

## P0 actual-workload activation and rejected NA-prefix re-entry (2026-07-19)

- The corrected 725-task gate census produced 725 result rows and 709 readable gate records. It
  identifies 51 regional-eligible tasks, but the strict paired run shows `rvf-loads-attempted=0`
  and `rvf-loads-reduced=0`: the stable transaction never activates after the first native
  non-atomic frontier. Candidate timing therefore cannot be credited as an RVF optimization.
- The valid 51-task paired run has exactly one 51-row XML and one log archive per lane. It preserves
  all statuses and semantic categories, but changes all-task CPU by +1.86% and common-terminal CPU
  by +124.4%, with effectively neutral aggregate RSS (-0.047%). Because mechanism activation is
  zero, timeout partial-progress counter differences are not optimization evidence. Do not repeat
  this rejected configuration.
- A later supported-atomic re-entry after a native non-atomic prefix passed the full 6,480-call
  unrolled oracle, but failed the loop oracle with n1/n2 count drift and a real lost error at shape
  `011100`, outcome `1112`, n1. It is fully removed. After rollback, both focused regional CTests
  pass, and the previously decisive shape passes 324/324 server-Docker calls with zero violations.
- Formal launch infrastructure now rejects missing cgroup setup, wrong path namespace, wrong input
  cardinality, multiple/missing result XMLs, and mismatched `<run>` counts. BenchExec exit status 0
  is explicitly not treated as proof that work ran.
- Closed-prefix census r1 ran zero tasks because its 300-GiB container could not admit 48 tasks at
  12 GB each. Preserve r1 as infrastructure failure; r2 uses 36-way concurrency and 500 GiB. Add
  explicit aggregate-memory arithmetic to future launcher preflight.
- Closed-prefix r2 validates 51/51 rows and logs. Thirty-seven tasks expose 63,899 mergeable
  native-only loads, but none is an ordinary `EventLabel::Read`: checks/admissions/attempts/reductions
  are all zero. The temporary re-entry code is removed; do not broaden ordinary RVF grouping to
  wait/spin/RMW reads without a separate semantic equivalence proof.
- Read-kind r3 shows all 63,899 loads and 188,498 nominally removable sources are CAS/lock; every
  other family is zero. Exact-HB refinement r4 reduces that CAS opportunity to zero. Different
  unlocked-value sources carry different synchronization histories, so value-only lock quotienting
  is unsound. Regional P0 is exhausted for this 51-task production cohort.

## P2 no-consistent-extension Gate A1 (2026-07-20)

- A graph-matched, observation-only RF/CO `DecisionState` replaces stamp-derived pseudo-depth. It
  follows real execution copies and vector-clock cuts and never changes exploration.
- The strict fixed-15 recursive-PSO run preserves 7 correct terminals, 8 TIMEOUTs, every comparable
  search counter, and effectively neutral resources (all CPU +0.0095%, common-terminal CPU
  -0.0028%, summed RSS +0.0071%).
- Eleven tasks provide final or timeout-progress census evidence. Installed conflicts, derived
  cores, mapped choices, non-local conflicts, recurrence, and backjump distance are all zero. Four
  remaining timeouts stop before the hook has any learned fact or descendant to block.
- Together with full-725 V9/V10 evidence (zero all-sibling/parent-prefix elimination; 14,193,035
  core hits, zero extra checks avoided, CPU +4.99%), Gate A1 rejects CDCL/subtree blocking before
  implementation. Full report: `p2-backjump-gate-a1-report-20260720.md`.

## P1 clean broad resource decision (2026-07-20)

- A stable-based five-item P1 candidate completed a strict 725-by-2 server comparison. On 438
  common-solved tasks it reduced CPU geometric mean by 3.47%, summed CPU by 1.83%, and summed RSS
  by 3.21%, with zero semantic-summary or execution-count mismatches.
- The full workload does not pass the no-tradeoff rule: all CPU rises 0.43%, and the 31 common OOM
  tasks take 15.0% longer after NUMA assignment is swapped while every task still reaches the same
  12-GB cap. This is more progress before OOM, not an improved result.
- Canonical empty EventDeps alone delays the same OOM cohort by 8.19%; do not promote it despite
  earlier small/common-terminal benefits. Inline revisit alone is +1.40% on OOM and had only an
  approximately one-percent fixed-panel gain, so it is retained on dev without another broad run.
- Figures 3 and 4, strict claim limits, raw server roots, and exact hashes are in
  `p1-clean-full-725-report-20260720.md`. The next algorithmic direction is value-first/source-lazy
  RVF integration in the Deagle/Yogar finite solver, not another retained-layout micro-variant.

## Deagle/RVF first-model evidence (2026-07-20)

- The 283-task census admits 225 programs and finds same-value class opportunity in all 225. Value
  grouping removes 25.94% of RF selectors and 58.85% of pair terms.
- On 223 common first models, CPU geometric-mean/summed ratios are 0.64561/0.74641 and RSS ratios
  are 0.98306/0.98344. This passes the simultaneous time/memory research gate, but abstract RF is
  rejected before CAT/SC materialization. Exact member/order refinement is required before any
  verification or stable claim. Full report: `deagle-rvf-census-first-model-report-20260720.md`.
- Infrastructure: census r1 used a 100-GiB container for 48 tasks capped at 4 GiB, so BenchExec
  started zero tasks; launchers now use 220 GiB plus exact row checks. Panel r1 omitted stats-only
  and includes native continuation time, so it is pre-evidence only. A host rebuild later reused a
  Docker-owned tree and failed before compilation; rebuilding inside the original image passed
  18/18. Never rebuild that tree with host CMake.

## Deagle/RVF source-refinement r1 (2026-07-20)

- Cartesian class-member refinement is rejected: concrete-SC completes 2/15 within 120 seconds,
  while the candidate times out on 15/15. Incremental lazy member selectors repair that explosion,
  but `safe010_tso` only changes 310 -> 305 concrete candidates and about 45.58 -> 45.00 seconds.
  Do not repeat or broaden this sub-threshold result.
- The nine-combination concrete/refined SC oracle passes under ASan+UBSan. Abstract assignments
  remain rejected at materialization; only concrete refinements can be checked or replayed.
- Next: encode “some member of the selected value class is the latest same-location write before
  the load” directly in Deagle's SC ordering theory. Merely delaying source selectors does not
  remove repeated SC completion. Full report: `deagle-rvf-refinement-r1-report-20260720.md`.
