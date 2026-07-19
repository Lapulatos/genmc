# P1b/P1c exact SC completion and refinement report

## Decision

The exact active-candidate SC completion is semantically viable and cheap for one fixed RF
candidate, but repeated graph blocking and deletion-based RF-core extraction are rejected as
the performance mechanism. The next core implementation must use one active-candidate ordering
query with RF assumptions and a native UNSAT core.

No production verdict path was changed. `noWitness` was never treated as TRUE; completed
candidates still require exact recursive-SC base checks and native replay.

## Correctness construction

- The completion problem is built from the same materialized `R/W/IW/po/rf/rmw/tc/tj/loc`
  primitives consumed by the generic CAT evaluator.
- The independent `SCGoodWritesSolver` fixes RF and derives a total order; CO is projected from
  that order and checked against recursive SC and RMW atomicity.
- RMW atomicity is relaxed during no-witness search. Therefore relaxed `noWitness` is safe for
  refinement; a witness that fails exact CAT atomicity is inconclusive and is not learned.
- Local, server GCC 13 Release, and Linux ASan+UBSan focused gates pass 24/24.
- The exhaustive unit oracle compares all nine fixed-RF shapes of a two-write/two-read graph
  against every CO permutation.

## Fixed 15-task iterations

All runs use 20 seconds, 4 GiB, one core per task, and 15 parallel workers.

| run | mechanism | completed candidates | CPU s | decisive observation |
|---|---|---:|---:|---|
| a | first abstract candidate + exact completion | 0/15 | 102.4-class first-query cost | all first candidates infeasible; completion itself 0.16--3.31 ms |
| b | block whole control/RF graph, max 1000 | 0/15 | 273.50 | 12 timeouts; three TRUE tasks reject 1000 graphs |
| c | RF core present but disabled for RMW | 0/15 | 273.50 | all 15 contain RMW; correctly fell back to whole-graph blocking |
| d | relaxed-RMW sound core + greedy deletion | 0/15 | 275.41 | two TRUE tasks exhaust in 0.59/1.46 s; core extraction dominates others |
| e | QuickXplain core | 0/15 | 274.87 | same coverage; two TRUE tasks 0.46/0.86 s; 13 timeouts remain |
| f | one active-candidate rank SMT + native assumption core | 0/15 | 315.58 | all 15 time out inside the first ordering query |

Run e's first cores have size one but require 12 completion checks. This proves that the issue
is not core strength; it is obtaining the core by repeated solver calls.

Run f rejects generic timestamp/rank SMT as the replacement. The earlier 17--50 graph states
were dead-end search depths, not active-event counts. Pairwise distinct ranks plus latest-write
RF constraints rebuild a quadratic ordering formula and fail before returning either SAT or an
UNSAT core. The next admissible design is a graph-native incremental ordering theory that adds
derived order only on demand; no further rank-encoding tuning is justified.

## Infrastructure correction

The first run-a launch was mistakenly invoked from the server host and failed before executing
tasks because the result parent is Docker-root-owned. The valid run executed entirely inside
`genmc15noble:sujie`. The launcher now checks `/.dockerenv` and refuses host execution before
creating output. Old results were untouched.

## Evidence

- `server-results/finite-sc-completion-panel-20260718a/`
- `server-results/finite-sc-completion-panel-20260718b/`
- `server-results/finite-sc-completion-panel-20260718c/`
- `server-results/finite-sc-completion-panel-20260718d/`
- `server-results/finite-sc-completion-panel-20260718e/`
- `server-results/finite-sc-completion-panel-20260718f/`
