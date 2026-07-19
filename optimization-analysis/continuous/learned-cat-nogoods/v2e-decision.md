# P0-A V2e pay-as-you-go explanation credit: pilot rejection

## Decision

Reject V2e as a default optimization and do not run the formal matrix. Requiring a
newly retained clause to earn only its literal count admits additional explanations
too quickly on low-yield tasks.

## Evidence

- Correctness: Release 165/165, ASan+UBSan 5/5, mutation 39/5,441, broad 852/12/0.
- Core: zero new terminals; both methods finish only `exponential-4`.
- Hit-rich common-terminal CPU ratios: SC 1.0192, TSO 1.0583, PSO 1.1195, all
  models 1.0626 (23 cells).
- PSO `fib_safe-5` improves from TIMEOUT to TRUE in 43.402 seconds; PSO
  `triangular-2`, `fib_unsafe-5`, and `reorder_2` have ratios 0.6252, 0.8156, and
  0.7543.
- PSO `queue` and `circular_buffer_bad` regress to 2.5814 and 1.8017. Queue still
  admits seven explanations after 72 hits, performs 54,849 literal checks, and records
  2,860 credit-blocked inconsistent queries.

## Next step

V2f applies the already frozen 64-literal database bound as a minimum amortization
credit after every attempt, including successful learning. This remains independent
of model, predicate, task, verdict, and program shape. It can delay or omit optional
learning only; normal CAT/CAAT evaluation remains the fallback.
