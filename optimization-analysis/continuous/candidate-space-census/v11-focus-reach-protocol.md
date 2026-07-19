# V11 focus-directed preventive reach protocol

## Objective

Remove V9's complete selected-root materialization from each certified RF/CO choice
point while preserving exactly the same candidate set. The complete 725-task V9 run
contains 1,205,405 preventive root queries and 2,943,514,438 emitted root-edge
candidates. Preventive preparation/synchronization records 610.26 seconds, while final
RF/CO lookup records only 12.20 seconds. This is the next measured hot path.

## Required proof before implementation

The initial epoch proposal is rejected. In unbounded exploration,
`findConsistentRf()` and `findConsistentCo()` accept the final candidate through
maximal extensibility without calling `isExecutionValid()`. Therefore the checker
cannot prove that every graph reaching `getCoherentStores()` or
`getCoherentPlacings()` has an authoritative CAT check for the same epoch.

V11 may instead omit V9's global prefix cycle scan only when the analyzer issues a
structural *unassigned-focus sink certificate*. The certificate must prove, for the
selected positive preventive root and separately for a fresh read with no RF and a
fresh write with no CO placement, that the appended focus has no outgoing root edge.
Since the previously accepted graph is consistent by the driver's extensibility
contract, adding a root sink cannot create a cycle. This is a model-expression proof,
not a runtime assumption: unsupported bases/operators, lifecycle ambiguity, or a
focus that is not the current thread suffix fail open to V9.

No assertion or empirical observation substitutes for this certificate. Until its
soundness is implemented and property-tested, V11 stops before changing production
behavior.

## Exact mechanism

For the one analyzer-certified preventive order:

1. materialize only the required stable base primitives, as V9 does;
2. interpret the normalized positive root in a direction-aware lazy cursor;
3. compute the exact forward reachable set from the current RF/CO focus;
4. compute the exact reverse reachable set by interpreting the transposed root:
   relation bases use predecessor cursors, composition reverses operand order, and
   union/intersection/identity/optional/positive closure retain their exact algebra;
5. apply the unchanged V9 `preventsRf` and `preventsCo` tests to those two sets.

Unsupported operator, missing predecessor cursor, recursion-depth overflow, inconsistent
epoch or failed derivation delegates the entire choice point to V9. V11 never returns a
partial reach set.

## Counters

- V11 attempts, exact forward successes, exact reverse successes and V9 fallbacks;
- lazy forward/reverse emitted candidates and visited events;
- full-root checks and root-edge candidates avoided;
- preparation, materialization and final lookup nanoseconds;
- unchanged RF/CO offered, pruned, queued, work-added and work-popped counters.

## Gates

1. Pure randomized property: directional lazy reach equals the materialized V9 root's
   forward and reverse BFS for every event in supported generated models.
2. Sink-certificate unit tests over supported and adversarial normalized expressions;
   RF replacement, CO move, lifecycle ambiguity, non-suffix focus, cut and rollback
   must take V9.
3. Server Release, GCC 13 ASan+UBSan, mutation oracle and 864 broad differential.
4. Direct complete 725-task PSO V9/V11 run with separate rewrite roots and disjoint
   24-core pools. The 96-task set is not an effectiveness gate.
5. Zero mismatch on every common completed verdict, complete-execution count and search
   counter. Resource-limit differences are reported separately.
6. Retain only if full-root checks or emitted root candidates fall, CPU task-bootstrap
   95% upper bound is below 1, RSS upper bound is at most 1.02, and no V9-completed task
   becomes TIMEOUT/OOM.
