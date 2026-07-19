# P0-A V2d selective literal probes: rejection

## Decision

Reject and remove the cardinality/late-endpoint probe order. Current-snapshot
cardinality does not predict which positive literal will disappear in future snapshots.
Return to the V2c compiled first-seen order.

## Evidence

- Correctness: Release 164/164, ASan+UBSan 4/4, mutation 39/5,441, broad 852/12/0.
- Core: zero new terminals.
- Hit-rich CPU ratios: SC 1.0513, TSO 1.1091, PSO 1.1080, all models 1.0883.
- PSO queue literal checks increase from V2c 144,722 to 356,296 and CPU remains 2.178
  of baseline. TSO butterfly checks increase from 6.75M to 7.99M.

## Next step

V2e changes admission rather than ordering: after learning one clause, require that
clause to earn at least its literal count in new hits before another explanation is
allowed. This is a generic realized-benefit credit, not a model/program heuristic.
