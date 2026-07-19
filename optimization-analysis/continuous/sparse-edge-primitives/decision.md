# Optimization 15 / P0.6c decision

## Decision

Retain the combined structural-view and size-adaptive CSR implementation. It passes all
correctness gates, exceeds the frozen total-base and RSS targets, and has no repeatable
whole-process CPU regression.

## Correctness and completeness

- Release unit/property tests: 150/150, including a 513-event graph-snapshot oracle for
  exact `rf/co/fr/rmw/tc/tj` CSR construction.
- Mutation oracle: 39 rows and 5,441 complete Phase-2 recomputations, zero mismatch.
- Broad differential: 288 programs, 864 SC/TSO/PSO pairs, 852 matches, 12 mutually
  unsupported pairs, and zero mismatch.
- Formal matrix: 1,152 before plus 1,152 after cells; all 24 run sets exited zero.
- Zero opposite common-terminal verdict and zero common-terminal execution-count mismatch.

## Formal performance

- Common-terminal CPU after/before: 1.00209, task-bootstrap 95% CI
  [0.99644, 1.00783].
- Common-terminal wall time: 0.98544 [0.95977, 1.01173].
- Completed tasks with at least 512 stable events:
  - total current/history base bytes: 0.14139 [0.10608, 0.16869];
  - process RSS: 0.61512 [0.53543, 0.70107];
  - materialization time: 0.79688 [0.70568, 0.89969].
- The five large model-task pairs all reduce total base below 0.175 of baseline; the
  PSO 8,033-event case reaches 0.0806.

## Status interpretation

Correct true/false counts are unchanged (712/340). Twenty cells change from OOM to
TIMEOUT, so OOM falls 60 to 40 while TIMEOUT rises 40 to 60. This proves that memory
pressure was removed for those runs, but the remaining exploration/evaluation work did
not finish within 60 seconds. It is not counted as a new solved task.

## Mechanism

- `po/int/ext/loc` use exact O(N) metadata above 512 events.
- `rf/co/fr/rmw/tc/tj` use exact CSR only when owned CSR bytes are at most half the dense
  packed matrix; dense coherence automatically stays dense when its edge list is larger.
- Dense construction, closure, composition, inverse, and range retain representation-
  specialized fast paths. Sparse composition iterates actual edges instead of N²
  membership queries.
- `fr` is built directly from the exact equation `rf^-1 ; co`; CAT/CAAT remains the sole
  consistency and verdict authority.

## Evidence

- Analysis: `analysis/comparison.json`, `analysis/rows.tsv`
- Full XML/log archives: `server-results/formal-sparse-v2/`
- Unit/mutation/broad: `server-results/unit-sparse-v2-final.log`,
  `server-results/mutation-sparse-v2.log`, `server-results/broad-sparse-v2/`
- Pilot diagnosis: `server-results/sparse-v1-smoke-*.log`,
  `server-results/sparse-v2-smoke-*.log`
