# Property-directed finite lane: actual pthread-wmm result

## Decision

Reject and remove the hard `error-active` SMT objective. On all 283 actual
`pthread-wmm` tasks it reduces correct terminals from 104 to 35 and raises TIMEOUT from
179 to 248. It must not be extended to the 725-task workload.

## Experiment

- Same Release binary and recursive SC CAT model for both run sets.
- 60 CPU seconds, 4 GiB, one core per task.
- Concurrent disjoint 24-core queues.
- Candidate retains the 10,000 assignment cap but adds an exact disjunction requiring an
  active error event.
- Evidence: `server-results/finite-symbolic-pthread-wmm-20260718e/`.

## Results

| Metric | Baseline | Candidate |
|---|---:|---:|
| tasks | 283 | 283 |
| correct FALSE | 90 | 35 |
| correct TRUE | 14 | 0 |
| TIMEOUT | 179 | 248 |
| total CPU seconds | 11,635.69 | 15,119.27 |
| peak RSS | 27.1 MB | 630.6 MB |

All 179 baseline TIMEOUTs remain TIMEOUT. Another 55 baseline FALSE and all 14 baseline
TRUE tasks become TIMEOUT. There are no new correct terminals.

Only 58 candidate logs reach a final statistics line, all with zero finite assignments.
The other 225 tasks are exactly the statically admitted cohort and time out in the first
monolithic Z3 query. Thus the regression is not CAT enumeration after a model: it is the
combination of an error-path constraint with eagerly encoded RF/CO variables and
constraints for the complete event skeleton.

## Consequence

The hard objective was removed immediately. A viable property-directed lane needs staged
or lazy construction: solve enough control/property structure to identify a relevant
event slice, then instantiate RF/CO only for that slice, while retaining native replay and
native fallback. Lowering the assignment limit cannot repair a timeout before assignment 1.
