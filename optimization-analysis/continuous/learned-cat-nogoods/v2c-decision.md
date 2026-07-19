# P0-A V2c compiled literals: pilot rejection

## Decision

Do not retain V2c by itself and do not advance it to the formal matrix. It improves V2a
but remains a net regression with large individual losses. Use it only as the
experimental representation base for an explicitly ordered V2d matcher.

## Evidence

- Correctness: Release 164/164, ASan+UBSan 4/4, mutation 39/5,441, broad 852/12/0.
- Core: zero new terminals.
- Hit-rich CPU ratios: SC 1.0283, TSO 1.0828, PSO 1.1096, all models 1.0715.
- PSO retains fib_safe-5 TIMEOUT-to-TRUE at 56.072 seconds and speeds triangular-2,
  fib_unsafe-5, and reorder_2 to 0.6941, 0.8416, and 0.7587 of baseline.
- PSO queue and circular_buffer_bad still regress to 2.1807 and 1.8542.
- Maximum clause storage on the listed PSO pilot falls to 1.5--16.5 KiB.

## Attribution caveat

Interning predicates also changed canonical/match order from lexical predicate/key order
to first-seen integer order. Literal checks consequently fell from V2a's 559,440 to
144,722 on PSO queue and from 33.6 million to 6.75 million on PSO butterfly. V2c is
therefore a combined representation/order measurement; its improvement must not be
attributed solely to integer lookup.

V2d makes probe order explicit and makes subsumption order-independent. It then ranks
present literals by current base-predicate cardinality and later stable endpoint, both
generic snapshot signals.
