# P1 production-path profile

Date: 2026-07-15

## Decision

Do not prioritize persistent delta history, transactional-copy specialization, or
hybrid online operators on this corpus. The current certified SC/TSO/PSO path performs
almost no successful online propagation: all 243,236 observed primitive-changing
queries comprise 195,430 retracting changes and 47,806 monotone changes, while the
incremental worklist accumulated zero time. The next prototype instead removes an
unreachable duplicate history copy from certified adaptive-offline epochs and moves
the freshly materialized base into the evaluator.

## Experiment

- Binary: reverted production binary after the rejected measured selector.
- Image: `genmc15noble:sujie`.
- Input: established 96-task SV-COMP sample under SC, TSO, and PSO.
- Total: 288 BenchExec rows, one 96-task run set per model.
- Scheduling: `-N48`, cores `0-23,28-51`, one CPU, 4 GB, and 60 seconds per task;
  GenMC `--nthreads=1`.
- Instrumentation: opt-in `--cat-stats` only.
- Server output: `/data3/sujie/experiments/caat-optimization/p1-profile/results/`.
- Local mirror: `server/`; parsed per-run TSV and aggregate JSON: `analysis/`.

The host load average was already high because an unrelated user's benchmark was
running. Verdicts and structural counters remain usable. Absolute time and the
one-pass timing shares are diagnostic only and must not be presented as a controlled
before/after speed comparison. Profiling itself also scans primitive deltas and space,
so `sync-ns` includes diagnostic overhead that is absent from ordinary runs.

## Coverage and correctness

| Model | Rows | Logs with terminal CAAT statistics | Resource-limited logs | Correct | Incorrect |
|---|---:|---:|---:|---:|---:|
| SC | 96 | 88 | 8 | 88 | 0 |
| TSO | 96 | 87 | 9 | 87 | 0 |
| PSO | 96 | 79 | 17 | 79 | 0 |
| Total | 288 | 254 | 34 | 254 | 0 |

“Correct” and “incorrect” are BenchExec property classifications against the YAML
expectations. A timeout or OOM is unknown, not evidence of a false positive or false
negative.

## Measured structure

| Metric | SC | TSO | PSO | Total |
|---|---:|---:|---:|---:|
| Profiled queries | 254,871 | 58,591 | 13,293 | 326,755 |
| Adaptive-offline changed queries | 191,118 | 39,062 | 13,052 | 243,232 |
| Primitive-changing queries | 191,120 | 39,064 | 13,052 | 243,236 |
| Retracting primitive queries | 146,329 | 36,127 | 12,974 | 195,430 |
| Monotone-only primitive queries | 44,791 | 2,937 | 78 | 47,806 |
| Successful insertion transitions | 0 | 0 | 0 | 0 |
| Maximum active events | 8,029 | 8,029 | 432 | 8,029 |
| Maximum snapshot-equivalent state | 218.52 MB | 453.23 MB | 1.43 MB | 453.23 MB |

The small-graph certificate covers every completed PSO log and 97.7% of completed
SC/TSO logs. By query count, adaptive-offline rebuilds account for 75.0% of SC,
66.7% of TSO, and 98.2% of PSO queries; the remainder is primarily exact unchanged
hits, not online propagation.

## Diagnostic timing

| Model | Aggregate `sync-ns` | Offline share | Materialization share | History-search share | Transactional-copy share |
|---|---:|---:|---:|---:|---:|
| SC | 163.46 s | 22.49% | 9.09% | 0.53% | 0.14% |
| TSO | 149.25 s | 33.85% | 8.33% | 0.59% | 0.38% |
| PSO | 16.61 s | 29.36% | 7.13% | 0.00% | 0.00% |
| Total | 329.32 s | 27.98% | 8.65% | 0.53% | 0.24% |

`rebuild-ns` contains `offline-ns`, so they must not be added. The large unassigned
portion of `sync-ns` includes opt-in primitive-delta and space scans plus base ownership
copies that currently have no dedicated timer.

## Selected Optimization 05 prototype

For a certified adaptive-offline epoch at or below 512 stable events:

1. pass the just-materialized `BaseValues` by value and move it into
   `IncrementalCaatEvaluator` after exact offline evaluation;
2. do not retain a duplicate `HistoryEntry::base` or evaluator checkpoint because the
   next changed small query reaches the adaptive-offline rebuild before any history
   lookup;
3. preserve the evaluator's current exact state so unchanged queries remain cached and
   a later large monotone graph can still use insertion; every other transition keeps
   the existing exact rebuild fallback.

This changes ownership and fallback cost only. It does not prune a candidate, reuse an
approximate value, or alter the least-fixed-point result. A dedicated test crosses from
a no-history small epoch to an ordinary retained-history large epoch and compares the
published state with the Phase 2 oracle.
