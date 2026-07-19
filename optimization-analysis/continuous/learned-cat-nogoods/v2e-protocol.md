# P0-A V2e: pay-as-you-go explanation credit

## Change

The first explanation remains allowed. After retaining a clause of length `L`, block
further Reasoner work until cumulative nogood hits increase by at least `L`. A failed,
unsupported, duplicate, or subsumed attempt requires 64 additional hits before retry.
The existing 16-attempt hard bound remains.

V2c's compiled literal representation and first-seen probe order are restored. V2b
witness admission and V2d cardinality ordering remain removed.

## Expected effects

- Low-yield tasks stop learning after one or a few clauses: V2c queue has only 20 hits
  after 16 attempts; circular_buffer_bad has 13.
- High-yield tasks continue: fib_unsafe-5 has 46,842 hits, triangular-2 227,047,
  reorder_2 6,123, and butterfly 198,425.
- The rule uses realized reuse and clause size only. It does not inspect a model name,
  predicate name, memory model, or benchmark family.
- Fewer clauses reduce Reasoner time, match probes, and clause memory. Delayed learning
  may increase evaluator calls but cannot change verdict or completeness.

## Gates

Repeat all correctness and identical pilot gates. Advance only if all-model CPU is at
most 1.01, no >1-second task regresses by more than 20% without status improvement, and
at least one resource/CPU benefit survives. Report attempts, admission skips, hits,
literal checks, evaluator calls, and bytes.
