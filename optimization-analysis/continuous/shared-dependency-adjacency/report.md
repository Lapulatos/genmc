# Optimization 06: shared immutable dependency adjacency

Date: 2026-07-15

## Change

The CAT analyzer already constructs deterministic operand-to-user adjacency while
computing SCCs. `ModelAnalysis` now retains that immutable adjacency, and both offline
and incremental CAAT evaluators reference it directly instead of allocating and
reconstructing the same nested vectors.

The change preserves predicate IDs, dependency multiplicity and order, SCC strata,
enqueue order, fixed-point operations, checks, and candidate enumeration.

The fresh Release binary's text segment decreases by 3,048 bytes
(3,165,614 to 3,162,566); its data segment increases by 64 bytes. This is structural
evidence only and is not used as the performance decision criterion.

## Correctness gates

- Server Release unit/property suite: 141/141 passed.
- Mutation stress: 39 rows and 5,441 full-oracle checks, zero mismatch.
- Broad differential: 288 programs x SC/TSO/PSO = 864 pairs; 852 comparable matches,
  12 mutually unsupported pairs, zero mismatch.

## Performance protocol

- Fresh before and after builds from isolated source snapshots in the same
  `genmc15noble:sujie` image with the same Release/Ninja configuration.
- 96 tasks x SC/TSO/PSO x six repetitions x two variants = 3,456 run cells.
- Before and after run simultaneously as 24-task queues on disjoint core pools 0--23
  and 28--51; core pools swap every repetition.
- Primary unit: task. Each task-model ratio uses the median of six paired repetitions;
  the aggregate first takes the geometric mean across models for each common task.
- Uncertainty: 20,000-sample task bootstrap 95% confidence intervals. Correctness also
  checks wrong verdicts, common-solved verdict equality, and safe execution counts.

## Performance result

- Raw completeness: 36 XML files, 36 log archives, 36 text tables, 36 console logs,
  and 3,456 run rows.
- Correct cells: 1,554 before and 1,554 after; each side has 174 resource-limited
  errors and zero wrong verdicts.
- Common-solved verdict mismatches: 0. Safe-task execution-count mismatches: 0.
- Measured combined task overlap: 44--48.
- Strict task-clustered aggregate CPU ratio: 1.00879, 95% CI
  [1.00416, 1.01357].
- CPU by model: SC 0.99783 [0.99166, 1.00409], TSO 1.02149
  [1.01273, 1.03047], PSO 1.00610 [0.99776, 1.01483].
- Strict task-clustered aggregate wall ratio: 1.03076 [1.01353, 1.04998].
- Strict task-clustered aggregate RSS ratio: 0.99996 [0.99976, 1.00015].
- Aggregate exact sign test for CPU: p=0.00232, Holm-adjusted p=0.00697.

Raw data is under `server/formal-fresh/`; parsed rows and basic consistency results are
under `analysis/`; task-clustered statistics are under `analysis-output/`.

## Decision

Reject and remove. The primary aggregate CPU interval is wholly above 1.0, the measured
regression is above the pre-registered 0.5% tolerance, TSO regresses independently, and
RSS has no measurable improvement. The saved adjacency reconstruction is too small to
justify retaining a change whose observed code-layout/access-path effect is negative.
The matrix does not isolate a unique microarchitectural cause, so no stronger causal
claim is made.
