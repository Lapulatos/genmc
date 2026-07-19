# Optimization 05b: adaptive-offline history elision

## Final decision

**Rejected and removed from production source.** Correctness gates passed and the
intended history allocation disappeared, but two complete 3,456-cell matrices did not
show a general speedup. The build-controlled matrix was aggregate-neutral and produced
supported 1.21% CPU regressions on both SC and PSO. TSO's post-hoc improvement is not
sufficient to introduce a model-specific policy.

## Evidence summary

- Correctness: 142/142 tests, 5,441 mutation oracle checks, 864 broad pairs with zero
  mismatch, zero formal verdict/exploration mismatch.
- First matrix aggregate CPU: 1.00524 [1.00054, 1.00995].
- Fresh-build aggregate CPU: 1.00250 [0.99779, 1.00734].
- Fresh SC/TSO/PSO CPU: 1.01209 / 0.98967 / 1.01211, with every model interval excluding
  1 in the corresponding direction.
- Structural profile: history maximum decreased on 174/254 common tasks, never increased;
  nonzero history tasks fell from 178 to 4.
- Actual combined BenchExec overlap: 45--48 tasks in every paired queue.

## Artifacts

- Raw first matrix: `server/formal/`; fresh-build matrix: `server/formal-fresh/`.
- Each matrix contains 36 XML files, 36 complete log archives, console logs, and progress.
- Strict fresh analysis: `analysis-output-fresh/`.
- Original and fresh normalized rows: `analysis/rows.tsv` and `analysis-fresh/rows.tsv`.
- Structural profile: `server/profile-after/`, `analysis/profile-after.tsv`, and
  `analysis/profile-after.json`.
- Reproduction protocol and runners: `experiment-protocol.md`, `run_paired*.sh`, and
  `definitions/`.
