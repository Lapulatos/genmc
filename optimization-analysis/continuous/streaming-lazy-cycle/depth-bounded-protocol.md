# P0.7d2 depth-bounded streaming refinement

Date frozen: 2026-07-16
Baseline: `959f8697cfd3b20d47a4ca9d925e5e7e8ffdc508`
Parent prototype: P0.7d1 streaming lazy-cycle traversal

## Failure being corrected

The complete P0.7d1 matrix produced 20 deterministic `SEGMENTATION FAULT` cells: the
same five TSO Goblint race-reach tasks in all four repetitions. Each failed after about
9.2 seconds at approximately 1.057 GB RSS, while the retained baseline timed out. The
streaming callback nests event DFS recursion inside expression emission; a sufficiently
long acyclic path therefore exhausts the process stack.

P0.7d1 fails its frozen gate and is not retainable. P0.7d2 is a predeclared safety
refinement, and its evidence cannot reuse the failed matrix as a passing result.

## Exact refinement

- Stream event DFS only below a compile-time depth limit of 2,048 events.
- Reaching the limit is not a verdict. Abort the partial streaming traversal and rerun
  the complete check from scratch with an iterative buffered DFS.
- The fallback stores one heap frame per active event. Each frame owns the exact sorted,
  deduplicated successor row produced by the retained P0.7b emitter. It never recurses on
  event graph depth; expression recursion remains bounded by the finite normalized model
  DAG.
- Statistics count all work actually performed, including the abandoned streaming prefix
  and fallback enumeration. Persistent predicate values, checkpoints, host checker and
  model selection remain unchanged.

Both paths decide cycles in the same exact root relation. The fallback is selected only
by algorithmic recursion depth, never model name, host profile, verdict, or built-in
checker result.

## Additional correctness gates

1. An 8,193-event chain plus closing back edge must complete under ASan+UBSan and return
   a valid exact witness without process-stack recursion.
2. The five previously crashing TSO tasks must no longer signal under a focused 15-second
   BenchExec run; TIMEOUT is acceptable because that is the retained baseline status.
3. Repeat the complete Release, sanitizer, mutation, broad differential and 2,304-cell
   matrix. None of P0.7d1's correctness/performance results substitute for P0.7d2 runs.

## Retain gate

Use the original P0.7d1 thresholds unchanged, plus zero `SEGMENTATION FAULT` or other new
runtime error. If the fallback prevents crashes but erases the frozen CPU/RSS benefit,
restore all production/test changes to `959f869`.

