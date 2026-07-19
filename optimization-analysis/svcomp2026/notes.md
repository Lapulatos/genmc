# Notes: SV-COMP 2026 C.Concurrency

## Sources

- Official verified results: C.Concurrency has 3124 valid tasks (2520 true,
  604 false, 53 void).
- Official rules define no-data-race as a separate property; it is not identical
  to CAT memory-model consistency.

## Current Local Evidence

- Final SC/TSO suite: CAAT 1.039x GenMC and 0.898x CAT.
- PSO suite: CAAT 1.124x CAT because certified pruning/adaptive offline do not apply.
- SC Flat Combiner after pruning: 43 queries, 12 rebuilds, 421 operator evaluations.
- Stage 8 generic COW reduced transactional copy but regressed small-suite time.
- Stage 9 object-level operand deltas covered most operations but allocation/merge overhead regressed small-suite time.
- Delta trail reduced evaluator restoration packed storage by about 49.7%, while increasing rollback time.

## Local Audit

- `Relation` uses `N * ceil(N/64)` dense words.
- Stable materialization uses persistent `keys_.size()` and rebuilds required
  primitives; pair primitives contain nested active-event loops.
- History entries copy full `BaseValues`; subset history search scans relation cells.
- `tryInsert()` copies/grows all values and recomputes affected operators as full values.
- `tryReplace()` is restricted to acyclic Base/Alias/Union models.
- Adaptive offline uses a fixed 512-event threshold only for certified candidates.

Deliverable: `optimization-analysis.md`.
