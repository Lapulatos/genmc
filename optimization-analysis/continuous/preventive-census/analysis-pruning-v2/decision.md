# P0.3 preventive-order pruning decision

## Decision

Retain the implementation as an **opt-in exact-recursive-PSO coverage optimization**,
but do not enable it by default and do not claim a statistically established universal
speedup. The original solved-task CPU gate rejects that stronger claim: CPU
pruning/baseline is 0.95369 with task-bootstrap 95% CI [0.87527, 1.04274].

The separate large-program objective has repeatable evidence. Across four paired
96-task repetitions, five tasks changed from TIMEOUT to a terminal result in every
repetition (three TRUE and two FALSE), while OOM stayed 20 versus 20. There are zero
safe execution-count mismatches. RSS is 1.00004 [0.99958, 1.00042].

## Mechanism evidence

On the 82 tasks correct in all eight cells, the pruning path changes evaluator work as
follows:

| Counter | Baseline | Pruning | Change |
|---|---:|---:|---:|
| Profiled consistency queries | 2,618,116 | 1,282,004 | -51.0% |
| Offline evaluations | 2,430,212 | 1,094,076 | -55.0% |
| Synchronization time | 493.04 s | 273.68 s | -44.5% |
| Offline evaluation time | 339.42 s | 105.52 s | -68.9% |
| Preventive lookup time | 0 | 1.69 s | added |

Across all normally terminated pruning cells, 14,187,144 of 18,057,904 offered RF/CO
candidates were removed (78.56%), with zero all-pruned fallbacks. Thus the main saving
comes from avoiding candidate graph synchronization and fixed-point evaluation, not
from making one consistency check marginally faster.

## Space boundary and hard cohort

The first prototype eagerly synchronized single-candidate prefixes. One deterministic
20,000-event task then changed from a 2.96-second FALSE to 4 GB OOM because the recursive
`reach` closure was materialized before the property error. Skipping zero/one-candidate
choice points is exact: the all-pruned safety fallback could not shrink such a choice
set. After this gate, the same pruning run returns FALSE in 3.40 seconds at 4 GB.

On two repetitions of the independently rewritten 60-task adapted hard cohort, pruning
changed the same three tasks from OOM to TIMEOUT, with no terminal-result gain: baseline
was 31 OOM / 11 TIMEOUT / 18 compilation errors, and pruning was 28 OOM / 14 TIMEOUT /
18 compilation errors. This suggests lower memory growth on those three searches, but
the remaining exploration is still too large for 60 seconds.

The summed maximum retained-history base bytes on the 82 common tasks increased from
293.6 MB to 366.8 MB even though process RSS stayed flat. The next implementation should
therefore avoid retaining a GraphSynchronizer checkpoint solely for a preventive query.

## Correctness evidence

- Release unit tests: 145/145.
- Mutation stress: 39 rows and 5,658 offline-oracle checks after single-candidate gating.
- Broad differential: 288 programs, 864 pairs, 852 comparable matches, 12 mutual
  unsupported, zero status/signature/execution-count mismatch.
- Model boundary: only the exact normalized recursive-PSO fingerprint is accepted;
  recursive SC and non-recursive PSO fail closed.
- No built-in SC/TSO checker result is used for consistency or candidate pruning.

## Next optimization

Replace dense preventive `reach` materialization with a sparse exact event-order graph.
Maintain only direct `ppo/lifecycle/rfe/fr/co` adjacency plus an incremental topological
order; answer the required reverse-path query inside the affected topological interval.
Expected space falls from dense O(V²) closure storage to O(V+E) graph state, and a
candidate pays O(affected subgraph) rather than a full fixed-point synchronization.
The generic CAAT evaluator remains the oracle and fallback.
