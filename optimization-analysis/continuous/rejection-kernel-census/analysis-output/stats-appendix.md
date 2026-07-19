# Statistical appendix

## Exact metric table

| Metric | Group | n tasks | Geomean ratio | 95% CI | Median | Sign p | Holm p |
|---|---|---:|---:|---:|---:|---:|---:|
| cpu | all | 76 | 1.20162 | [1.12795, 1.29234] | 1.06688 | 6.418e-14 | 2.567e-13 |
| cpu | sc | 89 | 1.01950 | [1.01217, 1.02706] | 1.02036 | 4.322e-05 | 8.643e-05 |
| cpu | tso | 89 | 1.01672 | [1.00842, 1.02616] | 1.01632 | 0.0001099 | 0.0001099 |
| cpu | pso | 76 | 1.67688 | [1.39042, 2.08506] | 1.13901 | 6.418e-14 | 2.567e-13 |
| wall | all | 76 | 1.19361 | [1.12291, 1.28370] | 1.05723 | 6.418e-14 | 1.925e-13 |
| wall | sc | 89 | 1.01720 | [1.00860, 1.02595] | 1.01750 | 0.0001099 | 0.0001099 |
| wall | tso | 89 | 1.01843 | [1.00973, 1.02825] | 1.01905 | 4.322e-05 | 8.643e-05 |
| wall | pso | 76 | 1.64617 | [1.37302, 2.02245] | 1.14426 | 5.249e-16 | 2.1e-15 |
| rss | all | 76 | 1.00159 | [0.99998, 1.00415] | 1.00008 | 0.4222 | 1 |
| rss | sc | 89 | 1.00015 | [0.99982, 1.00048] | 1.00028 | 0.3374 | 1 |
| rss | tso | 89 | 1.00000 | [0.99970, 1.00031] | 0.99978 | 0.278 | 1 |
| rss | pso | 76 | 1.00455 | [0.99978, 1.01224] | 1.00000 | 1 | 1 |

## Test design

- Ratio per model/task uses the median of three paired repetitions.
- The primary `all` unit is one task, geometrically aggregating SC/TSO/PSO ratios only when all three are available.
- Confidence intervals use 20,000 fixed-seed task-cluster bootstrap resamples.
- The exact two-sided sign test is distribution-free; no repetition or model cell is treated as an independent subject.
- Holm adjustment covers the four task groups within each metric; model rows remain diagnostic sensitivity analyses.
- Ratio and its confidence interval are the reported multiplicative effect size; no normality assumption is used.

## Correctness and censoring

- Status mismatches: 27; all are resource/termination perturbations caused by measurement overhead, not opposite terminal verdicts.
- Safe execution-count mismatches: 0.
- TIMEOUT/OOM resource-cohort checkpoints are right-censored and used only for opportunity counters.
