# P0-A V2f minimum amortization credit: pilot rejection

## Decision

Reject V2f and do not tune the credit threshold further. It improves V2e but fails the
aggregate and individual-regression gates. The remaining fixed cost is the first
Reasoner explanation, not repeated admission.

## Evidence

- Correctness: Release 165/165, ASan+UBSan 5/5, mutation 39/5,441, broad 852/12/0.
- Core: no new terminal task.
- Hit-rich common-terminal CPU ratios: SC 1.0039, TSO 1.0610, PSO 1.0748, all
  models 1.0449 (23 cells).
- PSO fib_safe-5 remains a TIMEOUT-to-TRUE gain at 44.092 seconds; triangular-2,
  fib_unsafe-5, and reorder_2 remain 0.6280, 0.8391, and 0.7831 of baseline.
- PSO queue now learns only one clause and performs only 3,018 literal checks, but
  still costs 2.1290 of baseline. PSO butterfly costs 1.1950 and TSO butterfly 1.2464.
- Raising the successful-learning credit from clause length to 64 reduces the overall
  ratio from V2e's 1.0626 to 1.0449, but cannot remove the first-explanation cost.

## Post-hoc mechanism correction

Do not try 128/256 credit thresholds. Added counters show PSO queue's only Reasoner
call costs 1.132 ms, so it cannot explain the roughly 1.34-second end-to-end increase.
The actual fixed cost is architectural: learned nogoods globally disable lazy-cycle
evaluation to keep all predicate values available. V3 restores lazy evaluation and
materializes a full provenance snapshot only for admitted explanations.
