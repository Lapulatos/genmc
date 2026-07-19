# P0-A V4: bounded-work warm-up admission

## Change

Keep V3 lazy-preserving provenance and V2f hit credit. Before the first optional
Reasoner call, require 1,024 snapshot match queries (`literalLimit × attemptLimit`, or
64 × 16 in production). Record warm-up skips separately from credit and hard-cap skips.

## Expected effects

- Explorations that finish before 1,024 candidate snapshots never allocate provenance
  tables, materialize full values, or retain clauses.
- Large explorations pay at most the first 1,024 normal lazy evaluations before
  learning. This may reduce useful hits but leaves fib/triangular/butterfly/reorder with
  thousands to hundreds of thousands of later queries.
- Persistent space remains zero before warm-up and unchanged afterward.
- The rule depends only on observed generic work and existing database bounds. It does
  not inspect memory model, predicate, benchmark, source shape, or verdict.

## Gates

Repeat Release, focused sanitizer, mutation, broad differential, core, hit-rich pilot,
and—only if the pilot advances—the four-repetition 2,304-cell formal matrix. Use the
same semantic, CPU, RSS, and coverage gates as V3.
