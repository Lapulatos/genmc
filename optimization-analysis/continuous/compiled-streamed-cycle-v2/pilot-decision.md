# P0.7g2 minimal ordered stream program: pilot decision

Date: 2026-07-17  
Baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe`  
Decision: **advance to the frozen formal matrix; this is not yet a retain decision**

## Correctness and layout evidence

- Release unit/property tests: 161/161 passed.
- Focused GCC 13 ASan+UBSan tests: 22/22 passed.
- Mutation oracle: 39 rows / 5,441 full recomputations, zero mismatch.
- Broad differential: 852 comparable matches / 12 mutual unsupported / zero mismatch
  over 864 SC/TSO/PSO pairs.
- `ModelAnalysis`, fixed-point result/statistics, incremental evaluator/statistics and
  lazy statistics all retain exact baseline sizes.
- Candidate source hashes match the source used by the server Release binary.

## Six-repetition pilot

All 36 paired cells completed. Status, category, complete-execution count, lazy-check
count, emitted-candidate count, snapshot-equivalent bytes and maximum-current-base bytes
have zero mismatch. There is no coverage loss.

| Model/task | CPU median | RSS median | Candidate max / baseline max RSS |
|---|---:|---:|---:|
| PSO fib | 0.999983 | 1.000471 | 0.999530 |
| PSO queue | 0.918100 | 1.013865 | 0.994650 |
| SC fib | 0.994848 | 1.000792 | 1.001266 |
| SC queue | 0.993701 | 1.000181 | 0.999948 |
| TSO fib | 0.942600 | 1.000629 | 1.000000 |
| TSO queue | 0.923781 | 0.980675 | 0.986145 |

Every task satisfies CPU median <=1.03, RSS median <=1.02 and maximum RSS ratio <=1.02.
Multiple representatives above one baseline CPU second satisfy the <=0.98 speed witness.
The candidate therefore advances under the protocol frozen before implementation.

P0.7g remains rejected. P0.7g2's pass is attributable to its independently measured
representation: no extra lifetime counters, no extra analysis vector and unchanged
public object layouts. It does not retroactively relax the earlier gate.

## Next gate

Run four repetitions of 96 tasks x SC/TSO/PSO x before/after = 2,304 cells with 48
BenchExec workers. Retention still requires the aggregate four-repetition model-task
CPU bootstrap CI upper bound below 1.0, every model ratio <=1.01, zero correctness or
coverage loss and all formal RSS/state gates.
