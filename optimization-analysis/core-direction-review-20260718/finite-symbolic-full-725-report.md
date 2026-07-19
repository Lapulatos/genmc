# Finite symbolic FALSE lane: full 725-task result

## Decision

Reject the current candidate. It confirms only two errors already found quickly by the
baseline, reduces correct results from 398 to 318, increases TIMEOUT from 239 to 302, and
introduces 19 additional ABORTED results. The mode remains opt-in and must not be promoted.

## Frozen experiment

- Workload: the same 725 adapted SV-COMP unreach-call tasks.
- Model: recursive SC CAT for both configurations.
- Limits: 60 CPU seconds, 4 GiB, one core per task.
- Scheduling: concurrent disjoint 24-core queues (baseline 0--23, candidate 28--51).
- Candidate-only flags: `--finite-symbolic-errors --finite-symbolic-max=10000`.
- Raw evidence: `server-results/finite-symbolic-full-725-20260718a/`.

## Terminal results

| Metric | Baseline | Candidate | Change |
|---|---:|---:|---:|
| correct | 398 | 318 | -80 |
| wrong | 30 | 28 | -2 (converted to non-terminal outcomes, not fixes) |
| error/non-terminal | 297 | 379 | +82 |
| TIMEOUT | 239 | 302 | +63 |
| OOM | 32 | 32 | 0 |
| false(unreach-call), correct | 130 | 73 | -57 |
| true, correct | 268 | 245 | -23 |
| ABORTED | 2 | 21 | +19 |
| total CPU seconds | 16,131.35 | 19,530.80 | +21.1% |

Among the 318 tasks correct in both configurations, summed CPU rises from 361.10 s to
496.66 s. The median per-task ratio is 0.999 because fail-open/very small tasks dominate;
the large regression is concentrated in admitted finite tasks.

## Finite-lane activity

Counters were recovered from all 725 archived logs; 399 reached a final stats line.

- assignments: 33,150 total; 10 tasks hit the 10,000 budget;
- CAT rejected: 33,138;
- CAT consistent: 12 across 9 tasks;
- replay attempts: 2;
- replay confirmed: 2;
- fail-open: 352.

Both confirmed errors were already fast baseline successes. One regressed from 0.060 s to
44.05 s; the other changed from 0.066 s to 0.090 s. Thus the lane produced zero new correct
terminal results.

## Correctness defect

The candidate creates 21 ABORTED rows versus two in baseline. An archived log shows an
uncaught `std::out_of_range` (`vector::_M_range_check`, index 1 with size 1). This is a real
candidate defect even though the lane is opt-in. It must be reproduced and fixed before
any narrower retry.

## HTML

`server-results/finite-symbolic-full-725-20260718a/html/finite-symbolic-baseline-vs-candidate-725.table.html`
contains 725 rows and two run sets. The corresponding `.diff.html` contains 96 changed
rows. Raw XML and both log ZIP archives remain beside it.
