# Deagle SC-order shared-before decision (2026-07-20)

## Decision

Reject shared before-load atoms after one strict panel.  Aggregate resource changes are
below the project's practical-effect threshold and the candidate loses one terminal SAT
classification.  Do not repeat or run the full workload.  Candidate code is removed;
the experiment definition and result root are retained.

## Mechanism

The current latest-write formula repeatedly constructs `active(w) && w <sc r` for the
same `(load, write)` while considering different value classes and class members.  The
candidate memoizes that exact Boolean expression once per pair and reuses its solver
handle.  It changes neither the logical formula nor concrete `rf/co` extraction.

The 36-shape observation oracle runs both the baseline and shared formulation.  Each
matches direct concrete RF plus exact SC completion, and every extracted assignment is
accepted by the independent exact SC base.

## Strict panel

Authoritative root:
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-shared-before-panel-20260720-r1`.
It contains two complete 15-row XML lanes, 30 logs, a zero-status manifest, and a
completion marker.  Each task receives one CPU, 4 GiB, and 120 seconds.

| Metric | Baseline | Shared-before | Candidate/control |
|---|---:|---:|---:|
| summed CPU | 1,177.447 s | 1,167.568 s | 0.99161 |
| summed wall | 1,177.803 s | 1,167.788 s | 0.99150 |
| summed per-task peak RSS | 2,853,867,520 B | 2,838,691,840 B | 0.99468 |
| SAT / UNSAT / unknown | 4 / 3 / 8 | 3 / 3 / 9 | -- |
| classified | 7 | 6 | -- |

All six common terminal verdicts agree and all SAT witnesses are concrete and CAT-clean.
However, `mix035.oepc.yml` changes from SAT to timeout.  On the common cohort, CPU and
wall geometric-mean ratios are 1.05139 and 1.05278 with two regressions; RSS changes by
less than one percent.  Summed common CPU appears favorable because task sizes are highly
skewed, but it cannot compensate for the lost terminal result or the geometric-mean
regression.

## Interpretation

Z3 already hash-conses much of the repeated AST, so reducing wrapper-owned handles has
only sub-percent aggregate effect.  The changed construction order perturbs solver
branching enough to delay one SAT task.  This is neither a robust speedup nor an
acceptable coverage tradeoff.  Further SC-order work should restructure the existential
latest-write search itself rather than add more expression caches.
