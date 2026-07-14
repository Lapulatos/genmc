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
