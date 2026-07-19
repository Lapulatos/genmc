# P0 Regional SC-RVF Exact-NA Full-725 Decision

Date: 2026-07-20

## Decision

Reject and remove the exact-non-atomic-event continuation candidate. It preserves the observed
verdicts in this run, but commits zero RVF load reductions and materially slows the regional
common-terminal cohort. The candidate therefore does not satisfy the required joint time/memory
retention rule and does not solve P0 activation.

Regional SC-RVF returns to the conservative non-atomic frontier: before a transaction opens, the
frame is dropped and native RF-DPOR continues; after a transaction opens, encountering a
non-atomic event revokes the speculative descendants and replays the untouched native entry.

## Authoritative experiment

- Server result root:
  `/data3/sujie/experiments/caat-optimization/p0-regional-rvf-paired-725-exact-na-20260720a`
- Definition: `regional-rvf-paired-725.xml`
- Limits: 120 seconds, 12 GB, one task core; simultaneous baseline/candidate lanes used 8 workers
  each on disjoint CPU sets.
- Acceptance: both lane processes exited 0; `manifest.tsv` records both statuses as zero;
  `complete.txt` exists; each lane has exactly one compressed XML with 725 `<run>` rows and one
  archive with 725 logs. The strict analyzer also proves identical 725-task XML/log sets.
- Derived artifacts in the result root: `paired.tsv` and `analysis.json`.

## Correctness observations

- Status differences: 0/725.
- Semantic-summary differences among 465 common-terminal pairs: 0.
- Regional-gate tasks: 51.
- Tasks that published an exact non-atomic read constraint: 50; total constraints: 377.
- Tasks with `rvf-loads-reduced > 0`: 0.
- Published reduced loads: 0.
- Published RVF load attempts: 0.
- Published fail-open events: 0.

The zero published-attempt count does not establish that speculative work never began. The
transaction deliberately discards speculative statistics when it revokes and publishes the durable
prefix plus native replay result. Together with zero committed reductions, this means the exact-NA
extension produced no durable quotient benefit on the full workload. The previously measured
post-frontier opportunity (37 tasks with mergeable reads) remains behind a frontier that this
candidate failed to own through completion.

## Resource and exploration effects

| Cohort | Tasks | CPU baseline | CPU candidate | CPU change | summed peak RSS change |
|---|---:|---:|---:|---:|---:|
| All | 725 | 31,114.972 s | 31,122.684 s | +0.0248% | +0.0145% |
| Common terminal | 465 | 2,061.451 s | 2,073.509 s | +0.5849% | — |
| Regional gate, all | 51 | 614.679 s | 626.428 s | +1.9114% | +0.0012% |
| Regional gate, common terminal | 46 | 9.736 s | 21.434 s | **+120.1587%** | — |
| Fallback, all | 674 | 30,500.293 s | 30,496.257 s | -0.0132% | +0.0145% |

The near-neutral full-set aggregate is dominated by 120-second limits and fallback tasks. The
mechanism-specific regional cohort is decisive: its common-terminal time more than doubles while
memory remains neutral rather than improving.

Regional published search totals also fall (`rf-offered` 3,068,736 -> 1,240,260; `work-popped`
1,317,703 -> 554,075), but this is not accepted as quotient evidence: no reduced load commits, five
regional tasks time out in both lanes, and slower speculative/replay work can perform fewer search
operations within the same timeout. Search-counter reduction without a committed equivalence class
and without a time improvement is not an optimization.

## Rejected implementation

The rejected candidate:

1. admitted non-atomic reads and writes as ordinary events in `SCExecutionGraphAdapter`;
2. fixed every executed non-atomic read to its concrete singleton source in `GoodW`;
3. kept the regional frame alive across those exact events;
4. relied on the existing future-write/ancestor revocation logic for later completeness hazards.

It passed the generated two-iteration oracle (20 shapes, 1,620 observable states, 6,480 native/RVF
invocations) with zero verdict/outcome violations and 314 locally reduced RVF cells. The full-725
result shows that this local activation does not transfer to production tasks. The oracle remains
useful correctness evidence for the rejected experiment, but it cannot override the production
activation/resource gate.

## Infrastructure notes

The first post-run probe incorrectly used Bash array/glob assumptions through the remote default
shell and passed empty names to `bzcat`/`zipinfo`; no result was accepted from that probe. A POSIX
`find`-based rerun established the exact 725/725 cardinalities. The first analyzer run then rejected
an original, non-rewritten SV-Benchmarks source path; the parser was generalized from the rewrite
prefix to the common `/c/<task>.{c,i}` suffix and the entire 725-task analysis was rerun without
skipping the compile-error task.

## Next P0 requirement

Another regional continuation should not be attempted by merely freezing or exactly encoding the
non-atomic prefix. A future design needs an ownership/completion certificate that covers the native
prefix's backward alternatives and survives loops/future writes without replaying all speculative
benefit. Until such a design exists, the conservative frontier is the correct complete behavior.

## Rollback verification

The synchronized GCC 13/LLVM 15 server build succeeds after removing exact-NA continuation.
Focused tests pass 12/12 (`SCExecutionGraphAdapter.*` and `RegionalRvfTransaction.*`). The two
integration gates also pass: `sc-rvf-regional-na-load` now proves exact native fallback with equal
observable semantics/execution count under one and two workers, while `sc-rvf-regional-loop`
continues to exercise the retained loop/transaction path. No second broad resource run is warranted
for a rejected mechanism whose conservative behavior was already measured in the earlier 51-task
paired gate.
