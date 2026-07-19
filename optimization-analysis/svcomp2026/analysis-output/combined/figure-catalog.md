# Combined figure catalog

- `../sc/performance-profile.svg`: failure-aware SC performance profile.
- `../tso/performance-profile.svg`: failure-aware TSO performance profile.
- `../pso/performance-profile.svg`: failure-aware PSO performance profile.
- `../sc/coverage-status.svg`, `../tso/coverage-status.svg`, and
  `../pso/coverage-status.svg`: solved/timeout/OOM coverage across five runs.
- `../scale-diagnostics/stable-events-vs-wall-ratio.svg`: CAAT/CAT wall ratio
  versus CAAT stable event-domain size; equal time is the dashed line.
- `../scale-diagnostics/relation-density-vs-wall-ratio.svg`: CAAT/CAT wall
  ratio versus base-relation density.

The two scale figures contain only task-model cells that both CAT and CAAT
solve correctly in all five formal repetitions and whose separate CAAT
diagnostic run completed.
