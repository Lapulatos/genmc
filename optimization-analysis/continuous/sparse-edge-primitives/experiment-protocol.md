# Optimization 15 / P0.6c: size-adaptive sparse edge primitives

## Objective

Extend the exact P0.6b representation layer to stable base primitives whose observed
edge count is small, so the combined candidate reduces total base storage by at least
2x without making dense small-graph evaluation slower. The CAT/CAAT evaluator remains
the only verdict authority; neither model names nor GenMC's built-in checker select a
verdict or a representation.

## Exact representation and selection

- Add one immutable explicit-edge relation with packed CSR row offsets and sorted target
  IDs. It denotes exactly the supplied pairs, including stable-ID holes.
- Candidates are only base `rf`, `co`, `fr`, `rmw`, `tc`, and `tj`. Existing exact
  thread/location views continue to represent `po`, `ext`, `int`, and `loc` above 512
  stable events.
- Build or derive the exact edge list first. Select CSR only when stable events exceed
  512 and `csr_bytes <= dense_bytes / 2`; otherwise build the existing dense packed
  relation. This makes the space decision data-dependent, not model-dependent, and
  prevents a dense coherence relation from becoming a larger edge list.
- CSR rows are sorted and duplicate-free. Membership uses binary search; count is the
  target-array length; equality/subset/grow/shrink and dense materialization are exact.
- A mutation of an immutable CSR value materializes its exact dense matrix once before
  applying `insert` or `erase`, preserving rollback and undo semantics.

## Expected time effects by step

1. Stable materialization: avoid allocating, zeroing, and writing an O(N^2) matrix for
   selected sparse primitives; pay O(N + E) CSR construction and sorting. `rf`, `rmw`,
   `tc`, and `tj` are expected to benefit directly. `co` and `fr` select CSR only when
   their measured payload is at most half the dense payload.
2. Base copying/history: immutable CSR arrays are shared, replacing vector copies of
   O(N^2) packed words with shared ownership plus O(1) metadata.
3. Algebra: sparse operands iterate actual edges/rows. Dense-only closure/composition
   retain the V7 fast path. No inner loop performs a per-cell representation dispatch.
4. Incremental mutation: a rare mutation may pay one O(N^2) dense fallback. Counters
   must expose fallback count/bytes; a workload that repeatedly falls back is rejected
   by the CPU gate rather than hidden.

## Expected space effects

- Dense relation payload: `N * ceil(N/64) * 8` bytes.
- CSR payload with 32-bit IDs: approximately `(N+1+E) * 4` bytes, plus fixed object
  overhead. Selection requires at least a 2x payload saving.
- At PSO `queue_ok_longer` (N=8,033), one dense base is about 8.10 MB. P0.6b leaves
  48.78 MB after removing three structural matrices. Compressing any two sparse edge
  bases should remove more than the 12.34 MB still required to put the combined total
  below half of the 72.88 MB baseline.
- Derived `order/reach` and other dense evaluator values are unchanged. This stage can
  reduce base/history and process RSS but does not solve all exploration-state OOM.

## Correctness and completeness gates

1. Unit/property tests compare CSR membership, count, equality, subset, grow/shrink,
   mutation fallback, and every CAT algebra operator against independently built dense
   relations, including empty, dense-rejected, stable-hole, and boundary cases.
2. Direct graph materialization compares every selected primitive with the legacy dense
   snapshot under rf/co replacement, RMW/lifecycle edges, cuts, and address reuse.
3. Server Release suite, 39-row full oracle, and 864-pair broad differential have zero
   mismatch and zero common-safe execution-count difference.
4. Four repetitions of the frozen 96-task SC/TSO/PSO matrix have no new wrong result,
   OOM, or common-terminal execution mismatch.

## Retention gate

Retain the combined P0.6b+P0.6c candidate only if all correctness gates pass and:

- completed tasks with at least 512 stable events reduce total current base bytes to at
  most 0.5 of the P0.6a baseline and process RSS to at most 0.9; and
- common-terminal CPU has no repeatable regression above 5%.

Alternatively, a repeatable new correct terminal result may replace the RSS condition,
but not the 2x total-base condition. Remove the complete uncommitted representation
prototype if this gate fails.
