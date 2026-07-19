# Figure catalog

## `performance-profile.svg`

- Purpose: compare task coverage and relative wall time without dropping failures.
- Data: per-task median wall time across complete repetitions; failures have infinite cost.
- Read: higher curves are better; the right-edge height is the solved-task fraction.
- Caveat: close curves on sub-0.1 s tasks are sensitive to process-startup noise.

## `coverage-status.svg`

- Purpose: expose solved, timeout, OOM, and other outcomes before solved-only ratios.
- Data: exact repeated-run counts from `descriptive-summary.tsv`.
- Read: compare both solved fraction and the type of resource failure.
- Caveat: repeated runs are shown for stability, not treated as independent tasks.
