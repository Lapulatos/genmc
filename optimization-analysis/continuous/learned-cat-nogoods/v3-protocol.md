# P0-A V3: lazy-preserving on-demand provenance materialization

## Mechanism correction

Post-hoc counters show that V2f's first Reasoner call is not the PSO queue bottleneck:
it costs 1.132 ms, while the end-to-end regression is about 1.34 seconds. Enabling
learned nogoods currently disables `IncrementalCaatEvaluator` lazy-cycle evaluation for
every query so that all predicate values are always available to `Reasoner`.

## Change

Keep lazy-cycle evaluation enabled under learned nogoods whenever no other consumer
already requires full values. On an inconsistent query for which the credit/cap policy
requests an explanation:

1. use the incremental values directly if every predicate is materialized;
2. otherwise run one exact non-lazy `CaatEvaluator` on the synchronizer's current base
   snapshot;
3. build the positive explanation from that full result and immediately release it.

The lazy evaluator remains the consistency authority for the normal query. The full
evaluation is used only to construct a sufficient replayable explanation. Any missing,
erroneous, or inconsistent provenance state fails closed to normal evaluation without
learning.

## Expected time and space effects

- Ordinary queries regain lazy cycle checks and avoid full fixed-point population.
- Full materialization occurs at most once per admitted explanation call (hard cap 16,
  plus V2f's 64-hit credit).
- Reasoner time is unchanged per admitted explanation; matcher work is unchanged.
- Temporary full predicate values increase peak memory during an admitted explanation,
  but are released before the query returns. Persistent learned database space is
  unchanged.

## Gates

Repeat Release, focused ASan+UBSan, 39/5,441 mutation oracle, 864-pair broad
differential, core pilot, and paired hit-rich SC/TSO/PSO pilot. Advance only if:

- zero status, verdict, safe-execution, oracle, or sanitizer mismatch;
- common-terminal CPU geometric ratio is at most 1.01;
- no >1-second task regresses by more than 20% without a status gain;
- at least one resource or coverage improvement survives;
- logs confirm lazy checks are active and full materializations are bounded by
  Reasoner calls.
