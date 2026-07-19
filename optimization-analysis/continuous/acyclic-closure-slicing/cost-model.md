# Optimization 12 cost model

## Stage-by-stage effect

| Stage | Before | After | Expected effect |
|---|---|---|---|
| Normalization | retain `X = R+`; cycle check reads `X` | prove sole cycle-check use; canonicalize to `acyclic(R)`; remove `X` | one consumer scan and deterministic ID remap |
| Analysis | one closure predicate/dependency/stratum | closure node absent | one fewer predicate and dependency |
| Offline evaluation | packed closure allocation and Warshall-style row unions | no closure evaluation | remove `O(n²·ceil(n/64))` work |
| Online insertion/rebuild | recompute/publish complete `R+` | maintain only `R` and its existing producers | remove closure recomputation and publication |
| Transactional copy/undo | retain/copy closure value | value absent | remove one dense relation from state |
| Check | DFS over often denser `R+` | DFS over seed `R` | usually sparser traversal; same `O(n²)` dense scan bound |
| Explanation | reduce closure self-edge to a seed path | explain the seed cycle directly | avoids closure provenance; witness may contain more vertices |
| Oracle | compare optimized offline/online values | same | independent algebraic property remains required |

## Space prediction

The removed packed relation costs approximately `n²/8` bytes, plus optional value,
undo, reason-table, and container overhead. Relative to Optimization 11, this removes
the remaining named closure value rather than only its generated recursive-composition
intermediate.

## Costs and possible regressions

- Normalization performs a one-time consumer scan and ID remap.
- The seed-cycle witness can be longer than a diagonal edge in `R+`; explanation output
  may contain more vertices, but no extra base literals beyond the actual cycle.
- A dense seed can make the check itself no cheaper. The closure computation and state
  are still removed.
- Rewriting a check while retaining a downstream closure consumer would save almost
  nothing and complicate provenance, so the first version deliberately fails closed.
- Removing a closure must not invalidate exact candidate-pruning certification. Updated
  certificates remain closed-world summaries of the sliced generic IR.

## Falsifiable predictions

- Candidate count, profiled queries, adaptive-offline transitions, terminal verdicts,
  and safe exploration counts remain identical on common completed tasks.
- `offline-ns` and `sync-ns` fall beyond Optimization 11, with the largest effect on SC.
- `peak_snapshot_equivalent_bytes` decreases on every completed task that exercises the
  sliced closure and never increases because of the slice.
- If only total time changes while closure state does not, the hypothesized mechanism
  is not supported and the optimization is rejected.
