# Six-method adapted C.Concurrency comparison

## Scope

- Dataset: the same 725 adapted SV-COMP 2026 `C.Concurrency` tasks.
- Limits: 60 CPU seconds, 4 GiB, one core per task.
- GenMC-family nondeterministic input: fixed seed 1995; exploration is complete only
  for that input stream.
- Current CAAT binary: retained P0.7d2 build, committed as
  `d963c49e45b82066ca9dcd036232c7384f6a95fe`.
- Previous CAAT result: the archived post-dataset-repair direct SC run from
  2026-07-15. Its BenchExec XML records GenMC's version as `commit #unknown`, so the
  archived XML/log identity, rather than an unverified source SHA, defines this column.
- No historical task was rerun to create this comparison.

## Coverage

| Method | Correct | Wrong | Terminal | TIMEOUT | OOM | Other error |
|---|---:|---:|---:|---:|---:|---:|
| Previous GenMC+CAAT-SC | 388 | 30 | 418 | 224 | 57 | 26 |
| GenMC | 408 | 31 | 439 | 210 | 50 | 26 |
| Current GenMC+CAAT-SC | 398 | 30 | 428 | 240 | 31 | 26 |
| Current GenMC+CAAT-TSO | 385 | 30 | 415 | 253 | 31 | 26 |
| Current GenMC+CAAT-PSO | 355 | 29 | 384 | 274 | 41 | 26 |
| Deagle | 615 | 3 | 618 | 16 | 10 | 81 |

TSO and PSO are shown descriptively against SC-labelled SV-COMP tasks. Their
`correct`/`wrong` labels are not model-specific competition scores.

## Previous versus current CAAT-SC

- Correct results: 388 -> 398; terminal verdicts: 418 -> 428.
- OOM: 57 -> 31; TIMEOUT: 224 -> 240.
- Resource-to-terminal improvements: five OOM -> false, four TIMEOUT -> false, and
  one TIMEOUT -> true.
- Twenty-one former OOM tasks now reach the time limit instead of the memory limit.
- There are no terminal-verdict changes and no terminal-to-resource regressions.
- On the 418 same-verdict terminal pairs, current/previous CPU geomean is 0.915446
  (task-bootstrap 95% CI [0.894591, 0.934561]); RSS geomean is 0.994343
  ([0.986828, 1.00024]).

This means the cumulative optimization primarily reduces memory growth and makes ten
additional tasks terminal. The larger TIMEOUT count is mostly a reclassification of
21 old OOM cases, not a loss of previously solved tasks.

## Artifacts and verification

- Full table: `html/adapted-725-previous-latest-sc-tso-pso-genmc-deagle-60s.table.html`
- Difference-only table: `html/adapted-725-previous-latest-sc-tso-pso-genmc-deagle-60s.diff.html`
- Raw XML, 4,350 logs, and the 725-task snapshot: `raw/`
- Table definition: `table-definition.xml`; it deliberately keeps only status,
  category, total CPU time, wall time, memory, and termination reason. Per-core
  `cputime-cpuNN` columns remain available in the raw XML but are not rendered.
- Table verification: 725 unique rows; six non-empty status cells per row; 4,350/4,350
  log targets and 725/725 task targets exist.

`table-generator` warned that the local experiment-only GenMC ToolInfo module was not
installed. This affects optional re-extraction from log text, not the status, CPU,
wall-time, memory, termination reason, or other columns already stored in the six XML
files.
