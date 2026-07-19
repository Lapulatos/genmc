# P0.7g2 figure catalog

## Figure 1: CPU ratios

- Files: `figures/figure-01-cpu-ratios.pdf` and `.png`.
- Purpose: show the overall and per-model task-cluster CPU point estimates with 95%
  clustered bootstrap intervals relative to the no-change line at 1.
- Data source: `server-results/formal/analysis/summary.json`.
- Error bars: 20,000-resample model-task clustered bootstrap 95% intervals.
- Reader should notice: only SC's interval is wholly below 1; the overall, TSO and PSO
  intervals cross 1.
- Decision implication: the generic retain gate fails even though the overall point
  estimate is slightly below 1.
- Caveat: intervals describe sampled task-model variation and exclude clusters without
  four common-terminal repetitions.

## Figure 2: memory ratios

- Files: `figures/figure-02-memory-ratios.pdf` and `.png`.
- Purpose: show the paired RSS ratio distribution by memory model and its distance from
  the frozen 1.02 regression boundary.
- Data source: `server-results/formal/analysis/pairs.tsv`.
- Box/whisker meaning: box is interquartile range, red line is median, whiskers are the
  10th and 90th percentiles; outliers are intentionally omitted from the plot and
  reported exactly in `stats-appendix.md`.
- Reader should notice: all central distributions are centered near 1 and far below
  1.02.
- Decision implication: P0.7g2 fixed the earlier layout-associated memory issue; CPU,
  not RSS, causes rejection.
- Caveat: the plot is descriptive and process RSS includes allocator/runtime noise.
