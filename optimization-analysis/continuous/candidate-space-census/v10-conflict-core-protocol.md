# V10 prospective CAT conflict-core protocol

## Objective

Reduce the number of selected-order evaluations and repeated candidate subtrees, not
only the cost of one consistency query. V10 learns a bounded sufficient positive base
fact conjunction from a candidate that V9 already proves cyclic. Future RF/CO choices
matching the same conjunction are rejected before the selected order is interpreted or
the revisit is enqueued.

This is a candidate-space experiment, not an evaluator-cache experiment. A V10 hit must
occur before the rejected RF/CO revisit is materialized or enqueued. The stronger target
is to retain the core for its sound rollback lifetime, associate its literals with
decision levels, and cut/backjump over every descendant for which the positive core
remains true. A hit observed only after candidate materialization may be reported as a
checker saving but cannot satisfy the advancement gate.

## Soundness boundary

V10 is enabled only on V9's analyzer-certified preventive order. For a rejected
candidate, it records:

1. one exact current-order path from the proposed edge's target back to its source;
2. one exact positive base derivation for every order edge on that path;
3. the proposed RF, CO, or FR base edge, plus structural filter facts needed by the
   certified seed.

The conjunction is retained only when every derivation is positive, ground, active,
deduplicated, and no longer than 64 literals. Unsupported set difference, missing
provenance, empty/oversized clauses, or unstable endpoints fail open.

If a later current snapshot plus one proposed candidate delta contains every literal,
the same checked-order cycle is present. Rejecting that candidate cannot remove a CAT-
consistent execution. The database never treats a hash as proof; matching is exact.

## Bounded representation

- worker-local only;
- at most 4,096 clauses and 64 literals per clause;
- stable predicate IDs and stable event IDs from V9's preventive adapter;
- sorted literals, exact duplicate elimination and subset/superset minimization;
- FIFO eviction loses pruning power only;
- clauses indexed by candidate predicate/endpoint where practical, but the exact
  literal conjunction remains the verdict authority.

## Expected cost changes

- A hit avoids V9's root interpretation, CSR construction, forward/reverse BFS, and
  creation/enqueue of the rejected revisit subtree.
- Learning pays one BFS-path reconstruction and one deterministic lazy derivation only
  on a V9-proven rejected candidate admitted by the bounded policy.
- Misses pay exact clause probes; admission must use realized hits to prevent repeated
  expensive provenance extraction, following the earlier learned-nogood evidence.
- Persistent space is bounded clause/literal storage. No graph snapshot or dense
  relation is retained.

The completed V9 matrix gives the relevant ceiling: 1,231,356 direct root checks and
468.29 s of preventive preparation. CO/RF lookup is only 7.84 s and is not the target.

## Required counters

- learn attempts, learned, unsupported, duplicate/subsumed, evicted;
- match queries, literal checks, hits;
- direct-root checks avoided;
- RF/CO candidates pruned by learned cores;
- current/maximum clauses, literals and bytes.

## Gates

No small performance pilot is used.

1. Pure randomized property: every learned/matched conjunction replays an acyclic-check
   violation in a fresh fully materialized CAAT evaluation.
2. Release and GCC 13 ASan+UBSan suites.
3. Mutation oracle 39/5,441 and broad differential 852/12/0.
4. Four balanced 24+24-worker formal repetitions over the same 96 PSO tasks.
5. Zero verdict, safe-execution-count and accepted-candidate mismatch.
6. Advancement requires a measured reduction relative to V9 in direct-root checks or
   offered/queued/work counters, with learned hits accounting for the reduction.
   Results must distinguish individual pre-enqueue rejection from whole-subtree or
   equivalence-class elimination.
7. Default retention still requires the frozen CPU upper 95% bound below 1 and RSS upper
   bound at most 1.02. Otherwise archive and continue to EOG/CEGAR exploration reduction.
