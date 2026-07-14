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
