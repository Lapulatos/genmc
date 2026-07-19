# Deagle SC-order compact-distinct decision (2026-07-20)

## Decision

Reject both compact `distinct` formulations and stop this direction.  They reduce
retained memory but regress fixed-budget time and lose one terminal classification.  The
pairwise guarded same-address constraints remain the development baseline.  Candidate
code is removed; the paired definition and raw result roots are retained for audit.

## Variants and correctness

Both variants replace many guarded pairwise bit-vector inequalities with one native Z3
`distinct` expression per address:

1. r1 applies distinctness only to static writes and allows read/read and read/write rank
   ties, which are semantically extensible to a total SC order;
2. r2 applies distinctness to every same-address static event, preserving the control's
   rank symmetry breaking and changing only the constraint representation.

The solver sort oracle and the 36-shape concrete RF plus exact-SC observation oracle pass
for the native-distinct candidate.  Every terminal candidate agrees with the control and
all candidate SAT witnesses are concrete and CAT-clean.

## Strict panels

Raw roots:

- r1 write-only distinct:
  `/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-compact-panel-20260720-r1`;
- r2 all-event distinct:
  `/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-compact-panel-20260720-r2`.

Each root has two XML files with 15 runs each, 30 logs, a zero-status manifest, and a
completion marker.  Both lanes use one CPU, 4 GiB, and 120 seconds per task.

| Variant | CPU ratio | Wall ratio | RSS ratio | Control/candidate classified |
|---|---:|---:|---:|---:|
| r1: writes only | 1.00308 | 1.00315 | 0.83392 | 7 / 6 |
| r2: all events | 1.01498 | 1.01493 | 0.90813 | 7 / 6 |

For r1, the six common terminal tasks have CPU/wall/RSS summed ratios
0.89668/0.89668/0.84393, but the fixed-budget task set regresses slightly and
`mix035.oepc.yml` changes from SAT to timeout.  The favorable completed subset therefore
cannot justify promotion.

For r2, common-task CPU/wall summed ratios regress to 1.02589/1.02598 while RSS improves
to 0.87439.  Two of six common tasks become slower, and `mix035.oepc.yml` is again lost.
This isolates the native n-ary representation itself as an unfavorable memory-versus-
search tradeoff; the r1 result is not merely caused by permitting read-rank ties.

## Interpretation

The compact AST reduces BenchExec peak RSS, but Z3's handling of native bit-vector
`distinct` changes branching and delays a previously reachable SAT model.  Since both a
semantic partial-order variant and a representation-only variant fail the simultaneous
time/memory/coverage gate, no repetition or 283-task run is warranted.  Future work
should target latest-write formula sharing or decomposition rather than coherence-rank
distinctness.
