# GenMC optimization branch workflow

All optimization development starts on `genmc-caat-opt-dev`.  The branch owns intermediate
implementations, diagnostics, experiment launchers, compact results, rejected directions, and
research notes.

The stable `genmc-caat` branch receives only optimization commits that have passed the required
correctness and performance gates.  Promotion uses an explicit merge or cherry-pick from
`genmc-caat-opt-dev`; experiments are never developed directly on `genmc-caat`.

## Promotion gate

1. Build and run the relevant unit/property tests in a clean snapshot.
2. Pass sanitizer, mutation-oracle, and broad-differential checks proportional to the change.
3. Run the fixed diagnostic panel before expanding to larger paired benchmark sets.
4. Audit terminal verdicts, resolved-case regressions, TIMEOUT/OOM transitions, CPU, wall, and RSS.
5. Record the retained/rejected decision and raw-evidence location.
6. Merge only the minimal effective implementation, tests, launcher, and evidence report into
   `genmc-caat`.

## Repository hygiene

Compiler build trees, downloaded binaries, local assistant state, credentials, raw per-task logs,
ZIP archives, and generated HTML stay outside Git.  Compact BenchExec XML, CSV/TSV/JSON summaries,
analysis scripts, figures, and decision reports may be versioned on the development branch.
