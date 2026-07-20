# P1 compact race-causality retention experiment

## Decision

Reject and remove the candidate. It lowers retained memory during bounded smoke runs, but all 31
formal OOM tasks still reach the same 12-GB cap and candidate CPU increases by 32.3%. No task reaches
a terminal result.

## Cross-family attribution

Three direct 15-second heaptrack traces were finalized by terminating only GenMC and allowing
heaptrack to flush its output:

| Family/task | Dominant retained allocation evidence |
|---|---|
| Weaver `mult-dist` | 675.58 MB under 2,814,904 NA loads; 232.70 MB under 938,304 NA stores |
| LibVSync `rec_ticketlock` | 649.74 MB under 2,707,239 reads; 173.26 MB in `EventLabel::addView` |
| Goblint `list_racing` | 48.11 MB in `View` backing storage during the sampled prefix |

The stacks pass through `configureProbe`, `SCChecker::calculateViews/updateMMViews`, and
`ExecutionState` access-frontier updates. This shows that label/View retention is important for
Weaver and LibVSync even though another Goblint task was previously dominated by CAT matrices.

Raw root:
`/data3/sujie/experiments/caat-optimization/p1-cross-family-heap-20260720-r1`.

## Candidate

When `--disable-race-detection` was active, access interval maps retained the access position and
per-thread maxima but omitted the complete causal View used to eliminate dominated race-witness
predecessors. Mixed-width boundaries, atomic-write presence, and value reconstruction metadata were
preserved. The normal path with race detection enabled was unchanged.

Focused `ExecutionState`, interval-map, CAT value, offline CAAT, and incremental CAAT tests passed.
The sanitizer gate was intentionally not run after the decisive resource experiment rejected the
candidate.

## Three-task smoke, 30 seconds and 16 GB

| Task | Baseline | Candidate | CPU | RSS |
|---|---|---|---:|---:|
| LibVSync `rec_ticketlock` | TIMEOUT | TIMEOUT | 31.355 -> 31.338 s | 6.870 -> 6.717 GB |
| Weaver `mult-dist` | TIMEOUT | TIMEOUT | 31.470 -> 31.395 s | 9.468 -> 8.522 GB |
| Goblint `list_racing` | OOM | TIMEOUT | 18.309 -> 31.429 s | 16.000 -> 9.002 GB |

The two common TIMEOUT tasks improve both resources slightly. Goblint survives longer, but that is
not a terminal result and cannot be counted as a speedup.

Root: `/data3/sujie/experiments/caat-optimization/p1-race-causality-smoke-20260720-r1`.

## Formal OOM31, 120 seconds and 12 GB per task

Two 24-worker lanes ran 48 tasks simultaneously with the established privileged Docker,
host-cgroup mount, and BenchExec `--no-container` setup. Both lanes contain exactly 31 XML rows and
31 archived logs.

| Metric | Baseline | Candidate | Ratio |
|---|---:|---:|---:|
| Status | 31 OOM | 31 OOM | no improvement |
| CPU sum | 1,456.431 s | 1,926.664 s | 1.32286x |
| Wall sum | 1,457.954 s | 1,928.364 s | 1.32265x |
| RSS sum | 371,999,936,512 B | 371,999,936,512 B | 1.00000x |

Root: `/data3/sujie/experiments/caat-optimization/p1-race-causality-oom31-20260720-r1`.

## Interpretation

The compact frontier removes intermediate View pressure, but later state still fills the complete
memory budget. The extra headroom merely permits more exploration before OOM. Future P1 work must
delete or share whole retained exploration states, or reduce the number of explored representatives;
compressing another field without crossing a terminal boundary should not be repeated.

## Infrastructure/test corrections

- The first new test used the wrong `SAddr::createHeap` arity and did not compile; it was corrected.
- The first full-mode oracle misunderstood `AdaptiveView::Update`: its causal View affects later
  aggregation rather than direct iteration. The corrected dominance test passed.
