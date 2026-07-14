# Phase 3 research question: incremental and online CAAT in GenMC

## Question

Can GenMC maintain the CAAT least fixed point while an execution graph grows and
backtracks, and use an already detected violation for sound early pruning,
without changing the executions accepted by the Phase 2 offline evaluator?

## Hypothesis

For the positive, stratified and domain-independent fragment already accepted
by GenMC's online CAT path, base-predicate insertions can be propagated through
the normalized dependency graph as deltas. Every derived predicate grows
monotonically, so `empty`, `irreflexive`, and `acyclic` violations persist under
further insertions. A timestamped undo trail can restore an earlier exact state
for depth-first backtracking. Graph changes that are not a supported insertion
or exact rollback can safely fall back to Phase 2 recomputation.

## Current evidence

- CAAT Section 4.1 states that an old least fixed point is a valid seed after
  base insertions and that the resulting iteration reaches the same new least
  fixed point. It explicitly leaves general deletion and non-monotonic
  difference as open problems.
- CAAT's delta worklist construction gives operator-local propagation rules and
  proves equality with the corresponding Kleene approximants.
- Current Dat3M contains a predicate hierarchy with insertion propagation,
  timestamped derivables, `backtrackTo(time)`, and constraint listeners. Its
  difference implementation rejects a dynamic right operand and documents that
  difference in recursion is unsupported.
- Kater's GenMC integration incrementally checks a newly added event and relies
  on the fact that a new cycle in a previously consistent graph contains the
  new event. It also restricts cached relations whose edges may later disappear.
- Incremental view-maintenance systems such as DRed, Differential Dataflow and
  DBSP show broader insertion/deletion techniques, but importing their runtime
  and multiset semantics would be disproportionate for GenMC's packed finite
  relations. They remain future deletion-strategy references.

## Missing evidence to produce

1. Operator-by-operator property tests showing incremental values equal
   from-scratch Phase 2 values after every insertion.
2. Exact rollback tests, including recursive derivations with different
   insertion times.
3. End-to-end evidence that GenMC graph synchronization classifies insertion,
   rollback, and fallback correctly across label addition, `rf`/`co` changes,
   graph cuts, and revisits.
4. Broad program/model differential results with identical verdict, complete
   execution count, error class, and unsupported count.
5. Measurements proving the online path is actually exercised and avoids a
   material number of full recomputations.

## Support criteria

The hypothesis is supported only if all incremental checkpoints equal the
offline oracle, all end-to-end differential rows match, rollback/fallback tests
cover every declared mutation class, and runtime counters show delta updates on
real concurrent programs.

## Falsification criteria

Any incremental state that differs from a fresh Phase 2 evaluation, any early
prune that removes an execution accepted by the offline path, or any graph
mutation that can be mistaken for a pure insertion falsifies the corresponding
implementation contract. The affected case must be fixed or routed to the
offline fallback; it cannot be accepted as an optimization trade-off.

## Minimal next action

Implement a standalone incremental evaluator with explicit `initialize`,
`push`, `rollback`, and `snapshot` operations, then test it against
`CaatEvaluator` before connecting it to `CATChecker`.
