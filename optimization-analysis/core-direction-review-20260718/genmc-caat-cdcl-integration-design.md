# GenMC / CAAT / CDCL integration design review

## Decision

The current finite symbolic lane is not the production integration path.  It builds a
whole-program SMT assignment in `lli/main.cpp`, materializes a second CAT graph, optionally
solves a second ordering problem, and finally asks ordinary GenMC to replay an error.  This
duplicates graph and search state and measured worse on the 725-task run.

The active architecture keeps the real GenMC `ExecutionGraph` as the only event/relation
state, keeps the existing worker-local incremental CAAT synchronizer, and adds CDCL only at
GenMC's existing RF/CO/revisit choice boundary.  The standalone finite lane remains a
diagnostic/oracle and receives no further performance tuning.

## Existing integration that must be reused

`BasicCATChecker` already supplies most of the graph/theory boundary:

- `getCoherentStores()` enumerates an actual read's RF candidates before installation;
- `getCoherentPlacings()` enumerates an actual write's CO positions before installation;
- analyzer-produced `PreventiveOrderCertificate` proves when proposed RF/FR/CO edges enter
  a checked positive order;
- `preparePreventivePrefix()` computes exact full-root or focus-specific reachability;
- `preventsRf()` and `preventsCo()` reject a proposal that closes the certified top-level
  order;
- `GraphSynchronizer` classifies real graph evolution as unchanged, insert,
  rollback-insert, replacement, or rebuild;
- `IncrementalCaatEvaluator` already has worker-local checkpoint/rollback state;
- `learnConflictCore()` derives a top-level lazy-cycle explanation into stable base facts.

Therefore a new duplicate relation graph or SC completion solver is not justified.  The
missing part is a decision-literal layer and clause propagation across GenMC revisits.

## Why the current conflict database is not CDCL

`ConflictCoreDatabase` stores conjunctions of positive CAT base facts and linearly matches
them against a prefix plus one proposed delta.  It does not provide:

- Boolean variables for mutually exclusive RF/CO/guard choices;
- watched-literal unit propagation;
- resolution that eliminates alternative CO/internal decisions;
- decision levels or non-chronological backjumping;
- learned-clause activity/deletion;
- a single solver state shared by successive revisits.

The archived V3/V4 result (about 1% CPU regression) is evidence against adding more linear
core matching, not evidence against a properly integrated CDCL layer.

## Ownership and completeness contract

First integrated version deliberately leaves scheduling with GenMC/DPOR.

| Choice/state | Owner | Persistent representation |
|---|---|---|
| Actual LLVM execution and event creation | GenMC | current execution/prefix |
| Schedule and revisit enumeration | GenMC/DPOR | existing compact worklist |
| Guard and concrete program values | GenMC initially | actual labels; optional stable literal later |
| RF source and CO placement choices | GenMC enumerates; CDCL filters/orders | stable choice literals |
| CAT base and derived relations | CAAT | synchronized current graph plus undo history |
| Cross-revisit conflicts | CDCL | learned clauses |

CDCL may reject or prioritize an existing GenMC candidate only from a proof returned by
the certified CAAT top relation.  It must never assume responsibility for enumerating a
choice that GenMC then stops enumerating unless an explicit completeness proof and oracle
test cover that handoff.

## Stable literals

Initial literal vocabulary should be limited to choices already visible at the preventive
hook:

```text
RF(read-event-key, source-event-key-or-init-address)
CO(write-event-key, predecessor-event-key-or-init-address)
```

Event keys use the existing `StableGraphAdapter` identity.  A literal is meaningful only
under the prefix facts that make both endpoints active.  Learned clauses must therefore
include an activation/prefix guard or remain scoped to the compatible GenMC prefix.  The
first implementation should prefer scoped clauses; global clause reuse is enabled only
after the activation guard is explicit and tested.

## Theory conflict path

For a proposed RF/CO choice:

1. materialize its exact RF/FR/CO base delta with the existing helper;
2. perform certified bottom/intermediate shortcuts only when the analyzer proves their
   relation is included in a checked positive root;
3. otherwise derive the checked-root edge/path lazily;
4. if a cycle closes, derive every root edge to stable base facts;
5. translate those base facts to active choice literals;
6. reject the current proposal and add the negated conjunction as a clause;
7. if translation is incomplete, use the existing direct rejection only and do not learn.

An RF-only core is not a separate algorithm.  It appears only after clauses for all
relevant CO alternatives are resolved.  Until then the clause remains mixed RF/CO and can
reject only the corresponding branch.

## Solver lifetime and memory boundary

- One solver instance per verification worker, not per execution or work item.
- The GenMC worklist continues to store compact revisit/prefix objects, never solver
  snapshots.
- Solver checks use assumptions for the current compatible prefix and proposed choice.
- Learned clauses stay resident only when their activation scope is explicit.
- CAAT and `ExecutionGraph` rollback remain authoritative; the solver does not own a
  second event graph.
- No eager all-event RF matrix, CO rank, all-different order, or transitive-closure matrix.

This preserves GenMC's current-trace memory shape while obtaining CDCL propagation over
repeated choice combinations.

## Cost risks and mandatory counters

The potential win is avoiding installation/revisit of known-inconsistent choices.  The
costs are prefix preparation, relation reachability, proof derivation, literal translation,
solver propagation, and retained clauses.  Measure separately:

- actual RF and CO candidates offered;
- preventive queries, direct/focus reach visits, and candidates pruned by level;
- proposals later rejected by full CAAT despite passing preventive checks;
- proof derivations and translatable/untranslatable conflicts;
- unique stable RF/CO literals;
- clause count, literals, bytes, propagations, conflicts, and deletions;
- solver calls and nanoseconds at candidate hooks;
- revisit prefixes avoided and realized;
- CAAT synchronization insert/rollback/replace/rebuild counts and time;
- total CPU and peak RSS.

Do not infer a CDCL benefit from cycle counts alone.  A conflict is valuable only if the
same literal combination or a propagated subset would otherwise recur.

## Empirical gates

### Gate A0: existing candidate-conflict evidence

The completed V9/V10 725-task experiment already answers whether replaying immediate
candidate cycle conflicts through a clause structure is useful:

- V10 matched 14,193,035 learned positive cores but changed no common-correct search
  counter and regressed CPU by 4.99%; it is rejected.
- Re-aggregation of the V9 archived logs gives 5,814,007 RF proposals and 12,899,587 CO
  proposals pruned by the preventive root, but zero
  `preventive-all-pruned-fallbacks` and zero `preventive-prefix-inconsistent`.

Thus no observed choice point had every sibling eliminated by these immediate cycle
proofs.  Replacing the linear database with watched clauses may reduce matching cost, but
the same clauses do not establish a new subtree reduction.  Do not reimplement this gate.

### Gate A1: non-local conflict/backjump census

Extend statistics without changing candidate order or pruning.  For proposals accepted by
the preventive hook but rejected after installation by the complete CAAT check, derive the
top-level conflict and map its stable base facts to the RF/CO choices in the realized
prefix.  Record the current decision depth, highest and second-highest involved depth,
potential backjump distance, prefix guard, recurrence, and unresolved facts.  This is the
only remaining evidence that can justify CDCL ownership rather than candidate filtering.

Reject the CDCL implementation before coding if conflicts always involve the newest
decision only, are mostly untranslatable/unique, or explanation cost already exceeds the
potentially skipped realized prefixes.

### Gate B: shadow clause engine

Run a clause engine in observation mode.  It records whether a learned clause would have
filtered a later candidate but never changes GenMC.  Compare every shadow decision with
the ordinary full CAT result.  Any false prune blocks promotion.

### Gate C: active filtering, single worker

Enable only proven shadow hits.  Keep ordinary GenMC enumeration as fallback when the
solver is unavailable, scoped activation is unknown, or relation translation is
unsupported.  Pass unit, mutation oracle, recursive broad differential, Release, and
ASan+UBSan before the performance panel.

Only a clear panel reduction in realized revisits/full CAAT checks with controlled CPU and
RSS justifies a full `pthread-wmm` run.  Other packages and the complete 725 follow only
after that gate.
