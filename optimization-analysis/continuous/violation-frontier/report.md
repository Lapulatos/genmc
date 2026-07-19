# Optimization 03: incremental violation / cycle frontier

## Decision

Reject and remove both prototypes. They were semantically correct in the tested scope,
but neither improved performance. The authoritative first prototype regressed overall
wall time by 11.0% and CPU time by 12.1%.

## Implementations tried

1. Preserve an existing insertion-only violation witness. For a newly violated
   `empty` or `irreflexive` check, inspect only added facts. For `acyclic`, search for a
   return path from every newly added edge.
2. Preserve the cheap cases, detect newly added diagonal cycles directly, and fall back
   to one legacy full DFS whenever an acyclic frontier contains more than one edge.

Initialization, replacement, rollback, and rebuild always retained the full evaluator.
The experimental oracle compared every predicate and violated check and validated each
non-unique witness against the exact offline relation.

## Correctness evidence

- Server Docker Release tests: 142/142 with the prototype-specific regression test.
- Oracle matrix: 96 C.Concurrency tasks x SC/TSO/PSO = 288 runs.
- Completed oracle records: 237; full Phase 2 query comparisons: 39,879.
- Oracle mismatches: 0.
- Formal before/after common-solved verdict mismatches: 0.
- Formal safe-task exploration-count mismatches: 0.
- After removing the prototype, the server Docker suite returned to 141/141 passing.

## Performance evidence

The formal matrix used 96 tasks, three models, three repetitions, and before/after in
the same binary via an experiment-only switch: 1,728 run cells. BenchExec was configured
with 32 workers; XML interval reconstruction measured peak overlap of 26--29 tasks,
because many sub-second tasks completed while later tasks were still being prepared.

Ratios are after/before geometric means of each model-task's three-run median.

| Metric | SC | TSO | PSO | Overall |
|---|---:|---:|---:|---:|
| Wall ratio | 1.0485 | 1.1427 | 1.1428 | 1.1104 |
| Wall 95% CI | [1.0304, 1.0680] | [1.1177, 1.1697] | [1.0960, 1.1917] | [1.0909, 1.1305] |
| CPU ratio | 1.0586 | 1.0561 | 1.2611 | 1.1213 |
| RSS ratio | 0.9958 | 0.9747 | 0.9729 | 0.9811 |

Completed cells fell from 774 to 771. Status transitions included two `true -> TIMEOUT`
and one `true -> TIMEOUT (true)`. The RSS reduction is not useful because it came with a
large time regression and fewer completions.

The second prototype reduced the damage but remained slower in the one-repetition
screen: overall wall ratio 1.0449, TSO 1.0514, and PSO 1.0739. It was therefore stopped
before a costly formal 48-worker replication.

## Root cause

The recursive SC/TSO/PSO models mainly check `irreflexive reach`. The legacy operation
is already one diagonal scan. Scanning the added diagonal costs the same asymptotic work
and adds frontier bookkeeping. For `acyclic coherence`, repeating reachability from
each new edge is strictly worse than one DFS on dense deltas. A dynamic topological-order
structure would be a different, substantially larger algorithm and is not justified by
the measured share of this check.

## Artifacts

- Raw formal XML/logs: `server/wide-before/`, `server/wide-after/`
- Raw oracle XML/logs: `server/oracle/`
- Parsed rows and bootstrap result: `analysis/`
- Configured and measured concurrency: `analysis/concurrency.json`
- HTML: `html/violation-frontier-v1.table.html`

The production worktree retains no violation-frontier code or experiment-only CLI
switch. The negative result remains available for future design decisions.
