# Optimization 11: generic linear-recursion closure canonicalization

Date: 2026-07-16

## Decision

Retain the optimization. It passes every semantic gate and reduces aggregate CPU and
wall time with task-bootstrap confidence intervals wholly below 1. Peak RSS is
unchanged. The implementation remains in the generic CAT/CAAT normalization and
reasoning pipeline; it does not dispatch on model identity or invoke GenMC's built-in
SC/TSO consistency checkers.

## Transformation and semantic boundary

For finite relations, the least fixed points of

`X = R | (X ; R)` and `X = R | (R ; X)`

are exactly the non-reflexive transitive closure `R+`. The normalizer recognizes all
four union/composition operand orientations, requires the two seed expressions to be
algebraically identical and recursion-free, and lowers the equation to the existing
generic packed `TransitiveClosure` predicate. It retains the named predicate and its
complete relation value for downstream predicates, every check kind, oracle comparison,
and explanation.

The matcher fails closed for unequal seeds, other operators, extra terms, and mutual
recursion. It never inspects model names, filenames, host profiles, or task identities.
The bundled recursive SC, TSO, and PSO models each lose one generated composition
predicate while preserving their exact certification summaries.

## Correctness evidence

| Gate | Result |
|---|---:|
| Server Release unit/property tests | 144/144 passed |
| Random finite-relation Kleene oracle | all four orientations passed |
| Online mutation rows | 39 |
| Full Phase-2 oracle comparisons | 5,441, zero mismatch |
| Broad programs | 288 |
| SC/TSO/PSO broad pairs | 864 |
| Comparable broad matches | 852 |
| Mutually unsupported broad pairs | 12 |
| Broad mismatches | 0 |
| Formal common-solved verdict mismatches | 0 |
| Formal safe execution-count mismatches | 0 |

The first server test run passed 142/144 and exposed a provenance bug rather than a
verdict bug. For a non-reflexive closure, diagonal membership `(x,x)` requires a
non-empty seed cycle; the previous generic closure reasoner incorrectly returned the
empty path, which is valid only for reflexive closure. The corrected reasoner replays a
non-empty seed cycle. Both explanation regressions then passed, and no performance run
started before the full correctness gates were green.

## Controlled performance experiment

- Inputs: the established 96-task sample under SC, TSO, and PSO.
- Design: fresh before/after Release builds, six repetitions, 3,456 run cells.
- Limits: one core and 4 GB per task, 60 seconds, GenMC `--nthreads=1`.
- Scheduling: simultaneous 24-worker queues on disjoint core groups; groups swap each
  repetition.
- Measured combined overlap: 44--48 tasks.
- Analysis unit: for each model/task, take the median over six repetitions; bootstrap
  tasks, not individual run cells, with 20,000 fixed-seed resamples.

All 36 XML result files and 36 log archives completed. The before side had 1,554
correct run cells and the after side 1,569. Fifteen former timeouts completed after the
rewrite: twelve as `true` and three as `false(unreach-call)`. Three other status strings
changed from plain timeout to timeout-with-found-false and remain resource-limited, not
new correct results.

### Strict common-solved ratios (`after / before`)

| Metric | Group | Tasks | Geometric mean | Task-bootstrap 95% CI | Median |
|---|---|---:|---:|---:|---:|
| CPU | aggregate common tasks | 80 | 0.9901 | [0.9832, 0.9959] | 0.9953 |
| CPU | SC | 90 | 0.9552 | [0.9292, 0.9777] | 0.9927 |
| CPU | TSO | 89 | 0.9833 | [0.9724, 0.9933] | 0.9967 |
| CPU | PSO | 80 | 0.9814 | [0.9681, 0.9929] | 0.9935 |
| Wall | aggregate common tasks | 80 | 0.9896 | [0.9824, 0.9956] | 0.9935 |
| Wall | SC | 90 | 0.9563 | [0.9299, 0.9791] | 0.9933 |
| Wall | TSO | 89 | 0.9832 | [0.9722, 0.9935] | 0.9953 |
| Wall | PSO | 80 | 0.9787 | [0.9660, 0.9903] | 0.9913 |
| RSS | aggregate common tasks | 80 | 1.00002 | [0.99985, 1.00019] | 0.99996 |

The aggregate CPU and wall improvements are about 1%. SC benefits most, while TSO and
PSO still have confidence intervals below 1. No model regresses under the preregistered
0.5% point-estimate guard. RSS is statistically and practically unchanged.

## Mechanism profile

A separate one-repetition, `--cat-stats`-enabled profile was used only for structural
attribution. It is not mixed into the formal timing estimates. There are 259 common
completed logs; after additionally completes two logs.

| Paired aggregate | Before | After | Ratio | Decreased / same / increased tasks |
|---|---:|---:|---:|---:|
| Profiled queries | 842,811 | 842,811 | 1.000 | 0 / 259 / 0 |
| Adaptive-offline queries | 689,082 | 689,082 | 1.000 | 0 / 259 / 0 |
| Offline evaluations | 689,269 | 689,269 | 1.000 | 0 / 259 / 0 |
| Offline time (ns) | 186,530,086,512 | 116,464,223,722 | 0.624 | 153 / 76 / 30 |
| Synchronization time (ns) | 272,994,089,067 | 204,814,161,837 | 0.750 | 150 / 76 / 33 |
| Transactional-copy time (ns) | 704,511,575 | 670,988,746 | 0.952 | 3 / 255 / 1 |
| Peak snapshot-equivalent bytes | 851,127,888 | 830,639,384 | 0.976 | 183 / 76 / 0 |
| Maximum active events | 33,619 | 33,619 | 1.000 | 0 / 259 / 0 |
| Maximum history-base bytes | 174,173,816 | 174,173,816 | 1.000 | 0 / 259 / 0 |

Offline-time ratios by model are SC 0.351, TSO 0.766, and PSO 0.690. Snapshot-equivalent
state decreases on 183 common tasks and increases on none. Candidate/query counts,
active-event counts, and retained base history do not change. This directly supports
the proposed mechanism: eliminate one recursive composition value and repeated
fixed-point work without pruning candidates or weakening consistency checks.

The current certified adaptive-offline path does not publish internal `eval-ops`,
`value-changes`, or `queue-pushes` counters, so those fields remain zero on both sides.
They are not used to claim a reduction. Offline time and snapshot-equivalent bytes are
the available direct measurements.

## Artifacts

- Protocol: `experiment-protocol.md`
- Pre-implementation cost map: `cost-model.md`
- Strict metrics: `analysis-final/strict/metric-summary.tsv`
- Complete comparison audit: `analysis-final/base/comparison.json`
- Normalized 3,456 rows: `analysis-final/base/rows.tsv`
- Paired profile tables: `analysis-final/profile-before.tsv` and
  `analysis-final/profile-after.tsv`
- Unit/oracle/broad evidence: `server/evidence/`
- Complete formal XML/logs: `server/formal-fresh/`
- Complete profile XML/logs: `server/profile-fresh/`

## Retention rule audit

- Zero verdict mismatch: pass.
- Zero safe exploration-count mismatch: pass.
- Aggregate CPU task-bootstrap upper CI below 1: pass (0.9959).
- No model point regression above 0.5%: pass; every model improves.
- No RSS regression: pass; aggregate ratio 1.00002 with CI spanning 1.
- Full value and explanation completeness: pass after the explicit diagonal-cycle fix.

The four production/test files are therefore retained for the next integration review.
Experiment/server paths, definitions, XML, logs, and analysis artifacts remain outside
the production commit candidate.
