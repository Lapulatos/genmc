# P0-A V2f: minimum amortization credit

## Change

The first explanation remains allowed. After every explanation attempt, require at
least `literalLimit` new learned-nogood hits before another Reasoner call. The
production limit is 64. The existing 16-attempt cap and V2c compiled matcher remain.

## Expected effects

- PSO queue's 72 total V2e hits can admit at most two attempts instead of seven.
- Circular buffer and Szymanski stop after their first low-yield clause.
- Fib, triangular, reorder, and butterfly can continue only after demonstrating 64
  actual evaluator-skipping hits per paid explanation.
- Fewer explanations reduce Reasoner calls, retained clauses, predicate resolutions,
  and literal checks. Delayed learning can increase normal evaluator calls but cannot
  change the accepted execution set.

## Gates

Repeat Release, sanitizer, mutation, broad differential, core, and hit-rich pilot.
Advance only when all-model common-terminal CPU is at most 1.01, no >1-second task
regresses by more than 20% without a status gain, and at least one resource or coverage
benefit survives.
