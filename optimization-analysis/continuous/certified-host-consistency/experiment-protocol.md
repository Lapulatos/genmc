# Optimization 10: certified host consistency

## Decision

Rejected by method scope before completing the formal matrix. Although the prototype
passed 142 unit/property tests, 5,441 mutation-oracle checks, and the 864-pair broad
differential, it replaces exact SC/TSO CAT/CAAT consistency queries with GenMC's built-in
generated checker. That measures a fixed-model bypass rather than an improvement to the
generic CAT evaluator or incremental/online CAAT integration. One completed SC paired
repetition is retained only as quarantined process evidence and is not used for a
performance claim. The production and test changes were reverted exactly.

## Hypothesis

For the exact bundled recursive SC and TSO models, the normalized semantic fingerprint
contains every checked relation and axiom and matches the corresponding generated GenMC
checker. On candidates produced by the same certified host enumeration, GenMC already
enforces coherence and atomicity, so delegating the remaining consistency query avoids
CAT graph materialization and fixed-point evaluation. This is not a claim about arbitrary
externally assembled graphs. PSO and every edited/custom model remain on generic CAAT.

## Correctness boundary

- Enable only when `certifiedCandidateProfile() == hostProfile()`.
- Disable under `--cat-oracle` so the existing full-recomputation oracle remains active.
- Disable under `--explain-cat` because explanations require CAAT predicate values.
- Fail closed for PSO, renamed/reordered/edited models, and arbitrary CAT input.
- Candidate pruning behavior is unchanged.

## Gates

1. macOS: compile only; no tests or benchmarks.
2. Server Docker: complete unit/property suite.
3. Server Docker: 39-row mutation suite with per-query Phase 2 oracle.
4. Server Docker: frozen 288-program × SC/TSO/PSO broad differential suite.
5. Explicit activation audit: SC/TSO use the certified path; PSO, oracle, explanation,
   and a near-neighbor model do not.
6. Fresh simultaneous before/after BenchExec matrix: 96 tasks × SC/TSO/PSO × six
   repetitions × two variants = 3,456 cells, 44--48 measured overlap.

## Pre-registered retention rule

Keep only if all correctness gates have zero mismatch, SC and TSO show a statistically
supported aggregate CPU reduction (task-cluster bootstrap 95% CI below 1), and PSO has
no material change beyond the existing 0.5% point-regression tolerance. Record wall,
CPU, peak RSS, solved cells, verdicts, and safe execution counts. Otherwise revert the
entire prototype. Raw XML and logs stay outside Git.
