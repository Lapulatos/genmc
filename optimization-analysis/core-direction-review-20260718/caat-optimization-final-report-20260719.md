# GenMC CAAT optimization final report (2026-07-19)

## Outcome

The retained exact foundation combines unchanged primitive caching, small dense primitive build,
packed check witnesses, successor-cursor composition, and successor-cursor cycle checks. It does
not change TruSt equivalence, candidate generation, search bounds, CAT semantics, or safety
reporting. Every candidate kept an old evaluator/materializer path as an oracle and stopped on any
mismatch.

On the actual simultaneous 725-task experiment, correct terminals increase 394 to 402 and total
TIMEOUT+OOM falls 275 to 267. Common-terminal CPU falls 27.87%, total CPU 4.49%, and suite wall
3.78%. Eight pthread-wmm tasks change from TIMEOUT to correct `false(unreach-call)`. No resolved
terminal regresses and no terminal verdict differs.

The operational tradeoff is explicit: 20 Goblint tasks change TIMEOUT to OOM because faster
exploration reaches the unchanged 4-GB limit before 60 seconds. Aggregate RSS consequently rises
0.92%, although maximum RSS remains the same. For this reason the retained foundation stays behind
experimental switches rather than becoming the default for every CAT workload.

## Direction decisions

| Direction | Strongest isolated result | Decision |
|---|---:|---|
| Exact primitive cache + dense build | completed CPU -22.49%, materialization -48.85% | Retain as foundation |
| Packed witness checks | only 0.15% beyond prior foundation | Retain exact code, no standalone expansion |
| Successor-cursor composition | composition -88.61%, completed CPU incremental -4.64% | Retain as foundation |
| Successor-cursor cycle checks | fixed-15 all CPU -12.74%; 283 rescues 8 TIMEOUTs | Retain; enabled actual 725 gate |
| Ordered dense coherence/FR | materialization -5.02%, all CPU -0.08% | Stop before 283 |
| Descriptor miss-build | scan -8.87%, all CPU +0.02% | Stop before 283 |
| Descriptor vector reuse | materialization -11.01%, all CPU -0.52% | Stop before 283 |
| Forced incremental small-graph path | common CPU +32.73%, one new TIMEOUT | Reject |
| Larger checkpoint history | zero subset match | Reject |
| Changed-query primitive delta | mutation oracle missed FR edges; 4.8x local cost | Reject |

The remaining fixed-15 foundation profile is approximately 35 seconds CAT consistency within 110
seconds completed CPU: materialization about 20 seconds and offline evaluation about 9 seconds.
The separately tested remaining subphases have end-to-end ceilings below the strict expansion gate.
Further micro-combination would not justify another 283/725 run without a new mechanism that moves
a hard resource outcome or removes substantially more work.

## Correctness and completeness evidence

- Full local and server Release unit suites pass after each retained stage.
- Server ASan+UBSan focused suites pass with halt-on-error and leak detection.
- The final mutation gates cover SC/TSO/PSO, one/two workers, RF/CO/RMW/lifecycle/dynamic graph
  changes, with thousands of independent old-path evaluator checks and zero mismatch.
- Broad differential gates cover 864 pairs: 852 exact matches, 12 mutually unsupported, zero
  mismatch.
- Fixed-15 paired runs preserve terminal verdicts and exact complete/blocked/bound counters.
- The 283 and actual 725 runs show no terminal verdict mismatch or resolved-case regression.
- No candidate truncates exploration, guesses a bound, or reports safety from an incomplete search.

## Deliverables

Authoritative actual results are under
`server-results/primitive-fast-compose-cycle-paired-725-20260719a/`. Its `html/` directory contains
the BenchExec 3.25 725-row/two-run-set table, 28-row difference table, CSV companions, and generator
log. The generated artifacts contain no `cputime-cpux` field.

All later negative micro-optimization results retain their independent reports and raw fixed-panel
evidence. Failed or incomplete launches are explicitly marked invalid and are not used in any
performance conclusion.
