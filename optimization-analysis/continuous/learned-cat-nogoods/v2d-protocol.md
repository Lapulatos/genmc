# P0-A V2d: explicit selective literal probes

## Change

At learning time, order each clause's probes by:

1. ascending cardinality of the literal's current base predicate;
2. descending maximum stable endpoint ID;
3. compiled predicate/type/endpoints as deterministic tie breakers.

Subsumption and duplicate detection become set-based and independent of probe order.
V2c's compiled integer representation and V2a's 16-attempt bound remain unchanged.

## Expected effects

- Rare predicates and later events are more likely to be absent after a graph change,
  so a non-matching clause should fail in fewer literal checks.
- Hit clauses still inspect every literal; no acceptance condition changes.
- Cardinalities are computed only for at most 16 explanation attempts, not per query.
- Set-based subsumption is O(clauses * literals squared) only in the bounded learning
  path; the query hot path remains a linear early-exit scan.

## Generality and correctness

The heuristic uses only generic primitive cardinality and stable IDs. It contains no
predicate-name special case, model-name check, WMM recognizer, or host-checker verdict.
Changing conjunction order is semantics-preserving; set-based subset checks preserve
V1's duplicate/superset behavior.

## Gates

Repeat all correctness and identical pilot gates. In addition to CPU/status, require
PSO queue and TSO/PSO butterfly literal checks to fall relative to V2c. Advance only if
the all-model CPU ratio is at most 1.01 and no >1-second task regresses by more than 20%
without a status improvement.
