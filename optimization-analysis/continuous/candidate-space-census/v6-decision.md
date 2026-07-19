# Generic preventive V6 decision

V6 proves that the structural reversal certificate reduces GenMC's candidate space,
but its current captured-relation representation is not retained as the default.

Four balanced 96-task PSO repetitions contain 768 cells. Baseline totals are 228 TRUE,
108 FALSE and 48 TIMEOUT; V6 totals are 240 TRUE, 112 FALSE and 32 TIMEOUT. The same
four tasks become terminal in every repetition (`circular_buffer_ok`, `fib_safe-5`, and
`triangular-1` TRUE; `fib_unsafe-6` FALSE). There are zero OOM, zero coverage losses,
and zero safe execution-count mismatches.

This is generation-time reduction, not query caching. Across 336 cells with complete
accounting on both sides:

- RF+CO choices offered fall 3,569,216 -> 1,148,616 (67.82%);
- RF+CO choices queued fall 1,812,404 -> 330,988 (81.74%);
- work added falls 1,910,700 -> 429,268 (77.53%);
- work popped falls 1,628,696 -> 423,932 (73.97%);
- maximum retained work falls 51,601 -> 801;
- realized revisit prefixes fall 1,592,216 -> 387,452 (75.67%).

The representation still fails the frozen default-performance gate. Common-terminal
CPU is 1.04146 with task-bootstrap 95% CI [0.97512, 1.13572], wall is 1.03863
[0.97289, 1.13091], and RSS is 1.03698 [1.00010, 1.09524]. V6 is therefore rejected as
the final representation while its theorem and opt-in implementation advance to V7.

Profiling and source inspection identify an avoidable construction cost: buffered lazy
DFS appends unique edges as 16-byte `(size_t,size_t)` pairs in DFS source order, then
`Relation::sparse` globally sorts/deduplicates and copies them into 4-byte CSR targets.
V7 stores each already sorted, duplicate-free successor row at DFS frame completion and
constructs CSR directly. This removes O(E log E) sorting and the pair-vector peak while
preserving the exact edge set, witness verdict and pruning decisions.
