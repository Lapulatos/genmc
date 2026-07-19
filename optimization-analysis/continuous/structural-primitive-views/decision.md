# Optimization 14 / P0.6b decision

## Decision

Do not retain the exact structural primitive views as a standalone optimization. Keep
the uncommitted prototype only as the experimental base for P0.6c, which must compress
additional sparse primitives and pass a new complete gate before any source is committed.

## Correctness evidence

- Server Release unit/property suite: 148/148 passed.
- Mutation oracle: 39 rows and 5,441 complete offline recomputations, zero mismatch.
- Broad differential: 288 programs, 864 SC/TSO/PSO pairs, 852 matches, 12 mutually
  unsupported pairs, and zero mismatch.
- Formal matrix: 1,152 before plus 1,152 after cells, all 24 run sets exited zero.
- Formal comparison: zero opposite terminal verdict, zero common-terminal execution-count
  mismatch, and identical status counts (60 OOM, 40 TIMEOUT, 340 false, 712 true).

## Performance evidence

- Common-terminal CPU after/before: 1.00337, task-bootstrap 95% CI
  [0.99762, 1.00946].
- Common-terminal wall time: 0.99955 [0.99381, 1.00547].
- Completed tasks with at least 512 stable events:
  - process RSS: 0.87544 [0.86020, 0.88975];
  - materialization time: 0.91451 [0.89059, 0.93906];
  - current/history base bytes: 0.70217 [0.66986, 0.73603].
- No terminal status changed, so the alternate resource-terminal gate does not apply.

## Why the retention gate failed

The frozen gate requires at least a 2x reduction in total current base bytes together
with at least 10% RSS reduction, or a repeatable new terminal result. RSS improves by
12.5%, but total base bytes improve by only 29.8% (1.42x), and no terminal result changes.

At 8,033 events, PSO `queue_ok_longer` falls from 72,879,408 to 48,780,408 base bytes.
The 24,099,000-byte saving is exactly the removal of three dense matrices minus their
linear structural keys. The remaining dense `rf/co/fr/rmw/tc/tj` family prevents a 2x
total-base reduction. P0.6c must use an exact size-adaptive sparse representation for
those edge relations; lowering the gate or merely changing the 512-event threshold is
not justified.

## Evidence paths

- Strict analysis: `analysis/comparison.json`, `analysis/rows.tsv`
- Full XML and log archives: `server-results/formal/`
- Unit, mutation, and broad gates: `server-results/unit-after-v7.log`,
  `server-results/mutation-v7.log`, `server-results/broad-v7/`
- Excluded pilots and smoke diagnostics: `server-results/formal-pilot-*`,
  `server-results/v*-smoke-*.log`
