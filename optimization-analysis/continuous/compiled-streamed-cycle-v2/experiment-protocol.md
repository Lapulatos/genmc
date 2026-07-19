# P0.7g2 minimal ordered stream program protocol

Date frozen: 2026-07-17  
Baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe`

## Question and independence from P0.7g

Can the generic ordered-stream compiler retain P0.7g's exact CPU mechanism without
enlarging lifetime statistics/result/evaluator objects or adding a second per-check
analysis vector?

P0.7g remains rejected under its original per-observation RSS gate. P0.7g2 is a new
candidate with a different representation and a repetition-rich memory protocol frozen
before implementation. It does not reinterpret or erase the earlier result.

This optimization must not inspect model names, host profiles, task paths, expected
verdicts, built-in checker results or program-family identities. Optimization 6.1 is
outside this experiment.

## Representation

Replace the existing
`vector<optional<PredicateId>> lazyCycleRoots` with an equal-count
`vector<optional<LazyCyclePlan>> lazyCyclePlans`. Each admitted plan owns:

- the exact root predicate ID; and
- zero or more immutable ordered stream terms.

An empty term list invokes the retained generic interpreter. A non-empty list is built
only if compilation removes at least one union or atomic-intersection dispatch. This
replacement keeps the number of vector members in `ModelAnalysis` unchanged. It adds no
field to `FixedPointStatistics`, `CaatEvaluationResult`, `IncrementalStatistics`,
`IncrementalCaatEvaluator` or `LazyCycleStatistics`, and adds no CAT stats output.

The server layout probe must report the retained baseline sizes for all of those public
types. Failure rejects the candidate before performance measurement.

## Compiled semantics

For each admitted lazy-cycle root:

1. resolve aliases and flatten top-level unions left-to-right;
2. compile a base relation into its original packed-successor loop;
3. compile an atomic intersection only when the retained deterministic choice resolves
   its candidate to a base relation and its filter to a base relation or identity;
4. compile identity into the same set-membership guard and same-event callback;
5. encode every other term as `Generic(predicate)` and call the retained interpreter.

The resulting callback sequence must preserve targets, duplicates and order exactly.
The event DFS, three-colour state, parent state, 2,048-depth fallback, witness projection,
base values, incremental updates, checkpoints and rollback are unchanged.

## Expected time and space effects

- **Time reduction:** eliminate repeated union recursion, predicate-kind switches and
  atomic-intersection type-erased callback layers while preserving candidate count.
- **Immutable analysis cost:** one `O(K)` traversal and one small term allocation for
  each admitted check, paid once and shared by workers.
- **Per-check metadata:** `optional<LazyCyclePlan>` is larger than
  `optional<PredicateId>`, but it replaces rather than accompanies the old vector. The
  exact bytes are bounded by the number of model checks and terms, not graph events.
- **Query memory:** colour/parent/fallback allocations are unchanged. No `Q*N` product,
  successor cache or materialized derived relation is introduced.
- **Persistent/rollback memory:** plans are immutable model metadata and never enter
  snapshots or undo trails. Lifetime result/statistics/evaluator type sizes must remain
  exact.

## Correctness gates

1. A test-only detail oracle enumerates all successors for every source and compares
   generic versus compiled callback vectors exactly, including duplicates and order.
2. Randomized and crafted tests cover nested unions, base/base and base/identity
   intersections, identity, aliases, composition fallback, self-loops and repeated
   evaluation.
3. Full GCC 13 Release unit/property suite passes.
4. Focused GCC 13 ASan+UBSan suite passes.
5. Online mutation oracle completes 39 rows and at least 5,441 full recomputations with
   zero mismatch.
6. Broad differential completes 288 programs x SC/TSO/PSO with zero mismatch; mutual
   unsupported cases remain separately classified.

Any callback-vector, status, verdict, complete-execution, witness, mutation-oracle or
rollback mismatch rejects the candidate regardless of performance.

## Layout gate

Compile the same `layout_probe.cpp` against fresh baseline and candidate sources in
`genmc15noble:sujie`. Candidate values must equal the retained baseline:

| Type | Required bytes |
|---|---:|
| `ModelAnalysis` | 176 |
| `FixedPointStatistics` | 48 |
| `CaatEvaluationResult` | 144 |
| `IncrementalStatistics` | 184 |
| `IncrementalCaatEvaluator` | 496 |
| `LazyCycleStatistics` | 24 |

## Pilot gate

Run six balanced repetitions of the established SC/TSO/PSO fib/queue pilot. Alternate
before/after core ranges and launch order. Advance only if all conditions hold:

- zero status/category/terminal-verdict/complete-execution mismatch and no coverage loss;
- exact lazy-check and emitted-candidate counts for every paired terminal cell;
- the compiled plan is non-empty for affected TSO/PSO representatives and absent or
  unused where no lazy plan is admitted;
- every task's paired CPU median ratio is at most 1.03;
- at least one representative above one baseline CPU second has CPU median at most 0.98;
- every task's paired RSS median ratio is at most 1.02;
- for every task, maximum candidate RSS is at most 1.02 times maximum baseline RSS;
- snapshot-equivalent and maximum-current-base bytes are exact.

The median and maximum rules are frozen to distinguish paired timing noise from an
actual capacity regression; they do not retroactively change P0.7g's rejection.

## Formal gate

Only after the pilot advances, run the established four-repetition 2,304-cell matrix.
Retain only if:

- zero status, terminal-verdict and complete-execution mismatch, no coverage loss and
  no new OOM;
- aggregate four-repetition model-task CPU geometric-mean bootstrap 95% CI upper bound
  is below 1.0;
- SC, TSO and PSO point ratios are each at most 1.01;
- process-RSS P90 ratio, large-task RSS and snapshot-equivalent state are each at most
  1.02;
- exact candidate counts and source-independent plan census prove mechanism coverage.

Otherwise archive the exact diff and XML/log evidence, restore `d963c49`, and do not
commit production code.

Protocol clarification before formal launch: a statistical smoke against the archived
P0.7f matrix showed that the established strict analysis unit is one model/task with
all four paired terminal repetitions, using the ratio of after/before medians. The
20,000-sample fixed-seed bootstrap resamples those model/task ratios. This exactly
reproduces P0.7f's 0.999616 point estimate and SC/TSO/PSO ratios; it is used here instead
of treating individual repetitions as independent samples. The upper-below-1 threshold
is unchanged.
