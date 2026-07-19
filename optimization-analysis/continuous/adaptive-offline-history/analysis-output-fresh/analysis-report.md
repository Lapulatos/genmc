# Optimization 05b strict analysis

## Decision

**Reject and remove.** Adaptive-offline history elision is exact on the exercised
transitions and reduces the retained history representation, but it does not produce a
portable performance improvement. In the build-controlled matrix, aggregate CPU time
was neutral at 1.00250 after/before (task-clustered 95% CI [0.99779, 1.00734]), while SC
and PSO regressed by 1.21% with confidence intervals entirely above 1.0. TSO improved,
but selecting TSO only after observing these results would be post-hoc specialization.

The production worktree was restored to the retained commit before Optimization 06
started.

## Question and comparison unit

The primary question is whether removing an unreachable `HistoryEntry::base` and
incremental checkpoint from certified adaptive-offline epochs improves CPU time without
changing verification semantics. One SV-COMP task is the statistical unit. Each task's
before and after value is the median of six paired repetitions. Model-specific inference
resamples tasks; the aggregate includes only 79 tasks solved in all three models and
first takes each task's geometric mean over SC/TSO/PSO before resampling.

## Correctness evidence

- Server Release unit/property suite: 142/142 passed.
- Dedicated small-epoch/threshold test: every transition compared with a fresh Phase 2
  evaluation.
- Mutation stress: 39 rows and 5,441 full oracle checks, zero mismatch.
- Broad differential: 288 programs × SC/TSO/PSO = 864 pairs; 852 comparable matches,
  12 mutually unsupported pairs, zero mismatch.
- Fresh formal matrix: 3,456 cells; zero incorrect verdict, zero common-solved verdict
  mismatch, and zero safe complete-exploration-count mismatch.

These are broad finite-sample checks, not a proof of soundness or completeness.

## Fresh-build performance result

| Metric/model | Tasks | After/before | Task-bootstrap 95% CI | Median ratio |
|---|---:|---:|---:|---:|
| CPU / all, task-clustered | 79 | 1.00250 | [0.99779, 1.00734] | 1.00265 |
| CPU / SC | 90 | 1.01209 | [1.00501, 1.01946] | 1.01043 |
| CPU / TSO | 89 | 0.98967 | [0.98288, 0.99659] | 0.98574 |
| CPU / PSO | 79 | 1.01211 | [1.00368, 1.02084] | 1.01346 |
| Wall / all, task-clustered | 79 | 0.99367 | [0.97610, 1.00968] | 0.99935 |
| Peak RSS / all, task-clustered | 79 | 0.99995 | [0.99978, 1.00012] | 1.00008 |

The exact two-sided sign test, Holm-corrected across aggregate/SC/TSO/PSO CPU contrasts,
agrees on model heterogeneity: adjusted p-values are 0.3682, 0.000119, 0.000797, and
0.00637 respectively. The task-bootstrap interval is the pre-specified primary
decision statistic.

The first matrix, which used a previously retained baseline binary, independently found
aggregate CPU 1.00524 [1.00054, 1.00995] and PSO 1.01824 [1.01023, 1.02648]. It is kept
as supporting evidence but is not pooled with the fresh-build matrix.

## Coverage and scheduling

- Fresh matrix rows: 1,728 before and 1,728 after.
- Correct cells: 1,553 before and 1,552 after; the difference is a PSO timeout boundary,
  not a wrong verdict.
- XML-derived combined task overlap: 45--48 for all 18 paired queues.
- Resource-bound status changes comprise timeout/OOM exchanges plus two unsafe timeout
  boundary changes; they remain unknown coverage changes.

## Structural memory effect

On 254 tasks with opt-in statistics in both the prior baseline profile and the new
profile, `max-history-base-bytes` decreased for 174 tasks, stayed equal for 80, and
increased for none. Nonzero history maxima fell from 178 tasks to 4. The sum of per-task
maxima fell from 173,866,288 to 172,111,464 bytes; four graphs above the small-epoch
limit dominate the remaining total. The packed snapshot-equivalent maxima decreased for
the same 174 tasks and never increased.

This confirms that the intended allocation is removed. Process peak RSS is too coarse
to expose it, and the supported SC/PSO CPU regressions mean the structural saving is not
a sufficient production trade.

## Claim candidates

- Claim:
  - Source evidence: fresh 3,456-cell matrix, strict task-clustered analysis, and paired
    structural profile.
  - Allowed wording: "History elision removed the intended retained state but was
    performance-neutral overall and regressed SC/PSO in the build-controlled matrix."
  - Forbidden stronger wording: "Removing history is always slower" or "the change can
    cause wrong verdicts."
  - Uncertainty: TSO improved in the fresh matrix, but this is not independently held-out
    evidence for a TSO-only policy.
  - Next check: prioritize query-hot model-static work rather than post-hoc history policy.
  - Decision: discard.

