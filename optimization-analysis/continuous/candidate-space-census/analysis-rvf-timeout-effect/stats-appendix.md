# Statistical appendix

- Primary metric: `(rf_same_value_candidate_upper_bound / rf_offered)`; higher means a larger optimistic local opportunity.
- Unit: task × memory-model row, analyzed separately by model.
- Groups: completed reachability verdict versus 60-second TIMEOUT; OOM is descriptive only.
- Distribution: bounded, zero-inflated and strongly skewed; therefore the primary test is two-sided Mann–Whitney with average ranks, tie-corrected normal approximation and continuity correction.
- Effect: rank-biserial correlation, positive when TIMEOUT values are larger.
- Uncertainty: 10,000 stratified bootstrap resamples of the task-level median difference, fixed seeds 20260717–20260719.
- Multiple comparisons: Holm adjustment across SC, TSO and PSO.

| Model | U | z | raw p | Holm p | Rank-biserial | Median-difference 95% CI |
|---|---:|---:|---:|---:|---:|---:|
| SC | 72295.0 | 14.363 | 8.825e-47 | 1.765e-46 | 0.689 | [29.39%, 29.80%] |
| TSO | 73110.0 | 14.449 | 2.534e-47 | 7.602e-47 | 0.686 | [29.20%, 29.63%] |
| PSO | 65975.0 | 13.967 | 2.487e-44 | 2.487e-44 | 0.679 | [28.77%, 29.44%] |

## Status inventory

- SC: ABORTED=2, ERROR (compilation)=13, ERROR (unsupported external)=2, ERROR (unsupported property: invalid memory access)=7, ERROR (unsupported property: uninitialized memory)=1, OUT OF MEMORY=32, SEGMENTATION FAULT=1, TIMEOUT=239, false(unreach-call)=130, true=298
- TSO: ABORTED=2, ERROR (compilation)=14, ERROR (unsupported external)=2, ERROR (unsupported property: invalid memory access)=7, ERROR (unsupported property: uninitialized memory)=1, OUT OF MEMORY=31, SEGMENTATION FAULT=1, TIMEOUT=252, false(unreach-call)=121, true=294
- PSO: ABORTED=2, ERROR (compilation)=13, ERROR (unsupported external)=2, ERROR (unsupported property: invalid memory access)=7, ERROR (unsupported property: uninitialized memory)=1, OUT OF MEMORY=22, SEGMENTATION FAULT=1, TIMEOUT=276, false(unreach-call)=112, true=289
