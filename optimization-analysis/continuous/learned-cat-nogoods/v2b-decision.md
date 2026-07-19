# P0-A V2b repeated-witness admission: rejection

## Decision

Reject and remove repeated-witness admission. Exact derived-witness recurrence is not a
useful predictor of base-clause reuse. Do not combine it with later matcher work.

## Evidence

- Correctness remains intact: Release 165/165, ASan+UBSan 5/5, mutation 39/5,441,
  broad differential 852/12/0.
- Core: zero new terminals.
- Hit-rich common-terminal CPU ratios: SC 1.0213, TSO 1.0863, PSO 1.4898, all models
  1.1705 over 23 pairs.
- PSO `fib_safe-5` becomes TIMEOUT again. `fib_unsafe-5`, `queue`, and
  `circular_buffer_bad` regress to 1.8083, 2.5214, and 2.8257 of baseline.
- Exact witness state remains bounded (observed maximum about 67 KiB on the pilot), so
  memory is not the rejection cause; predictive quality and CPU are.

## Next step

Return to V2a's simple 16-attempt backstop. V2c optimizes the matcher hot loop by
compiling predicate names and stable event identities to integer IDs. This directly
targets measured synchronization overhead without changing which conflicts are learned.
