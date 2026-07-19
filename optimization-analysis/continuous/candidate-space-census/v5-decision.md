# Generic preventive V5 decision

V5 is rejected. Selecting one structurally certified order did not make dense
materialization safe: across four 96-task PSO repetitions, the baseline totals were
228 TRUE, 108 FALSE and 48 TIMEOUT, while V5 produced 236 TRUE, 112 FALSE, 16 TIMEOUT
and 20 OOM.

Every repetition converted the same five Goblint TIMEOUT tasks to OOM. It also changed
`pthread/queue_ok_longest` from a completed TRUE to TIMEOUT, while three other tasks
became TRUE and `fib_unsafe-6` became FALSE. There were zero safe execution-count
mismatches among common terminal cells, but the coverage loss and OOMs reject the
representation independently of timing.

V5 pruned 14,187,144 of 18,057,904 offered RF/CO choices (78.56%) with zero all-pruned
fallbacks. Common-terminal CPU was 1.02375 of baseline with task-bootstrap 95% CI
[0.95643, 1.11232]; RSS was 1.02362 [1.00015, 1.07192]. These measurements confirm that
the structural theorem is useful but a dense checked order and dense transitive closure
remain the wrong data structure.

V6 restores lazy evaluation and O(V+E) focus reach. Unlike V4, it captures the selected
root's exact deduplicated CSR edges during the lazy-cycle traversal that consistency
checking already performs, so preventive preparation does not interpret the CAT edge
program a second time.
