# Exact unchanged primitive cache experiment

## Decision

Correct and beneficial, but reject as a standalone optimization because common-terminal CPU
improves only 4.15%, below the predeclared 10% retention gate. Keep the implementation as the
oracle-checked cache boundary for the next changed-query primitive-delta substep; do not enable it
by default or claim the core optimization complete.

## Method

An opt-in `--cat-primitive-cache` path scans an exact semantic descriptor containing active event
identity/order/classification, thread/index, address, SC membership, RF source, RMW/lifecycle
partners, every discovered location, and each location's complete coherence order. If the
descriptor is identical to the previous query, it returns the cached complete primitive snapshot.
Otherwise it invokes the original full materializer. Graphs above 512 stable candidates clear the
cache and stay on the existing large-graph path.

With `--cat-oracle`, every cache hit is independently rebuilt and compared across event count,
active count, every base value, and dense-to-stable witness mapping. The CAT evaluator remains the
only consistency authority.

## Correctness evidence

- Local unit suite: 217/217 passed; one expected Z3-availability test skipped.
- Local CAT/CAAT differential: 4/4 passed.
- Local full-materialization hit oracle: 8/8 cache hits matched on fcombiner.
- Server Docker Release: 217/217 passed; one expected skip.
- Server Docker ASan+UBSan with leak detection: 51/51 focused tests passed, no report.
- Server Docker mutation oracle: 39 rows, 5,441 evaluator oracle checks, zero mismatch.
- Server Docker broad differential: 288 programs x SC/TSO/PSO = 864 pairs; 852 comparable
  matches, 12 mutually unsupported, zero mismatch.
- Fixed panel: all 15 statuses match; all 10 terminal tasks have identical complete, blocked,
  work-added, work-popped, and validity-query counts.

## Paired server experiment

Baseline and candidate ran simultaneously in one Docker container on disjoint core sets (0--3 and
4--7), four BenchExec workers each, 60 seconds and 4 GiB per task. Raw results:
`server-results/primitive-cache-paired-15-20260718a/`.

| Metric, 10 common terminals | Baseline | Cache | Change |
|---|---:|---:|---:|
| CPU time | 142.274 s | 136.369 s | -4.15% |
| Materialization | 33.526 s | 27.723 s | -17.31% |
| CAT consistency | 65.763 s | 59.844 s | -9.00% |
| Aggregate reported RSS | 1.000x | 1.0012x | +0.12% |
| Worst per-task RSS ratio | 1.000x | 1.0043x | +0.42% |
| Terminal / timeout | 10 / 5 | 10 / 5 | unchanged |

The candidate records 146,360 exact cache hits and 292,784 misses, exactly matching the
synchronizer's unchanged and offline-evaluation split. Offline evaluation and both base-copy costs
remain essentially unchanged, confirming that the gain comes only from avoiding primitive rebuilds.

## Next substep

Add exact delta mutation for changed descriptors, beginning with universe/event sets, `id`, and the
single-edge `rf`, `rmw`, `tc`, and `tj` primitives. Every changed-query result must be compared with
independent full materialization before adding grouped or address-local relations. The unchanged
cache remains experimental until the combined candidate reaches the CPU gate.

