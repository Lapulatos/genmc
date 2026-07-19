# RVF opportunity and non-termination analysis

## Analysis question

Is the optimistic same-value RF opportunity concentrated in tasks that fail to finish within 60 seconds? The unit of analysis is one task within one memory model. Counter-bearing rows are included; a row with zero offered RF candidates has zero operational quotient opportunity.

## Key findings

- SC: the task-level median is 29.63% for TIMEOUT versus 0.00% for completed verdicts. Mann–Whitney U=72295.0, Holm-adjusted p=1.77e-46, rank-biserial effect=0.689; the bootstrap 95% CI for the median difference is [29.39%, 29.80%].
- TSO: the task-level median is 29.39% for TIMEOUT versus 0.00% for completed verdicts. Mann–Whitney U=73110.0, Holm-adjusted p=7.6e-47, rank-biserial effect=0.686; the bootstrap 95% CI for the median difference is [29.20%, 29.63%].
- PSO: the task-level median is 28.77% for TIMEOUT versus 0.00% for completed verdicts. Mann–Whitney U=65975.0, Holm-adjusted p=2.49e-44, rank-biserial effect=0.679; the bootstrap 95% CI for the median difference is [28.77%, 29.44%].

The unadjusted association is large and consistent across the three models, but it is partly a search-size effect: many completed tasks offer no RF choice at all. It supports measuring a quotient on the hard cohort; it does not show that an unusually high same-value fraction causes timeout.

## Exact descriptive table

| Model | Outcome | n with counters | Missing | Median [IQR] | Per-task mean | Aggregate weighted |
|---|---:|---:|---:|---:|---:|---:|
| SC | complete | 428 | 0 | 0.00% [0.00%, 27.44%] | 10.97% | 24.30% |
| SC | timeout | 200 | 39 | 29.63% [28.75%, 30.42%] | 29.24% | 29.67% |
| SC | oom | 4 | 28 | 0.00% [0.00%, 0.00%] | 0.00% | 0.00% |
| TSO | complete | 415 | 0 | 0.00% [0.00%, 27.29%] | 10.54% | 17.71% |
| TSO | timeout | 209 | 43 | 29.39% [28.63%, 30.32%] | 29.00% | 29.55% |
| TSO | oom | 4 | 27 | 0.00% [0.00%, 0.00%] | 0.00% | 0.00% |
| PSO | complete | 401 | 0 | 0.00% [0.00%, 26.68%] | 10.04% | 21.39% |
| PSO | timeout | 196 | 80 | 28.77% [27.88%, 29.87%] | 29.16% | 28.47% |
| PSO | oom | 2 | 20 | 0.00% [0.00%, 0.00%] | 0.00% | 0.00% |

## Search-size sensitivity

The table below repeats the task-level medians after excluding small searches. The 10,000-candidate row is the most useful common-support check; 100,000 leaves too few completed TSO/PSO tasks for a stable comparison.

| Model | Minimum RF offered | Completed n | Completed median | TIMEOUT n | TIMEOUT median | Difference |
|---|---:|---:|---:|---:|---:|---:|
| SC | 1,000 | 68 | 28.36% | 200 | 29.63% | +1.28% |
| SC | 10,000 | 43 | 29.01% | 199 | 29.63% | +0.62% |
| SC | 100,000 | 10 | 28.36% | 183 | 29.67% | +1.31% |
| TSO | 1,000 | 60 | 28.19% | 209 | 29.39% | +1.20% |
| TSO | 10,000 | 35 | 29.01% | 208 | 29.38% | +0.37% |
| TSO | 100,000 | 2 | 1.00% | 131 | 30.29% | +29.30% |
| PSO | 1,000 | 46 | 28.09% | 196 | 28.77% | +0.68% |
| PSO | 10,000 | 23 | 29.54% | 195 | 28.77% | -0.77% |
| PSO | 100,000 | 1 | 0.00% | 29 | 30.31% | +30.31% |

## Claim candidates

- Claim:
  - Source evidence: all 2,175 BenchExec task/model rows; task-level non-parametric contrasts above.
  - Allowed wording: Counter-bearing TIMEOUT tasks expose an aggregate 28–30% optimistic same-value RF opportunity and millions of concrete alternatives, so the hard cohort is a relevant target for an RVF experiment.
  - Forbidden stronger wording: TIMEOUT tasks have a uniquely larger same-value fraction after controlling for search size, or RVF will eliminate roughly 30% of runtime/timeouts.
  - Uncertainty: the unadjusted contrast is confounded by search size; same-value grouping is only an upper bound; full RVF classes and witness-generation cost are unmeasured.
  - Next check: integrate the SC representative generator and compare actual offered/queued/popped candidates on the same tasks.
  - Decision: weaken and keep.

## Limits

- Counter logging is missing for preprocessing/early-failure rows and for some TIMEOUT/OOM rows; missingness is reported per group and is not assumed random. Zero-offered counter rows are retained as zero opportunity.
- Tasks recur across SC/TSO/PSO, so the three model contrasts are not independent replications. Holm correction controls only the three reported within-model tests.
- The workload is the adapted fixed-seed SV-COMP set; conclusions do not establish official unbounded-input SV-COMP coverage.
- The search-size thresholds are sensitivity analyses, not preregistered cutoffs; no size-adjusted causal effect is claimed.
