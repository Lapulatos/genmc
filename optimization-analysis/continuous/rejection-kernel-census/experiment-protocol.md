# P0.1 rejection-kernel census protocol

Date frozen: 2026-07-16

## Question

Can exact CAAT violation explanations learned from one rejected execution be reused on
later graph prefixes often and early enough to justify an exact prefix blocker?

## Semantic contract

- `--cat-kernel-stats` is opt-in and measurement-only.
- Every query still executes the existing generic incremental CAAT evaluator.
- The census result is not read by RF/CO enumeration, revisit construction, scheduling,
  consistency return values, or error reporting.
- Only positive base literals enter the simulated blocker antichain. Negative literals,
  empty intrinsic explanations, failed replays, and capacity-overflow kernels are counted
  but cannot produce a simulated hit.
- Each accepted kernel is deletion-minimized and replayed from sparse base values through
  the complete `CaatEvaluator`; a replay must still violate at least one model check.
- Stable event IDs come from the existing `GraphSynchronizer`/incremental evaluator.
  Models without that online path do not create a census.
- Exponentially spaced progress records at 1, 2, 4, 8, ... queries preserve the
  latest partial counters when BenchExec kills a TIMEOUT/OOM process. Analysis uses the
  final record when present and otherwise the last complete checkpoint; partial records
  are never compared as if they described completed exploration.

## Correctness gates

1. Release unit/property and CLI tests pass.
2. Mutation suite retains every Phase-2 oracle match.
3. Broad SC/TSO/PSO differential retains every comparable verdict and safe execution
   count.
4. `kernel-hit-accepted` is exactly zero.
5. `kernel-replay-failures` is exactly zero for positive eligible explanations.
6. Flag-off versus flag-on runs have zero verdict and safe execution-count mismatch.

Any failure removes the instrumentation before performance interpretation.

## Measurement population

- Established 96-task sample under recursive SC/TSO/PSO.
- A 60-task stratified sample from the 725-task 60-second TIMEOUT/OOM cohort, balanced
  across model, timeout/OOM, task family, and observed graph size where available.
- `--nthreads=1`, one core, 4 GB, 60 seconds per task.
- Flag-off and flag-on run simultaneously on disjoint core groups and swap groups each
  repetition. Use three repetitions for overhead; the opportunity counters need one
  complete flag-on repetition per model/task.

## Primary opportunity measures

- reusable hit rate = `kernel-hit-rejected / kernel-rejected`;
- positive eligibility = `(explained - negative-skipped - empty-skipped - replay-failures)
  / explained`;
- exact duplicate and subsumption rates;
- minimized/raw literal ratio and antichain size;
- mean/min/max event count and query delay at simulated hits;
- fraction of tasks with at least one hit;
- additional TIMEOUT/OOM caused by instrumentation, reported separately rather than
  interpreted as a production regression.

## Decision rule

Proceed to an exact P0.2 blocker only if all correctness gates pass and at least one of
the following is established without capacity drops:

- aggregate reusable hit rate is at least 10%; or
- at least 20% of the TIMEOUT/OOM sample has a reusable hit rate of at least 10% and
  simulated hits occur below 75% of that task's maximum active-event count on average.

Otherwise remove the instrumentation or revise only the kernel representation/minimizer;
do not implement pruning. P0.2 must still begin with SC, one worker, and exact differential
oracles before extending to TSO/PSO or shared multi-worker state.

## Post-frozen diagnostic prompted by the SC large-task run

The frozen reusable-hit denominator is rejected queries. SC produced a high conditional
hit rate on one task but almost no rejected queries across 1.54 million observed queries.
The analysis therefore also reports `kernel-hit-rejected / kernel-queries`, task coverage,
and resource-status changes as a labeled sensitivity analysis. This does not replace the
frozen threshold after observing data: passing the original rule can justify a small
exact P0.2 prototype, but production retention additionally requires either at least 1%
of all observed queries to be avoidable in the intended cohort or a verified reduction
in TIMEOUT/OOM with no correctness mismatch.
