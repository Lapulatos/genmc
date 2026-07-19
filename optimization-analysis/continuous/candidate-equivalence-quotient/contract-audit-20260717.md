# SC-RVF Frozen-Contract Audit (2026-07-17)

## Decision

**Request generated-oracle evidence before server performance experiments.** The current
worktree is an executable SC-RVF prototype with useful bounded differential evidence,
but it does not yet establish the frozen completeness contract. The production path uses
an always-backtrack task decomposition rather than the paper's optional ancestor-signal
pruning. This is conservative, but no exhaustive generated test currently shows that the
native/RVF mixed path represents every future-write class under that decomposition.

The provisional `SmallVector` raw-prefilter micro-optimization has been removed. It did
not change candidate-space counters and is not part of the primary research objective.

## Authoritative baseline

- Git baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe`.
- Frozen design: `design.md`.
- Frozen experiment contract: `experiment-protocol.md`.
- All SC-RVF source remains uncommitted and can be reviewed or removed independently of
  the retained CAT/CAAT baseline.

## Requirement-by-requirement audit

| Frozen requirement | Current evidence | Status |
|---|---|---|
| Reduce RF/CO/revisit candidate exploration, not only evaluator cost | Bounded enabled cohort changes complete executions 1,308 to 1,283; detailed RVF counters exist | Partial: full-workload offered/queued/popped/realized evidence is missing |
| SC-only, local-safety, structurally certified recursive-SC model | `Config::validate` checks the SC certificate, verification mode, no bounds/liveness, and no preventive-pruning composition | Satisfied for declared scope |
| Whole `GoodW(read)` sets grouped by value and provenance | Driver constructs one `GoodW` set per viable `(value, provenance)` group | Implemented, bounded tests only |
| Independent SC realizability with structural state equality | `SCGoodWritesSolver` and exhaustive four-event oracle tests | Implemented and unit-tested |
| Exact `VisibleW_PO` | Independent state helper and unit tests; driver intersects it with native RF approximation | Implemented, integration proof incomplete |
| Per-read/per-thread causal map | Stable event-key cutoffs are stored in `Frame` and applied to viable writes | Implemented, exhaustive integration proof missing |
| Ancestor/backtrack signal when a future write creates a new source | `AncestorSignals` exists only in its unit test. Production instead always submits a parent continuation, conservatively behaving as if the signal were true | Not a completeness blocker; optional pruning is absent and the standalone signal code is unused |
| Interpreter replay of each witness | A witness RF map is applied to a cloned graph, submitted to `ThreadPool`, and the normal interpreter runs graph-driven replay from that task | Implemented; needs generated replay-oracle coverage |
| Recursive-SC CAT/CAAT remains authoritative | Derived state is rebuilt and `getConsChecker().isConsistent(childRead)` checks the whole graph before submission | Implemented; error/warning equivalence still needs server broad oracle |
| Mark a class covered only after successful realization and CAT acceptance | Representatives are submitted only after realizability, graph application, view rebuild, and CAT acceptance | Operationally ordered, but there is no explicit covered-class structure to audit |
| Unsupported operation fails open without losing prior work | Per-load fallback keeps the current execution worklist; static whole-program gates preserve the untouched native root | Partial: whole-program fallback is safe but narrower than the frozen subtree contract; fallback after earlier quotienting cannot restore ancestor classes |
| Every bounded maximal SC execution has a representative | Finite outcomes agree on six hand-selected reduced structures | **Unproven: no exhaustive generated program/state oracle for the task decomposition** |
| All traces in a class reach the same per-thread local state | Same value/provenance and graph-driven replay are present | **Unproven beyond finite outcome probes** |
| Parallel class ownership is shared or provably disjoint | Tasks are submitted to a global pool and n1/n2 counts agree on local fixtures | **Unproven: no ownership/partition invariant or exhaustive parallel oracle** |

## Process audit

The local 185-case matrix is a stabilization result, not a substitute for the frozen
protocol. It omits the V9/instrumentation control split, unchanged TSO/PSO controls,
server mutation oracle, 864-pair broad differential, 48-worker adapted workload, hard
TIMEOUT/OOM cohort, CPU/RSS, and the primary offered/queued/popped/realized counters.

The first one-shape generated-oracle smoke is invalid harness evidence and excluded. It
stored thread observations in non-atomic globals, forcing native fallback, and compared
RVF/n2 against baseline/n1. The corrected harness uses SC atomic observation slots and
same-worker baseline/RVF pairs.

## Required repairs before server execution

1. Document and test the production recursion mapping: representative tasks are recursive
   children; parent-continuation tasks are subsequent loop iterations; clearing a child
   frame's processed reads re-enables ancestor reads on the enlarged event set. Keep the
   always-backtrack variant until an independently tested signal optimization exists.
2. Add an exhaustive generated small-program oracle that compares reachable per-thread
   local states, not only verdict and complete-execution totals. Include future writes
   that become visible after a read frontier and nested read frontiers.
3. Prove or test that the native/RVF mixed path cannot leave stale `GoodW`, causal-map,
   or processed-read state across forward/backward revisits.
4. State and test the task-partition invariant for one and multiple workers.
5. Keep whole-program gates as explicit temporary scope restrictions; do not count
   fallback cases as RVF coverage.
6. Restore the full unit suite to green before synchronization. The config test now
   reflects the intentional rule that RVF-specific option overrides occur only after
   transformed-program certification, preserving exact native fallback.

## Server restart gate

Only after items 1--4 have implementation and generated-oracle evidence should server
performance work resume. Server correctness may be used to execute that oracle. Use
unique container names, an outer hard timeout for every invocation, and no unbounded
interactive `genmc` process. Run correctness in this order:

1. Release full unit/property tests;
2. GCC 13 ASan+UBSan full and focused tests;
3. generated exhaustive state oracle;
4. mutation oracle;
5. 864-pair SC/TSO/PSO broad differential;
6. only then the V9/RVF/instrumentation-control/TSO/PSO formal matrix and hard cohort.

## Generated-state oracle result after flow restoration

The corrected one-worker oracle passes one canonical shape (81 states), but the expanded
four-shape run fails two of 324 state cells:

- `shape=000001/outcome=1112`: native reports the safety violation; RVF reports none.
- `shape=000011/outcome=1012`: native reports the safety violation; RVF reports none.

The failures reproduce after applying the VerifySC prefix witness's per-location write
order to the cloned graph. That attempted repair passed 175/175 unit tests but did not
change either missing state, so it was removed. The evidence is preserved under
`local-results/generated-state-oracle-smoke-4shape-cofix-n1-20260717a/`.

The stronger diagnosis is that the online prefix quotient does not yet demonstrate the
full RVF-SMC recursion invariant for writes and reads that occur after the quotient
frontier. A witness for the current prefix cannot by itself represent future coherence
placements. This blocks server performance experiments: optimizing or benchmarking the
prototype before repairing the state-space loss would measure an incomplete checker.

## Correctness repair and restored gates

The two losses arise when one value group merges a read's same-thread source with an
other-thread source. In the GenMC graph integration those sources can constrain later
coherence placements differently, even though they write the same value. The repaired
class key is `(value, provenance, same-thread-source)`, matching the conservative
own/non-own distinction used by value-centric equivalence. VerifySC's representative
also applies its per-location write order to the cloned graph before RF readers are
attached; omitting that step caused late model fail-open in 162 of the four-shape cells.

Confirmed evidence after both repairs:

- local unit/property tests: 175/175;
- local generated oracle: 20 shapes, 1,620 states, 3,240 calls, 0 violations;
- server Release focused tests: 13/13;
- server generated oracle: 20 shapes × n1/n2, 6,480 calls, 0 timeout,
  0 late fail-open, 0 violations, and 1,620 reduced RVF cells;
- server GCC 13 ASan+UBSan: focused 13/13 and four-shape oracle 648 calls with
  0 violations;
- server 185-program differential: 740 rows, 0 timeout, 0 invariant violation;
  the enabled cohort retains the 1,308→1,283 complete-execution reduction and
  17 reduced loads.

This clears the generated-state blocker for the tested finite family. It does not by
itself prove the full algorithm for arbitrary programs, so mutation and broad corpus
gates remain required before formal timing.
