# Optimization 06 experiment protocol

Date: 2026-07-15

## Hypothesis

`Analyzer` already builds a deterministic operand-to-user adjacency for SCC analysis.
The immutable `ModelAnalysis` previously discarded it, after which every fresh
`CaatEvaluator::evaluate()` allocated an outer vector and rebuilt every dependency edge.
The production profile observed 243,232 certified adaptive-offline rebuilds, making this
nominally model-static work query-hot.

Retain the adjacency once in shared immutable analysis and use it directly in offline and
incremental worklists. Predicate IDs, edge multiplicity/order, strata, enqueue order,
fixed-point operations, checks, and candidate enumeration remain unchanged.

## Gates

1. Exact analysis test reconstructs adjacency from `dependencies()` and compares all
   rows and ordering.
2. Server Release unit/property suite passes.
3. Recursive focused differential, mutation stress/full oracle, and broad 288-program
   SC/TSO/PSO differential report zero mismatch.
4. Formal before/after matrix reports zero incorrect verdict, common-solved verdict
   mismatch, and safe complete-exploration-count mismatch.
5. Six paired repetitions use fresh before/after builds, simultaneous 24-task queues,
   swapped disjoint CPU pools, one task as the statistical unit, and 20,000-sample task
   bootstrap confidence intervals.

## Decision

Keep only if correctness gates pass and aggregate CPU time improves with a 95% CI upper
bound below 1.0, or a model-specific supported benefit causes no aggregate/model
regression above 0.5%. Record and remove the prototype otherwise.
