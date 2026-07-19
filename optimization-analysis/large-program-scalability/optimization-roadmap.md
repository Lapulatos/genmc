# GenMC large-program scalability roadmap

## Conclusion

The next useful optimization should reduce the number of concrete candidates and
retained prefixes. Another dense-relation micro-optimization is P2: Optimization 12
showed a real 9.5% offline saving but did not establish the primary aggregate CPU gate,
while the 725-task gap is 260 versus 26 TIMEOUT+OOM rows.

## P0.1: rejection-kernel census (no pruning)

### Implementation

- Canonicalize each exact CAAT violation explanation into stable base literals:
  event identity, relation kind, polarity, and edge endpoints.
- Run deletion-based or graph-kernel minimization to find a smaller sufficient reason.
- Record kernel size, duplicate rate, subsumption rate, earliest matching prefix, and
  model/check source.
- Replay the minimized kernel through the full evaluator before accepting it as evidence.

### Expected cost reduction

- None in the first measurement build; it only estimates opportunity.
- A high duplicate/subsumption rate predicts later reductions in candidate construction,
  CAT queries, execution-graph histories, and revisit queues.

### Stop rule

Stop if fewer than 10% of rejected candidates share a reusable kernel or if most kernels
can fire only after a complete execution graph already exists.

## P0.2: exact prefix blocker and antichain store

### Implementation

- Store sufficient positive kernels as an antichain: discard supersets of an existing
  blocker and replace existing supersets when a smaller blocker arrives.
- Query blockers when `rf/co/lifecycle` choices are proposed, before graph cloning and
  full CAT materialization.
- Version blockers by normalized-model fingerprint and stable event generation.
- Fail closed on negative literals, difference-derived reasons, or stale event IDs.

### Expected cost reduction

- Candidate generation: skip whole subtrees before execution completion.
- Time: fewer interpreter steps, graph synchronizations, and CAT queries.
- Space: fewer retained revisit prefixes/checkpoints and less base-history growth.

### Correctness

Every blocker is a sufficient inconsistent conjunction proved by exact explanation
replay. A match can only prune a prefix containing that conjunction; it cannot prune a
prefix that omits a required literal.

## P0.3: consistency-preserving choice propagation

### Measured status

The exact recursive-PSO prototype is retained as an opt-in coverage optimization, not
as a default general speedup. Four paired repetitions recover the same five TIMEOUT
tasks every time without adding OOM; CPU is 0.95369 [0.87527, 1.04274], so the confidence
interval does not establish a universal improvement. It removes 78.56% of offered
RF/CO candidates and cuts common-task offline evaluations by 55.0%. The authoritative
decision and correctness evidence are in
`../continuous/preventive-census/analysis-pruning-v2/decision.md`.

### Implementation

- First run the measurement-only counterfactual protocol in
  `p0.3-preventive-census-protocol.md` on the exact recursive PSO fingerprint.
- For a successful census, maintain the certified positive-order CAT fragment as a
  sparse event-order graph with incremental reachability. A proposed direct order edge
  `(u,v)` is forbidden only when the pre-choice `reach` already contains `(v,u)`.
- Classify this exact delta-reversal rule into the FM 2026 Self-Reversal, Stale-Read,
  RMW-Broadcast, and RF-Join cases; do not guess general atomic blocks that GenMC does
  not represent.
- Before committing a proposed reads-from/coherence choice, derive only its incident
  `rf/rfe`, `co`, and recomputed `fr` delta. If a delta edge closes a forbidden cycle,
  omit that choice.
- Use generic CAT/CAAT evaluation as the oracle and fallback; do not call the built-in
  SC/TSO checker.

### Expected cost reduction

- Avoid building candidates known to violate an order axiom.
- Avoid dense derived-relation materialization for rejected prefixes.
- Reduce reason/conflict construction because the stored fragile-choice reason is
  already a compact kernel.

### Risk

The order fragment must be certified structurally. Arbitrary sets, difference,
non-monotone recursion, and value-observing checks retain the existing evaluator.
FM 2026 proves the generalized patterns for SC; PSO pruning therefore additionally
requires a model-local proof that every tested delta edge belongs to the exact monotone
`order` whose recursive `reach` is checked. The implemented certified path now prunes
only under that condition; all other models retain ordinary CAT/CAAT evaluation.

## P1.1: property/dependence-guided exploration

### Implementation

- Build an LLVM SSA backward slice from assertions, memory addresses, branches,
  synchronization calls, and thread lifecycle.
- Rank `rf/co` choices by whether they affect this slice.
- Initially use the slice only for scheduling; this changes time-to-bug but not coverage.
- Later prototype a CEGAR abstraction in which non-critical reads share value classes;
  refine when concrete replay diverges.

### Expected cost reduction

- Heuristic version: earlier unsafe termination, no expected safe-task state reduction.
- CEGAR version: fewer distinct local-value/control states and smaller execution keys;
  potentially large TIMEOUT/OOM reduction on loop/data-heavy programs.

### Soundness boundary

The heuristic is complete because it only reorders work. The abstraction can report an
unsafe result only after concrete replay and can report safe only after a sound bounded
refinement fixpoint.

## P1.2: refinement-aware parallelism

### Implementation

- Partition workers by unresolved abstract choice/kernel frontier, not by uncoordinated
  concrete search.
- Share immutable learned kernels through per-worker read snapshots and batched merges.
- Measure useful kernel hits, duplicate work, merge contention, and total CPU.

### Expected cost reduction

- Wall time falls when workers discover complementary kernels.
- Total CPU can fall because shared blockers remove redundant subtrees, unlike the
  current `--nthreads` behavior.

## P2: bounded symbolic front-end / hybrid GenMC

Large programs with many local paths and loop iterations need more than concurrency
ordering improvements. Introduce a bounded SSA/guard layer that symbolically represents
local computation and exposes only property-relevant memory events to GenMC. GenMC then
serves as concrete witness/replay and exact weak-memory consistency authority, while the
symbolic layer performs CEGAR over data/control choices.

This is the path most likely to approach Deagle's coverage, but it is also a new
architecture. It should start only after P0 instrumentation shows how much of the 260
TIMEOUT/OOM cohort is caused by repeated order conflicts versus local path/value growth.

## Experiment sequence

1. 96-task instrumentation run plus 60 stratified TIMEOUT/OOM tasks at 60 seconds.
2. Offline replay: estimate prunable candidates using recorded kernels without changing
   execution.
3. Exact prefix blocker, single worker, SC only; differential/oracle gates first.
4. Extend certified fragment to TSO/PSO and CAT/CAAT.
5. Four-worker shared-kernel experiment against one worker and current four-worker mode.
6. Only then prototype dependence CEGAR on loop/data-heavy tasks.

Each stage reports coverage, candidate/prefix counts, CPU, wall, RSS, kernel bytes, and
wrong/unknown directions. Improvements on solved-only small tasks are insufficient for
retention.

## Current source insertion map

| Mechanism | Current insertion point | Reason |
|---|---|---|
| Learn exact violation kernel | `BasicCATChecker::isConsistent()` in `genmc/genmc/Execution/Consistency/CATChecker.cpp` | both incremental and offline paths expose violations and exact CAAT values here |
| Replay/minimize reason | `cat::Reasoner` after a failed result | converts derived CAT membership back to base literals without using a built-in checker |
| Reject proposed RF early | `GenMCDriver::findConsistentRf()` before `isExecutionValid()` in `genmc/genmc/Verification/GenMCDriver.cpp` | the tentative `ReadLabel::setRf()` choice is installed but alternatives are not yet queued/executed |
| Avoid queuing blocked RF alternatives | load handler immediately before adding `ReadForwardRevisit` entries | saves workqueue entries and later prefix clones, not just checker calls |
| Reject/avoid CO alternatives | `findConsistentCo()` and `calcCoOrderings()` | prevents forbidden placements from entering `WriteForwardRevisit` work |
| Avoid backward revisit subtrees | `calcRevisits()` before `constructBackwardRevisit()` | targets retained prefixes and OOM directly |
| Preserve stable keys across cuts | `GraphSynchronizer` / `StableGraphAdapter` | already maps graph mutations onto stable event identities and generations |
| Cross-worker sharing | worker-owned `BasicCATChecker` plus an immutable-snapshot kernel repository | avoids mutating evaluator state across workers; merges can be batched |

The existing generic CAT fallback currently keeps every revisit when no certified host
pruning theorem exists. The kernel path must therefore be an additional proved
sufficient-inconsistency filter; it must not re-enable host-checker pruning for an
uncertified CAT model.
