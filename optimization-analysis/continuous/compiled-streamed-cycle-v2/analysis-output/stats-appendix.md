# P0.7g2 statistics appendix

## Experimental unit and sample sizes

- Design: paired before/after, 96 tasks, SC/TSO/PSO, four repetitions.
- Raw cells: 2,304; paired cells: 1,152.
- Common-terminal paired cells: 1,076.
- CPU inference unit: one model-task cluster with all four common-terminal repetitions.
- CPU cluster counts: 95 SC, 90 TSO, 84 PSO; 269 total.
- RSS paired observations: 380 SC, 360 TSO, 336 PSO; 1,076 total.

Repeated task cells are not treated as independent observations for the primary CPU
claim. For each eligible model-task cluster, the statistic is median candidate CPU over
four repetitions divided by median baseline CPU over four repetitions. Ratios are
aggregated with a geometric mean.

## Interval method

The primary 95% confidence interval uses 20,000 fixed-seed bootstrap resamples of the
269 model-task ratios. The same clustered procedure is used independently for each model.
This method was frozen before the formal result and regression-tested by reproducing the
archived P0.7f point estimates.

| Scope | Ratio | 95% bootstrap CI | Relative point change |
|---|---:|---:|---:|
| Overall | 0.999423 | [0.994756, 1.004137] | -0.0577% |
| SC | 0.994100 | [0.989014, 0.999137] | -0.5900% |
| TSO | 1.004601 | [0.994941, 1.014564] | +0.4601% |
| PSO | 0.999926 | [0.991432, 1.008710] | -0.0074% |

The frozen generic retain rule requires the overall CI upper bound below 1 and each
model point ratio at most 1.01. The model point guard passes, but the overall interval
guard fails.

## Memory descriptives

| Model | n | Median RSS ratio | P90 RSS ratio | All-cell maximum |
|---|---:|---:|---:|---:|
| SC | 380 | 1.000011 | 1.003011 | 1.005393 |
| TSO | 360 | 0.999686 | 1.002376 | 1.006269 |
| PSO | 336 | 1.000000 | 1.002527 | 1.014419 |

Protocol-level combined gates are cell P90 1.002677, P90-of-levels 0.999844, and
large-task maximum 1.003172. The all-cell PSO maximum above is not the protocol's
large-task maximum; both are reported to avoid hiding the smaller-task outlier.

## Exactness and coverage

| Check | Count |
|---|---:|
| Terminal verdict mismatches | 0 |
| Execution-count mismatches | 0 |
| Candidate-count mismatches | 0 |
| Lazy-check mismatches | 0 |
| Snapshot-equivalent mismatches | 0 |
| Current-base mismatches | 0 |
| Coverage gains / losses | 0 / 0 |
| New OOM | 0 |

## Limits

- TIMEOUT cells do not enter the four-terminal-repetition CPU statistic.
- BenchExec RSS is process-level maximum resident memory; it does not isolate individual
  allocator objects.
- The bootstrap quantifies uncertainty across the sampled model-task clusters, not every
  possible CAT model or SV-COMP program.
- No post-result threshold tuning or model-specific selector is permitted.
