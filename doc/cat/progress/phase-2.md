# Phase 2 progress: offline CAAT backend

This append-only record tracks each Phase 2 substage required by
`doc/cat/phase-2-plan.md`.

## Phase 2.0: specification and baseline

- Starting commit: `2a58e63e4a0bb8befa2412f61e92f9f3a2690065`.
- Pre-check: branch `genmc-caat`; local and `origin/genmc-caat` matched; worktree
  was clean. Re-read `doc/development.md`, project constraints, the Phase 1
  report, and existing Phase 2 inputs before editing.
- Research inputs: local OOPSLA 2022 CAAT paper; current Dat3M revision
  `a7e3e4843359dde3a0e29500a821030e2433e316`; existing Phase 1 CAT IR,
  evaluator, adapter, tests, and broad-validation evidence.
- License decision: Dat3M is MIT licensed and compatible, but no Java source is
  copied. Its predicate hierarchy, recursive placeholders, worklist, and
  derivation reasoner are design references for a dependency-free C++23
  implementation. herd remains an external behavioral oracle.
- Contract: freeze the offline CAAT semantics, explicit unsupported boundary,
  seven implementation substages, oracle hierarchy, and measurable completion
  gates. No production code changes in 2.0.
- Baseline verification: `git diff --check` passed; Phase 1 CAT/CLI integration
  tests passed 4/4; the frozen result still contains 574 matches and two
  explicitly unsupported rows across 576 model pairs.
- Key boundary: Phase 2 computes complete-graph stratified least fixed points
  and explanations. Backtrackable/incremental propagation is Phase 3.
- Gap to Phase 2 plan after this substage: all implementation items 2.1--2.6
  remain; the next target is recursive syntax plus normalized typed IR.

## Phase 2.1: recursive frontend and normalized IR

- Starting commit: `75b55585ff48583c33f671721d9df2b91eeda1e0`.
- Pre-check: re-read repository development rules, project constraints, and
  the Phase 2 plan; branch was `genmc-caat` and the worktree was clean.
- Reuse survey: retained Phase 1 source spans, AST ownership, `ValueType`,
  packed values, relational operators, diagnostics, and CMake/unit harness.
  Dat3M's recursive placeholders informed the separate cyclic predicate IR;
  no Java source or runtime dependency was copied.
- Contract: parse `let rec ... and ...`, add CAAT `domain`/`range`, infer types
  across forward references, and lower every derived predicate to one operator
  without changing Phase 1's topological `ModelIR` or CLI selection.
- Implementation: recursive declarations receive stable group IDs; malformed
  groups are diagnosed; domain/range execute over packed relations; the new
  immutable `NormalizedModel` reserves named predicate IDs before lowering,
  supports cyclic operands, expands conventional aliases, creates one-operation
  temporary predicates, and rejects ambiguous recursive types.
- Verification: clean build passed; clang-format dry-run passed; complete
  unit/property suite passed 104/104; focused CAT/CLI integration passed 4/4.
  Phase 1 SC/TSO/PSO golden summaries remain byte-identical.
- Plan gap: normalization and recursive syntax are implemented, but the CLI
  deliberately still selects Phase 1 `ModelIR`. Dependency/polarity analysis
  and declared-recursion validation are the Phase 2.2 target; no fixed-point
  evaluation is claimed yet.

## Phase 2.2: dependency, polarity, and admissibility analysis

- Starting commit: `79370580ec248dceab4763f369361ef6e62cb896`.
- Pre-check: re-read development rules, project constraints, and the Phase 2
  plan; branch was `genmc-caat` and the worktree was clean.
- Reuse survey: used normalized stable IDs and source spans from 2.1 and the
  paper's signed dependency/stratification definitions. Dat3M's dependency
  hierarchy was inspected for behavior only; the deterministic Tarjan analysis
  is dependency-free C++23.
- Contract: produce an immutable dependency-first SCC stratification and reject
  undeclared/split recursion, negative recursion, derived difference RHS, and
  non-domain-independent axioms before evaluation.
- Implementation: `ModelAnalysis` stores signed edges, deterministic strata,
  dense component IDs, and per-predicate domain-independence. Diagnostics name
  the recursive predicate, required cut predicate, or failing axiom.
- Correctness adjustment: the first DI implementation started Boolean
  recursion at false and rejected productive equations such as
  `ob = base | ob ; ob`. DI validation now starts from the greatest Boolean
  solution and removes violations, which admits guarded recursion while still
  rejecting unguarded `_`/`id` use.
- Verification: clean build and clang-format passed; complete unit/property
  suite passed 108/108; focused CAT/CLI integration passed 4/4. Hand-proof
  fixtures cover positive mutual recursion, undeclared/split recursion,
  negative recursion, semi-positivity/cutting, and domain independence.
- Plan gap: admissibility and strata are complete. Phase 2.3 must compute typed
  values for those strata and compare its recursive worklist result with an
  independent naive Kleene oracle.

## Phase 2.3: stratified least-fixed-point evaluator

- Starting commit: `ff5c6070b967d3173be72f88c7ab209d8cbe1e64`.
- Pre-check: re-read development rules, project constraints, and Phase 2 plan;
  branch was `genmc-caat` and the worktree was clean.
- Reuse survey: reused packed `EventSet`/`Relation`, pure relational operators,
  Phase 1 base-value contract and violation shape, plus Phase 2.2 strata. The
  scheduling design follows CAAT/Dat3M's dependency worklist, implemented
  independently over GenMC values.
- Contract: evaluate dependencies before users, initialize positive recursive
  SCCs at bottom, reschedule only same-stratum dependents whose operands
  changed, and terminate by finite-domain convergence without an iteration cap.
- Implementation: `CaatEvaluator` supports recursive sets and relations,
  aliases, projections, product, Boolean algebra, composition, inverse and all
  closures. Results expose every predicate value, per-predicate evaluation
  counts, worklist pushes, value changes, violations, and base-value errors.
- Verification: complete unit/property suite passed 112/112; focused CAT/CLI
  integration passed 4/4; format and build passed. Fixtures cover recursive
  transitive closure, mutual recursion, an empty fixed point, recursive set
  projection, and violations. RapidCheck compares random four-event reachability
  instances against an independent naive Kleene recurrence.
- Plan gap: fixed-point semantics are complete but provenance is not recorded.
  Phase 2.4 must attach derivations and produce replayable base-literal
  explanations for all three axiom kinds.

## Phase 2.4: violation explanations and CLI diagnostics

- Starting commit: `911c674c3955e096a6555d3fd0565626a53185ee`.
- Pre-check: re-read `doc/development.md`, project constraints, and the Phase 2
  plan; branch was `genmc-caat`. The only dirty files were the uncommitted
  `Reasoner.hpp/.cpp` draft and its CMake entry from the interrupted start of
  this same substage; no unrelated user changes were present.
- Reuse survey: retained normalized predicate IDs, final packed values,
  structured axiom witnesses, Config validation, CAT checker snapshots, and
  existing CLI integration harness. Dat3M's derivation reasoner informed the
  base-literal projection contract; the C++23 implementation is independent
  and adds no runtime dependency.
- Contract: reconstruct finite derivations after offline evaluation, choose a
  deterministic shortest-known explanation, emit negative ground literals for
  semi-positive difference, reject stale snapshots, and expose opt-in CLI
  rendering without changing default output.
- Implementation: `Reasoner` rebuilds provenance stratum by stratum, including
  positive recursion, composition and closure paths; explanations are sorted,
  deduplicated and compared by literal count then stable lexical order. It
  handles set-empty, relation-empty, irreflexive and closed-cycle witnesses.
  `--explain-cat` normalizes/analyzes a non-recursive Phase 1 model only when
  requested and prints source position, witness and base literals for rejected
  candidates; the Phase 1 evaluator remains the verdict oracle.
- Verification: clean CMake build passed; complete unit/property suite passed
  116/116; focused CLI plus SC/TSO/PSO integration passed 4/4. Direct replay
  using relation algebra reproduces all three axiom kinds and the negative
  difference fixture. CLI tests prove the flag requires `--model-file`, emits
  on a real SB rejection, and leaves default output unchanged. `git diff
  --check` passed.
- Static-analysis boundary: `clang-tidy` read the generated database but the
  standalone Apple invocation could not find libc++ `<algorithm>`/`<cstddef>`;
  this is the recorded environment issue seen in Phase 1. Its actionable local
  readability notes were reviewed; compilation and tests use the configured
  LLVM 20 toolchain successfully.
- Plan gap: provenance and requested diagnostics are complete for the Phase 1
  path. Recursive CAT files still fail in the Phase 1 topological compiler and
  therefore cannot reach the checker. Phase 2.5 must select the normalized
  backend directly, add recursive model files, validate base availability, and
  exercise real concurrent programs with one and two workers.

## Phase 2.5: GenMC integration and recursive model corpus

- Starting commit: `df45ac7d6a9932435c1b71e041b3b54997ec468d`.
- Pre-check: re-read repository development rules, project constraints, and
  the Phase 2 plan; branch was `genmc-caat`, local/remote commits matched, and
  the worktree was clean.
- Reuse survey: retained Config's parse-once ownership, normalized/analyzed
  immutable models, the existing SC/TSO host profiles, conservative CAT
  candidate enumeration, `GraphAdapter`, and the CaatEvaluator/Reasoner. No
  Dat3M Java code or additional dependency was imported.
- Contract: select normalized CAAT for declared recursion and forward-reference
  models, keep Phase 1 files on their original evaluator, reject unsupported or
  non-prefix-monotone inputs before LLVM exploration, and prove real SC/TSO/PSO
  behavior with one and two workers.
- Implementation: Config now retains both normalized equations/analysis and,
  where applicable, the Phase 1 DAG. Recursive or forward-reference files set
  an explicit backend flag; checker/factory selection uses IR host metadata,
  never filenames. The generic checker computes a complete fixed point from
  scratch for every candidate snapshot and reuses that result for explanations.
- Soundness boundary: offline semi-positive difference remains implemented and
  tested, but a model requiring the CAAT backend is rejected if it contains a
  difference equation. A negative base fact can disappear as a GenMC prefix
  grows, so using it for Phase 2 pruning would be unsound; trail-aware handling
  remains Phase 3 work.
- Model corpus: added clean-room `recursive-sc.cat`, `recursive-tso.cat`, and
  `recursive-pso.cat`. Each preserves the corresponding Phase 1 equations and
  defines final reachability with a positive recursive least fixed point.
- Verification: build passed; 119/119 unit/property tests passed; focused CLI,
  Phase 1 SC/TSO/PSO, and recursive differential tests passed 5/5. The recursive
  test compares 3 models × 8 real C programs × 2 worker counts = 48 recursive
  results against Phase 1 status and semantic summaries. Coverage includes
  relaxation, same-location order, RMW, SC fences, thread lifecycle, heap
  storage, safety errors, and the PSO-distinguishing program. Recursive
  `--explain-cat` emits a fixed-point/base-literal explanation on SB.
- Plan gap: Phase 2.5 integration acceptance is complete. Phase 2.6 still needs
  at least 200 distinct recursive program/model pairs, the frozen 288-program
  Phase 1 regression, aligned herd/Dat3M evidence, throughput/memory/end-to-end
  measurements, requirement audit, final report, and clean remote equality.
