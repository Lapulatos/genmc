# GenMC-guided lazy CDCL(T): algorithm and cost plan

## Status and implementation hold

This document is the pre-implementation design gate requested on 2026-07-18.  No active
CDCL filtering, choice ownership, worklist change, or backjump implementation may start
until this flow, its completeness split, and its cost gates are accepted.  An initial
event-stamp-based census prototype was withdrawn before any accepted experiment: GenMC
event stamps are insertion identities, not RF/CO decision levels, and revisits can change
an old label's RF without changing its stamp.

## Objective

Combine the useful parts of the two existing engines without duplicating their state:

- retain GenMC's actual LLVM execution, DPOR schedule exploration, compact revisit
  worklist, and one current `ExecutionGraph` per retained execution;
- retain CAAT's exact CAT relation semantics and current graph synchronizer;
- use CDCL for repeated Boolean RF/CO combinations, theory propagation, clause learning,
  and pruning of incompatible queued descendants;
- never construct the eager whole-program event/RF/CO/rank formula used by the rejected
  finite symbolic lane.

## Explicit non-goals for the first implementation

- Z3 does not own scheduling; GenMC/DPOR remains complete for schedules.
- Z3 does not receive an `n x n` top-reach matrix, event ranks, or all-different order.
- The finite event skeleton is not the production event graph.
- A single candidate cycle is not promoted to an RF-only or prefix conflict.
- Generic non-monotone CAT (`difference` on the checked cone) is fail-open scope.
- Clauses are not shared across workers in the first version.

## State ownership

Each verification worker owns exactly:

```text
GenMC Execution stack
  - real ExecutionGraph(s)
  - existing revisit WorkList(s)
  - DecisionState matching each real graph
  - immutable DecisionSnapshot id per queued Revisit

CAAT theory
  - existing StableGraphAdapter / GraphSynchronizer
  - existing IncrementalCaatEvaluator checkpoints
  - analyzer-certified checked roots and inclusion shortcuts

CDCL engine
  - one persistent solver per worker
  - lazy RF/CO variables
  - prefix activation variables
  - learned guarded clauses
  - explicit unexplored RF/CO frontier for every delegated scope
```

No work item owns a solver snapshot.  A work item references an immutable, structurally
shared `DecisionSnapshot` containing the exact RF/CO assumptions of the prefix when the
item was created plus its proposed alternative.  The current `Execution` owns a mutable
`DecisionState` whose entries are keyed by the real choice subject, not by event stamp:

```text
ChoiceKey = (RF | CO, subject Event)
DecisionEntry = (chosen alternative, decision ordinal, activation-scope id)
```

The state is copied/filtered with the same `VectorClock` as `ExecutionGraph::getCopyUpTo()`
for a backward revisit.  It is filtered against the surviving real labels after a forward
`cutToStamp()`.  A clone/reset of graph stamps does not change a decision identity.  A
revisit replaces the entry for its subject and assigns a fresh branch-local ordinal; it
does not append a second active decision for the same read/write.

This is deliberately not a linear SAT trail.  A backward revisit may select a vector-clock
prefix with holes, so its active choices are a filtered persistent map, not necessarily an
ancestor chain of the current chronological history.

## Literal model

### Choice literals

```text
RF(read Event, source Event or Init(address))
CO(write Event, predecessor Event or Init(address))
```

These are created only when the corresponding real GenMC choice becomes available.  RF
choices for a read form exactly-one under that read's activation.  CO predecessor choices
for one insertion form exactly-one under that write's activation.  A future write that
creates a backward revisit registers a new RF alternative lazily; it does not require a
whole-program RF matrix.

### Activation literals

An `Event(thread,index)` identity alone is not a sufficient global scope: different
control/value prefixes can expose the same position.  Each clause is guarded by the
smallest currently proven activation scope:

```text
¬PrefixActivation(scope) OR ¬choice_1 OR ... OR ¬choice_k
```

The first implementation uses an exact structural prefix-scope ID and reuses a clause only
when the current snapshot proves that scope active.  It must not infer ancestry merely
from chronological creation order.  Cross-prefix global reuse is disabled until
control/value guards are exact Boolean literals and an independent oracle validates the
translation.

### Facts that are not choice literals

`po`, `tc`, `tj`, address/category membership, and unsupported negative facts remain part
of the prefix activation guard.  They must not be silently dropped from a globally reused
clause.  An explanation with an untranslatable fact may reject the current concrete
candidate but cannot be learned by CDCL.

## Algorithm flow

### A. Starting or restoring one GenMC execution

1. Restore/copy the ordinary GenMC graph exactly as today.
2. Synchronize CAAT through `GraphSynchronizer`; classify insert, rollback-insert,
   replacement, or rebuild.
3. Reconstruct solver assumptions from the execution's `DecisionState` and prefix
   activation.  Assert in observation/shadow builds that these RF/CO choices exactly match
   the real graph.
4. If assumptions are already UNSAT from retained clauses, discard this queued execution
   without interpreting further instructions.  The clause proof, not solver state, is the
   authority for this deletion.

### B. RF choice point

1. `BasicCATChecker::getCoherentStores()` obtains the complete native candidate vector.
2. Existing analyzer-certified preventive checking removes candidates that individually
   close a checked positive order.  This remains the cheapest first gate.
3. Register/retrieve one lazy RF literal per surviving source.
4. Query the solver under the current decision snapshot plus each proposed literal:
   - UNSAT: omit this candidate only if the conflicting clause is valid under the current
     activation scope;
   - SAT/unknown/unsupported: retain the candidate.
5. If all candidates are omitted, return an explicit `NoConsistentChoice` result to the
   driver; do not use the current safety fallback that re-adds one candidate.
6. GenMC installs one retained RF choice by replacing that read's `DecisionState` entry.
   Other retained alternatives are enqueued with an immutable snapshot of the exact base
   decision set and the proposed replacement, preserving normal DFS order initially.

### C. CO placement point

Use the same flow for `getCoherentPlacings()`.  Successful RMW adjacency remains a native
single choice and is not encoded as an alternative.

### D. Complete CAAT consistency query after installation

1. Synchronize the real graph; run the exact complete CAAT checks.
2. If consistent, continue the real execution.
3. If inconsistent, obtain a checked-root cycle/path explanation:
   - prefer the sparse lazy checked-root witness and `deriveLazyEdge()`;
   - do not use the quadratic offline `Reasoner` on the hot path;
   - translate RF/CO/FR base facts to active choice literals;
   - retain all other dependencies in the prefix activation scope.
4. Add the guarded negated conjunction as a learned clause.
5. Reject the current execution exactly as GenMC already does.

### E. Backjump semantics in a GenMC worklist

GenMC's current worklist is a stack of independent `Revisit` objects, not a SAT decision
stack.  Directly calling solver `pop()` cannot backjump GenMC.  Backjump is implemented by
snapshot-aware queue invalidation:

1. A learned clause identifies its highest and second-highest decision ordinals within
   the current `DecisionState`.  These ordinals affect priority only; clause truth under an
   exact snapshot is the sole authority for pruning.
2. The current execution returns to ordinary GenMC control.
3. Before a queued revisit is destructively restored, test its immutable base snapshot
   plus proposed replacement.  After native restrict/copy/revisit, rebuild the current
   `DecisionState` and assert that it equals the predicted snapshot; mismatch disables the
   optimization and retains native exploration.
4. All queued descendants made UNSAT by the clause are discarded together.
5. The next compatible work item is the effective non-chronological backjump target.

This preserves the existing execution-stack structure.  It also makes the benefit
measurable as `queued-items-rejected-before-restore`, not as an abstract solver backjump
counter.

### F. Final result and fallback

- Every retained complete execution still passes the unchanged full CAAT evaluator.
- Error reporting uses the actual current GenMC execution; no separate replay is needed.
- Solver unavailable, incomplete translation, unsupported CAT, or replacement outside the
  exact scope simply receives no solver-derived rejection; GenMC retains and explores that
  native candidate/work item.
- Search ownership is partitioned, not advisory: GenMC owns control flow, scheduling,
  dynamic event discovery, and revisit materialization; the solver owns exhaustive RF/CO
  feasibility search for every scope explicitly delegated to it.  Joint completeness
  requires every feasible delegated assignment to be either the current execution, a
  queued GenMC revisit, or an explicit member of the solver's unexplored frontier.
- Learned clauses are redundant consequences of immutable base constraints and exact CAAT
  theory lemmas.  Standard CDCL database reduction may delete an unlocked learned clause
  without losing completeness; base constraints, active reasons, and the unexplored
  frontier may not be deleted.
- If the solver can no longer represent or enumerate its delegated frontier, verification
  is resource-exhausted/inconclusive.  It may not silently continue to a TRUE verdict using
  only the remaining GenMC queue.  An exact native fallback would require first exporting
  every remaining delegated assignment as GenMC work or restarting that scope from a
  completeness checkpoint; that mechanism is explicitly deferred from the first version.

## Bottom/intermediate/top relation checks

Checks are ordered by expected cost:

1. direct self/reverse edge in an analyzer-certified subrelation;
2. sparse reachability in a certified bottom/intermediate relation included in a checked
   positive root;
3. focus reach in the certified checked order;
4. complete top-level CAAT fixed point and axiom witness.

A cycle in an arbitrary operand is not sufficient.  The analyzer must prove that every
edge in that relation is included in the checked root.  Intersection, difference, and an
operand of composition do not receive this certificate merely because they feed the root.

## Candidate linear-memory, TruSt-preserving pruning mechanism

The first measured hypothesis is **extension-closed prefix nogoods**.  It is not yet the
selected core optimization: archived V9/V10 data suggests directly cyclic fixed prefixes
may be common as candidate-local failures but rare as whole-subtree certificates.  If a sparse
CAAT proof shows that a set of already-fixed, stable RF/CO choices derives a cycle in a
prefix-monotone checked root, then every later execution containing those choices remains
inconsistent:

```text
RF(r1,w2) AND CO(w3,w4) AND RF(r5,w6)  =>  checked-root cycle

learn:
NOT RF(r1,w2) OR NOT CO(w3,w4) OR NOT RF(r5,w6)
```

Unresolved future choices do not appear in this proof.  The analyzer certificate and proof
DAG establish that graph extension can only add derivations to the checked root and cannot
remove the witnessed cycle.  This makes the nogood a no-consistent-extension certificate
for every native TruSt work item whose exact snapshot entails all of its literals.

The pruning order is:

1. native TruSt enumerates the complete candidate/revisit set;
2. V9-style singleton preventive reach rejects a choice that closes a certified cycle;
3. watched propagation over retained prefix nogoods rejects/unit-forces combinations of
   several earlier RF/CO choices;
4. sparse bottom/intermediate/top CAAT checking derives a new proof when propagation does
   not decide the candidate;
5. minimize the proof to stable RF/CO causes plus an exact monotone activation scope;
6. reject the current candidate or native queued revisit only when its snapshot entails
   that complete nogood;
7. leave native revisit construction, maximality, and ordering unchanged for everything
   else;
8. retain the complete CAAT evaluator as the final oracle.

This generalizes preventive pruning without changing the DPOR equivalence relation.  It can
skip a whole family of inconsistent suffixes sharing a mixed RF/CO cause, whereas V9 sees
only a candidate-local reversal.  It does not merge two consistent execution classes.

The stronger CDCL opportunity is **resolution-derived prefix UNSAT**, not merely reusing a
cycle already present in the fixed prefix.  For example, if every currently possible RF
alternative under an exactly-one domain leads to a certified conflict, resolution can
derive a parent nogood even though the parent itself contains no cycle.  This may prune a
TruSt subtree.  It is sound only with a domain-closure certificate: future writes and
backward revisits must not be able to add another RF/CO alternative to that delegated scope.
Without domain closure, an all-current-siblings conflict is not no-consistent-extension.

The observation gate must therefore distinguish:

1. direct fixed-prefix cycles;
2. recurring candidate-local cycles that save only a repeated check;
3. clauses that become unit before native candidate installation;
4. resolution-derived parent UNSAT with a valid domain-closure certificate;
5. actual native work items covered before graph restore.

If categories 3--5 are negligible, prefix nogoods are rejected as a core direction and
retained at most as the already-measured V9 local gate.  Clause/core hit counts alone do not
justify implementation.

The additional live memory must be bounded by the current TruSt working set, not by total
history:

```text
DecisionState                    O(active events)
one snapshot/root reference      O(1) per native work item
persistent snapshot delta nodes  O(active depth + retained work items)
CAAT proof scratch               O(current sparse proof), released after minimization
learned clauses/literals         O(alpha*n) / O(beta*n) per live execution family
watched lists                    O(retained learned literals)
```

Here `n` is the current/live-family event count and `alpha`,`beta` are measured proportional
cache factors, not history-wide fixed proof limits.  Clauses are redundant and may be
deleted when unlocked.  Scope-local variables, clauses, and snapshot nodes are reference-
counted and reclaimed when no current execution or native work item can activate them.
No path assignment, transitive-closure matrix, rank vector, or complete CAAT proof DAG is
retained after the compact nogood is validated.

Strict linearity still depends on sparse/lazy CAT evaluation: a model whose exact checked
relation itself contains quadratic many live pairs cannot have a general linear-space
materialization.  The extension must not introduce such a matrix; this claim concerns its
incremental state above the existing graph/evaluator boundary.

## Correctness and completeness argument required

### TruSt optimality preservation

TruSt's optimality is defined relative to its DPOR equivalence relation: it explores one
representative interleaving per extendible execution-graph class.  The solver must not
replace that equivalence relation or become an alternative path generator.

The native mechanisms remain authoritative and unchanged:

- `getRevisitView()` selects the graph prefix represented by a backward revisit;
- `constructBackwardRevisit()` creates the native alternative;
- `isMaximalExtension()` and label revisitability enforce TruSt maximality;
- `ChoiceMap` and the native worklist retain the alternative-exploration structure.

Every native TruSt work item is classified exactly once as one of:

```text
EXPLORE: restore it through the unchanged TruSt path;
PROOF_PRUNED: retain a complete checked-root certificate that no CAT-consistent
              extension represented by this work item exists.
```

Solver SAT, a conflict under only the current concrete suffix, a candidate-local cycle, or
a partial/unsupported explanation is not sufficient for `PROOF_PRUNED`.  The certificate
must cover the work item's activation scope and every unresolved RF/CO alternative below
that scope.  It must also carry the analyzer's prefix-monotonicity certificate showing that
later graph extension cannot remove the witnessed violation.  Otherwise the item remains
`EXPLORE`.

Under this rule:

- uniqueness is inherited from TruSt because the extension creates no new revisit or
  representative;
- completeness is preserved because a representative is removed only when its entire
  class has no model-consistent extension;
- pruning inconsistent prefixes can reduce failed work, but it does not claim a new,
  coarser DPOR equivalence or stronger optimality result;
- candidate ordering may change time-to-bug, but changing the revisit construction,
  maximality test, or equivalence-class ownership requires a separate proof and is outside
  this optimization.

The first active implementation is therefore restricted to proof-pruning native TruSt
items.  A solver-owned explicit program-path frontier, model blocking that regenerates
executions, or RF-value quotienting cannot be combined with the TruSt-optimality claim.

### Sound pruning

A candidate/work item is removed only if:

1. an exact checked-root cycle proves the concrete choice inconsistent; or
2. the CDCL solver reports UNSAT from clauses, each of which was derived from such a cycle
   and guarded by a compatible prefix activation.

The final complete CAAT check remains authoritative.

### Complete enumeration

- GenMC continues to enumerate every schedule and every candidate not proven
  inconsistent.
- RF/CO exactly-one constraints cover exactly the candidates GenMC supplied at that
  dynamic choice point.
- New future-write revisits add alternatives rather than assuming a closed static set.
- `unknown`, missing activation scope, and unsupported explanation retain the native
  candidate.
- No TRUE verdict is derived from the solver alone.

### Required executable oracles

1. Shadow mode: every would-prune choice is still executed and must be rejected by full
   CAAT.
2. Small exhaustive decision trees: native GenMC terminal set equals active filtering.
3. Mutation oracle for insertion, rollback, replacement, and revisits.
4. Recursive broad SC/TSO/PSO differential.
5. Release plus ASan+UBSan.
6. Fixed 15-task actual panel before a package run.
7. TruSt representative oracle on bounded small programs: compare native and shadow/active
   execution-graph signatures; every native signature is either explored exactly once or
   has a replayable no-consistent-extension certificate, and no new signature is generated.
8. Revisit ledger: assign a diagnostic ID at native enqueue and require exactly one terminal
   classification (`EXPLORE` or `PROOF_PRUNED`) with zero missing/duplicate IDs.

## Time-cost model

Let:

- `n` be active real/virtual events in the current graph;
- `e` be edges visited in a certified checked order;
- `k` be candidates at one RF/CO choice;
- `d` be active choice-trail length;
- `C` be retained clauses and `L` their total literals;
- `q` be clauses touched by watched-literal propagation;
- `p` be normalized CAT predicates.

### Existing unavoidable costs

- GenMC interpretation/graph work: unchanged.
- Complete CAAT query: current synchronizer/evaluator cost; worst-case dense relational
  storage and composition remain quadratic or worse in `n`, although sparse/lazy roots
  avoid materializing some closures.

### New hot-path costs

| Operation | Expected | Worst case / risk |
|---|---:|---:|
| Register `k` lazy choice literals | `O(k)` | total alternatives can reach `O(RW + W^2)` |
| Preventive candidate reach | existing `O(e)` per choice point | repeated root traversal dominated V11 |
| Solver assumption/propagation | proportional to `q` | `O(L)` under pathological clauses |
| Replace one current decision | expected `O(1)` or `O(log d)` | persistent-map allocation |
| Forward snapshot | `O(1)` structural share plus one replacement | reclamation bookkeeping |
| Backward decision copy/filter | `O(d)` | no asymptotic increase over the existing `O(n)` graph copy because `d <= n` |
| Work-item compatibility check | watched propagation on snapshot delta | replaying all `d` assumptions if solver state has no common-prefix cache |
| Sparse cycle explanation | cycle/path edges plus derivations | expression composition can enumerate many paths |
| Offline diagnostic Reasoner | not hot path | `O(p n^2)` reason tables plus derivation work |

The implementation may apply assumptions incrementally only when two snapshots expose a
verified shared persistent-map root.  Otherwise it must rebuild assumptions in `O(d)`;
assuming a false ancestor relation would be unsound.  The census must measure this rebuild
rate before active CDCL is approved.

### Evidence-based time risks

- V9 is useful but modest: 67.82% fewer offered RF+CO and 73.97% fewer work pops produced
  only a 3.895% CPU reduction versus V8.
- V10 retained up to 61,700 cores / 1,960,748 literals (31.37 MB) and performed 8.60
  billion literal probes; CPU regressed 4.99% with no search change.
- V11 reduced internal reach candidates but regressed CPU 10.85%.
- Global rank SMT timed out on the first ordering query for all 15 panel tasks.

Therefore a new solver must reduce realized execution/work items, not merely relation
visits or candidate proof time.

## Space-cost model

### Preserved GenMC state

Real graphs remain unchanged.  Each active memory-choice label has one compact sidecar
entry, so mutable state is `O(d)`.  A forward revisit adds a structurally shared snapshot
root and one replacement node.  A backward revisit may need to filter `O(d)` entries; its
new persistent nodes are reclaimed with the child execution/work item.  The census must
measure unique retained snapshot nodes rather than estimating one node per chronological
choice.

Archived V9 bounds give a first engineering envelope: the worst worker/task added 409,678
work items, but the maximum simultaneously retained worklist was 16,388.  At 16/24/32
bytes per uniquely retained snapshot node that visible worklist component is about
0.25/0.375/0.50 MiB.  Conversely, never reclaiming one node per added item would cost
about 6.3/9.4/12.5 MiB on the worst worker/task before map/watch overhead.  These are not
peak-total guarantees: backward-filter nodes, live execution children, allocator overhead,
clauses, and watches must be measured separately.

### New solver state

- Variables: lazy, `O(actual RF/CO alternatives observed)` rather than all potential
  skeleton pairs.
- Clauses: `O(C + L)` plus watched lists and activity metadata.
- Activation scopes: one compact ID per retained compatible prefix; exact guard formulas
  are deferred.
- No rank vectors or transitive-closure Boolean matrix.

Initial measurement thresholds per worker for the prototype:

```text
learned clauses:       50,000
learned literals:   1,000,000
solver/accounted heap: 32 MiB above the V9 worker
```

These are telemetry thresholds for the observation/shadow prototype, not hard correctness
bounds.  Crossing one records the exact point, clause/literal/frontier counts, RSS, and
time; it does not alter search during observation mode.

For an active solver, learned-clause garbage collection is allowed because learned clauses
are redundant, but it must preserve base clauses, locked reason clauses, theory lemmas still
used as reasons, activation definitions, and the complete unexplored frontier.  If garbage
collection cannot keep the solver operational, the run terminates as resource exhausted
and cannot report TRUE.  A fixed 32 MiB hard heap promise is therefore removed until census
data demonstrates that the required non-deletable state fits it.

This distinction is essential:

```text
delete redundant learned clause       -> complete CDCL search continues
lose unexplored assignment/frontier   -> completeness is lost; TRUE forbidden
export all remaining assignments      -> exact handoff possible, but not yet implemented
```

## Pre-implementation measurement gate

### Required observation-only `DecisionState`

The census cannot infer decision depth from event stamps and cannot assume a linear parent
trace.  It needs a sidecar state matching the current graph:

```text
DecisionEntry {
    kind = RF | CO;
    subject Event;
    alternative Event-or-Init;
    monotonically increasing branch-local ordinal;
    activation-scope id;
}
```

- A normal newly created read/write choice inserts one subject entry.
- A forward revisit uses its saved base snapshot, removes decisions cut by the native
  restore, and replaces the alternative RF/CO entry carried by that revisit.
- A backward revisit filters the state by the exact vector-clock view used by
  `getCopyUpTo()`, including holes, then replaces the revisited read's RF entry.  Decisions
  whose subject or required alternative is absent from the copied graph are removed.
- An in-place revisit replaces the entry for the revisited subject and assigns a fresh
  ordinal; the old event stamp is never treated as a level.
- RMW's forced adjacent CO placement is tagged as implied by its RF node, not a new
  independent decision.

For the census this state is observational: it records the branch GenMC already chose and
does not change worklist order or candidate sets.  A consistency assertion must compare
the state's reconstructed RF/CO decisions with the actual graph after every restore,
forward revisit, backward revisit, and in-place revisit.  Any mismatch invalidates the
census row.

The census must additionally report: active decisions, unique retained snapshot nodes,
snapshot bytes, backward-filter entries visited/copied, assumption rebuild count and
literals, verified shared-root transitions, and predicted queued-item rejections.  Without
these measurements there is no implementation gate for active filtering.

The observation-only census must answer, on the 15 actual tasks:

1. How many complete-CAAT rejections remain after V9 preventive pruning?
2. How many explanations translate completely to RF/CO plus a prefix scope?
3. In how many cores is the second-highest relevant choice at least two validated decision
   ordinals below the current choice?
4. How many queued/realized prefixes would actually become UNSAT, not merely how many
   solver-level backjumps are possible?
5. What CPU/RSS does explanation add?

Proceed to implementation only if there is a repeated non-local population large enough
to plausibly save more work than V10/V11 added.  A proposed quantitative gate is:

- at least 10% of full-CAAT rejections fully translatable;
- at least 5% of realized revisit prefixes are shadow-invalidated before restoration;
- diagnostic explanation CPU is below 10% of baseline CPU after replacing offline
  Reasoner cost with the projected lazy explanation cost;
- no semantic/search mismatch in shadow mode.

These thresholds may be tightened from measured distributions, but must not be relaxed
merely to force implementation.

## Implementation phases after the gate

1. `P0`: compact `ChoiceLiteral`, graph-matched `DecisionState`, immutable snapshots, and
   observation-only mapping tests.
2. `P1`: shadow guarded-clause engine; no pruning.
3. `P2`: active filtering at work-item restore only; existing candidate generation stays.
4. `P3`: solver filtering/order at RF/CO hooks.
5. `P4`: optional property/interference-guided decision order after search reduction is
   demonstrated.

Each phase must separately pass the full correctness gates and the 15-task panel.  Do not
combine phases in one performance result.
