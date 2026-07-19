# P0-A V2c: compiled ground-literal matcher

## Change

- Intern each base-predicate name once per worker-local nogood database.
- Store stable event IDs directly in learned literals. `StableGraphAdapter` IDs are
  worker-lifetime, never reused, and already survive graph cuts.
- At the start of one match query, resolve each interned predicate to the current
  snapshot value once. The literal loop uses vector indexing and integer relation/set
  membership only.

V2a's 16-attempt bound and clause selection remain unchanged. No witness admission,
model-name branch, built-in checker, or benchmark transformation is present.

## Expected effects

- Remove two stable-key map lookups plus one string-hash base lookup from every literal
  check. V2a records 559,440 checks on PSO `queue`, 33.6 million on PSO `butterfly`,
  and 22.3 million on TSO `butterfly`.
- Reduce literal storage by replacing predicate strings and event-key variants with
  integer IDs; add only one owned string per distinct base predicate.
- Leave Reasoner calls and evaluator-call reduction identical to V2a, enabling direct
  attribution.

## Correctness argument

The compiled IDs are an injective representation inside one checker lifetime. Predicate
interning uses exact string equality. Stable event IDs are never reused. Missing or
inactive IDs and absent/type-mismatched predicates still fail the clause match, so the
same positive conjunction is required before rejection.

## Gates

Repeat unit/property, Release, sanitizer, mutation, broad, core, and hit-rich gates.
Advance only if all-model common-terminal CPU is at most 1.01, no task above one second
regresses by more than 20% without a status improvement, and at least one resource or
CPU gain remains. Report predicate resolutions, literal checks, clause bytes, and RSS.
