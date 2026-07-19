# Statistical appendix

| Metric | Group | n | Geomean | 95% CI | Median | Holm p |
|---|---|---:|---:|---:|---:|---:|
| cpu | all | 81 | 1.04343 | [1.00788, 1.09055] | 1.02372 | 1.697e-05 |
| cpu | sc | 90 | 1.00179 | [0.99339, 1.00968] | 1.00165 | 0.752 |
| cpu | tso | 89 | 1.00138 | [0.99508, 1.00761] | 0.99672 | 0.2747 |
| cpu | pso | 81 | 1.13116 | [1.01913, 1.29104] | 1.04766 | 3.45e-08 |
| wall | all | 81 | 1.03929 | [1.00462, 1.08558] | 1.01916 | 0.0003798 |
| wall | sc | 90 | 0.99954 | [0.99273, 1.00621] | 0.99628 | 0.4922 |
| wall | tso | 89 | 0.99801 | [0.99216, 1.00391] | 0.99831 | 0.525 |
| wall | pso | 81 | 1.12467 | [1.01455, 1.28235] | 1.05319 | 2.082e-06 |
| rss | all | 81 | 1.01808 | [1.00038, 1.04728] | 1.00008 | 1 |
| rss | sc | 90 | 0.99992 | [0.99965, 1.00021] | 0.99978 | 0.4963 |
| rss | tso | 89 | 0.99996 | [0.99965, 1.00025] | 0.99992 | 1 |
| rss | pso | 81 | 1.05538 | [1.00125, 1.14823] | 1.00031 | 0.3551 |

- Unit: one task; four repetitions are paired and summarized by a median.
- Aggregate unit geometrically combines models only for tasks terminal-correct in all cells.
- CI: 20,000 fixed-seed task-cluster bootstrap resamples.
- Effect size: multiplicative cache/baseline ratio; lower is better.
- Holm correction covers the four task groups within each metric.
