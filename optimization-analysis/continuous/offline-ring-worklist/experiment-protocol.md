# Optimization 07 experiment protocol

Date: 2026-07-15

## Hypothesis

Production profiling observed hundreds of thousands of complete offline evaluations.
The offline evaluator currently creates a `deque` and a predicate-sized `vector<bool>`
for every SCC stratum. Replace these repeated per-stratum allocations with one
predicate-sized circular FIFO and one queued bitmap owned by each complete evaluation.

The queue remains FIFO. A predicate can appear at most once in the active queue, so its
capacity is exactly the model predicate count. Popping clears the queued bit before the
operator runs, preserving the existing ability for a changed predicate to reschedule
itself. The ring retains no historical pushes and therefore has a strict memory bound.

## Correctness gates

1. Recursive unit coverage must force more pushes than the ring capacity and preserve
   one operation evaluation per push.
2. Server Release unit/property suite passes.
3. Recursive differential, 39-row mutation stress/full oracle, and the broad
   288-program SC/TSO/PSO differential report zero mismatch.
4. A complete before/after matrix reports zero incorrect verdict, common-solved verdict
   mismatch, and safe complete-exploration-count mismatch.

## Performance protocol

Use the same fresh-build, six-repetition, simultaneous 24+24 queue design as
Optimization 06: 96 tasks x three models x six repetitions x two variants = 3,456
cells, alternating disjoint core pools, task-level medians, and 20,000-sample
task-bootstrap confidence intervals.

## Decision rule

Keep only if all correctness gates pass and aggregate CPU improves with a 95% CI upper
bound below 1.0, or a clearly pre-specified model benefit has no aggregate/model
regression above 0.5%. Otherwise remove the prototype.
