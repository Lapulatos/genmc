# Paired BenchExec analysis

- Unit of analysis: task; repetitions are summarized within each task.
- Input rows: 3625; tasks: 725; repetitions per complete cell: 1.
- Baseline: `Deagle`.
- Cross-backend verdict mismatches excluded: 24.
- Tasks with at least one timeout/OOM/error: 371.
- Uncensored ratios include only tasks solved by both methods in every repetition.
- The performance profile retains failures as infinite cost.

## Pairwise wall-time results

- `GenMC` / `Deagle`: 0.195917x (task-bootstrap 95% CI 0.153459–0.251961), CPU 0.171327x [0.131583, 0.222843], peak RSS 0.987891x [0.891646, 1.09296], n=375, wins/losses/ties=310/65/0, Holm sign-test p=8.98579e-39.
- `GenMC+CAAT-PSO` / `Deagle`: 0.141773x (task-bootstrap 95% CI 0.11083–0.183126), CPU 0.122751x [0.0942487, 0.16182], peak RSS 0.918423x [0.822962, 1.02311], n=330, wins/losses/ties=284/46/0, Holm sign-test p=3.66381e-42.
- `GenMC+CAAT-SC` / `Deagle`: 0.180484x (task-bootstrap 95% CI 0.141061–0.232856), CPU 0.159379x [0.122443, 0.209292], peak RSS 0.979084x [0.881514, 1.08342], n=367, wins/losses/ties=305/62/0, Holm sign-test p=5.71205e-39.
- `GenMC+CAAT-TSO` / `Deagle`: 0.163391x (task-bootstrap 95% CI 0.129356–0.209776), CPU 0.144266x [0.111147, 0.189278], peak RSS 0.966747x [0.871765, 1.07293], n=359, wins/losses/ties=301/58/0, Holm sign-test p=6.45534e-40.
- `GenMC+CAAT-SC` / `GenMC`: 1.08958x (task-bootstrap 95% CI 1.0446–1.13705), CPU 1.10209x [1.06593, 1.14047], peak RSS 1.05819x [1.02343, 1.10228], n=404, wins/losses/ties=195/209/0, Holm sign-test p=1.
- `GenMC+CAAT-TSO` / `GenMC`: 1.11747x (task-bootstrap 95% CI 1.06011–1.17916), CPU 1.14077x [1.08841, 1.19668], peak RSS 1.01958x [1.01055, 1.03413], n=391, wins/losses/ties=197/194/0, Holm sign-test p=1.
- `GenMC+CAAT-PSO` / `GenMC`: 1.39358x (task-bootstrap 95% CI 1.25175–1.55845), CPU 1.43059x [1.28547, 1.60613], peak RSS 1.02471x [1.01105, 1.04566], n=360, wins/losses/ties=182/178/0, Holm sign-test p=1.
