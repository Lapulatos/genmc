# Primitive-delta adapter algorithm plan

## Objective

Reduce repeated `StableGraphAdapter::materialize()` work on the authoritative CAT path
without changing GenMC/TruSt exploration, CAT equations, or consistency verdicts. The target is
the 23.5 s materialization component measured on the common pthread-wmm terminals; this plan
does not attempt deletion propagation inside recursive CAAT.

## Why this follows from the experiments

- Directly forcing incremental evaluation was rejected: CPU +32.73% and one terminal lost.
- Three medium tasks show 46,520 insertion rejections, 51,486 examined history entries, and
  zero base-subset match. The checkpoint limit is not the cause.
- Existing structural-view and host-consistency bypass experiments must not be repeated:
  the first helped only large graphs and failed its retention gate; the second was outside
  generic CAT method scope.
- Full primitive materialization remains paid on every query in both offline and incremental
  modes. It is independent of the rejected evaluator strategy.

## Exact state

One worker-local adapter keeps:

```text
stable ID -> EventDescriptor
location -> ordered stable write IDs
location -> virtual initial-write ID
cached exact BaseValues
```

`EventDescriptor` contains only facts needed to reconstruct primitives:

```text
active, label kind, thread, po index, address, ordering,
rf source ID, rmw partner ID, create/join partner ID
```

The descriptor table is not a second search state. It is a cache of the current
ExecutionGraph projection, discarded with its worker-local CAT checker.

## Per-query algorithm

1. Scan active labels and per-location coherence lists once: `O(events + writes)` plus
   stable-ID lookup. Produce new descriptors and ordered write lists.
2. Compare descriptors with the cached version and classify:
   - newly active/inactive event;
   - changed RF source;
   - changed CO order for one address;
   - lifecycle/RMW metadata change;
   - stable-universe growth.
3. Grow every cached value exactly when stable IDs grow.
4. Apply exact primitive deltas:
   - sets `_`, `R`, `W`, `F`, `IW`, `SC`: change one bit per changed descriptor;
   - `id`: change the active diagonal bit;
   - `rf`, `rmw`, `tc`, `tj`: erase the old single edge and insert the new edge;
   - `po`, `int`, `ext`, `loc`: update only pairs incident to an activated/deactivated or
     metadata-changed stable ID, comparing it with all active IDs (`O(changed * events)`);
   - `co`: for each changed address, replace exactly that address's write-order submatrix;
   - `fr`: for each address whose RF or CO changed, replace rows of reads at that address
     from the exact new RF/CO lists.
5. Return the cached complete `BaseValues` to the existing synchronizer/evaluator. The
   evaluator still performs the full offline fixed point under the current <=512 policy.

No relation is approximated. A delta miss is a correctness bug, not a fail-open result.

## Required representation support

Add dense-only exact row/submatrix mutation helpers rather than scalar `erase()` loops:

- `Relation::replaceDenseRow(row, EventSet)`;
- optionally `Relation::clearDenseRow(row)`;
- keep sparse/structural values on the existing full materialization fallback initially.

The first prototype is admitted only for `stableEvents <= 512`, where the current adapter
already uses dense primitives and adaptive-offline evaluation. Large sparse/structural graphs
remain byte-for-byte on the existing path.

## Ownership boundary found before implementation

`IncrementalCaatEvaluator::initialize(eventCount, const BaseValues &)` copies the complete
primitive map into `base_`. `GraphSynchronizer::retainCurrent()` then copies the evaluator base
again into history. Therefore an adapter-owned persistent `BaseValues` would otherwise become a
third retained full snapshot and could reduce construction time while increasing peak memory.

The proof prototype must consequently separate and report:

```text
descriptor scan -> delta mutation -> full-oracle comparison -> evaluator base copy -> equality
```

For the current `<=512` adaptive-offline branch, the adapter cache may own one complete base only
if the generic retained-history copy is elided for that branch or ownership is transferred without
leaving another retained copy. The full oracle is sampling/debug state, not retained production
state. No ownership redesign is admitted until observation-only phase counters establish which
copy/construction component is material.

## Correctness and completeness invariants

1. At every query, cached base equals a fresh independent full materialization for every
   required primitive.
2. CAT evaluator remains the sole consistency authority; no host checker verdict bypass.
3. Candidate lists, work items, revisit order, complete/blocked executions, and all
   exploration counters remain identical.
4. Cache lifetime is worker-local; it introduces no shared mutable state.
5. Unsupported representation, universe compaction, or unexpected descriptor change uses
   the existing full materializer for that query and replaces the cache exactly. It does not
   return TRUE from partial state.

## Cost model

Current small-graph materialization is approximately:

```text
O(events^2 + writes^2 + reads*writes) allocations and row writes per query
```

Expected delta path:

```text
O(events + writes) scan
+ O(changed_events * events)
+ O(changed_address_writes^2 + changed_address_reads*writes)
```

Worst case remains quadratic and may fall back to the full builder. Persistent cache size is
one complete primitive snapshot per worker; the current path already holds one evaluator base
plus one transient materialized snapshot during rebuild. Measure RSS rather than assuming the
allocator releases the transient copy.

## Implementation sequence

### P0 — proof-bearing prototype

1. Split current function into `materializeFull()` and an opt-in cached entry point.
2. Add descriptor extraction and exact cache replacement with no deltas; oracle equality
   establishes the cache boundary.
3. Add set/single-edge deltas.
4. Add grouped `po/int/ext/loc` incident-pair deltas.
5. Add address-local CO/FR replacement.

Each substep must pass the mutation oracle before proceeding. Do not combine all relation
types in one unreviewable patch.

### P1 — validation

- Unit mutation sequences: append, RF replacement, CO move, cut-to-stamp, non-LIFO revisit,
  stable holes, new address, RMW, create/join, 63/64/65 word boundary.
- Per-query full-materialization oracle under Release and ASan+UBSan.
- Existing recursive differential, mutation stress, 864 broad oracle.
- Fixed 15 paired panel. Retain only if all 10 baseline terminals remain terminal, search
  counters match exactly, common-terminal CPU improves at least 10%, and RSS does not grow
  more than 5%.
- Only after retention: pthread-wmm 283, then the actual 725 dataset and HTML without
  `cputime-cpux`.

## Stop conditions

- Any base mismatch, verdict mismatch, or search-count mismatch: revert the delta substep.
- Materialization falls but offline evaluator grows enough to miss the 10% CPU gate: reject
  as standalone optimization.
- Cache raises common-terminal RSS >5% or increases OOM: reject or redesign ownership before
  further performance work.
