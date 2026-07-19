# Optimization 13 / P0.6a: grouped packed primitive materialization

## Scope

Replace `StableGraphAdapter`'s scalar active-event pair classification for `po`, `int`,
`ext` and `loc` with exact thread/location groups and packed row insertion. This is the
first reversible step toward structural relation views; it changes construction only,
not the `Value` representation or CAT semantics.

## Expected effect

- Time: remove repeated thread/type/address tests from the O(N²) pair loop and write
  packed successor masks a word at a time. `po`, `int` and `loc` become group-driven;
  `ext` remains dense in space but is constructed with packed row operations.
- Space: persistent relation storage is deliberately unchanged. The same dense
  relations are produced. Construction adds O(events + threads + locations) transient
  grouping metadata, so this stage cannot fix relation OOM and must not be claimed as a
  memory optimization.
- Downstream: evaluator/worklist time is unchanged because extensional values are
  identical. P0.6b/P0.7 can later replace those packed results with views/EOG only after
  this grouped metadata path is proven exact.

## Correctness and completeness gates

1. Every packed row insertion has an independent dense pairwise unit oracle.
2. Direct stable materialization matches legacy dense-build/remap across graph mutations.
3. Release unit/property tests, 39-row full oracle and 864 broad differential have zero
   mismatch and zero safe execution-count difference.
4. No model-name or built-in checker verdict is consulted.

## Performance gate

Run four balanced repetitions of the 96-task SC/TSO/PSO sample. Retain only if:

- no new wrong/OOM result and no safe execution-count mismatch;
- `materialize-ns` falls at least 30% on completed N>=512 cells; and
- aggregate CPU upper 95% confidence bound is below 1, or the targeted large-event
  cohort gains a repeatable terminal result with no OOM increase.

If adapter time falls but process CPU does not, remove the prototype; Optimization 12
shows composition/closure dominate predicate time, so a local adapter win alone is not
enough.
