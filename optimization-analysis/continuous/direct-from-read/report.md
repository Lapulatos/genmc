# Optimization 08: direct from-read materialization

Date: 2026-07-15

## Change

The prototype constructs `fr = rf^-1 ; co` directly from GenMC's functional rf edges
and coherence successors. It avoids the inverse relation and generic composition, and
does not allocate a dense rf value when the model needs only fr.

## Results

- Correctness: 141/141 tests, 39 mutation rows / 5,441 oracle checks, and 864 broad
  pairs with zero mismatch.
- Formal data: 3,456 rows, 36 XML, 36 log archives, and 45--48 measured task overlap.
- Correct cells: 1,554 before and after; zero status, verdict, or safe-count differences.
- Strict aggregate CPU: 1.00835 [1.00164, 1.01510].
- CPU by model: SC 1.00568 [0.99420, 1.01753], TSO 1.01079
  [0.99907, 1.02225], PSO 1.00811 [0.99566, 1.02068].
- Aggregate wall: 1.01984 [1.00510, 1.03749].
- Aggregate RSS: 0.99998 [0.99984, 1.00011].

## Decision

Reject and remove. Scalar target scanning and per-edge insertion lose the packed-word
row-union advantage of generic composition, producing a statistically visible CPU and
wall regression. The functional-rf idea is retained only as a hypothesis for a separate
word-parallel row-copy prototype; this implementation is exactly reverted.
