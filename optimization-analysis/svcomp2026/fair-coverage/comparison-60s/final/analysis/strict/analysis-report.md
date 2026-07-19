# Paired BenchExec analysis

- Unit of analysis: task; repetitions are summarized within each task.
- Input rows: 2900; tasks: 725; repetitions per complete cell: 1.
- Baseline: `GenMC`.
- Cross-backend verdict mismatches excluded: 24.
- Tasks with at least one timeout/OOM/error: 376.
- Uncensored ratios include only tasks solved by both methods in every repetition.
- The performance profile retains failures as infinite cost.

## Pairwise wall-time results

- `Deagle` / `GenMC`: 5.29121x (task-bootstrap 95% CI 4.06603–6.83277), CPU 5.52282x [4.23306, 7.20335], peak RSS 1.01895x [0.920329, 1.1288], n=375, wins/losses/ties=63/312/0, Holm sign-test p=4.76937e-40.
- `GenMC+CAAT` / `GenMC`: 1.28535x (task-bootstrap 95% CI 1.22276–1.35293), CPU 1.16232x [1.10484, 1.22622], peak RSS 1.02263x [1.0049, 1.04641], n=394, wins/losses/ties=183/211/0, Holm sign-test p=0.173677.
- `GenMC+CAT` / `GenMC`: 1.67894x (task-bootstrap 95% CI 1.50677–1.88624), CPU 1.39319x [1.24038, 1.57996], peak RSS 1.02362x [1.00366, 1.05156], n=355, wins/losses/ties=95/260/0, Holm sign-test p=2.19065e-18.
- `GenMC+CAAT` / `GenMC+CAT`: 0.692344x (task-bootstrap 95% CI 0.628602–0.756588), CPU 0.745772x [0.677823, 0.814432], peak RSS 0.99996x [0.995059, 1.00579], n=355, wins/losses/ties=250/105/0, Holm sign-test p=1.80338e-14.
- `Deagle` / `GenMC+CAT`: 6.68705x (task-bootstrap 95% CI 5.2–8.59114), CPU 8.59454x [6.54904, 11.29], peak RSS 1.12004x [1.00402, 1.25475], n=325, wins/losses/ties=41/284/0, Holm sign-test p=4.26578e-45.
- `Deagle` / `GenMC+CAAT`: 5.26845x (task-bootstrap 95% CI 3.99226–6.82472), CPU 6.13995x [4.65571, 8.0267], peak RSS 1.04026x [0.938883, 1.15298], n=362, wins/losses/ties=64/298/0, Holm sign-test p=1.30092e-36.
