# P0.7e3 cache-resident product selector protocol

## Frozen rule

Restore P0.7e1's recursive product traversal and admit it only when its one-byte color
array satisfies `automaton states × current events <= 32 KiB`. Any larger product uses
the retained exact P0.7d2 streamed/buffered event traversal. The rule does not inspect
the model name, task name, verdict, memory model, or previous timing.

The 32 KiB bound is a conservative common L1 data-cache capacity and is also the L1D
size of the experiment server's Xeon Platinum 8280M cores. It bounds the new random-
access color working set independently of the existing event graph. Product native
recursion remains bounded at 2,048 vertices; exceeding it restarts the exact product
check with heap frames as in P0.7e1.

## Evidence before implementation

- P0.7e1 TSO fib: max 319 stable events, 17,786 product checks, about 12.1k visited
  product states per check; CPU ratios 0.9107 and 0.9166 with RSS near 1.0.
- P0.7e1 TSO queue: max 4,029 events, two product checks; CPU ratios 1.2243/1.1761 and
  RSS 1.3662/1.3651.
- P0.7e1 PSO queue: max 8,033 events, 17 product checks; CPU ratios 1.0316/1.0293 and
  RSS 1.0180/1.0178.
- P0.7e2 proves that replacing recursive callbacks with explicit per-transition frames
  is not the remedy: TSO fib and queue CPU rise to about 1.28 and 1.33.

The selector is expected to retain the small, frequently checked TSO fib product while
falling back before the large queue product allocates color state or builds a deep
product path.

## Gates

1. Release 161/161, focused ASan+UBSan 7/7, mutation 39/5,441, broad 852/12/0.
2. The same balanced two-repetition pilot must have zero baseline-terminal regression,
   no repeatable terminal CPU ratio above 1.02, and RSS/snapshot ratios at most 1.02.
3. Only if the pilot passes, run the frozen 2,304-cell matrix. Retain only with zero
   verdict/execution mismatch, aggregate common-terminal CPU upper 95% bootstrap bound
   below 1.0, and large RSS/snapshot ratios at most 1.02.
