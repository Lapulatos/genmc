# P1 scheduler-cache attribution decision (2026-07-20)

## Decision

Reject scheduler instruction-cache compaction before implementation. It is not active in
the measured OOM cohort and it is absent from the three cross-family heap-allocation hot
paths. Changing `Scheduler::seenPrefixes` cannot improve the resource failures that
motivated P1.

The next P1 target is the monotonically growing active execution graph, specifically the
repeated-load/spin behavior that adds tens of millions of labels before a second useful
choice or CAT query. This is not retained exploration history.

## Evidence

The immutable baseline archive is:

`/data3/sujie/experiments/caat-optimization/p1-race-causality-oom31-20260720-r1/baseline/p1-cumulative-paired-oom31.2026-07-20_01-26-30.logfiles.zip`.

Four of the 31 killed tasks emitted timeout-surviving progress records. Their last records
show:

| Metric | Minimum | Median | Maximum |
|---|---:|---:|---:|
| `max-scheduler-cached-labels` | 0 | 0 | 0 |
| `max-retained-work` | 2 | 5 | 6 |
| `max-current-graph-labels` | 25,700,002 | 27,800,003 | 29,900,006 |
| `max-stack-graph-labels - max-current-graph-labels` | 33 | 39.5 | 44 |
| `shared-history-graph-copies` | 1 | 1 | 1 |

The tasks are LibVSync `rec_ticketlock`, `ttaslock`, `rwlock`, and `ticketlock`. They
offer 25.7--29.9 million RF choices while retaining only two to six work items. Each
records one to three spin starts, all immediately after loop begin, zero loop side effects,
and zero spin-loop blocks.

The independent 15-second heaptrack profiles under
`/data3/sujie/experiments/caat-optimization/p1-cross-family-heap-20260720-r1/`
cover LibVSync, Weaver, and Goblint. None of their reports attributes a retained or peak
allocation hot path to `Scheduler::cacheEventLabel`, `retrieveFromCache`, the scheduler
trie, or cached-label cloning.

## Claim boundary

Only four OOM logs survived long enough to publish exploration statistics, so the exact
active-graph ratios are not claimed for all 31 tasks. The result is nevertheless decisive
for scheduler-cache compaction as an OOM mechanism: the cache owns zero labels in every
observable member, and no independent heap profile exposes it as a material allocator.

Do not implement shared pointers, hash-consing, or compact replay descriptors for
`Scheduler::seenPrefixes` without a new workload that first demonstrates a nonzero,
material cached-label footprint. Such changes would add lookup/refcount costs while
leaving the present active graph untouched.
