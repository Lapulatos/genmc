# P0-A V2b: repeated-witness explanation admission

## Change

Maintain a worker-local count for each exact violation signature:

- CAT check name and kind;
- stable event-ID witness sequence emitted by the evaluator.

The first observation is recorded without running `Reasoner`. The second observation
admits exactly one explanation attempt for that signature. Later observations reuse the
learned clause, if any, but never reconstruct the same signature again. The V2a global
limit of 16 explanation attempts remains a final bound.

## Generality and correctness

The signature uses only generic CAT violation output and worker-stable IDs. It does not
inspect model filenames, benchmark families, WMM encodings, or built-in checker
results. Deferring or declining an explanation only loses pruning; normal CAAT
evaluation remains the verdict authority and exploration remains complete.

## Expected effects

- Avoid Reasoner work for one-off or continuously changing conflicts, targeting V2a
  `queue` (16 attempts but only 20 hits) and `circular_buffer_bad`.
- Preserve conflicts with observable recurrence, targeting `fib_safe-5`,
  `fib_unsafe-5`, `triangular-2`, and `reorder_2`.
- Add bounded signature-map space. V2a's 16 selected signatures bound explained state;
  observation state must also be capped at 4,096 entries with FIFO eviction. Eviction
  only delays or loses learning.
- Leave literal matching unchanged; TSO `butterfly` may remain a V2c target.

## Gates

Repeat V2a's unit, sanitizer, mutation, broad, core, and hit-rich gates with identical
layout. Advance only if all-model common-terminal CPU is at most 1.01, no task above one
second regresses by more than 20% without a resource-status improvement, and at least
one V1/V2a resource or CPU gain remains.
