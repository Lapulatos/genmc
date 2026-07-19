# Optimization 08 experiment protocol

Date: 2026-07-15

## Hypothesis

`StableGraphAdapter` currently materializes `fr` by allocating `inverse(rf)` and invoking
generic packed relation composition. GenMC graphs already guarantee at most one rf
source per read. Record each `(source-write, read)` edge during the label scan and build
`fr = rf^-1 ; co` by copying only that source write's coherence successors to the read.

For a model that needs `fr` but not `rf`, do not allocate the dense `rf` relation at all.
The direct construction remains exact for initial writes, ordinary writes, rf
replacement, coherence reorder, cuts, inactive stable IDs, and revisits.

## Correctness gates

1. The existing direct-vs-dense adapter test compares every primitive after rf/co/cut
   mutations; extend it with an `fr`-only adapter whose sole value equals the legacy
   `compose(inverse(rf), co)` result.
2. Server Release unit/property suite, mutation oracle, and 864-pair broad differential
   all pass with zero mismatch.
3. The paired matrix has zero wrong verdict, common-solved verdict mismatch, and safe
   execution-count mismatch.

## Performance and decision

Use the same 3,456-cell fresh-build simultaneous 24+24 matrix, alternating core pools,
six repetitions, task medians, and 20,000-sample task bootstrap. Keep only if aggregate
CPU 95% CI lies below 1.0 or a pre-specified model benefit introduces no aggregate/model
regression above 0.5%.
