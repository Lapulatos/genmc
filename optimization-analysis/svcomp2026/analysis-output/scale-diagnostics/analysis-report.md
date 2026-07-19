# CAT/CAAT scale-diagnostic analysis

- Timing ratios come only from the formal five-repetition paired runs.
- Graph metrics come from a separate one-repetition `--cat-stats` run and are not used as performance timings.
- CAT emits no incremental graph counters, so these metrics describe CAAT's internal workload; associations are not causal attributions.
- Comparable tasks with complete diagnostics: SC 83, TSO 80, PSO 77.
- Spearman correlations use task as the unit and deterministic task bootstrap confidence intervals.

## Interpretation boundary

A confidence interval crossing zero does not support a monotone association. Even when an interval excludes zero, the result only associates CAAT graph structure with the paired CAAT/CAT ratio; it does not identify which implementation operation caused the difference.
