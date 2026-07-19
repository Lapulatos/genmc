# Fast-check recovery report (2026-07-19)

## Decision

`--cat-fast-checks` is exact and measurably reduces ordinary check-witness scanning, but it does
not pass the frozen expansion gate. Retain it only as an experimental combined switch; do not run
pthread-wmm 283 or the actual 725 from this result.

## Recovery and correctness

- Corrected the misplaced `Relation::firstPair()` and `Relation::firstReflexive()` declarations.
- Local GCC 13 / LLVM 15: `genmc` linked; 221 unit tests passed and one Z3-availability test
  skipped by design.
- Server Release: 221 passed, one expected skip.
- Server ASan+UBSan relevant suites: 31/31 passed.
- Server mutation oracle: 39 rows, 5,416 full evaluator checks, zero mismatch.
- Server broad differential: 852 matches, 12 mutually unsupported, zero mismatch over 864 pairs.

The local mutation runner was nondeterministic on the early-error TSO/two-worker
`malloc-not-hb0.c` cell and was not counted as passing. The established server environment
exercised all 39 rows successfully.

## Fixed 15 paired result

| Metric | Baseline | Cache + fast-build + fast-checks | Change |
|---|---:|---:|---:|
| All-task CPU | 471.39 s | 432.72 s | -8.20% |
| Completed-task CPU | 166.39 s | 127.72 s | -23.24% |
| Four-job suite wall | 138.42 s | 134.40 s | -2.90% |
| CAT consistency | 89.976 s | 51.205 s | -43.09% |
| Offline evaluation | 49.098 s | 25.594 s | -47.87% |
| Ordinary check witnesses | 17.577 s | 16.678 s | -5.12% |
| Materialization | 37.493 s | 19.398 s | -48.26% |
| Aggregate RSS | 398,561,280 B | 398,786,560 B | +0.06% |
| Peak task RSS | 27,054,080 B | 27,021,312 B | -0.12% |
| Terminal / TIMEOUT | 10 / 5 | 10 / 5 | unchanged |
| Complete / blocked / exceeding bound | 18,693 / 197,703 / 0 | 18,693 / 197,703 / 0 | exact |

Relative to the prior cache + fast-build candidate, candidate CPU changes only
433.35 to 432.72 s (-0.15%), while materialization is essentially unchanged and fast-checks save
about 0.90 s of profiled check time. This is not enough to attribute an end-to-end improvement
beyond run noise, and no hard task crosses the 60-second cap.

## Evidence

- Broad TSV: `server-results/fast-check-20260719a/broad.tsv`
- Paired XML, logs, manifests and input hashes:
  `server-results/primitive-fast-paired-15-fast-check-20260719a/`
