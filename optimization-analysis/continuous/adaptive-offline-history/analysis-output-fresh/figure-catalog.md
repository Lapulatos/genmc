# Figure catalog

## Figure 01: metric ratios

- Files: `figures/figure-01-metric-ratios.pdf` and `.svg`.
- Purpose: show the fresh-build after/before effect and task-bootstrap uncertainty for
  CPU, wall, and peak RSS.
- Data: `metric-summary.tsv`; six-repetition per-task medians, 20,000 task bootstrap.
- Caption requirements: state that ratios below 1 favor the prototype; error bars are
  95% task-bootstrap intervals; aggregate contains 79 task clusters solved in all models.
- Observation: aggregate CPU and RSS cross 1; SC/PSO CPU intervals are above 1; TSO CPU
  is below 1.
- Interpretation: the optimization is model-heterogeneous and not a general production
  improvement.
- Caveat: panel x-axis ranges differ, especially the intentionally narrow RSS scale.

## Figure 02: CPU task distribution

- Files: `figures/figure-02-cpu-task-distribution.pdf` and `.svg`.
- Purpose: expose task dispersion hidden by aggregate point estimates.
- Data: `ratios.tsv`; boxplots plus all task points on a logarithmic ratio axis.
- Caption requirements: define each point as one task's ratio of six-run medians and the
  dashed line as no change.
- Observation: SC and PSO distributions shift above 1, TSO below 1, while aggregate
  remains close to 1.
- Interpretation: opposing model effects explain the neutral aggregate.
- Caveat: task points are not execution repetitions; repeated runs were collapsed first.

## Figure 03: retained history memory

- Files: `figures/figure-03-history-memory.pdf` and `.svg`.
- Purpose: verify that the implementation removes the intended history state even though
  process RSS does not move.
- Data: `profile-pairs.tsv`; 254 tasks with opt-in statistics in both profiles.
- Caption requirements: axes are `log10(1+bytes)` and the diagonal marks no change.
- Observation: 174 points move to zero after, 80 remain on the diagonal, none increase.
- Interpretation: the structural memory mechanism works, but its magnitude does not
  justify the SC/PSO CPU regressions.
- Caveat: each point is a per-run maximum, not simultaneous aggregate memory.

