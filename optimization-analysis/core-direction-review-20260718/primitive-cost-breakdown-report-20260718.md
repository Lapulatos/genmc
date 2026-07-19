# Primitive construction and ownership cost breakdown

## Decision

Proceed with the exact small-graph primitive-delta prototype. Do not optimize history retention
or evaluator base copying as standalone work: together they account for only 1.82% of measured
CAT consistency time, while repeated primitive materialization accounts for 50.83%.

## Experiment

- Dataset: frozen pthread-wmm easy/medium/hard panel, 15 tasks.
- Execution: server Docker container, four BenchExec workers, one CPU and 4 GiB per task,
  60-second task limit.
- Binary: Release build from the same source that passed the Docker Release and sanitizer gates.
- Raw result:
  `server-results/decisive-diagnostic-15-primitive-cost-20260718a/`.
- Baseline comparison:
  `server-results/decisive-diagnostic-15-20260718a/`.

Both runs terminate on the same 10 tasks and time out on the same five tasks. All 10 terminal
tasks have identical complete, blocked, work-added, and work-popped counts. Statuses are also
identical. The observation-only run uses 142.605 CPU seconds versus 142.031 seconds in the earlier
run; the 1.0040 ratio is diagnostic noise, not an optimization result. Peak reported RSS changes
from 26,828,800 to 26,927,104 bytes.

## Aggregated terminal-task costs

| Component | Time (s) | Share of CAT consistency |
|---|---:|---:|
| Primitive materialization | 33.591 | 50.83% |
| Offline evaluator total | 27.150 | 41.08% |
| Offline fixed-point evaluation only | 26.230 | 39.69% |
| Evaluator primitive-base copy | 0.630 | 0.95% |
| History primitive-base copy | 0.573 | 0.87% |
| Base equality | 0.277 | 0.42% |
| CAT consistency total | 66.084 | 100.00% |

The offline-total minus fixed-point interval is 0.920 seconds and includes publication and other
initialization bookkeeping. There are 292,784 offline evaluations, 292,774 adaptive-offline
selections, and 146,360 unchanged queries.

## Implications for the prototype

1. Removing the history copy alone cannot reach the 10% CPU retention gate.
2. A delta cache can tolerate one packed primitive copy from a time perspective, but it must still
   obey the RSS gate. The admitted path remains restricted to at most 512 stable events; larger
   structural/sparse graphs keep the existing implementation.
3. The first performance prototype may retain one cached primitive map and copy it into the
   existing snapshot/evaluator boundary. This preserves the evaluator API and isolates risk. It is
   retained only if measured RSS stays within +5%; otherwise ownership must be redesigned before
   continuing.
4. Correctness remains query-by-query equality against independent full materialization. No delta
   result can directly establish consistency or safety.

## Correctness gates completed before measurement

- Local: 215/215 unit tests passed, one expected Z3-availability test skipped; four CAT/CAAT
  differential tests passed.
- Server Docker Release: 215/215 passed, one expected Z3-availability test skipped.
- Server Docker ASan+UBSan: 49/49 focused tests passed with leak detection and no sanitizer report.
- Server Docker end-to-end: SC, TSO, PSO, and recursive CAAT differential tests passed 4/4.

