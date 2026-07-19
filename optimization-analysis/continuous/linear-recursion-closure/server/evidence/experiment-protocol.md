# Optimization 11: linear recursion to closure canonicalization

## Hypothesis

For a finite relation, the least fixed point of `X = R | (X ; R)` and of
`X = R | (R ; X)` is exactly the non-reflexive transitive closure `R+`. Recognizing
that syntax structurally during generic CAAT normalization replaces repeated full
composition/union fixed-point rounds with the existing packed closure operator while
preserving the complete value of `X` for checks, oracle comparison, and explanation.

## Method boundary

- Match expression structure and recursive binding identity only; never inspect model
  name, host profile, filename, or select a GenMC built-in checker.
- Require the seed expression to be recursion-free and byte-structurally identical in
  the base and composition positions.
- Accept left/right composition and union order; all near-neighbor equations fail closed
  to the original recursive SCC.
- Preserve the named predicate, source span, complete closure value, and provenance.

## Correctness gates

1. Property comparison against an independent naive Kleene fixed point across random
   finite relations and all admitted syntactic orientations.
2. Near-neighbor normalization tests for unequal seed, wrong operator, extra term, and
   mutual recursion.
3. Explanation replay and exact bundled certificate regression.
4. Server Docker full unit/property suite, 39-row mutation oracle, and frozen 864-pair
   SC/TSO/PSO differential.

## Performance gate

Fresh simultaneous before/after matrix: 96 tasks × SC/TSO/PSO × six repetitions × two
variants = 3,456 cells, alternating disjoint 24-core groups. Keep only with zero verdict
and safe exploration-count mismatch, aggregate CPU task-bootstrap 95% CI below 1, and
no model point regression above 0.5%. Report wall, CPU, RSS, solved cells, fixed-point
operation/value-change counts, and measured overlap.

The pre-implementation cost propagation and falsifiable metric predictions are recorded
in `cost-model.md`; implementation must not begin until that map identifies both reduced
and potentially increased costs.
