# P0.7b1 exact lazy cycle-EOG protocol

Date frozen: 2026-07-16
Baseline: `35a595ab5a706c7fbae52a0f4ccc6d29655fcdd8`

## Question

Can GenMC evaluate an exclusive positive CAT `acyclic` cone without materializing its
union/composition intermediates, while preserving the exact finite relation semantics
and reducing large-graph time or memory?

## Activation contract

The compiler admits one normalized check only when all of the following hold:

1. the check kind is `acyclic` and its predicate has relation type;
2. every derived predicate in the backwards cone is non-recursive and uses only the
   explicitly supported exact operations: alias, union, composition, identity,
   optional, and an intersection with an exact membership-filter operand;
3. every derived cone predicate is consumed only inside this plan and by this check;
4. base predicates are never elided;
5. all unsupported operators, shared consumers, other checks, or malformed types reject
   the whole plan and retain the generic evaluator.

The first implementation is disabled for `--explain-cat`, `--cat-oracle`, and
`--cat-preventive-pruning`. These modes require published predicate values or
reachability state; disabling the plan is the exact fail-closed behaviour.

## Exact execution contract

For each source event, recursively enumerate the extensional successors of the root CAT
expression. Composition enumerates `lhs(source,middle)` followed by
`rhs(middle,target)`; union enumerates both operands; identity and optional preserve CAT
semantics; admitted intersections enumerate one side and test exact membership in the
other. Per-source generation marks remove duplicates, and ascending target order keeps
cycle witnesses deterministic. A three-colour DFS reports a violation exactly iff the
materialized root relation is cyclic.

The generic CaatEvaluator remains the oracle. This optimization does not call SC/TSO
host checker consistency code and does not change RF/CO candidate enumeration.

## Expected time and space effects

- Avoid one dense allocation/write for every elided union/composition predicate.
- Avoid storing and transactionally copying those values in incremental updates.
- Avoid their undo deltas and snapshot-equivalent checkpoint bytes.
- Retain `O(N)` colour/parent/dedup state plus the deduplicated successor rows held by
  the active recursive DFS path (`O(E_path)` worst case). This is transient and avoids
  one persistent relation per cone predicate, but it is not claimed to be strict
  `O(N)` auxiliary space; BenchExec RSS remains the authority.
- Risk: a composition can enumerate the same target through multiple middle events;
  generation marks bound stored targets but not enumeration work. CPU is therefore a
  measured gate, not an assumed improvement.
- Base materialization, atomicity/non-admitted checks, and GenMC exploration state are
  unchanged.

## Correctness gates before performance

1. independent randomized lazy-vs-materialized cycle equivalence for every admitted
   operator shape, including duplicates and self-loops;
2. near-neighbour tests proving fail-closed behaviour for shared consumers, recursive
   cones, unsupported operations, explanation/oracle/preventive selection;
3. complete Release unit/property suite;
4. 39-row mutation oracle and 288-program x SC/TSO/PSO broad differential, with zero
   opposite verdict and zero common-safe execution-count mismatch.

Any correctness mismatch removes the candidate before performance testing.

## Performance matrix and retain gate

Use the established balanced matrix: 96 tasks, SC/TSO/PSO, before/after, four
repetitions, odd/even core placement swapped, 48 BenchExec task workers, one core,
4 GiB and 60 seconds per cell. Preserve complete XML and compressed logs.

Retain only if all are true:

- no opposite terminal verdict, no common-safe execution-count mismatch, and no new OOM;
- aggregate common-terminal CPU bootstrap 95% CI upper bound <= 1.02;
- each of TSO and PSO CPU point ratios <= 1.03;
- and at least one predeclared benefit holds: large-event peak snapshot-equivalent bytes
  and RSS both fall by >= 15%, or at least four baseline TIMEOUT/OOM cells become the
  same correct terminal result in every repetition.

Otherwise restore all production/test changes to the baseline and retain only the
experiment record.

## Frozen P0.7b2 composition-cost selector

The first complete matrix is diagnostic input for one predeclared refinement, not the
final retained measurement. It shows exact opposite model behaviour: SC CPU is 1.04231
[1.02308, 1.06364] and large SC RSS is 1.25667, while TSO/PSO CPU is 0.96124/0.93532
and their composition-heavy large graphs obtain the large state reductions.

The follow-up selector is structural and deterministic: after compiling exact plans,
enable them only if at least one admitted cone contains a `Composition` predicate. If
no admitted cone contains composition, clear every plan and publish every generic
value. This rejects the simple union-only recursive SC model without testing model name,
host profile, event count, runtime verdict, or built-in checker output. TSO/PSO retain
their plans because their preserved-order/lifecycle equations contain compositions.

Expected effects:

- SC returns byte-for-byte to generic fixed-point evaluation and avoids the measured
  lazy enumeration/RSS regression;
- TSO/PSO retain avoided composition allocations, copies and checkpoints;
- semantic soundness/completeness are unchanged because both selected paths are exact.

The selector must repeat all correctness gates and the full 2,304-cell matrix. The same
original retention thresholds apply; the first matrix is not substituted for the final
candidate's evidence.
