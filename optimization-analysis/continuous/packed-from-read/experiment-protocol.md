# Optimization 09 experiment protocol

Date: 2026-07-15

## Hypothesis

Optimization 08 proved scalar direct construction slower because it discarded packed
row unions. Retain functional-rf specialization but expose a checked
`Relation::unionRowFrom()` that ORs `ceil(events/64)` words. For every `(write, read)` rf
edge, copy the write's entire coherence-successor row into the read's fr row. This is
exactly `rf^-1 ; co` without allocating inverse rf or scanning all possible middle IDs.

## Gates

1. Cross-relation row union is tested across a 130-event, three-word row.
2. An fr-only stable adapter matches legacy generic composition across rf replacement,
   co reorder, cut, and revisit sequences.
3. Release unit/property, mutation oracle, and 864-pair broad differential have zero
   mismatch.
4. Use the same fresh 3,456-cell paired matrix and task-clustered statistics.

## Decision

Keep only if aggregate CPU task-bootstrap 95% CI is below 1.0, or a pre-specified model
benefit has no aggregate/model regression over 0.5%. Correctness differences always
force rejection.
