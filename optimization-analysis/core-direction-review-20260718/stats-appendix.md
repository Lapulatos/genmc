# Statistical appendix

## Comparison units

- V9/V8: task-clustered ratios from the direct balanced comparison reported in
  `candidate-space-census/v9-decision.md`.
- V10/V11/V11.1: one paired 725-task PSO run per final decision, with task bootstrap over
  common-correct tasks. Resource transitions are reported separately.
- V12: per-task medians across two paired 725-task repetitions; 373 tasks are correct for
  both variants in both repetitions.
- SC-RVF: 90 common-solved SC rows, but quotient activation is zero; the ratio measures
  framework drift only.
- opportunity census: one task/model row; SC/TSO/PSO are related views of many of the same
  tasks and are not independent replications.

No pooled p-value is computed across optimization families. The baselines, repetitions,
and cohorts are heterogeneous, so a meta-analytic winner claim would be invalid.

## End-to-end ratios carried from source analyses

| Family | Mechanism changes generated search? | CPU ratio | 95% CI | Interpretation |
|---|---|---:|---:|---|
| V9 vs V8 | yes | 0.96105 | [0.93854, 0.98130] | supported reduction |
| V10 vs V9 | no | 1.049916 | [1.025769, 1.077275] | supported regression |
| V11 vs V9 | no | 1.108462 | [1.067238, 1.154924] | supported regression |
| V11.1 vs V9 | no | 1.099922 | [1.056791, 1.148091] | supported regression |
| V12 after/before | no | 0.998008 | [0.988150, 1.009684] | inconclusive/neutral |
| SC-RVF vs baseline | intended yes, activated no | 1.00281 | [0.99650, 1.00864] | zero-activation drift |

Confidence intervals are shown to preserve uncertainty, not to license a direct ranking
between differently designed studies.

## Opportunity statistics

The prior timeout-effect analysis uses Mann--Whitney tests on task-level same-value shares,
with Holm correction over SC/TSO/PSO and rank-biserial effects:

| Model | TIMEOUT median | completed median | U | Holm-adjusted p | rank-biserial |
|---|---:|---:|---:|---:|---:|
| SC | 29.63% | 0.00% | 72,295 | 1.77e-46 | 0.689 |
| TSO | 29.39% | 0.00% | 73,110 | 7.60e-47 | 0.686 |
| PSO | 28.77% | 0.00% | 65,975 | 2.49e-44 | 0.679 |

These unadjusted contrasts are dominated partly by completed tasks with no meaningful RF
search. At a common threshold of at least 10,000 RF offers, median differences shrink to
+0.62, +0.37, and -0.77 percentage points. Consequently no causal timeout claim is made.

## Correctness-statistic boundary

For search-preserving V9--V12 mechanisms, equal common-correct execution and search
counters are valid invariants. For a coarser quotient they are not: lower execution and
work counts are intended effects. Quotient correctness must compare error reachability,
reachable local observations, class coverage, witness replay, and fail-open accounting.

## Limitations

- Full 725 runs use a fixed adapted SV-COMP workload and fixed resource bounds; they do
  not establish official unbounded-input SV-COMP performance.
- Some TIMEOUT/OOM rows lack final counters. Missingness is reported in the source census
  and is not assumed random.
- The same-value number is an optimistic upper bound, not a count of valid RVF merges.
- The bounded RVF outcome oracles are strong finite evidence but not a proof for arbitrary
  programs, relaxed memory, or all external effects.
