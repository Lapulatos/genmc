# Exact successor-cursor composition report (2026-07-19)

## Candidate

`--cat-fast-composition` replaces the quadratic lhs membership scan in CAAT relation composition
with exact `Relation::nextSuccessor()` enumeration. The rhs row merge is unchanged for packed
dense, explicit CSR, and other structural relations. The switch is evaluated on top of
`--cat-primitive-cache --cat-fast-primitive-build --cat-fast-checks`; the incremental mutation
oracle deliberately continues to use the old composition implementation.

## Correctness gates

- Local GCC 13 / LLVM 15: 223 passed, one expected Z3-availability skip.
- Server Release: 223 passed, one expected skip.
- Server ASan+UBSan with leak detection: 44/44 focused tests passed.
- Mutation: 39 rows, 5,416 old-implementation oracle comparisons, zero mismatch. One earlier run
  hit the known parallel early-error path before recording an oracle check and is not counted.
- Broad: 288 programs x SC/TSO/PSO = 864 pairs; 852 matches, 12 mutually unsupported, zero
  mismatch.

## Fixed-15 simultaneous paired result

Both lanes used the same Release binary and started together under BenchExec: baseline on CPUs
0-3 and candidate on CPUs 4-7. Five identical 61-second TIMEOUTs dominate both totals.

| Metric | Baseline | Candidate | Change |
|---|---:|---:|---:|
| all-task CPU | 471.234 s | 426.753 s | -9.44% |
| completed-task CPU | 166.275 s | 121.763 s | -26.77% |
| suite wall | 138.39 s | 133.80 s | -3.32% |
| CAT consistency | 89.888 s | 45.972 s | -48.86% |
| offline evaluation | 48.859 s | 20.504 s | -58.03% |
| ordinary checks | 17.486 s | 16.601 s | -5.06% |
| composition | 5.631 s | 0.641 s | -88.61% |
| materialization | 37.636 s | 19.223 s | -48.92% |
| aggregate RSS | 398,741,504 B | 398,729,216 B | -0.003% |
| maximum RSS | 27,021,312 B | 26,988,544 B | -0.12% |

Terminal/TIMEOUT counts remain 10/5. Complete, blocked, and exceeding-bound counts match exactly
at 18,693 / 197,703 / 0.

Against the immediately preceding cache + fast-build + fast-check candidate, the new composition
path changes all-task CPU 432.698 -> 426.753 s (-1.37%), completed-task CPU 127.688 -> 121.763 s
(-4.64%), offline evaluation 25.594 -> 20.504 s (-19.89%), and composition 5.621 -> 0.641 s
(-88.59%). Aggregate RSS falls 0.014%.

## Decision

Retain the exact path behind its experimental switch and use it as the next combined foundation.
It is a real evaluator improvement with independent-oracle evidence, but it removes no hard
TIMEOUT and the fixed-panel all-task CPU improvement is 9.44%, below the strict 10% expansion
gate. Do not run pthread-wmm 283 yet. The remaining ordinary offline cost is now dominated by
check evaluation (16.601 s), while composition is only 0.641 s.

Raw evidence:

- `server-results/fast-composition-20260719c/`
- `server-results/primitive-fast-compose-paired-15-20260719b/`
