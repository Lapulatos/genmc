# Paired BenchExec analysis

- Unit of analysis: task; repetitions are summarized within each task.
- Input rows: 1440; tasks: 96; repetitions per complete cell: 5.
- Baseline: `genmc-tso`.
- Cross-backend verdict mismatches excluded: 0.
- Tasks with at least one timeout/OOM/error: 14.
- Uncensored ratios include only tasks solved by both methods in every repetition.
- The performance profile retains failures as infinite cost.

## Pairwise wall-time results

- `caat-tso` / `genmc-tso`: 1.34973x (task-bootstrap 95% CI 1.14324–1.63094), CPU 1.36491x [1.15811, 1.6606], peak RSS 1.08515x [1.01092, 1.21577], n=89, wins/losses/ties=34/55/0, Holm sign-test p=0.0668334.
- `cat-tso` / `genmc-tso`: 1.30844x (task-bootstrap 95% CI 1.0961–1.62978), CPU 1.33966x [1.11751, 1.6754], peak RSS 1.04671x [1.01018, 1.12358], n=82, wins/losses/ties=28/54/0, Holm sign-test p=0.0163078.
- `caat-tso` / `cat-tso`: 0.854544x (task-bootstrap 95% CI 0.745459–0.952066), CPU 0.842517x [0.732786, 0.939592], peak RSS 0.995502x [0.98507, 1.00081], n=82, wins/losses/ties=51/31/0, Holm sign-test p=0.0668334.
