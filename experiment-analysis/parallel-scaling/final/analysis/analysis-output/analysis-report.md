# Parallel-scaling analysis report

## Analysis question

Does GenMC's `--nthreads` reduce wall time without changing verdicts or complete safe-task exploration, and what CPU, coverage, and RSS costs result for GenMC, CAT, and CAAT under SC/TSO/PSO?

## Key findings

- The matrix is complete: 15,360 runs over 96 tasks, five repetitions, and 160 method/model/thread cells. There are zero wrong verdicts, duplicates, safe-task exploration-count mismatches, or missing safe counts.
- Parallel exploration is not an effective default on this corpus. Across all 21 parallel comparisons, the largest wall-speedup is 1.046 (SC CAAT, four workers), while eight-worker CPU-efficiency ratios range from 0.731 to 0.766.
- Coverage generally falls as worker count rises because GenMC's parallel runtime produces ABORTED/other unknown results on unsafe tasks. These are coverage losses, not false positives or false negatives.
- Peak RSS changes little for CAT/CAAT on common-solved pairs (eight-worker inverse ratios 0.978--0.991) but GenMC's ratios fall to 0.947--0.950.
- The sampled CAAT oracle covers 288 runs and 2,444 full recomputation checks with zero verdict disagreement. This supports the tested incremental evaluator states; it is not a proof for all programs or schedules.

## Exact summary

| Model | Method | Threads | Solved/480 | Wall speedup | CPU efficiency | RSS inverse |
|---|---:|---:|---:|---:|---:|---:|
| PSO | CAAT | 1 | 395/480 | 1.000 | 1.000 | 1.000 |
| PSO | CAAT | 2 | 392/480 | 1.003 | 0.882 | 0.999 |
| PSO | CAAT | 4 | 389/480 | 0.969 | 0.822 | 0.995 |
| PSO | CAAT | 8 | 386/480 | 0.919 | 0.739 | 0.989 |
| PSO | CAT | 1 | 410/480 | 1.000 | 1.000 | 1.000 |
| PSO | CAT | 2 | 404/480 | 1.026 | 0.914 | 0.997 |
| PSO | CAT | 4 | 411/480 | 0.983 | 0.833 | 0.996 |
| PSO | CAT | 8 | 412/480 | 0.910 | 0.735 | 0.989 |
| SC | CAAT | 1 | 450/480 | 1.000 | 1.000 | 1.000 |
| SC | CAAT | 2 | 445/480 | 0.969 | 0.839 | 0.999 |
| SC | CAAT | 4 | 434/480 | 1.046 | 0.830 | 0.996 |
| SC | CAAT | 8 | 428/480 | 1.035 | 0.753 | 0.978 |
| SC | CAT | 1 | 445/480 | 1.000 | 1.000 | 1.000 |
| SC | CAT | 2 | 417/480 | 1.006 | 0.889 | 0.996 |
| SC | CAT | 4 | 419/480 | 0.991 | 0.832 | 0.994 |
| SC | CAT | 8 | 414/480 | 0.952 | 0.766 | 0.991 |
| SC | GENMC | 1 | 480/480 | 1.000 | 1.000 | 1.000 |
| SC | GENMC | 2 | 471/480 | 0.949 | 0.820 | 0.965 |
| SC | GENMC | 4 | 460/480 | 1.000 | 0.783 | 0.929 |
| SC | GENMC | 8 | 456/480 | 1.015 | 0.733 | 0.947 |
| TSO | CAAT | 1 | 445/480 | 1.000 | 1.000 | 1.000 |
| TSO | CAAT | 2 | 444/480 | 0.943 | 0.826 | 0.999 |
| TSO | CAAT | 4 | 432/480 | 1.005 | 0.809 | 0.996 |
| TSO | CAAT | 8 | 424/480 | 0.985 | 0.736 | 0.978 |
| TSO | CAT | 1 | 410/480 | 1.000 | 1.000 | 1.000 |
| TSO | CAT | 2 | 402/480 | 1.038 | 0.921 | 0.997 |
| TSO | CAT | 4 | 406/480 | 1.003 | 0.840 | 0.995 |
| TSO | CAT | 8 | 411/480 | 0.933 | 0.748 | 0.989 |
| TSO | GENMC | 1 | 480/480 | 1.000 | 1.000 | 1.000 |
| TSO | GENMC | 2 | 471/480 | 0.932 | 0.811 | 0.967 |
| TSO | GENMC | 4 | 462/480 | 1.013 | 0.798 | 0.940 |
| TSO | GENMC | 8 | 454/480 | 1.010 | 0.731 | 0.950 |

## Claim candidates

- Claim: Increasing `--nthreads` from 1 to 8 did not provide meaningful wall-time speedup on the tested 96-task corpus.
  - Source evidence: common-solved geometric speedups in `speedup.tsv`; task-paired bootstrap intervals in `paired-task-bootstrap.tsv`.
  - Allowed wording: "On this corpus and host, eight workers were approximately flat or slower while using substantially more total CPU."
  - Forbidden stronger wording: "GenMC parallel exploration can never speed up."
  - Uncertainty: task mix is timeout-heavy and limited to 96 public C.Concurrency tasks.
  - Decision: keep.

- Claim: Enabling CAT/CAAT did not introduce observed false alarms or missed bugs.
  - Source evidence: 15,360 formal rows and 288 oracle rows, both with zero verdict errors.
  - Allowed wording: "No wrong verdict was observed in the tested matrix."
  - Forbidden stronger wording: "CAT/CAAT are proven sound and complete."
  - Uncertainty: finite sample; parallel crashes are unknown results.
  - Decision: keep with scope.
