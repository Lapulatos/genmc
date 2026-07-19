# Optimization 13 decision: retain grouped packed primitive materialization

## Decision

Retain the construction-only implementation. It preserves the exact dense CAT values,
reduces stable primitive materialization time by 74.0% on the completed large-event
sample, and converts two resource-bound task/model pairs into repeatable terminal
results without adding an OOM, wrong verdict, or safe-execution-count mismatch.

This is not a relation-memory optimization. Persistent dense relation storage and
downstream composition/closure remain unchanged.

## Code scope

- `genmc/genmc/CAT/Value.hpp` and `Value.cpp`: add packed union of an `EventSet` into a
  single relation row.
- `genmc/genmc/CAT/StableGraphAdapter.cpp`: replace scalar active-event pair
  classification for `po`, `int`, `ext`, and `loc` with exact thread/location groups and
  packed row insertion.
- `tests/unit/CatEvaluatorTest.cpp`: verify packed insertion across word boundaries and
  compare direct stable materialization with dense-build/remap on two threads, two
  addresses, initial writes, and graph mutations.

No model name, built-in checker verdict, or host SC/TSO decision participates in the
construction or consistency result.

## Correctness and completeness

- Fresh Release builds: baseline 145/145 tests; candidate 146/146 tests.
- Mutation full oracle: 39 rows, 5,441 offline checks, zero mismatch.
- Broad differential: 288 programs × SC/TSO/PSO = 864 pairs; 852 matches, 12 mutual
  unsupported pairs, zero mismatch.
- Formal matrix: 1,152 paired cells per variant; zero opposite common-terminal verdict
  and zero common-terminal execution-count mismatch.
- OOM cells: 60 before and 60 after.

## Performance

The authoritative matrix has four balanced repetitions. Each repetition runs 48
BenchExec workers simultaneously: eight workers for each baseline/candidate SC, TSO,
and PSO group. Core sides swap on even repetitions. Every one of the 24 run sets exited
zero under 60 s CPU and 4 GB memory limits.

### Stable materialization, completed tasks with at least 512 stable events

| Model/task | Events | Candidate / baseline `materialize-ns` |
|---|---:|---:|
| PSO `queue_ok_longer` | 8,033 | 0.223 |
| SC `queue_ok_longer` | 4,029 | 0.389 |
| SC `queue_ok_longest` | 8,029 | 0.248 |
| TSO `queue_ok_longer` | 4,029 | 0.319 |
| TSO `queue_ok_longest` | 8,029 | 0.172 |

The task-clustered geometric mean is 0.2596 with 95% bootstrap CI
[0.2049, 0.3302]. All five model-task ratios pass the frozen 0.70 target.

### Whole-process metrics on four-repetition common terminal tasks

- CPU: 0.9954, 95% CI [0.9871, 1.0036]. This does not pass the universal CPU gate.
- Wall: 0.9917, 95% CI [0.9830, 1.0003].
- RSS: 1.00008, 95% CI [0.99989, 1.00025], consistent with the intentionally unchanged
  persistent representation.
- TSO common-terminal CPU is 1.0148 [0.9999, 1.0296]; therefore the implementation must
  not be described as a universal end-to-end speedup.

### Repeatable coverage gain

- PSO `fib_unsafe-5`: baseline TIMEOUT in 4/4 repetitions; candidate reports the correct
  `false(unreach-call)` in 4/4 at 54.3--56.9 CPU seconds.
- TSO `fib_unsafe-7`: baseline reports the correct false result in 1/4 repetitions;
  candidate does so in 4/4 at 51.7--53.7 CPU seconds.
- Aggregate statuses change only through seven `TIMEOUT -> false(unreach-call)` cells:
  TIMEOUT 47 -> 40, correct false 333 -> 340, OOM unchanged at 60.

These results satisfy the frozen alternative retention gate: repeatable terminal gain
with no OOM increase. They do not satisfy the separate universal-CPU-confidence gate.

## Space effect

The output `Relation` objects remain the same dense packed matrices. Construction adds
O(events + threads + locations) transient grouping metadata and replaces scalar pair
insertion with word-wise row insertion. The measured RSS ratio is effectively one, and
the OOM count is unchanged. P0.6b/P0.7 must remove dense structural values and avoid
dense composition/closure intermediates to reduce memory.

## Invalid and excluded launches

- The first build used `BUILD_TESTING` instead of this repository's `BUILD_TESTS`; both
  source builds succeeded, but the absent test binary caused exit 127.
- The first test retry waited for a redundant googletest download. It was stopped and
  reconfigured against an already downloaded dependency source from the same image and
  toolchain; no old binary was reused.
- The first performance launch lacked the explicit `/sys/fs/cgroup` Docker mount. All
  six BenchExec groups rejected the environment before starting a task. Its directory
  is retained as `server-results/formal-invalid-missing-cgroup` and excluded.

## Evidence

- Machine-readable comparison: `analysis/comparison.json`
- All 2,304 normalized rows: `analysis/rows.tsv`
- Full XML/log archives: `server-results/formal/`
- Mutation and broad evidence: `server-results/mutation.log` and
  `server-results/broad/results.tsv`
- Frozen protocol: `experiment-protocol.md`
- Determinism check: two complete analysis runs produced comparison SHA-256
  `9f047126269bf5763e4de1b2c970c0faeec10b303ba3604a5a0e1a4e0a4e99d3`.

## Next optimization

Proceed to P0.6b: represent `po/int/ext/loc` as exact structural views and lower only
certified CAT expression patterns directly over those views. Generic dense evaluation
remains the fail-closed fallback. The first prototype must target persistent and
intermediate relation bytes; repeating a construction-only optimization cannot address
the observed OOMs.
