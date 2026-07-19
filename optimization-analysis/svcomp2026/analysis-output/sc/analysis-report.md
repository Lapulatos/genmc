# Paired BenchExec analysis

- Unit of analysis: task; repetitions are summarized within each task.
- Input rows: 1440; tasks: 96; repetitions per complete cell: 5.
- Baseline: `genmc-sc`.
- Cross-backend verdict mismatches excluded: 0.
- Tasks with at least one timeout/OOM/error: 11.
- Uncensored ratios include only tasks solved by both methods in every repetition.
- The performance profile retains failures as infinite cost.

## Pairwise wall-time results

- `caat-sc` / `genmc-sc`: 1.28894x (task-bootstrap 95% CI 1.11976–1.52048), CPU 1.30125x [1.13282, 1.54595], peak RSS 1.06163x [1.00436, 1.16214], n=90, wins/losses/ties=35/55/0, Holm sign-test p=0.133793.
- `cat-sc` / `genmc-sc`: 1.38721x (task-bootstrap 95% CI 1.14092–1.74247), CPU 1.40937x [1.14938, 1.79701], peak RSS 1.07569x [1.00373, 1.20284], n=85, wins/losses/ties=34/51/0, Holm sign-test p=0.164109.
- `caat-sc` / `cat-sc`: 0.854252x (task-bootstrap 95% CI 0.753391–0.94921), CPU 0.848872x [0.745519, 0.945812], peak RSS 0.990156x [0.974489, 1.00065], n=85, wins/losses/ties=50/35/0, Holm sign-test p=0.164109.
