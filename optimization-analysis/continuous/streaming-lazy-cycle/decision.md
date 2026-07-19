# P0.7d streaming lazy-cycle decision

Date: 2026-07-16  
Baseline: `959f8697cfd3b20d47a4ca9d925e5e7e8ffdc508`  
Decision: **retain P0.7d2; reject P0.7d1**

## What changed

The retained P0.7b evaluator materializes no derived value for analyzer-certified
composition-heavy `acyclic` roots, but it still collected, deduplicated and sorted a
successor vector for every active DFS source. P0.7d streams exact extensional derivations
directly into the three-colour cycle DFS and makes every expression emitter
interruptible. A detected cycle therefore stops the remaining union, intersection and
composition enumeration immediately.

Duplicate derivations are deliberately not deduplicated on the streaming path. Parallel
copies cannot change directed-cycle existence; white, grey and black target colours
retain exact DFS semantics. The reported witness is deterministic and every consecutive
edge is checked against the independently materialized root relation in property tests.
Explanation, oracle and preventive modes retain their existing fail-closed materialized
path.

`lazy-unique-edges` is replaced by `lazy-depth-fallbacks`. Exact unique-edge counting
would require the generation-mark state that streaming removes; `lazy-edge-candidates`
continues to count every derivation actually consumed and therefore remains an exact
work counter.

## P0.7d1 rejection

P0.7d1 nested recursive event DFS inside expression callbacks without an event-depth
bound. Although its pilot reduced CPU and RSS, the full matrix changed 20 retained TSO
TIMEOUT cells into deterministic segmentation faults: five Goblint race-reach tasks in
all four repetitions, each after about 9.2 seconds and 1.057 GB RSS. The native call
stack grew with the event path. This is a correctness/runtime regression, so P0.7d1 is
rejected regardless of its aggregate cost ratios.

## P0.7d2 safety refinement

P0.7d2 streams below a 2,048-event recursive depth. Reaching the bound produces no
verdict: it abandons the partial attempt and reruns the whole exact root check with an
iterative heap-frame DFS. Each fallback frame owns the exact sorted and deduplicated
successor row used by P0.7b. Thus the fallback may recover `O(E_path)` transient heap
state, but event-path depth never consumes the native stack and persistent evaluator,
checkpoint and undo state remains unchanged.

An 8,193-event chain with a closing back edge completes under ASan+UBSan, returns an
edge-valid witness of 8,194 vertices and records exactly one depth fallback. The five
former crash tasks all reach the 15-second BenchExec TIMEOUT without a signal.

## Correctness evidence

- Release unit/property tests: 158/158.
- Focused ASan+UBSan tests: 4/4, including the 8,193-event fallback.
- Mutation/oracle comparison: 39 rows and 5,441 full-offline checks, zero mismatch.
- Broad differential: 288 programs × SC/TSO/PSO; 852 comparable matches, 12 mutual
  unsupported pairs and zero mismatch.
- Formal matrix: 24 XML files and 24 log archives, 1,152 baseline plus 1,152 candidate
  cells. All runsets exit zero; there are zero status changes, opposite terminal
  verdicts and common-terminal execution-count mismatches.
- Formal status counts are identical: 716 true, 360 false and 76 TIMEOUT per variant.

## Frozen performance gate

| Gate | Required | Observed | Result |
|---|---:|---:|---|
| aggregate CPU CI upper | <= 1.01 | 0.99306 | pass |
| aggregate CPU point benefit | <= 0.995 | 0.98593 | pass |
| TSO CPU point | <= 1.02 | 0.97871 | pass |
| PSO CPU point | <= 1.02 | 0.97746 | pass |
| large process RSS | <= 1.02 | 0.97676 | pass |
| peak snapshot-equivalent state | = 1.00 | 1.00 | pass |
| new errors/OOM/lost correct | none | none | pass |

Aggregate CPU improves by about 1.41%, with task-bootstrap 95% CI
`[0.97878, 0.99306]`. TSO and PSO improve by about 2.13% and 2.25%. SC lazy activation
is exactly zero and SC CPU is 1.00044, as required by the structural selector rather
than a model-name branch.

The candidate consumes 5,694,936,384 lazy derivations versus the retained P0.7b2
6,180,631,972, a 7.86% reduction. This does not pass the alternative 15% candidate
threshold, but retention is independently justified by the predeclared aggregate CPU
criterion. There are 7,034,520 lazy checks and 80 exact depth fallbacks: 16 on the four
TSO `queue_ok_longest` profiles and 64 on the four PSO profiles. No fallback activates
under SC.

## Time and space effects by processing stage

1. **Root-edge production:** unchanged on safe acyclic portions except that the emitter
   no longer writes a generation-mark table or appends to a successor vector.
2. **Cycle discovery:** a grey target interrupts all remaining derivations of the active
   source and its enclosing expression callbacks. This accounts for the measured 7.86%
   candidate reduction.
3. **Successor processing:** the normal path removes per-source deduplication and sort.
   Duplicate derivations now pay only a colour test.
4. **Deep paths:** after depth 2,048 the abandoned prefix adds work, then the exact
   fallback reconstructs sorted successor rows on the heap. This is intentionally a
   safety cost, not a speed claim.
5. **Persistent state and rollback:** derived values remain elided exactly as in P0.7b;
   snapshot-equivalent state is 1.00 relative to the baseline in every measured model.

## Limits and next target

- Large internal offline time is 1.04474 and large copy time is 1.04149; neither is an
  improvement claim. Whole-process CPU, not these noisy internal subsets, satisfies the
  frozen gate.
- The former crash cohort still times out and retains about 1.057 GB RSS because native
  stack pages touched before the 2,048 limit remain resident. P0.7d2 prevents the crash;
  it does not make those tasks scalable.
- Safe graphs still regenerate composition macro edges, leaving 5.695 billion candidate
  derivations. The next optimization must avoid repeated macro-edge enumeration with an
  exact product-state/EOG traversal or reusable rollback-safe reachability state, rather
  than merely changing the final DFS container.
- The later 725-task corrected-census comparison is cumulative evidence across all
  intervening optimizations, not an isolated P0.7d2 effect. It improves previous CAAT-SC
  correct coverage 388→398 and paired CPU to 0.915, but cannot be attributed solely to
  this change.

## Artifacts

- Protocols: `experiment-protocol.md`, `depth-bounded-protocol.md`.
- Formal raw results: `server-results/formal-v2/`.
- Formal analyses: `server-results/analysis-v2/comparison.json` and `extra.json`.
- Correctness logs: `unit-after-v2.log`, `sanitizer-tests-v2.log`,
  `mutation-after-v2.log`, `broad-after-v2.log`.
- Former crash cohort: `crash-cohort-v2/`.
