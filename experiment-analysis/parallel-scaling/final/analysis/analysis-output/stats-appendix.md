# Statistical appendix

- Unit of analysis: SV-COMP task. Five repetitions are aggregated within each task before bootstrap resampling; the 15,360 task-runs are not treated as independent samples.
- Primary effect: geometric mean of paired wall-time ratios (`t1 / parallel`) on pairs where both runs return a verdict.
- Uncertainty: deterministic 10,000-resample task bootstrap, seed 20260715; percentile 95% intervals.
- CPU and RSS ratios use the same common-solved pairing in `speedup.tsv`.
- Timeout/ABORTED rows are included in completion rates but excluded from common-solved timing ratios. This avoids assigning arbitrary times while making coverage loss explicit.
- No p-value winner claims are made: the 96 tasks are a selected public corpus, not a random population sample, and missingness depends on method/thread configuration. Effect estimates and intervals are more interpretable here.
- Multiple comparisons: all 21 planned model/method/thread contrasts are reported; no post-hoc best-only selection is used.
