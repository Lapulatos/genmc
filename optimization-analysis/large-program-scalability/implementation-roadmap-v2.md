# Large-program scalability roadmap v2

Date: 2026-07-16

## Conclusion

The next optimization should target **dense relation materialization and fixed-point
state**, not checkpoint retention or another full-graph rejection cache. The current
CAAT path pays quadratic time and space before and during consistency evaluation:

- `StableGraphAdapter::materialize()` scans all active-event pairs to construct `po`,
  `loc`, `int`, and `ext`;
- every `Relation` owns `N * ceil(N/64)` 64-bit words regardless of edge count;
- `po`, `loc`, and `ext` are structural relations that can be queried from event
  metadata without storing all pairs;
- `rf` is functional per read, `co` is a per-location total order, and `fr` can be
  derived from those two relations on demand;
- the bundled recursive SC/TSO/PSO models consume `reach` only as a cycle property, but
  the generic evaluator materializes a full transitive relation.

This is the closest transferable part of Deagle's EOG/order-theory implementation. It
does not replace CAT/CAAT with a built-in GenMC checker. The CAT model is compiled only
when its normalized relation equations and final checks admit an exact sparse plan;
every other model continues through the generic evaluator.

## Measured bottleneck

The completed P0.4 experiment gives a concrete large completed task rather than a
theoretical estimate:

| Metric on `pthread/queue_ok_longer.yml` | Value |
|---|---:|
| maximum active/stable events | 8,033 / 8,033 |
| one dense `Relation` at this universe | 8,097,264 B (7.72 MiB) |
| maximum current base bytes | 72,879,408 B |
| maximum history base bytes | 91,170,576 B |
| peak checkpoint-equivalent evaluator bytes | 587,519,208 B |
| maximum base relation pairs | 37,531,996 |
| materialization time | 1.563 s |
| fixed-point worklist time | 21.841 s |
| synchronization time | 31.427 s |
| BenchExec peak RSS | 1,060,139,008 B |
| CPU time | 31.593 s |

The task uses one exploration thread. The measured bytes come from the current binary's
`--cat-stats` counters, and CPU/RSS come from the paired BenchExec XML.

P0.4 suppresses 1,231,328 preventive checkpoints across its formal matrix and reduces
summed internal maximum history/snapshot-equivalent bytes to 20.61% of baseline. Yet RSS
is 0.99992 [0.99949, 1.00030], CPU is 0.99528 [0.98831, 1.00201], and the hard cohort
gains no terminal result. This falsifies checkpoint history as the primary present OOM
cause. It does not mean history is free; it means denser, longer-lived relation values
dominate the process peak.

## What Deagle actually does differently

The inspected SV-COMP Deagle source archive is
`/data3/sujie/container/deagle-source/deagle-980fe9e.tar.gz`, corresponding to commit
`980fe9e6757e967d12454c7d16e2ec4de1a1cae4`.

Relevant implementation mechanisms are:

1. `Closure` and `ICD` store sparse per-node `out`/`in` edges and inactive candidate
   edges indexed by endpoints, rather than one dense matrix per relation.
2. They keep decision-level edge trails and roll back only activated edges.
3. RF and write-serialization activation derives only the newly implied FR edges.
4. `SegmentSolver` collapses intra-thread program order into chains, stores only cross
   edges, and queries first successors/last predecessors per chain.
5. `need_add_edge()` avoids storing an edge already implied by the chain/EOG.
6. Reasons are pooled and attached to activated edges; conflict clauses are built from
   the actual cycle instead of snapshotting every relation.
7. Preventive propagation is integrated at the decision boundary.

These observations agree with the ordering-theory papers: use compact program-order
structure, add only relevant interference/order edges, propagate consequences at edge
activation, and extract a small conflict reason when a cycle is created.

## Transferable literature mechanisms

### Yin et al.: EOG and refinement

[Scheduling-constraint abstraction](https://doi.org/10.1109/TSE.2018.2864122) avoids an
exact scheduling formula initially, validates abstract counterexamples on an EOG, and
adds the negation of small cycle/kernel reasons. A constraint fallback provides
soundness and completeness relative to the loop bound.

For GenMC, the immediate transfer is not a whole BMC replacement. It is:

- represent one exact concrete execution graph as a compact EOG;
- attach base-choice reasons to derived order edges;
- make a failed cycle produce a canonical sufficient base-choice kernel;
- eventually share only compact, replayable kernels across nearby revisits/workers.

[Parallel refinement](https://doi.org/10.1109/ICSE.2019.00074) shares refinement
constraints and selected learned clauses between asynchronous engines. Its important
lesson for `--nthreads` is that useful parallel workers must exchange compact knowledge.
Duplicating concrete exploration without knowledge sharing does not reduce total work.
The paper also warns that sharing too many learned clauses hurts performance; its
adaptive policy favors short clauses.

[Incremental refinement adaptation](https://doi.org/10.1145/3728976) reuses constraints
through kernel sources. Within one GenMC run, stable event keys and transition provenance
can play the same role, but only after blocker lookup becomes cheaper than P0.2's full
snapshot/cache path.

### He et al.: ordering theory and sparse propagation

[Ordering consistency theory](https://doi.org/10.1145/3453483.3454108) and its
[SC/TSO/PSO extension](https://doi.org/10.1145/3579835) represent executions as event
orders and integrate theory propagation, cycle consistency, and conflict reasons into
the solver. They avoid forcing every partial order into unnecessary numeric timestamps.

[Interference-guided solving](https://doi.org/10.1145/3503221.3508424) prioritizes
cross-thread interference decisions. This transfers safely as RF/CO candidate ordering;
it cannot by itself justify dropping a candidate.

[Consistency-preserving propagation](https://doi.org/10.1145/3563321) and the newer
[generalized preventive reasoning](https://doi.org/10.1007/978-3-032-26204-2_26) reject
some choices before a full inconsistent graph is formed. P0.3 already confirms this
mechanism in GenMC: it prunes 78.56% of offered candidates and recovers five formal
TIMEOUT cells per repetition. The remaining opportunity is to execute those tests on a
sparse EOG without first materializing dense structural relations.

### Cai et al.: dependency and pointer-flow guidance

[SeqCheck](https://doi.org/10.1145/3468264.3468549) computes a necessary feasible event
set and a trace-closed partial order. [Pointer-flow reordering](https://doi.org/10.1145/3597503.3623300)
uses a lightweight dynamic LLVM slice to expose interference hidden by changing pointer
values. [Necessary consistent reads](https://doi.org/10.1109/ICSE55347.2025.00149) uses
SSA def-use, control, critical-section, and address dependencies to remove unnecessary
trace constraints while preserving sound bug reports.

These are sound bug-prediction techniques, not complete safety exploration. Their safe
near-term use is therefore:

- prioritize RF/CO choices whose reads influence assertions, branches, lock operations,
  or later addresses;
- choose diverse worker seeds based on different interference/dependency slices;
- replay every reported bug concretely;
- do not prune safety exploration based only on the dynamic slice.

A future CEGAR layer may use these dependencies to choose an initial abstraction, but a
safe result requires exact counterexample validation and an abstraction-fixpoint proof.

## Prioritized implementation plan

### P0.5: per-relation and per-predicate storage census

Implement diagnostics only. Record, per query and per primitive/predicate:

- universe size, pair count, packed bytes, density, and growth-copy bytes;
- construction time for `po/rf/co/fr/rmw/loc/int/ext/tc/tj`;
- fixed-point value bytes and worklist evaluations by normalized predicate;
- bytes retained in current state, undo trail, history, and temporary transactions;
- the first allocation size and predicate immediately preceding OOM where observable.

Opportunity gate:

- structural base relations or final reachability must account for at least 30% of
  relation bytes on completed large tasks; and
- projected sparse/structural storage must reduce at least 2x on tasks with `N >= 512`.

This census is necessary because the current `max-base-relation-density-ppm` reports only
the densest base relation and hides which relation dominates bytes/time.

### P0.6: structural relation views

Replace only certified structural operands:

| Relation | Exact compact representation | Avoided work |
|---|---|---|
| `po` | thread ID + program index / thread chains | pairwise scan and dense matrix |
| `int` / `ext` | thread-ID equality/inequality filter | two dense complements |
| `loc` | location ID equality / per-location buckets | pairwise scan and dense matrix |
| `rf` | one source ID per read + sparse reverse lists | dense mostly-empty matrix |
| `co` | ordered write vector/rank per location | dense transitive write pairs |
| `fr` | iterate writes after `rf(read)` in the location order | dense composition result |

The generic `Relation` remains available. Operators gain row/pair iterators over either
a dense value or a structural view. Equality/oracle tests compare the view's extensional
contents with the existing dense result.

Expected effects:

- materialization: `O(N^2)` pair classification becomes `O(N + explicit edges)`;
- base space: several `O(N^2/8)` matrices become `O(N + locations + edges)` metadata;
- insertion: adding one event updates one thread chain/location bucket instead of growing
  and repacking every dense base relation;
- transaction/checkpoint copies become smaller because base snapshots refer to immutable
  metadata plus deltas.

Correctness boundary: a view is an exact alternative representation, not an
approximation. Every operation must pass randomized extensional comparison against the
dense oracle before entering the graph path.

### P0.7: certified sparse EOG cycle plan

Compile a normalized CAT model to this plan only if analysis proves:

1. a positive `order` relation is built from exact structural/filter/union/composition
   operands;
2. `reach = order | reach ; order` (or an equivalent positive closure) has no consumer
   other than `irreflexive`/`acyclic` checks and preventive reachability queries;
3. all other checks, such as atomicity/coherence, have their own exact sparse plan or
   remain evaluated by the generic oracle;
4. the certificate includes check kind, relation equation, host primitives, and model
   fingerprint.

Maintain sparse adjacency, a rollback trail, and an incremental topological/SCC state.
Reject an inserted edge when it closes a cycle and recover the exact base-edge reason.
Do not materialize transitive `reach` unless another CAT expression requests its pairs.

Expected effects:

- final consistency space becomes `O(N + E)` instead of storing dense `order` and
  `reach` relations;
- cycle checks inspect only the affected topological region after one RF/CO change;
- rollback removes edge deltas rather than restoring relation snapshots;
- preventive P0.3 queries reuse the same reachability state.

This plan is a compiled implementation of the CAT equation. It is not a call to GenMC's
built-in SC/TSO checker.

### P1.1: compact exact blocker sharing

Revisit kernel sharing only after P0.6/P0.7. P0.2 failed because cache lookup and full
snapshot materialization outweighed saved evaluation and increased RSS. A new blocker
must be generated directly from a sparse cycle reason and indexed by its newest RF/CO
decision. Share only short exact blockers (initially at most four base literals), use a
bounded lock-free/read-mostly table, and fall back on every miss.

Expected effects:

- fewer candidate queries and revisits across executions/workers;
- fewer execution graphs retained by the exploration layer;
- useful scaling from `--nthreads` because workers share deductions rather than only CPU.

Stop if hit validation still needs a full snapshot or if blocker bytes grow with complete
executions.

### P1.2: dependency-guided complete exploration order

Compute an LLVM SSA backward slice from assertions, branch conditions, locks, and memory
addresses. Rank RF/CO candidates and worker partitions by cross-thread reads in this
slice. All candidates remain enumerable, so completeness is unchanged.

Expected effect: shorter time to a concrete bug on unsafe tasks and more diverse parallel
search. Expected non-effect: it will not solve safe-task OOM because it does not reduce
the complete search space.

### P2: scheduling-constraint CEGAR above concrete GenMC

For execution-count explosion, introduce an over-approximate symbolic scheduling layer:

1. represent families of RF/order choices symbolically;
2. validate an abstract counterexample with the exact GenMC+CAT/CAAT path;
3. refine with a sufficient EOG cycle/program-feasibility kernel;
4. report unsafe only after concrete replay;
5. report bounded safe only when the over-approximation is exhausted.

This is the architectural step capable of approaching Deagle's large-program coverage.
It addresses families of executions; P0.6/P0.7 only make one graph cheaper. It should not
start until the sparse exact EOG and reason certificates are stable.

## What should not be done next

- Do not add another unindexed full-snapshot kernel cache; P0.2 already falsified it.
- Do not remove more checkpoint classes merely because internal bytes fall; P0.4 showed
  no RSS or coverage gain.
- Do not use dependency slices to prune a safe proof without CEGAR validation.
- Do not increase `--nthreads` alone; duplicated concrete work and per-worker dense state
  can accelerate OOM.
- Do not delegate CAT/CAAT consistency to a built-in SC/TSO checker. Structural/EOG plans
  require model-derived certificates and generic-oracle differential tests.

## Validation contract

Every production stage must pass:

1. Release unit/property tests and randomized dense-vs-compact relation operations;
2. the 39-row mutation suite with every online state compared to the Phase 2 oracle;
3. 864 SC/TSO/PSO broad pairs with zero verdict and safe-execution mismatch;
4. four balanced repetitions on the 96-task sample;
5. the adapted 60-task TIMEOUT/OOM cohort when the mechanism gate passes;
6. per-task CPU/wall/RSS confidence intervals plus event/edge/value byte counters;
7. fail-closed model certification tests, including near-miss CAT models.

Retention target for P0.6/P0.7: no new wrong/OOM result, at least 30% lower peak CAT
relation bytes on `N >= 512`, and either CPU upper 95% CI below one or at least 10%
fewer TIMEOUT+OOM cells on the targeted large-event cohort.

## Research Question Card

Question: Can a CAT-certified structural/EOG representation remove quadratic relation
storage while preserving exact GenMC exploration and generic CAT fallback?

Type: applied and confirmatory.

Hypothesis: Most large-task CAT memory and worklist time comes from materializing
structural relations and transitive reach as dense matrices. Exact metadata views plus a
certified sparse cycle plan will reduce consistency-state bytes by at least 30% on
`N >= 512` and convert some current OOM/TIMEOUT tasks without changing explored safe
executions.

Current evidence: the 8,033-event measured task, P0.4 falsification of history as the
dominant peak, current source complexity, Deagle's sparse/segmented implementation, and
the EOG/ordering-theory papers.

Missing evidence: bytes/time by relation and normalized predicate, especially on tasks
that are killed before final statistics.

What would support it: the P0.5 census passes its 30% and 2x opportunity gates, followed
by dense-oracle exactness and retained end-to-end gains.

What would falsify it: derived relations remain dense enough that compact bases do not
reduce peak RSS, or conversion/iterator overhead makes completed-task CPU worse without
recovering resource failures.

Minimal next action: implement P0.5 diagnostics only; do not alter relation semantics or
allocation policy until the per-relation census is complete.

Decision: run experiment.

## Evidence records

### ER-20260716-p04-history-01

Source type: experiment artifact. Source:
`optimization-analysis/continuous/preventive-history/analysis-no-retain/` and
`analysis-hard/`.

Supports: checkpoint history can fall sharply without reducing process RSS or hard-task
resource failures.

Limitation: recursive PSO sample and 4 GB/60 s hard cohort; it does not measure killed
process internals after the last emitted statistics line.

Claim strength: strong for rejecting P0.4, supported for locating the next bottleneck.

### ER-20260716-large-event-01

Source type: experiment artifact. Source: `queue_ok_longer` log/XML in P0.4 formal r01.

Supports: at 8,033 events, dense CAAT state reaches hundreds of MiB and worklist time
dominates one completed 31.6-second task.

Limitation: one concrete task; P0.5 must establish distribution across relations/tasks.

Claim strength: observed.

### ER-20260716-deagle-source-01

Source type: full source artifact. Source: Deagle commit
`980fe9e6757e967d12454c7d16e2ec4de1a1cae4`, especially `Closure`, `ICD`, and
`SegmentSolver`.

Supports: sparse edge activation, thread-chain compression, on-demand FR, rollback
trails, pooled reasons, and preventive propagation are implemented together in the
comparison tool.

Limitation: Deagle is an SMT/BMC architecture; data structures cannot be copied into
GenMC without a CAT model certificate and oracle validation. License review is required
before copying any code. This roadmap transfers algorithms, not source text.

Claim strength: supported.
