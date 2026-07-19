# Optimization 12: cycle-check transitive-closure slicing

Date: 2026-07-16

## Decision

Reject and remove the production/test change. Every semantic gate passed and the
mechanism profile directly confirms reduced closure work and state. However, the
pre-registered primary analysis over tasks that completed all three models has an
aggregate CPU ratio of 0.99584 with task-bootstrap 95% CI [0.98957, 1.00131]. The upper
bound is not below one, so the result does not satisfy the retention rule. A broader
model-task analysis is favorable, but substituting it after seeing the result would
change the analysis unit post hoc.

## Transformation and semantic boundary

For every finite relation `R`, `acyclic(R+)`, `irreflexive(R+)`, and `acyclic(R)` are
equivalent. The generic normalizer therefore redirects terminal `acyclic` and
`irreflexive` checks on `R+` to `acyclic(R)` and removes the closure predicate only if
no normalized predicate consumes its complete value.

The rewrite is deliberately fail-closed. `empty(R+)`, a mix of cycle and value-observing
checks, downstream predicate use, and reflexive closure `R*` retain the complete closure.
Nested terminal closure chains are sliced one layer at a time. Predicate IDs, operands,
and checks are remapped deterministically; check names and source spans are preserved.
The implementation neither inspects a model name nor invokes a built-in GenMC memory
model checker.

## Stage-level cost change

| Stage | Removed work/state |
|---|---|
| Normalized analysis | one closure predicate, dependency, and stratum |
| Offline evaluation | packed transitive-closure allocation and row-union computation |
| Online integration | closure recomputation and publication |
| Checkpoint/rollback | one dense relation value from copied or retained state |
| Consistency check | DFS scans the usually sparser seed relation instead of `R+` |
| Explanation | explains the concrete seed cycle without closure provenance |

The candidate executions, reads-from choices, coherence choices, graph synchronization,
and base relations are unchanged. This optimization can reduce CAT/CAAT cost per
candidate; by itself it does not reduce GenMC's number of explored equivalence classes.

## Correctness evidence

| Gate | Result |
|---|---:|
| Server Release unit/property tests | 147/147 passed |
| Independent random finite-relation DFS oracle | passed for `acyclic(R+)` and `irreflexive(R+)` |
| Online mutation rows | 39 |
| Full Phase-2 oracle comparisons | 5,441, zero mismatch |
| Broad programs | 288 |
| SC/TSO/PSO broad pairs | 864 |
| Comparable broad matches | 852 |
| Mutually unsupported broad pairs | 12 |
| Broad mismatches | 0 |

The first expanded unit run passed 146/147. The only failure was expected certificate
invalidation: slicing changed the exact normalized summaries of the bundled recursive
SC/TSO/PSO models. While updating those closed-world fingerprints, the audit found that
`NormalizedModel::summary()` had omitted each check's kind. It now includes `empty`,
`irreflexive`, or `acyclic`, and a one-axiom TSO neighbor proves that changing only a
check kind revokes candidate/adaptive certification.

## Controlled performance experiment

- Inputs: established 96-task sample under SC, TSO, and PSO.
- Design: independently configured fresh before/after Release builds, six repetitions,
  3,456 cells.
- Limits: one core and 4 GB per task, 60 seconds, `--nthreads=1`.
- Scheduling: simultaneous 24-worker queues on disjoint core groups, swapped each
  repetition.
- Retention: zero verdict/safe-count mismatch; aggregate CPU bootstrap upper bound below
  one; no model CPU point regression over 0.5%; no RSS regression over 0.5%; mechanism
  profile must show unchanged candidates and reduced closure work/state.

All 36 XML files and 36 log archives completed, for exactly 3,456 rows. Measured
combined task overlap was 43--48. There were zero common-solved verdict mismatches and
zero safe-task execution-count mismatches. The after variant completed nine additional
run cells: seven plain timeouts and two timeout-with-found-false rows became terminal
`false(unreach-call)` results.

### Strict common-solved ratios (`after / before`)

The primary `all` group contains the 82 task identities with six paired observations
under every SC/TSO/PSO model. Each task contributes the geometric mean of its three
model ratios before task-level bootstrapping.

| Metric | Group | Tasks | Geometric mean | Task-bootstrap 95% CI | Median |
|---|---|---:|---:|---:|---:|
| CPU | aggregate common tasks | 82 | 0.99584 | [0.98957, 1.00131] | 0.99873 |
| CPU | SC | 90 | 0.99805 | [0.98393, 1.00851] | 1.00678 |
| CPU | TSO | 89 | 0.99734 | [0.99090, 1.00327] | 1.00077 |
| CPU | PSO | 82 | 0.98425 | [0.97441, 0.99349] | 0.99145 |
| Wall | aggregate common tasks | 82 | 0.99574 | [0.98980, 1.00067] | 0.99970 |
| RSS | aggregate common tasks | 82 | 0.99956 | [0.99891, 0.99994] | 0.99978 |

The secondary model-task analysis contains 261 pairs and estimates aggregate CPU at
0.99345 [0.98740, 0.99877]. It is useful sensitivity evidence but is not the frozen
primary analysis and therefore does not reverse the decision.

## Mechanism profile

A separate one-repetition profile compared only the 262 log identities completed by
both variants. One additional after-only log is excluded from every paired total.

| Paired aggregate | Before | After | Ratio | Decreased / same / increased logs |
|---|---:|---:|---:|---:|
| Profiled queries | 1,261,169 | 1,261,169 | 1.000 | 0 / 262 / 0 |
| Adaptive-offline queries | 1,054,311 | 1,054,311 | 1.000 | 0 / 262 / 0 |
| Offline evaluations | 1,054,517 | 1,054,517 | 1.000 | 0 / 262 / 0 |
| Offline time (ns) | 208,210,246,699 | 188,438,987,147 | 0.905 | 126 / 76 / 60 |
| Synchronization time (ns) | 339,800,305,474 | 318,687,142,193 | 0.938 | 126 / 76 / 60 |
| Transactional-copy time (ns) | 2,282,304,096 | 2,255,180,356 | 0.988 | 3 / 257 / 2 |
| Peak snapshot-equivalent bytes | 1,301,021,624 | 1,272,422,584 | 0.978 | 186 / 76 / 0 |
| Maximum history-base bytes | 247,172,648 | 247,172,648 | 1.000 | 0 / 262 / 0 |
| Maximum active events | 42,025 | 42,025 | 1.000 | 0 / 262 / 0 |

This supports the predicted local mechanism: the optimization removes closure
evaluation and dense derived state without changing candidate/query counts. It also
shows why it is not enough for the larger scalability problem: total exploration and
base-history size remain unchanged, and the saved per-query cost does not produce a
statistically established aggregate CPU improvement under the frozen primary unit.

## Artifacts

- `experiment-protocol.md`: frozen semantic and experiment contract.
- `cost-model.md`: pre-implementation time/space predictions.
- `analysis-final/base/rows.tsv`: normalized 3,456-row formal matrix.
- `analysis-final/base/comparison.json`: verdict, coverage, execution-count, overlap,
  and model-task sensitivity audit.
- `analysis-final/strict/metric-summary.tsv`: frozen task-level primary analysis.
- `analysis-final/profile-{before,after}.tsv`: per-log mechanism counters.
- `server/`: pulled XML, complete log archives, progress files, and correctness logs.
- Server root: `/data3/sujie/experiments/caat-optimization/acyclic-closure-slicing/`
  (not committed).
