# Optimization 14 / P0.6b: exact structural primitive views

## Scope

Represent only the stable-graph base primitives `po`, `int`, `ext`, and `loc` as exact
immutable structural relations inside the existing `Relation` type. All derived values
and all other primitives remain dense packed relations. The generic CAT algebra accepts
either representation and always produces the same dense result unless it can return an
unchanged structural operand.

After the V1/V2 pilot, structural views are enabled only above 512 stable events. At or
below 512, the certified recursive models deliberately select the adaptive from-scratch
evaluator, where repeatedly consuming a view cost more CPU than the small dense base
saved. The threshold changes representation only; both sides denote the same relation.

This is representation polymorphism, not a built-in-checker shortcut. Membership is
defined only from stable event thread/index/category/address metadata. Phase 1 and Phase
2 consistency checks remain the sole verdict authority.

## Representation and ownership

- One structural relation owns a shared immutable O(stable events) key vector. Copies in
  base maps, normalized base predicates, and checkpoints share that payload.
- Key zero denotes an inactive or irrelevant stable ID. Thread keys distinguish initial
  writes from active threads and retain program-order index. Location keys identify one
  exact address class.
- A mutating operation on a structural relation first materializes its exact dense
  matrix. This fail-closed path preserves incremental undo semantics instead of adding
  a second relation mutation algorithm.
- Dense and structural relations use semantic equality. Same-kind structural values use
  O(events) key equality; mixed representations use exact membership comparison.

## Expected time and space effects

- Adapter construction above 512 events: reduce `po/int/ext/loc` construction from
  dense row writes to four O(events) key-vector builds. Smaller snapshots retain the
  P0.6a grouped packed construction.
- Persistent base space: reduce each requested structural primitive from
  `events * ceil(events/64) * 8` bytes to approximately `events * 8` bytes, before
  allocator/object overhead. At 8,033 events, four dense matrices occupy about 32.4 MB;
  four key vectors occupy about 0.26 MB.
- History/checkpoint space: structural copies share immutable vectors until a rollback
  mutation requires dense fallback. This can remove repeated dense base snapshots.
- Algebra: union/intersection/difference and composition consume structural rows on
  demand and still allocate the exact dense derived result. Thus this stage does not
  remove dense `order`, `reach`, composition, or closure intermediates; P0.7 is still
  required for that.
- Risk: on-demand row generation can raise CPU for models that repeatedly consume a
  structural primitive. Dense fast paths remain unchanged, and the performance gate
  rejects the prototype if memory gains do not compensate for that cost.

## Correctness and completeness gates

1. Unit oracles compare all four view memberships, equality, growth/shrink, mutation
   fallback, and every relation algebra operator used by the bundled recursive models
   against independently constructed dense relations.
2. Direct stable materialization equals dense-build/remap across multiple threads,
   locations, initial writes, stable holes, rf/co changes, and cuts.
3. Release tests, 39-row full oracle, and 864 broad differential have zero mismatch and
   zero safe-execution-count difference.
4. A four-repetition 96-task SC/TSO/PSO matrix has no new wrong result, no OOM increase,
   and no common-terminal execution-count mismatch.
5. Unsupported or ill-shaped values fall back to dense algebra; no CAT expression is
   approximated and no model fingerprint selects a verdict.

## Retention gate

Retain only if all correctness gates pass and at least one of these is true without a
repeatable CPU regression above 5%:

- completed tasks with at least 512 stable events reduce `max-current-base-bytes` by at
  least 2x and process RSS by at least 10%; or
- the historical/formal resource cohort gains a repeatable terminal result with no OOM
  increase, while the measured structural base payload falls by at least 2x.

Remove the prototype if exactness requires eager dense materialization of the four base
views, because that would retain both the O(events) metadata and O(events²) matrices.
