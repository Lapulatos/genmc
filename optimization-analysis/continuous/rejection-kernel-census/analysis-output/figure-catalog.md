# Figure catalog

## `figures/figure-01-formal-overhead.svg`

- Purpose: show the measurement implementation's CPU, wall, and RSS cost.
- Data source: three-repetition 96-task paired BenchExec XML.
- Error bars: 95% task-cluster bootstrap CI of the geometric mean ratio.
- Notice: values above one are overhead; the PSO row includes expensive Reasoner work.
- Decision impact: the census implementation is measurement-only and must be removed.

## `figures/figure-02-large-task-opportunity.svg`

- Purpose: distinguish high conditional reuse among rejected queries from actual all-query coverage.
- Data source: latest complete checkpoint in each 60-task TIMEOUT/OOM log.
- Bars: pale is hit/rejected; solid is hit/all consistency queries.
- Notice: SC/TSO conditional rates look high but their all-query rates are negligible; PSO is qualitatively different.
- Decision impact: scope P0.2 to generic PSO/order-model paths and use a cheap activation certificate.
