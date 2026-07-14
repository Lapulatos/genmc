# Phase 3 plan: incremental and online CAAT integration

## 1. Objective and completion boundary

Phase 3 replaces repeated from-scratch CAAT evaluation on GenMC's exploration
hot path with a stateful, backtrackable evaluator. For each checker worker it
must:

1. maintain normalized CAAT predicate values as base events and edges are
   inserted;
2. propagate only semantic deltas through recursive SCCs until the same least
   fixed point as Phase 2 is reached;
3. create exact checkpoints and restore them during exploration backtracking;
4. detect graph changes that cannot be handled incrementally and perform a
   correct, observable offline rebuild;
5. update `empty`, `irreflexive`, and `acyclic` violations online; and
6. use a violation for early pruning only when static analysis proves that the
   checked predicate is monotone under all accepted future changes.

Completion means the incremental path is used by real SC, TSO, and PSO
recursive CAT files during GenMC exploration, not merely exposed as an unused
library API. Its verdicts and explored executions must match the Phase 2
from-scratch oracle over the frozen broad corpus. General deletion maintenance,
arbitrary CAT, and online SMT integration are not silently claimed.

## 2. Evidence and reuse decision

| Source | Revision/license | Reused part | Decision |
|---|---|---|---|
| Phase 1/2 GenMC CAT implementation | this repository, Apache-2.0/MIT | normalized IR, analysis, packed values, graph adapter, offline oracle, checker/tests | extend without changing accepted semantics |
| CAAT paper | OOPSLA 2022, CC BY 4.0 paper | insertion-seeded least fixed point, delta worklist, online limitations | normative semantic and algorithm reference |
| Dat3M CAAT | `a7e3e4843359dde3a0e29500a821030e2433e316`, MIT | predicate hierarchy, timestamped materialization, rollback notification | port the design idiomatically to C++23; no Java runtime or mechanical translation |
| Kater | POPL 2023 paper and generated GenMC code | new-event cycle reasoning, restrictions on saved relations | use for safe online constraint checks and mutation audit |
| Static Analysis of Memory Models for SMT Encodings | OOPSLA 2023 | model preprocessing and dependency classification | use as an analysis reference; implement only properties needed to prove pruning |
| DRed/DRed-C, Differential Dataflow, DBSP | published algorithms | recursive deletion and general incremental view maintenance | research reference only in Phase 3; dependency/runtime cost is not justified |
| RAT-CAT-SAT | OOPSLA 2026 | full rational CAT relation reasoning | not an online program-execution checker; retain as future CAT-analysis reference |

Dat3M's current source demonstrates useful incremental mechanisms but not a
complete solution to CAAT's online problem: dynamic difference RHS is rejected,
difference in recursion is unsupported, and the generic backtrack interface is
not evidence by itself of GenMC-style DPOR integration. Phase 3 therefore reuses
its trail/propagation architecture while retaining GenMC's offline oracle and
fallback.

## 3. Supported online contract

The online fast path accepts only models for which analysis proves all
check-reachable equations positive and monotone. In the current normalized
language this excludes difference, including semi-positive difference, because
future additions to its RHS can remove derived facts. Other Phase 2 models may
still be evaluated offline; rejection from the online path is not rejection of
their offline semantics.

The graph synchronization layer classifies each query as one of:

- **initialize/rebuild**: no state exists, the event universe changed in an
  unsupported way, or no retained state is a semantic predecessor;
- **insert**: every previous base fact is present and only new events/edges were
  added;
- **rollback**: the current base snapshot exactly matches a retained
  checkpoint; or
- **rollback-plus-insert**: a retained checkpoint is a subset of the current
  snapshot and the remainder is insertion-only.

Equality and subset tests cover all base values, not just label count. This is
required because GenMC can change `rf`, `co`, lifecycle and revisit edges
without changing the number of labels. Unknown or mixed insert/delete changes
always rebuild.

## 4. State and algorithms

```text
ExecutionGraph query
    -> CATGraphAdapter immutable semantic snapshot
    -> stable event-key remapping / universe growth
    -> classify against retained checkpoints
       -> exact rollback, insertion delta, or offline rebuild
    -> IncrementalCaatEvaluator
       timestamped base and derived memberships
       dependency-priority delta queue
       recursive SCC propagation to quiescence
       online axiom state
    -> consistency result
    -> optional debug/test cross-check with CaatEvaluator
```

### Event identity and domain growth

`EventPos` identifies real labels across append/cut operations. Virtual initial
writes use `(InitLabel, address)` keys. A worker-local key-to-index table never
reuses an index while the checker lives. Packed values grow while preserving
all old memberships; inactive keys are absent from the current `_` set rather
than renumbered. This prevents the current adapter's sorted virtual-init suffix
from turning an address insertion into a false deletion/reindexing.

### Delta propagation

Every work item identifies the changed operand, target predicate, and newly
inserted set elements or relation edges. Operators compute only consequences
involving that delta where practical; correctness-first operators may compute
their full next value and subtract the old value, but they must enqueue only
the difference. Recursive SCCs run until the queue is empty. Duplicate facts
retain their earliest live derivation time and are not re-enqueued.

### Rollback

Every inserted membership records an epoch on an undo trail. A checkpoint owns
the trail size, semantic graph fingerprint, active event set, and violation
state. Rollback removes trail entries in reverse order and invalidates later
checkpoints. Derived facts with multiple supports require either support-aware
timestamps or recomputation from the restored base checkpoint; tests decide
the simplest exact representation. No stale explanation or violation witness
may survive rollback.

### Online axioms and pruning proof

For positive predicates, derived memberships only grow between rollbacks.
Therefore a witnessed non-empty predicate, self-loop, or directed cycle cannot
be repaired by later insertions. The checker may reject immediately and GenMC
may prune that extension. After rollback, witnesses are restored or recomputed
from the live state. Models with a non-monotone check dependency use the
offline evaluator and cannot use this proof.

## 5. Substages and atomic delivery

### Phase 3.0: research, specification, and baseline

- Freeze this plan, research question, source revisions/licenses, mutation
  taxonomy, fallback rule, and test matrix.
- Update persistent project constraints to require this plan before every
  Phase 3 substage.
- Re-run the focused Phase 2 baseline and confirm the frozen broad-result
  counts before implementation.

Acceptance: the online semantics, safe-pruning proof obligation, unsupported
boundary, reuse decisions, and completion evidence are reviewable without
reading future code. Documentation-only commit is pushed.

### Phase 3.1: extensible values and standalone incremental state

- Add packed set/relation universe growth that preserves existing bits and
  property-test it across word boundaries.
- Add `IncrementalCaatEvaluator` initialization, immutable result access,
  statistics, and a correctness-oracle mode.
- Reuse Phase 2 analysis/dependency ordering; do not integrate with GenMC yet.

Acceptance: initialization equals Phase 2 for every existing evaluator fixture;
growth tests cover 0, 1, 63, 64, 65 and larger universes; sanitizers/unit tests
show no stale views or index corruption.

### Phase 3.2: insertion delta propagation

- Implement base-fact insertion and operator-local propagation for every
  positive online operator: alias, identity, domain/range, inverse, optional,
  product, composition, union/intersection, and both closures.
- Maintain recursive SCC queues to quiescence and expose delta counters.
- Reject non-monotone updates before mutating state.

Acceptance: randomized insertion sequences compare every predicate after every
step with a fresh Phase 2 evaluation. Tests include single/mutual recursion,
duplicate support, transitive paths arriving in adversarial orders, sets and
relations, and universe growth.

### Phase 3.3: checkpoints, rollback, and violation state

- Add epochs, checkpoint handles, undo/restore, and stale-handle diagnostics.
- Maintain or recompute online witnesses for all three axiom kinds.
- Ensure explanations are generated only from a current immutable snapshot;
  Phase 2 `Reasoner` may be invoked on demand rather than trailed.

Acceptance: randomized push/pop trees compare values, violations, and
explanations with the offline oracle at every node. Rollback across recursive
multi-support facts and 64-bit word boundaries is exact.

### Phase 3.4: stable graph synchronization

- Introduce stable real/virtual event keys and remap adapter snapshots into a
  persistent worker universe.
- Compare every base predicate to classify initialize, insert, rollback,
  rollback-plus-insert, or rebuild.
- Bound checkpoint memory and record rebuild reasons/counters.

Acceptance: focused graph tests cover label append, new initial-write address,
`rf` replacement, `co` placement/reordering, RMW, create/join, `removeLast`,
`removeAfter`, `cutToStamp`, and non-LIFO revisit shapes. Every classification
produces the same base values as a fresh adapter/evaluator.

### Phase 3.5: GenMC checker integration and early pruning

- Give each `BasicCATChecker` worker a mutable incremental state while keeping
  compiled model/analysis immutable and shared.
- Route admissible normalized models through synchronization on every
  `isConsistent` query; retain Phase 1 behavior for non-normalized models.
- Apply early rejection only under the recorded monotonicity certificate and
  expose opt-in diagnostic/statistics output suitable for tests.

Acceptance: real recursive SC/TSO/PSO programs exercise insertion and rollback
instead of only rebuild. One/two-worker results, exact execution counts,
warnings and errors match Phase 2; a deliberately non-monotone fixture proves
pre-execution rejection and therefore no unsafe early prune. The earlier
offline-fallback wording was removed after integration showed that GenMC calls
the consistency checker on prefixes: an offline evaluation of a non-monotone
prefix can reject an execution that a later insertion would repair.

### Phase 3.6: mutation/fallback hardening and differential stress

- Stress graph cuts, revisits, alternative `rf`/`co` candidates, dynamic
  addresses, lifecycle edges and repeated exploration branches.
- Add periodic debug/test oracle checks and deterministic mismatch dumps.
- Fix every unexplained rebuild misclassification or result mismatch; do not
  weaken the oracle comparison.

Acceptance: deterministic and randomized exploration stress has zero oracle
divergence, use-after-rollback, unsupported mutation, or unexplained fallback.

### Phase 3.7: broad validation, performance, and closure

- Re-run all unit/property/integration tests, Phase 1's 576 SC/TSO rows and
  Phase 2's 864 recursive rows.
- Add PSO and mutation-heavy online coverage so the final online corpus has at
  least 864 real program/model rows and covers safe/error programs, RMW,
  lifecycle, dynamic allocation, data structures, and one/two workers.
- Record incremental insertions, rollbacks, rebuilds and oracle comparisons,
  plus operation counts, end-to-end time and peak RSS versus Phase 2.
- Update user/developer documentation, complete a requirement-by-requirement
  audit, commit/push, and verify local/remote equality.

Acceptance: zero unexplained mismatch and zero unsupported row in the frozen
online corpus; incremental operations occur on real programs; no Phase 1/2
regression; every rebuild is classified; the branch is clean and equals
`origin/genmc-caat`.

## 6. Test oracle hierarchy

1. `CaatEvaluator` freshly recomputed at every randomized state.
2. Direct `std::set`/graph algorithms for packed growth, deltas and cycles.
3. Phase 1 evaluator for acyclic non-recursive models.
4. Built-in generated SC/TSO checkers for end-to-end execution behavior.
5. herd on aligned CAT/litmus event structures.
6. Dat3M only where base-event semantics and supported difference restrictions
   are demonstrably identical.

No performance counter, aggregate verdict, or absence of crashes substitutes
for predicate-by-predicate equality with the offline oracle.

## 7. Risks and controls

| Risk | Required control |
|---|---|
| graph mutation appears insertion-only after dense reindexing | stable event keys plus all-base subset comparison |
| a derived fact has multiple supports with different lifetimes | support-aware trail or exact recomputation at rollback; oracle tests decide |
| transitive closure delta propagation misses mixed old/new paths | randomized adversarial order tests against fresh closure |
| difference can shrink after an RHS insertion | exclude it from monotone online certificate; offline rebuild only |
| an early violation can later disappear | prune only for positive certified dependencies; differential execution counts |
| checkpoint history grows without bound | bounded ancestor cache with observable rebuild on eviction |
| mutable checker state crosses workers | one state per checker/worker; immutable model and analysis only are shared |
| external IVM library adds runtime/deployment cost | dependency-free C++23 implementation unless measurements overturn the decision |

## 8. Explicit non-goals

- Full herd CAT syntax, scopes, procedures, candidate generation, or arbitrary
  non-stratified recursion.
- General incremental deletion for recursive Datalog or dynamic difference.
- Embedding CAAT as a CDCL(T) plugin in an external SMT solver.
- Eliminating the Phase 2 offline evaluator or fallback.
- Claiming RAT-CAT-SAT's model-property checker as GenMC program verification.

## 9. Phase completion checklist

- [x] Phase 3.0--3.7 each have a precheck, reuse survey, contract, verification,
  gap analysis, commit SHA, and push result.
- [x] Every positive normalized operator has insertion-delta oracle coverage.
- [x] Checkpoint rollback is exact for values, violations and explanations.
- [x] Every declared GenMC mutation class is incrementally handled or safely
  rebuilt with an observable reason.
- [x] Online pruning has a static monotonicity certificate and execution-level
  differential evidence.
- [x] At least 864 real online program/model rows have zero unexplained mismatch
  and zero unsupported rows.
- [x] Phase 1 and Phase 2 frozen broad suites still have zero mismatch.
- [x] Runtime counters prove insertion and rollback occur on real programs.
- [x] Performance, peak memory, rebuild rate and fallback limits are recorded.
- [x] Documentation states exact supported semantics and non-goals.
- [x] Final branch is clean and equals `origin/genmc-caat`.
