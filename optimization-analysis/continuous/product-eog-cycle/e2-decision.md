# P0.7e2 decision: reject

P0.7e2 preserves the exact P0.7e1 automaton but replaces native recursive product DFS
with resumable heap frames and direct base-relation cursors.

## Correctness

- Release: 161/161 tests.
- ASan+UBSan: 7/7 focused tests.
- Mutation oracle: 39 rows, 5,441 full recomputations, zero mismatch.
- Broad differential: 288 programs × SC/TSO/PSO; 852 matches, 12 mutually
  unsupported, zero mismatch.

## Two-repetition pilot

| Cell | CPU after/before | RSS after/before |
|---|---:|---:|
| TSO fib | 1.2778 / 1.2802 | 1.0049 / 1.0000 |
| TSO queue | 1.3298 / 1.3331 | 0.9864 / 0.9860 |
| PSO queue | 1.1666 / 1.1316 | 0.9979 / 0.9973 |

One TSO fib repetition changes a baseline correct false into a timeout carrying a
late false witness. P0.7e2 therefore violates both the no-coverage-regression and
deterministic-time-regression gates. The 2,304-cell matrix is intentionally not run.

The explicit frames fix P0.7e1's queue RSS increase but raise per-transition dispatch
cost. On the identical 1,776,716,915-transition TSO fib workload, offline time rises
from about 25.65 s in P0.7e1 to 42.60 s in P0.7e2. P0.7e2 is rejected.
