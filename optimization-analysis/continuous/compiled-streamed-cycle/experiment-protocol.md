# P0.7g ordered compiled stream program protocol

Date frozen: 2026-07-17  
Baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe`

## Question

Can an analyzer-certified lazy CAT relation preserve the retained P0.7d2 event-level
DFS and exact candidate stream while removing repeated union recursion, predicate-kind
dispatch and atomic-intersection callback layers?

This is a generic CAT operator optimization. It must not inspect model names, host
profiles, task paths, expected verdicts, built-in checker results or observed benchmark
outcomes.

## Compiled representation

For each admitted lazy-cycle root, immutable analysis constructs one ordered term list:

1. recursively resolve aliases and flatten top-level unions left-to-right;
2. compile a base relation term to a direct packed-successor loop;
3. compile an intersection only when the existing deterministic candidate-selection
   rule chooses a base relation and the filter is a base relation or identity;
4. compile an identity term to a direct same-event guard;
5. encode every other term as `Generic(predicate)` and invoke the retained P0.7d2
   interpreter for exactly that term.

The program is emitted only if it removes at least one union layer or atomic
intersection callback. Unsupported or malformed analysis produces no program and uses
P0.7d2 exactly.

## Semantic contract

Flattening `A | B` into the ordered concatenation of the streams for `A` and `B`
preserves every extensional derivation, duplicate and left-to-right order. A compiled
base term enumerates the same `Relation::nextSuccessor` sequence as the interpreter.
For an admitted atomic intersection, the candidate relation and membership filter are
chosen by the existing rule; applying the filter inline accepts exactly the same
targets in the same order. Identity accepts exactly the current event under the same
set-membership predicate.

Generic terms call the old interpreter. Therefore the complete target callback sequence
must equal the baseline sequence, not merely have the same set or cycle verdict. The
event-level three-colour DFS, 2,048-depth fallback, witnesses, base values, incremental
state, checkpoints and rollback are unchanged.

## Expected time and space effects

- **Immutable analysis:** one `O(K)` traversal of each admitted root cone, where `K` is
  normalized predicate occurrences. This is paid once and shared by workers.
- **Per source event:** replace nested union calls and switches with one contiguous term
  loop. A compiled atomic intersection removes one type-erased callback and one generic
  membership dispatch per candidate.
- **Candidate work:** exact root candidate count must be identical to baseline; this
  optimization does not claim pruning or suffix sharing.
- **Query space:** colour, parent and depth-fallback state are byte-for-byte unchanged.
  No `Q*N` product state, successor cache or query-persistent memo table is added.
- **Persistent/rollback space:** only a small immutable term vector is added to shared
  model analysis. No term state enters snapshots, undo trails or rollback.

The expected benefit is largest when the same small stream plan is evaluated many
times over large base relations. One-time analysis overhead can dominate short tasks,
so end-to-end paired CPU, not internal counters alone, remains the retain authority.

## Correctness gates

1. Unit test exact callback-sequence equality between compiled and generic streams for
   unions, duplicates, base/base and base/identity intersections, self-loops and generic
   composition fallback.
2. Existing materialized-root property tests validate cycle existence and every witness
   edge; repeated evaluation remains deterministic.
3. Full Release unit/property suite.
4. Focused GCC 13 ASan+UBSan suite.
5. Online mutation oracle: 39 rows and at least 5,441 full recomputations.
6. Broad differential: 288 programs x SC/TSO/PSO, zero mismatch; mutual unsupported
   pairs remain separately classified.

Any callback-sequence, verdict, execution-count, witness-validity or rollback mismatch
rejects the candidate regardless of performance.

## Pilot and formal gates

Use the same two-repetition, swapped-core SC/TSO/PSO fib/queue pilot as P0.7f.
Advance only if:

- zero status/category/complete-execution mismatch and no coverage loss;
- compiled-stream checks are nonzero on an affected representative and zero where no
  lazy plan is admitted;
- root candidate counts are exactly equal before/after for every paired cell;
- every task CPU median ratio is at most 1.03 and every RSS ratio is at most 1.02;
- at least one representative above one baseline CPU second has CPU median at most
  0.98 or internal lazy-cycle time at most 0.97.

If the pilot advances, run the established four-repetition 2,304-cell matrix. Retain
only if:

- zero status, terminal-verdict and complete-execution mismatch, no coverage loss and
  no new OOM;
- aggregate task-clustered CPU geometric-mean 95% CI upper bound is below 1.0;
- SC, TSO and PSO point ratios are each at most 1.01;
- large-task process RSS and snapshot-equivalent state are each at most 1.02;
- candidate counts remain exact and counters prove the compiled path was exercised.

Otherwise archive the exact diff and all XML/logs, restore `d963c49`, and do not commit.
