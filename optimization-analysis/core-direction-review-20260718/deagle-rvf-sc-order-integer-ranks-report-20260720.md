# Deagle SC-order integer-rank decision (2026-07-20)

## Decision

Reject the integer-rank encoding and stop after one strict 15-task panel.  It improves
time and terminal coverage but nearly doubles aggregate memory, violating the project's
requirement that an optimization improve time and memory together.  The proven
bit-vector SC-order encoder remains the development baseline; the integer implementation
is removed rather than left as a selectable mode.

## Candidate

The candidate replaced each bounded bit-vector SC rank with an unbounded Z3 integer while
preserving the same value-class, program-order chain, same-location distinctness, and
latest-write constraints.  Model extraction supported negative integer ranks and emitted
the same concrete `rf/co` representation.  The nine-combination, 36-shape, and mutex
oracles were each run for both rank sorts and passed, as did the solver sort/value tests.

## Strict panel

Authoritative root:
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-rank-panel-20260720-r1`.
The launcher validated two XML files with exactly 15 runs each, 30 logs, and a completion
marker.  Both modes use one CPU, 4 GiB, and 120 seconds per task inside a 220-GiB Docker
container with the host cgroup mounted.

| Metric | Bit-vector control | Integer candidate | Candidate/control |
|---|---:|---:|---:|
| summed CPU | 1,159.401 s | 852.239 s | 0.73507 |
| summed wall | 1,159.603 s | 852.480 s | 0.73515 |
| summed per-task peak RSS | 2,852,454,400 B | 5,632,520,192 B | 1.97462 |
| SAT / UNSAT / unknown | 4 / 3 / 8 | 6 / 4 / 5 | -- |
| classified | 7 | 10 | -- |

All seven common classifications agree and neither CPU nor wall regresses on that
cohort.  Their CPU/wall summed ratios are 0.21311/0.21324.  However, common-task memory
has geometric-mean and summed ratios of 1.10814 and 1.24096; four of seven tasks regress.
No candidate SAT witness is abstract or CAT-invalid.

## Interpretation

Z3's arithmetic order theory solves this panel substantially faster than bit-blasted
ranks and obtains three extra terminal results, but its retained arithmetic state is much
larger.  Adding bounds would add constraints without removing the underlying integer
theory overhead, so no follow-up repetition or full run is justified.  Future SC-order
work should reduce the number of ordering/latest-write terms while keeping bounded
bit-vector ranks, rather than changing the rank sort.
