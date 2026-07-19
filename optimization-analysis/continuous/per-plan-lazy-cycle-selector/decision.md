# P0.7c per-plan composition selector decision

Date: 2026-07-16
Baseline: `959f8697cfd3b20d47a4ca9d925e5e7e8ffdc508`
Decision: reject and restore production/test source

## Correctness

- Release unit/property tests: 157/157 passed.
- Mutation oracle: 39 rows and 5,441 full comparisons, zero mismatch.
- Broad differential: 864 SC/TSO/PSO pairs, 852 comparable matches, 12 mutually
  unsupported, zero mismatch.
- Formal matrix: 1,152 before and 1,152 after cells, zero opposite terminal verdict,
  zero common-terminal execution-count mismatch, no status changes, and no new OOM.
- Focused activation: SC remains at zero lazy checks; TSO/PSO change from two to one
  lazy checks per offline evaluation.

## Measured effect

The four-repetition 2,304-cell matrix completed all 12 paired runsets with exit zero.

| Metric | After / before | Frozen bound | Result |
|---|---:|---:|---|
| Aggregate common-terminal CPU | 0.99326 [0.98708, 0.99937] | CI upper <= 1.01 | pass |
| TSO CPU | 0.99158 | <= 1.02 | pass |
| PSO CPU | 0.99252 | <= 1.02 | pass |
| Large-event offline time | 0.81017 | benefit <= 0.97 | pass |
| Large-event process RSS | 1.11401 [1.02215, 1.22390] | <= 1.02 | **fail** |
| Large-event peak snapshot bytes | 1.20292 [1.04333, 1.39818] | <= 1.02 | **fail** |

Lazy cycle checks fall from 7,034,520 to 3,517,260. Candidate root edges fall from
6.181 billion to 4.116 billion (33.4%), satisfying the work-reduction hypothesis.
However, TSO large RSS/snapshot rise to 1.24384/1.59045 and PSO rise to
1.45558/1.73694. Large copy time rises to 2.56352 for TSO and 2.09731 for PSO.

## Stage explanation

The per-plan selector removes colour/parent/seen allocation, DFS, successor
deduplication, sorting, and edge enumeration for the union-only coherence check. This
reduces offline CPU and root candidates.

The generic fallback must instead materialize and retain the complete coherence union
and intersection relations. Those derived values enter incremental state, undo/checkpoint
accounting, and copies. On large TSO/PSO graphs, that persistent state is substantially
larger than the transient lazy traversal. The change therefore conflicts with the main
goal of avoiding OOM on larger programs even though it is slightly faster.

## Consequence

Restore the two production/test files exactly to baseline and do not commit P0.7c. Keep
P0.7b's model-global selector: within a composition-heavy model, lazily evaluating the
simple coherence cone is a deliberate space optimization. The next candidate must reduce
macro-edge enumeration without rematerializing that cone, for example by exact
expression/product-state EOG traversal.

## Artifacts

- Raw XML/logs: `server-results/formal/`
- Core analysis: `server-results/analysis/comparison.json`
- Lazy metrics: `server-results/analysis/extra.json`
- Pilot: `pilot/`
- Correctness logs: `server-results/{unit,mutation,broad}-after.*`

