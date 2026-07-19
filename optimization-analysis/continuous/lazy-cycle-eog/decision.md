# P0.7b exact lazy cycle-EOG decision

Date: 2026-07-16
Baseline: `35a595ab5a706c7fbae52a0f4ccc6d29655fcdd8`
Decision: retain P0.7b2 composition-cost selector

## Semantic boundary

The analyzer admits only exclusive, non-recursive, positive relation cones rooted at an
`acyclic` check. The lazy evaluator enumerates the exact extensional relation and applies
deterministic three-colour cycle detection. Base predicates remain materialized. Shared
consumers, unsupported operators, recursive cones, explanation, oracle, and preventive
pruning fail closed to the generic evaluator. The implementation does not consult a
built-in SC/TSO checker verdict.

The P0.7b2 selector retains lazy evaluation only when an admitted cone contains a
composition. It is structural: it does not inspect the model name, host profile, event
count, runtime verdict, or checker result.

## Correctness evidence

- Release unit/property tests: 156/156 passed.
- Mutation oracle: 39 rows and 5,441 full comparisons, zero mismatch.
- Broad differential: 864 SC/TSO/PSO pairs, 852 comparable matches, 12 mutually
  unsupported, zero mismatch.
- Formal matrix: 1,152 before and 1,152 after cells, zero opposite terminal verdict and
  zero common-terminal execution-count mismatch.
- Activation smoke: SC performs zero lazy checks; TSO and PSO each perform 10.

## Frozen performance gate

The four-repetition, 96-task, SC/TSO/PSO matrix used 48 BenchExec task workers, one CPU,
4 GiB, and 60 seconds per cell. All 12 paired runsets exited zero.

| Metric | Result | Gate |
|---|---:|---:|
| Aggregate common-terminal CPU | 0.97950, 95% CI [0.95888, 0.99714] | CI upper <= 1.02 |
| TSO common-terminal CPU | 0.97303 | <= 1.03 |
| PSO common-terminal CPU | 0.94685 | <= 1.03 |
| Large-event peak snapshot bytes | 0.57732 | >= 15% reduction |
| Large-event process RSS | 0.66807 | >= 15% reduction |
| New OOM | 0 | none |

All frozen retention conditions pass. Forty baseline OOM cells become TIMEOUT rather
than a false terminal result. `c/pthread/queue_ok_longest.yml` changes from PSO TIMEOUT
to the same correct `true` result in all four repetitions.

## Where time and space fall

- Analysis removes only certified derived cycle-cone values from the incremental value
  array. This avoids their persistent relation storage.
- Incremental updates skip propagation, equality tests, undo data, and checkpoint copies
  for those elided values.
- Offline checks enumerate the final cycle relation directly, avoiding packed
  union/composition materialization. On large tasks, offline time falls to 0.68334 overall,
  0.38948 for TSO, and 0.14851 for PSO.
- Large-task copy time falls to 0.20126 overall, 0.06168 for TSO, and 0.10584 for PSO.
- The selector disables this path for union-only SC cones. SC lazy checks fall from
  893,948 in P0.7b1 to zero, and large SC RSS returns from 1.25667 to 1.00005 of baseline.

## Limits and next target

- SC common-terminal CPU is still 1.01531 [1.00701, 1.02376]. The frozen aggregate gate
  passes, but this result does not support an SC speedup claim.
- DFS retains deduplicated successor rows on its active recursion path. Auxiliary space is
  `O(N + E_path)` in the worst case, not strict `O(N)`.
- The retained TSO/PSO path still emits 6.181 billion candidate root edges for 4.739
  billion unique edges. The next optimization should reduce repeated composition
  enumeration with exact rollback-safe topological/EOG state, not broaden the current
  selector or substitute a built-in checker.

## Artifacts

- Raw XML/logs: `server-results/formal-v2/`
- Core analysis: `server-results/analysis-v2/comparison.json`
- Lazy-EOG analysis: `server-results/analysis-v2/extra.json`
- Correctness and activation logs: `server-results/*-selector.log` and
  `server-results/selector-*-smoke.log`

