# Optimization 07: bounded offline ring worklist

Date: 2026-07-15

## Change

The prototype replaced per-stratum `deque` and queued
bitmap allocations with one predicate-bounded circular FIFO and one queued bitmap per
complete offline evaluation. Relation algebra, fixed-point order, checks, and
incremental update code are unchanged.

## Results

- Correctness: 141/141 unit/property tests; 39 mutation rows and 5,441 oracle checks;
  864 broad pairs with 852 comparable matches, 12 mutually unsupported, zero mismatch.
- Raw performance data: 36 XML files, 36 log archives, 36 text tables, 36 console logs,
  and 3,456 run rows.
- Correct cells: 1,554 before and 1,554 after; there are zero status transitions,
  common-solved verdict mismatches, and safe execution-count mismatches.
- Measured combined overlap: 44--48 tasks.
- Strict task-clustered aggregate CPU ratio: 1.00121 [0.99548, 1.00682].
- CPU by model: SC 0.99944 [0.99252, 1.00592], TSO 1.00515
  [0.99649, 1.01394], PSO 1.00582 [0.99596, 1.01604].
- Aggregate wall ratio: 1.00171 [0.98162, 1.02618].
- Aggregate RSS ratio: 0.99999 [0.99980, 1.00019].

## Decision

Reject and remove. The aggregate CPU interval crosses 1.0 and does not establish a
benefit; TSO and PSO point estimates are slightly slower, and neither wall time nor RSS
improves. The saved container allocations are too small relative to relation operations,
and the circular-index bookkeeping offsets them in end-to-end runs. Production and test
sources were restored exactly to HEAD.
