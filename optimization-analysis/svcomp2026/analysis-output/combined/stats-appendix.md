# Combined statistical appendix

- Repeated-measure unit: task. Five runs are summarized to one median per
  backend and task before cross-task statistics.
- Wall-time, CPU, and RSS ratios are geometric means over fully paired,
  verdict-consistent tasks.
- Confidence intervals use 10,000 deterministic task-bootstrap samples.
- Win/loss inference uses the exact paired sign test with Holm correction for
  planned contrasts. Solved-only statistics are reported together with
  timeout/OOM/error coverage and failure-aware performance profiles.
- Scale associations use Spearman rank correlation between a separate CAAT
  diagnostic metric and the log paired CAAT/CAT wall ratio. Their 95%
  intervals use a deterministic 10,000-sample task bootstrap.
- The diagnostic run has one repetition and is not used for timing claims.
- External-tool census rows have no repeated trials. They support compatibility
  counts and descriptive resource summaries only, not significance claims.
- Different hosts, time limits, memory models, and data models remain separate
  comparison families.
