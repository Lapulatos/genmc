# P0.7e product-state EOG cycle protocol

## Starting state

- Branch: `genmc-caat`
- Starting commit: `d963c49e45b82066ca9dcd036232c7384f6a95fe`
- Retained baseline: P0.7d2 depth-bounded streaming lazy-cycle traversal.
- Baseline mechanism: 7,034,520 lazy checks and 5,694,936,384 root-relation
  candidate emissions in the completed 2,304-cell matrix.
- Dirty files at start are planning and experiment artifacts only; production and test
  sources match the starting commit.

## Reuse audit

- Reuse `NormalizedModel`, `ModelAnalysis`, immutable shared model metadata, packed
  `Relation` iteration, `setContains`, and the P0.7d2 exact streamed/buffered fallback.
- Reuse the generated checker layout idea of separate visit state per
  `(control-state,event)` only. Do not dispatch to `SCChecker` or `TSOChecker`, inspect
  model names, or reuse a built-in verdict.
- The ordering-consistency literature motivates incremental/event-graph cycle checking,
  but its SMT clauses and model-specific encodings are not copied into this runtime.
- Implement locally because the required object is a small C++23 automaton over the
  existing normalized CAT IR; adding an SMT/runtime dependency would not fit GenMC's hot
  path or licensing/deployment boundary.

## Semantic contract

For an analyzer-certified, non-recursive positive relation expression `R`, compile
`Base`, `Alias`, `Union`, `Composition`, `Identity`, and `Optional` into an acyclic
epsilon-NFA. Treat an admitted `Intersection` as one exact atomic transition evaluated
by the existing lazy interpreter. Add one reset transition from the root exit to its
entry and explore the product graph `(control state, event)`.

The product graph has a directed cycle iff `R` has an event cycle:

1. Every `R(e,f)` path runs from `(entry,e)` to `(exit,f)`; reset connects it to
   `(entry,f)`.
2. An event cycle therefore maps to a product cycle containing reset transitions.
3. The control graph without reset is acyclic, so every product cycle contains at least
   one reset.
4. Splitting such a product cycle at resets yields only accepted `R` paths and therefore
   an exact closed event witness.

Any compilation failure, unsupported normalized kind, recursive control dependency,
state-count overflow, product-size overflow, absence of an expanded composition, or
more than 64 MiB of product color state falls back to the retained P0.7d2 algorithm.
Fallback is an exact semantic path, not an assumed safe/unsafe result.

## Expected cost changes

- Time reduced: for `A ; B`, suffix `B(m,_)` is traversed once from product state
  `(after-A,m)` instead of once for every source that reaches `m`. The expected bound is
  proportional to visited product transitions rather than fully emitted macro edges.
- Space added: one byte of color per reachable-capable product vertex, `O(QN)`, where
  `Q` is compiled control states and `N` is events. The immutable automaton is built once
  in `ModelAnalysis` and shared by workers.
- Space avoided: no root relation, composition result, or successor row is persisted.
- Deep paths: P0.7e2 stores product DFS frames on the heap and resumes base-relation
  successor cursors directly. It has no product-depth restart. The retained P0.7d2
  fallback still bounds recursive event traversal at 2,048 vertices.
- Rollback: no product colors survive a query. Immutable automata remain valid while
  incremental base relations and elided derived predicates roll forward/back.
- Intersections: atomic fallback can still enumerate macro candidates inside that
  transition; no benefit is claimed for the unsupported source-dependent factorization.

## P0.7e2 refinement

P0.7e1 used native recursion and callback-based relation enumeration. Its two-repetition
pilot reduced the TSO queue's root macro candidates from 455.4M to 101.5M but traversed
1.777B product transitions; total CPU rose by 17.6--22.4% and RSS by 36.5--36.6%.
P0.7e2 preserves the same automaton and product graph while changing only traversal:

- one explicit `ProductFrame` per active product vertex;
- one `Relation::nextSuccessor` cursor for base relation transitions;
- scalar cursors for epsilon, reset, and set-guard transitions;
- a source-local successor vector only for an admitted atomic intersection, preserving
  exact emitter order and multiplicity.

This removes native product recursion, per-base-edge callbacks, whole-row successor
buffers, and exact-check restarts. The correctness and retain gates remain unchanged.

## Expected source scope

- `genmc/genmc/CAT/Analysis.hpp`
- `genmc/genmc/CAT/Analysis.cpp`
- `genmc/genmc/CAT/LazyCycle.hpp`
- `genmc/genmc/CAT/LazyCycle.cpp`
- `genmc/genmc/CAT/CaatEvaluator.hpp`
- `genmc/genmc/CAT/CaatEvaluator.cpp`
- `genmc/genmc/CAT/IncrementalEvaluator.hpp`
- `genmc/genmc/CAT/IncrementalEvaluator.cpp`
- `genmc/genmc/Execution/Consistency/CATChecker.cpp`
- `tests/unit/CatEvaluatorTest.cpp`

No parser semantics, graph adapter, host candidate generation, model file, server path,
Docker configuration, or BenchExec adapter belongs in the production diff.

## Correctness gates

1. Property test product-EOG vs fully materialized relation on randomized finite
   union/composition/identity/optional/intersection expressions; every witness edge must
   belong to the materialized root.
2. Deterministic zero-length, nested composition, shared-middle, atomic-intersection,
   overflow/fallback, and 8,193-event deep-cycle tests.
3. Server Release unit/property suite: no regression from 158/158.
4. Server ASan+UBSan focused product/lazy/incremental tests.
5. Mutation oracle: all 39 rows and at least 5,441 full recomputations, zero mismatch.
6. Broad SC/TSO/PSO differential: 852 comparable matches, 12 mutually unsupported,
   zero status/verdict/execution mismatch.
7. Formal before/after matrix: zero opposite verdict, safe execution-count mismatch,
   new runtime error, or solved-to-resource regression.

## Performance protocol and retain rule

- Pilot first on the retained 96-task SC/TSO/PSO corpus; quarantine the prototype on a
  correctness failure or deterministic slowdown.
- Formal matrix if the pilot passes: 96 tasks x 3 models x 4 repetitions x
  before/after = 2,304 cells, alternating disjoint NUMA-local task groups in the server
  Docker image.
- Primary unit: task; aggregate common-terminal CPU after/before geomean with fixed-seed
  10,000-task bootstrap 95% CI.
- Supporting metrics: model-specific CPU/wall ratios, peak RSS, retained/peak
  snapshot-equivalent bytes, lazy macro candidates, product states/transitions,
  depth fallbacks, TIMEOUT/OOM/status transitions, and large-task strata.
- Retain only if all correctness gates pass, aggregate CPU CI upper bound is below 1.0,
  large-task RSS and snapshot-equivalent ratios are each at most 1.02, and there is no
  new OOM/runtime error/solved regression. Product-transition reduction is mechanism
  evidence, not a substitute for the end-to-end gate.
- Otherwise remove the production/test diff and retain only XML, logs, analysis, and the
  rejection decision.

## P0.7e1 pilot result and P0.7e2 refinement

P0.7e1 passes Release 161/161, focused ASan+UBSan 7/7, 39 mutation rows with
5,441 oracle checks, and broad 852/12/0. Its two-repetition pilot nevertheless fails
before the formal matrix:

- TSO `queue_ok_longer` CPU ratios are 1.2243 and 1.1761; RSS ratios are 1.3662 and
  1.3651.
- PSO `queue_ok_longer` CPU ratios are 1.0316 and 1.0293.
- TSO `fib_unsafe-7` improves to 0.9107 and 0.9166 CPU, but a large-task gain cannot
  override deterministic regressions and the memory bound.

The observed extra RSS is consistent with a deeper product DFS retaining large C++
callback frames. P0.7e2 replaces only product traversal with an explicit heap stack:

- base relation transitions resume through `Relation::nextSuccessor` without a callback
  or successor row;
- epsilon and set-guard transitions are single-step cursors;
- whole-edge atomic intersections retain an exact per-active-frame successor buffer;
- product paths no longer use the 2,048 native-stack bound or restart after depth
  overflow; the retained P0.7d2 macro-edge fallback remains unchanged.

P0.7e2 must repeat every correctness gate and the same two-repetition pilot. It may enter
the formal matrix only if the deterministic TSO/PSO queue regressions disappear and the
TSO queue RSS ratio falls below 1.02 in both repetitions.
