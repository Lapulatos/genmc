# Statistics appendix

## Design

- Dataset: 96 selected SV-COMP 2026 C.Concurrency tasks.
- Models: recursive SC, TSO, and PSO.
- Repetitions: six before and six after for every model-task cell.
- Total: 3,456 run cells.
- Scheduling: simultaneous `-N24` queues on disjoint `0-23` and `28-51` CPU pools;
  pools swapped every repetition, three placements per variant after six repetitions.
- Limits: one CPU, 4 GiB, 60 seconds, `--nthreads=1` per task.
- Observed combined overlap: 45--48 tasks for every paired queue.

## Estimator and inference

For each model-task, only repetitions where both variants were BenchExec `correct` were
eligible. A task required all six paired repetitions. The effect is the ratio of after
median to before median. Point estimates are geometric means of task ratios. Confidence
intervals use 20,000 fixed-seed task bootstrap samples.

The aggregate avoids treating three rows from one program as independent: it requires a
task to have SC/TSO/PSO ratios, takes their geometric mean, and bootstraps the resulting
79 task clusters. No parametric normality or variance assumptions are used. Exact sign
tests are secondary and Holm-corrected across the four aggregate/model contrasts within
each metric family.

## Fresh-build descriptive and inferential statistics

| Metric | Group | n | Geomean | 95% CI | Mean ± SD | Median [IQR] |
|---|---|---:|---:|---:|---:|---:|
| CPU | all | 79 | 1.00250 | [0.99779, 1.00734] | 1.00274 ± 0.02186 | 1.00265 [0.99023, 1.01249] |
| CPU | SC | 90 | 1.01209 | [1.00501, 1.01946] | 1.01269 ± 0.03546 | 1.01043 [0.99685, 1.02768] |
| CPU | TSO | 89 | 0.98967 | [0.98288, 0.99659] | 0.99022 ± 0.03317 | 0.98574 [0.96899, 1.00761] |
| CPU | PSO | 79 | 1.01211 | [1.00368, 1.02084] | 1.01285 ± 0.03929 | 1.01346 [0.98831, 1.02780] |
| Wall | all | 79 | 0.99367 | [0.97610, 1.00968] | 0.99646 ± 0.07196 | 0.99935 [0.98179, 1.02888] |
| Wall | SC | 90 | 0.99017 | [0.97496, 1.00447] | 0.99271 ± 0.06964 | 1.00443 [0.95948, 1.02792] |
| Wall | TSO | 89 | 0.96439 | [0.93337, 0.99253] | 0.97370 ± 0.12361 | 0.99033 [0.95105, 1.02835] |
| Wall | PSO | 79 | 1.03797 | [1.01156, 1.06646] | 1.04599 ± 0.14231 | 1.02082 [0.99043, 1.06321] |
| RSS | all | 79 | 0.99995 | [0.99978, 1.00012] | 0.99996 ± 0.00079 | 1.00008 [0.99950, 1.00043] |

## CPU sign tests

| Group | After faster / slower / ties | Exact two-sided p | Holm-adjusted p |
|---|---:|---:|---:|
| all | 35 / 44 / 0 | 0.36819 | 0.36819 |
| SC | 25 / 65 / 0 | 0.0000297 | 0.0001187 |
| TSO | 62 / 27 / 0 | 0.0002656 | 0.0007968 |
| PSO | 26 / 53 / 0 | 0.003183 | 0.006366 |

The sign test concerns direction counts; the bootstrap concerns the geometric-mean
magnitude. The aggregate sign test is not significant even though SC/TSO/PSO have
opposing supported directions.

## Correctness and coverage accounting

| Check | Result |
|---|---:|
| Fresh before/after rows | 1,728 / 1,728 |
| Correct run cells | 1,553 / 1,552 |
| Incorrect verdict cells | 0 / 0 |
| Common-solved verdict mismatches | 0 |
| Safe execution-count mismatches | 0 |
| Unit/property tests | 142/142 |
| Mutation oracle checks | 5,441 |
| Broad comparable pairs / mismatches | 852 / 0 |

Status changes were 20 OOM→timeout, 11 timeout→OOM, one timeout-with-result→unsafe
completion, one unsafe completion→timeout, and one unsafe completion→timeout-with-result.
All resource-limited cells are treated as unknown, not correct or incorrect evidence.

## Limitations

- Peak RSS measures the whole process and is insensitive to small checker allocations.
- The structural profile was a separate opt-in pass; its timing counters are not used as
  controlled before/after timing evidence.
- The 96-task sample is broad but finite. TSO-only selection would require a pre-specified
  held-out validation rather than reuse of these model-specific findings.
- The first and fresh matrices are reported separately because pooling after inspecting
  their difference would overstate certainty.

