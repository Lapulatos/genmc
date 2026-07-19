# P1 clean cumulative 725-task decision

## Decision

Do not promote the five P1 storage/layout changes to `genmc-caat`. Keep them on the
development branch as implementation and negative-result evidence. The clean candidate improves
both CPU and RSS on completed tasks, but delays the same 31 memory failures by 15.0% without
turning any of them into a terminal result. This violates the project rule against exchanging time
for memory when the externally visible outcome does not improve.

The isolated empty-`EventDeps` representation accounts for more than half of that OOM delay
(+8.19%). The isolated inline-revisit allocation has only a +1.40% OOM CPU difference and about a
one-percent benefit on the earlier fixed panel. That effect is too small to justify another broad
run under the explicit stop rule.

## Candidate and protocol

- Baseline commit/binary: stable `53c667d5`, SHA-256
  `c1dbb815ec67df6deb4a7ca346d236220e23f6b455cd06730dace9c12d65b2bd`.
- Candidate commit/binary: clean stable-based detached commit `839ac8b9`, SHA-256
  `3cbcea8a1c266219293f95246cff8755a39bc0ab658967aaadf1853a315ba077`.
- Candidate contents: canonical empty dependencies, logical calculated-view deduplication,
  immutable calculated-relation sharing, same-worker view-base sharing, and inline revisit clocks.
- Workload: the fixed 725-task fair census, recursive SC CAT, one GenMC worker per task.
- Limits: 120 seconds and 12 GB per task; two 24-task BenchExec lanes in separate NUMA nodes.
- Result root:
  `/data3/sujie/experiments/caat-optimization/p1-cumulative-paired-725-r3-clean-20260720`.
- Acceptance: both launcher statuses are zero; each lane contains exactly one result XML and one
  log archive with exactly 725 rows; all input hashes were captured before execution.

The earlier `r2` result is not used because its candidate also contained unrelated research-branch
code. The accepted `r3` build contains only the P1 patch relative to stable.

## Correctness and completeness observations

- Status histograms differ on one task only: `pthread/fib_safe-7` changes from
  `TIMEOUT (true)` at 120.52 CPU seconds to `true` at 119.71 seconds.
- Its reported complete-execution count is 51,480 in both lanes and its semantic-summary digest is
  identical. The classification change is therefore a threshold crossing, not a search change.
- The 438 tasks solved by both binaries have zero semantic-summary differences and zero complete
  execution-count differences.
- Search counters are not comparable: neither clean stable binary emits the later research-only
  `Exploration statistics` line. No claim of search-counter equality is made.
- Pre-broad gates: 162/162 clean unit/property tests passed; SC, TSO, PSO differential tests,
  recursive CAAT and online mutation passed. Five aggregate CTest failures were missing fixtures
  already absent from the stable source, not candidate failures.

## End-to-end resources

| Cohort / metric | Baseline | Candidate | Candidate / baseline |
|---|---:|---:|---:|
| 438 common-solved CPU total (s) | 2,077.907 | 2,039.950 | 0.98173 |
| 438 common-solved CPU geometric mean | — | — | 0.96532 |
| 438 common-solved wall geometric mean | — | — | 0.96212 |
| 438 common-solved RSS geometric mean | — | — | 0.99645 |
| 438 common-solved RSS total (GB) | 35.653 | 34.509 | 0.96791 |
| all-task CPU total (s) | 31,220.409 | 31,355.508 | 1.00433 |
| all-task RSS total (GB) | 510.890 | 507.244 | 0.99286 |

A task-resampling bootstrap over the 438 common-solved rows gives 95% intervals of
`[0.96056, 0.97017]` for the CPU geometric-mean ratio and `[0.99349, 0.99875]` for RSS. These
intervals describe workload-row variation in this one paired run; they are not run-to-run or
machine-repetition confidence intervals.

The 229 common TIMEOUT rows are essentially neutral in time (CPU ratio 0.99984) while summed RSS
falls to 0.97570. The adverse aggregate time comes from the memory-limited cohort.

## OOM diagnosis and ablation

The full run initially placed baseline on NUMA0 and candidate on NUMA1. A strict 31-task rerun
swapped those assignments and retained the effect, ruling out a fixed-node explanation:

| Swapped-NUMA OOM candidate | Statuses | Baseline CPU (s) | Candidate CPU (s) | Ratio | RSS result |
|---|---:|---:|---:|---:|---|
| Five-item cumulative | 31 OOM / 31 OOM | 1,259.728 | 1,448.760 | 1.15006 | both hit 12 GB |
| EventDeps only | 31 OOM / 31 OOM | 1,262.467 | 1,365.812 | 1.08186 | both hit 12 GB |
| Inline revisit only | 31 OOM / 31 OOM | 1,263.810 | 1,281.458 | 1.01396 | both hit 12 GB |

Raw roots are respectively:

- `/data3/sujie/experiments/caat-optimization/p1-cumulative-paired-oom31-swapped-20260720-r1`;
- `/data3/sujie/experiments/caat-optimization/p1-cumulative-paired-oom31-eventdeps-20260720-r1`;
- `/data3/sujie/experiments/caat-optimization/p1-cumulative-paired-oom31-inline-revisit-20260720-r1`.

Canonical empty dependencies let the verifier retain more history before the fixed memory limit,
but the additional work is insufficient to solve any affected task. It is a useful representation
technique only if combined with a later algorithmic reduction that crosses an OOM/terminal boundary.
It is not an end-to-end optimization by itself.

## Claim boundary

Allowed: the cumulative candidate reduces time and memory on the common-solved cohort, and storage
compression lets OOM tasks execute longer before reaching the same cap. Also allowed: it is rejected
for production because no OOM outcome improves and total time increases.

Not allowed: P1 improves the full 725 workload; P1 solves a memory failure; the full candidate
preserves search counters; or the task-bootstrap interval establishes run-to-run significance.

## Next action

Stop engineering-only history-layout variants. Resume an algorithmic direction that reduces the
number of RF/order decisions or explored representatives. The most promising new path is a
value-first, source-lazy RVF layer inside the Deagle/Yogar finite solver, with ordering-theory
refinement and native replay confirmation.
