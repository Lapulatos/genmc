# Generic preventive V9 decision

V9 is a significant positive implementation change over V8 and becomes the working
basis for the next candidate-space optimization. It still does not satisfy the frozen
CPU confidence gate against the simultaneous no-pruning baseline, so preventive pruning
remains opt-in rather than becoming a default path.

## Correctness and completeness

- macOS compile-only build links `bin/genmc`;
- server Release: 161/161;
- GCC 13 ASan+UBSan: 161/161;
- mutation oracle: 39 rows / 5,441 full recomputations;
- broad differential: 852 matches / 12 mutual unsupported / 0 mismatch over 864 pairs;
- formal safe execution-count mismatches: 0;
- direct V9/V8 status differences: 0;
- direct V9/V8 search-counter differences: 0 over all pruning cells.

The structural proof and exact activation boundary are recorded in
`v9-direct-root-proof.md`.

## Formal result against simultaneous baseline

Four balanced 24+24-worker repetitions contain 768 cells. Baseline totals are 228 TRUE,
108 FALSE and 48 TIMEOUT; V9 totals are 240 TRUE, 112 FALSE and 32 TIMEOUT. The same four
tasks improve in every repetition: three TIMEOUT-to-TRUE and one TIMEOUT-to-FALSE, with
zero losses.

- CPU ratio: 0.97910 [0.91205, 1.05786];
- wall ratio: 0.97302 [0.90799, 1.05004];
- RSS ratio: 1.00787 [1.00034, 1.02000].

The CPU interval still crosses 1.0, so the frozen default gate says reject. Search-space
reduction remains exact: offered RF+CO -67.82%, queued RF+CO -81.74%, work added -77.53%,
work popped -73.97%, and peak retained work 51,601 -> 801.

## Direct V9 versus V8

- candidate CPU: 0.96105 [0.93854, 0.98130], a significant 3.90% reduction;
- candidate wall: 0.95698 [0.93535, 0.97650], a significant 4.30% reduction;
- candidate RSS: 0.98326 [0.95772, 1.00052];
- corresponding baseline CPU drift: 0.99848 [0.99062, 1.00621].

Across 352 pruning logs with final statistics, full offline evaluations fall from
1,840,748 to 609,440 (-66.89%), offline evaluator time from 405.23 s to 93.68 s
(-76.88%), and total preventive/synchronization time from 642.61 s to 468.29 s
(-27.13%). All 1,231,356 preventive prefix queries use the direct certified root.

The early-error outlier improves but remains unresolved. Median candidate CPU on
`pthread-demo-datarace-3.yml` falls from 2.136 s to 1.742 s (-18.42%), while its V9
baseline is 0.178 s. It still interprets 13,095,345 root-edge derivations for 1,719
prefix queries although both sides pop only 41 work items.

## Next decision

Do not spend another formal matrix on CO lookup micro-optimization: lookup time is only
7.84 s across the completed pruning logs, versus 468.29 s in preventive preparation.
The next experiment must reduce prefix-query count or prune a decision subtree before
re-evaluating its descendants. The preferred V10 direction is a bounded, positive
conflict core extracted from a certified cycle and matched prospectively against RF/CO
choices. Every learned core must be a sufficient CAT violation proof; unsupported
provenance fails open.

