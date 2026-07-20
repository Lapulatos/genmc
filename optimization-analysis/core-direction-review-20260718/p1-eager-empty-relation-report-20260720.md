# P1 eager empty CAT relation experiment

## Decision

Reject the implicit-empty relation candidate. It reduces memory before the cap on one long-running
task, but the formal 31-task OOM cohort remains 31/31 OOM and consumes 30.9% more CPU. The code was
removed; only this negative result and the raw artifacts are retained on the development branch.

## Allocation diagnosis

Heaptrack r2 directly profiled the real GenMC binary on
`goblint-regression/28-race_reach_03-munge_racing.c` for 20 seconds. The valid trace contains
1,194,619 allocations and an 8.08-GB peak heap:

- nine `cat::Relation::Relation(eventCount)` calls from
  `cat::Evaluation::initializeValues()` account for 7.21 GB at peak;
- `cat::relationUnion` accounts for another 800.88 MB;
- visible graph, interpreter, and history owners are MB-scale.

This falsifies the earlier retained-history hypothesis for this representative: the immediate peak
is eager quadratic CAT fixed-point storage.

Raw trace:
`/data3/sujie/experiments/caat-optimization/p1-retained-heap-profile-20260720-r2/munge.gz`.

## Candidate

The candidate represented fixed-point bottom relations as exact immutable implicit-empty values and
short-circuited empty operands in union, intersection, difference, inverse, composition, and
transitive closure. Existing dense construction semantics were unchanged. Focused relation tests,
CAAT property tests, and the GCC 13 production build passed before benchmarking.

## Experiments

### Representative, 32 GiB and 120 seconds

Both binaries timed out without OOM. Peak RSS fell from 15,062,316 to 11,900,808 KiB (0.79010x).
Total CPU was neutral: 120.66 versus 120.48 seconds. Because neither run reached a terminal result,
this is attribution evidence rather than an end-to-end speedup.

### Formal paired OOM31, 12 GiB per task

The established privileged-Docker plus BenchExec `--no-container` launcher ran two 24-worker lanes,
for 48 simultaneous tasks. Both lanes passed the exact 31-row XML and 31-log archive gates.

| Metric | Baseline | Candidate | Candidate / baseline |
|---|---:|---:|---:|
| Status | 31 OOM | 31 OOM | no improvement |
| CPU sum | 1,455.835 s | 1,905.980 s | 1.30920x |
| Wall sum | 1,457.993 s | 1,907.611 s | 1.30838x |
| Recorded RSS sum | 371,999,936,512 B | 371,999,936,512 B | both hit cap |

Immutable result root:
`/data3/sujie/experiments/caat-optimization/p1-empty-relation-oom31-20260720-r1`.

## Interpretation

Avoiding eager bottom matrices lets executions survive longer, but later materialized relations still
consume the full memory budget. Additional survival performs more work without solving a task. Under
the required joint gate—no time-for-memory tradeoff and preferably a terminal-boundary improvement—
this is not an optimization. A future attempt must reduce the number or asymptotic size of live
materialized relations, not merely delay their allocation.

## Invalid runs

- Heaptrack r1 profiled the Bash wrapper rather than GenMC and is not attribution evidence.
- The first timed comparison lacked `/usr/bin/time` and used zsh's read-only `status` variable; GenMC
  did not run.
- Heaptrack r3 was killed before trace finalization, producing a zero-byte gzip. Its print command
  also misused `-p` and assumed remote `rg`; it is not evidence.
