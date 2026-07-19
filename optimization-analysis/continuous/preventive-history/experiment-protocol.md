# Preventive-history retention census and optimization protocol

## Question

Does a checkpoint retained by a preventive-prefix synchronization later serve a real
rollback, or is it normally evicted/cleared without reuse? Removing it is worth trying
only if unused retained state is common enough to reduce history/checkpoint bytes and
does not replace cheap rollbacks with rebuilds.

## Measurement-only census

Tag each `GraphSynchronizer::synchronize()` call as exploration or preventive. Preserve
all transitions, evaluator operations, history order, and checkpoint lifetime. Record:

- preventive synchronizations and newly retained checkpoints;
- successful rollbacks to a preventive checkpoint;
- preventive checkpoints discarded by eviction, history truncation, or epoch reset;
- maximum base bytes attributable to preventive history entries;
- existing rollback, rebuild, offline-evaluation, retained-undo, CPU, and RSS metrics.

The census is enabled only with the existing `--cat-stats` diagnostics and must not
change candidate enumeration or consistency results.

## Decision gates

1. Correctness: 145+ Release tests, mutation oracle, and 864-pair broad differential
   must report zero mismatch.
2. Opportunity: at least 90% of retired preventive checkpoints must be discarded
   without a rollback hit, or preventive entries must account for at least 10% of peak
   history base bytes.
3. Prototype: suppress only a class with a proof that the missing checkpoint cannot be
   required for correctness. Performance fallbacks are allowed but must be measured.
4. Retention: zero verdict/execution-count mismatch, no OOM regression, aggregate CPU
   upper 95% CI below 1 or a repeatable TIMEOUT/OOM-to-terminal gain, and RSS/history
   must not regress by more than 2%.

## Expected effects

- Removing an unused checkpoint saves one copied `BaseValues` map in synchronizer
  history and prevents its evaluator undo trail from remaining live.
- It also removes checkpoint creation, eviction, and history-subset comparison work.
- Removing a useful parent checkpoint can turn rollback/rollback-insert into rebuild;
  this increases both time and transient fixed-point space. Such a class must be
  rejected or narrowed.
