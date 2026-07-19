# Optimization 05a: moved base plus unreachable-history removal

Date: 2026-07-15

## Decision

Reject and split the prototype. Correctness held, but a fully socket-balanced
3,456-cell matrix showed a statistically supported 0.57% aggregate CPU regression.
The by-value/move part is removed. A follow-up prototype keeps only the independently
justified removal of an unreachable small-epoch history/checkpoint.

## Change evaluated

1. Change `IncrementalCaatEvaluator::initialize` from `const BaseValues&` to a by-value
   parameter and move the just-materialized snapshot into evaluator ownership.
2. For a certified adaptive-offline epoch at or below 512 stable events, do not retain
   a duplicate `HistoryEntry::base` and evaluator checkpoint. The next changed small
   query rebuilds before history lookup; a large transition keeps exact insertion and
   rebuild fallbacks.

## Correctness gates

- Local macOS: compilation only, successful; no tests run.
- Server Docker Release build: 142/142 unit/property tests passed.
- Mutation stress: 39 rows, 5,441 Phase 2 oracle checks, zero mismatch.
- SV-COMP oracle: 288 rows; 237 completed logs contained 39,879 full recomputation
  checks, zero oracle mismatch and zero incorrect verdicts.
- Formal paired matrix: zero status changes, zero common-solved verdict mismatches,
  and zero safe-task execution-count mismatches.

## Formal matrix

- 96 tasks × SC/TSO/PSO × six repetitions × before/after = 3,456 cells.
- Before and after ran simultaneously as two `-N24` queues on disjoint `0-23` and
  `28-51` CPU pools.
- CPU pools were swapped each repetition; the sixth repetition made placement exactly
  balanced at three runs per variant per pool.
- XML-derived combined peak overlap was 45–47 tasks for all 18 pairs.
- Per-task medians require all six repetitions; confidence intervals use 20,000 fixed-
  seed task bootstrap samples.

### Strict common-solved after/before ratios

| Model | Task pairs | Wall ratio (95% CI) | CPU ratio (95% CI) | RSS ratio (95% CI) |
|---|---:|---:|---:|---:|
| SC | 90 | 0.9910 [0.9579, 1.0291] | 0.9971 [0.9909, 1.0031] | 1.00052 [1.00027, 1.00080] |
| TSO | 89 | 1.0122 [0.9825, 1.0465] | 1.0139 [1.0057, 1.0224] | 1.00021 [0.99993, 1.00051] |
| PSO | 80 | 0.9843 [0.9533, 1.0139] | 1.0063 [0.9981, 1.0149] | 1.00030 [0.99994, 1.00073] |
| All | 259 | 0.9961 [0.9774, 1.0158] | 1.00568 [1.00129, 1.01023] | 1.00035 [1.00017, 1.00054] |

Before and after each produced 1,554 correct cells: SC 540, TSO 534, and PSO 480.

## Interpretation

The expected copy saving did not appear in ordinary execution. A plausible mechanism
is loss of capacity reuse: copy assignment can reuse the evaluator's existing map and
packed-vector allocations, whereas move assignment discards that state and transfers a
freshly allocated materialization on every rebuild. The experiment does not isolate
allocator events, so this is a hypothesis, not a proven root cause. The decision does
not depend on it: the balanced CPU result is sufficient to reject the combination.

## Artifacts

- Raw XML, complete log archives, console logs, and progress ledger: `server/formal/`
- Normalized 3,456 rows: `analysis/rows.tsv`
- Bootstrap comparison and concurrency peaks: `analysis/comparison.json`
- Oracle XML/log archives: `server/oracle/`
- Oracle aggregate: `oracle-profile.json`
- Reproducible definitions and paired runners: `definitions/`, `run_paired.sh`,
  `run_paired_r06.sh`
