# Paired BenchExec analysis

- Unit of analysis: task; repetitions are summarized within each task.
- Input rows: 960; tasks: 96; repetitions per complete cell: 5.
- Baseline: `cat-pso`.
- Cross-backend verdict mismatches excluded: 0.
- Tasks with at least one timeout/OOM/error: 17.
- Uncensored ratios include only tasks solved by both methods in every repetition.
- The performance profile retains failures as infinite cost.

## Pairwise wall-time results

- `caat-pso` / `cat-pso`: 1.0748x (task-bootstrap 95% CI 1.02843–1.13215), CPU 1.09134x [1.04457, 1.15301], peak RSS 1.00119x [1.00102, 1.0013], n=79, wins/losses/ties=32/47/0, Holm sign-test p=0.11466.
