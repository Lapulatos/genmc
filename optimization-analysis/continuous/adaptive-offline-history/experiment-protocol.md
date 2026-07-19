# Optimization 05b experiment protocol

Date: 2026-07-15

## Hypothesis

For a certified adaptive-offline epoch at or below 512 stable events, the current
snapshot is replaced before any history search. Omitting its duplicate history base and
checkpoint should reduce checkpoint/base-copy work and retained history bytes without
changing the evaluator state, candidate set, accepted executions, or fallback behavior.

The by-value/move initialization change from Optimization 05a is deliberately absent.

## Correctness and completeness gates

1. Server Release build and all unit/property tests pass.
2. The dedicated transition test compares every small rebuild and the small-to-large
   threshold transition with a fresh Phase 2 evaluation.
3. Mutation stress reports nonzero oracle checks and no oracle mismatch.
4. Broad Phase 1 versus recursive-model differential reports no status or observable
   signature mismatch on SC, TSO, or PSO.
5. The formal before/after matrix reports no incorrect verdict, common-solved verdict
   mismatch, or safe-task complete-exploration-count mismatch.

Any failed correctness gate rejects the prototype regardless of performance.

## Performance design

- Workload: the established 96-task SV-COMP 2026 C.Concurrency sample.
- Models: recursive SC, TSO, and PSO CAAT backends.
- Repetitions: six per model and variant.
- Total: 96 × 3 × 6 × 2 = 3,456 run cells.
- Scheduling: before and after run simultaneously as two 24-task BenchExec queues on
  disjoint CPU pools; pools swap each repetition and are exactly balanced after six.
- Per-run limits: one core, 4 GiB, 60 seconds, GenMC `--nthreads=1`.
- Statistical unit: one task. For each task, take the median of all six paired solved
  repetitions, form after/before ratios, then use a 20,000-sample task bootstrap.
- Primary metric: CPU time. Supporting metrics: wall time, peak RSS, solved coverage,
  XML-derived peak concurrent overlap, and opt-in CAAT history/checkpoint counters.

## Decision rule

Keep only if all correctness gates pass and the isolated change has a supported benefit:

- preferred: aggregate CPU-time ratio has a 95% CI upper bound below 1.0; or
- acceptable memory trade: a reproducible structural history-memory reduction with no
  material CPU/wall regression (aggregate upper bound at most 1.005) and no model whose
  lower confidence bound exceeds 1.0.

Otherwise remove the prototype and retain the negative result. Peak process RSS alone
is not treated as a sensitive measure of the small history allocation.

## Build-control extension

The first formal matrix compared a previously retained production build with a fresh
prototype build. Its task-clustered aggregate CPU ratio was 1.00524 with 95% CI
[1.00054, 1.00995]. Because this effect is only about 0.5%, build age/layout is a
decision-relevant nuisance variable even though the source and compiler image match.

Before final rejection, build the baseline afresh from the isolated `source-before`
tree with the same image, generator, Release mode, and target options as the prototype.
Repeat the complete six-repetition simultaneous/swapped-core matrix into a separate
`formal-fresh` directory. Do not pool the two matrices unless their directions agree;
report both and prefer the fresh-build comparison for the causal decision.
