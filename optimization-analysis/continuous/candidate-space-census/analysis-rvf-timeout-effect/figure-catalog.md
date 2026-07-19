# Figure catalog

## Figure 1: `figures/figure-01-rvf-fraction-ecdf.svg`

- Purpose: compare the full task-level opportunity distributions rather than only aggregate totals.
- Data: counter-bearing completed and TIMEOUT rows; panels are SC, TSO and PSO.
- Caption requirements: define the optimistic fraction, ECDF, outcome groups and per-panel sample sizes from the descriptive table.
- Observation: TIMEOUT curves are shifted toward larger fractions, while many completed tasks have no RF opportunity.
- Interpretation: the unadjusted difference motivates a hard-cohort experiment but is partly explained by search size.
- Caveat: the plot does not control for RF search size and does not measure realizable RVF classes or runtime saved.

## Figure 2: `figures/figure-02-opportunity-vs-search-size.svg`

- Purpose: show whether the association is confined to tiny traces or persists at large RF candidate counts.
- Data: completed, TIMEOUT and OOM rows with counters; x is logarithmic RF offered and y is the optimistic fraction.
- Caption requirements: state log scale, point transparency and that each point is one task/model row.
- Observation: large completed and TIMEOUT searches both cluster around roughly 28–30%; TIMEOUT rows dominate the largest-search region, while OOM counter coverage is sparse.
- Interpretation: absolute removable-candidate opportunity grows in the timeout cohort, but fractional enrichment beyond search size is not established.
- Caveat: overlapping points and censored 60-second runs prevent a causal slope estimate.
