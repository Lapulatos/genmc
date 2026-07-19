# Notes: CAT/CAAT Optimization Sequence

## Authoritative Input
- Requested stages: attachment `pasted-text-1.txt`, items 1--11.
- Explicit exclusion: item 12, CAT model static compilation.
- Repository branch/HEAD at start: `genmc-caat` / `0171e637110cf6cea452ea523cd61ebc2355335a`.
- Pre-existing untracked paths: `.DS_Store`, `experiment-analysis/`.

## Existing Baseline Evidence
- SC/TSO fixed suite: GenMC 0.614 s, CAT 0.694 s, CAAT 2.056 s per repetition.
- SC flat-combiner medians: GenMC 0.090 s, CAT 0.165 s, CAAT 1.440 s.
- Process peak RSS around 50--52 MiB; no checker-specific difference resolved.
- Phase 3 broad counters: insert 128,801; rollback-insert 620,887; rebuild 364,023; offline initialization/rebuild 364,803.

## Code Findings Before Optimization
- `CATChecker::isConsistent` copies incremental `result.values` into `caatValues` even when `--explain-cat` is off.
- `IncrementalCaatEvaluator::tryInsert` reconstructs the immutable dependency adjacency for every update.
- `GraphAdapter` constructs pair primitives with an event-pair double loop on every consistency query.
- `tryInsert` copies all predicate values transactionally; checkpoints copy full base/result states; synchronizer history also retains base values.

## Stage 0 Baseline
- Main samples: 150; stats samples: 9.
- SC/recursive flat-combiner: CAAT mean 1.442 s, median 1.430 s, peak RSS median 51.938 MiB; CAT median 0.160 s.
- One profiled-equivalent run has 327 consistency queries, 176 rollback-insert transitions, 122 rebuilds and 4,489 operator evaluations.

## Stage 1: Avoid Unused Explanation-state Copy
- Implementation: copy incremental `result.values` only when `--explain-cat` is enabled.
- Correctness: recursive differential, mutation oracle, incremental-path and reasoner tests passed; 150 benchmark rows have zero semantic mismatch against stage 0.
- Result: no stable core improvement at this graph size. SC flat-combiner median 1.470 s vs 1.430 s baseline; all-CAAT suite mean 2.430 s vs 2.470 s baseline. Keep as a complexity-removing, semantically neutral optimization; do not claim measured speedup.

## Stage 2: Cache Dependency Adjacency
- Implementation: construct reverse predicate dependency adjacency once in `IncrementalCaatEvaluator`, reuse it in every `tryInsert`.
- Correctness: mutation oracle and insertion property tests passed; 150 rows have zero semantic mismatch.
- Result: SC flat-combiner median 1.410 s (1.4% faster than stage 0); all-CAAT suite mean 2.366 s (4.4% faster than stage 0). Five repetitions are enough for direction-finding, not a final significance claim.

## Stage 3: Opt-in Timing Attribution
- Timers execute only with `--cat-stats`; ordinary benchmark path has no clock calls.
- SC flat-combiner, 327 queries: GraphAdapter 58.3 ms; synchronize 1,264.6 ms.
- Synchronize breakdown (overlapping categories): stable materialization 182.1 ms; equality 0.7 ms; insertion attempts 171.2 ms; history path 840.6 ms; rebuild 60.0 ms. Evaluator subcomponents: worklist 461.0 ms; transactional copy/grow 8.3 ms; checkpoint copy 6.5 ms; rollback copy 3.8 ms.
- Decision: full checkpoint copying is not the first bottleneck for current small graphs. Stage 4 should target stable materialization and history/delta classification before undertaking Stage 7.

## Stage 4: Direct Stable-ID Graph Materialization
- Implementation: online CAAT now materializes `ExecutionGraph` directly into persistent stable IDs, eliminating the transient dense `GraphAdapter` plus remap pass. This is a full direct rebuild per query, not yet an event-edge delta API.
- Correctness: direct-vs-legacy base-value equality across insertion, rf replacement, co move and cut; mutation oracle and synchronizer tests passed; 150 rows had zero semantic mismatches.
- Result: SC flat-combiner median 1.14 s, 1.254x faster than stage 0. Stable materialization fell from 182.1 ms to 55.1 ms in the profiled run.

## Stage 5: Materialize Only Referenced Primitives
- Implementation: normalized base predicates select a fixed primitive-use mask; unreferenced sets/relations are not constructed. `fr` retains its implicit rf/co dependencies.
- Negative probe: calling `std::set<string>::contains` inside event-pair loops raised materialization from about 54 ms to 360 ms; this version was rejected.
- Corrected implementation: compute boolean use flags once before the loops. Tests and 150-row differential passed.
- Result: SC flat-combiner median 1.05 s (1.362x vs stage 0); mean CAAT suite 1.910 s; process peak RSS mean 51.975 MiB.

## Stage 6: Certified Candidate Pruning
- Implementation: generated SC/TSO coherent-store, revisit and co-placement pruning is used only when the complete deterministic normalized model exactly matches the bundled recursive SC/TSO semantic fingerprint. Renaming, reordering, changing an axiom, PSO, or any custom model fails closed to generic enumeration. `--cat-oracle` disables pruning so mutation stress still evaluates the generic candidate superset.
- Correctness: exact/fail-closed certificate unit test passed; SC, TSO, PSO, recursive differential and 5,396-query mutation oracle tests passed. Across 15 model/program cells, status and complete/blocked execution counts match stage 0/5.
- Result: SC flat-combiner median 0.17 s versus 1.05 s at stage 5 (6.18x marginal) and 1.43 s at stage 0 (8.41x cumulative). Mean CAAT suite 1.064 s (2.32x cumulative). Mean CAAT peak RSS 52.019 MiB versus 51.975 MiB at stage 5 (+0.044 MiB, unresolved process-level noise).
- Mechanism: profiled SC flat-combiner queries 327 -> 43; rollback-insert 176 -> 10; rebuild 122 -> 12; operator evaluations 4,489 -> 421; synchronization time 954.0 ms -> 73.7 ms.

## Stage 7: Delta-trail Checkpoints
- Implementation: checkpoints are O(1) trail markers. Each committed monotone insertion retains only added primitive/derived facts, evaluation-count deltas and prior violation metadata. Rollback removes facts in reverse and shrinks the event universe; forgotten prefixes are compacted.
- Correctness: nested checkpoint, stale-handle, push/pop property, synchronizer, recursive differential and mutation-oracle tests passed. All 150 performance rows matched execution counts.
- Time: CAAT suite mean 1.064 -> 0.970 s, but SC flat-combiner median 0.17 -> 0.18 s. Treat as time-neutral/noisy rather than a speedup. Rollback rose from about 5.1 ms to 21.7 ms because undo is now proportional to changed facts.
- Space: SC flat-combiner peak packed undo storage 368,744 B versus 733,280 B full-snapshot equivalent, a 49.7% reduction. Process peak RSS 52.019 -> 52.009 MiB cannot resolve this sub-MiB saving.

## Stage 8: Copy-on-write Packed Bitsets (Rejected)
- Probe: `EventSet`/`Relation` copies shared packed words and detached on mutation.
- Correctness: value algebra, insertion/rollback properties, recursive differential and mutation oracle passed.
- Local win: transactional copy 0.83 -> 0.58 ms (about 30%).
- Regression: CAAT suite mean 0.970 -> 1.050 s (+8.2%); SC flat-combiner median 0.18 -> 0.19 s. Atomic reference counting/detach overhead dominated the saved copies on these small relations. Stage 8 binary/data retained; code reverted before Stage 9.

## Stage 9: Semi-naive Union/Composition Propagation (Rejected)
- Probe: tracked per-operand insertion deltas and used exact delta rules for alias, union and composition; unsupported operators retained full recomputation.
- Correctness: evaluator properties, recursive differential and mutation oracle passed; 150 rows matched execution counts.
- Coverage: 365 of 421 operator evaluations in profiled SC flat-combiner used a semi-naive rule.
- Regression: CAAT suite mean 0.970 -> 1.220 s (+25.8%); core median remained 0.19 s. Delta allocation/merge overhead exceeded full-operation savings at current graph sizes. Stage 9 binary/data retained; code reverted before Stage 10.

## Stage 10: Restricted Support-aware Replacement
- Implementation: same-universe replacement is handled locally only for acyclic normalized models whose derived equations are alias/union. Recomputing union operands preserves a tuple until its last support disappears. Recursive SCCs, composition and closure fail closed to history/rebuild.
- Correctness: dedicated last-support test, graph rf/co replacement classification, properties, recursive differential and mutation oracle passed.
- Main-suite applicability: bundled recursive SC/TSO/PSO are deliberately ineligible, so no replacements occur there. Suite mean 0.984 s and core median 0.18 s are time-neutral relative to stage 7.

## Stage 11: Adaptive Online/Offline Backend
- First probe: persistent universe threshold 64 selected zero offline queries (`adaptive-offline=0`); stage 11 data is retained but is not an evaluation of backend switching.
- Stage 11b probe: threshold 512 selected offline epochs for 34 changed SC flat-combiner queries, but applying it to every model bypassed online-path coverage for custom models.
- Internal result: sync 96.9 -> 16.7 ms; 35 offline evaluations consumed about 11.3 ms; undo trail was unused for this small-graph run.
- Final stage 11c: adaptive offline requires the exact bundled SC/TSO candidate certificate and is disabled under `--cat-oracle`; PSO/custom models remain online. SC flat-combiner median 0.11 s versus 0.18 s at stage 10 and 1.43 s baseline (13.0x cumulative). CAAT suite 1.010 s, 2.45x cumulative; peak RSS 52.006 MiB; zero semantic mismatch.
