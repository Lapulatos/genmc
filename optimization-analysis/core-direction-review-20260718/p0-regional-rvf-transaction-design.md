# P0 regional SC-RVF transaction design

## Objective

Enable reads-value-from quotienting inside dynamically encountered supported regions even when the
complete transformed program contains operations outside the current SC-RVF proof scope. The
implementation must preserve every native maximal execution and error observation. A late
unsupported operation must never continue from a graph whose ancestor RF alternatives were already
merged.

## Why a frame-local reset is insufficient

`handleRVFLoad()` currently replaces several concrete RF alternatives with one representative and
submits the representative plus a parent continuation to the global pool. Resetting `Frame` in one
descendant cannot recreate the concrete alternatives removed at an ancestor. Correct regional
fallback therefore needs a transaction rooted immediately before the first quotient merge.

## Ownership objects

Each active region has a pool-owned `RegionTransaction`, a sibling pool-owned result buffer, and
each submitted execution carries a small `(region-id, epoch)` token. Keeping `VerificationResult`
inside the pool also avoids crossing the core-library/test ABI boundary created by debug-only
result fields.

The transaction owns:

- the untouched native entry execution, captured before the first merge;
- an exact count of queued and running region descendants;
- a covered-class ledger keyed by stable read and source identities;
- a pool-local buffered `VerificationResult` for speculative descendants;
- an exit/fail-open ledger with the first unsupported operation and graph position;
- an atomic state: `active`, `revoking`, `committed`, or `revoked`;
- a monotonically increasing epoch so stale queued tasks cannot re-enter a reused identity.

The RVF `Frame` owns only region-local solver state and the region token. The global queue owns task
lifetime. No raw pointer from a frame to a queue entry is permitted.

## State machine

```text
NATIVE
  first quotient opportunity
    -> begin(active, native-entry-snapshot)
    -> submit parent continuation and representative children with the same token

ACTIVE
  supported descendant
    -> may extend the covered-class ledger and submit token-matching children
  clean descendant completion
    -> buffer its result and decrement outstanding descendants
  unsupported boundary or incomplete proof
    -> atomically change active -> revoking

REVOKING
  queued descendants
    -> discard before execution
  running descendants
    -> stop at the next driver boundary without publishing results
  last speculative descendant retired
    -> discard buffered results
    -> submit exactly one native entry snapshot with no region token
    -> revoked

ACTIVE with zero outstanding descendants
    -> publish the buffered result exactly once
    -> committed
```

## Result isolation

Region tasks are speculative until commit. Their errors, warnings, explored counts, statistics,
messages, and specifications cannot be merged into a worker's durable result, print a final trace,
or halt the pool early. The worker loop therefore needs per-task result extraction:

1. reset the driver's task result before `initFromState()`;
2. execute one task;
3. move the task result to `ThreadPool::completeTask()`;
4. merge a native result immediately;
5. buffer an active-region result;
6. discard a revoked-region result;
7. publish the aggregate buffered result only when the transaction commits.

Error metadata replay remains inside the same speculative task. Pool-wide halt is deferred until a
committed transaction publishes a hard error.

## Completeness invariants

1. The native entry snapshot is captured before any RF class is merged.
2. Exactly one of the following becomes durable: all committed quotient descendants, or the native
   fallback rooted at the entry snapshot.
3. A revoked token cannot enqueue new descendants or publish a result.
4. Every submission and retirement changes the transaction's outstanding count under the same
   synchronization boundary as queue visibility.
5. Nested regions use a token stack. Revoking an ancestor revokes every descendant transaction;
   committing a child buffers into its parent rather than publishing globally.
6. Stable event identities include thread and dynamic event index; region epochs distinguish loop
   iterations and replayed entry snapshots.
7. Future writes remain owned by the parent continuation while a region is active. After revocation,
   native RF-DPOR alone owns them from the restored entry.
8. Cross-worker execution is permitted only through shared transaction state; n1 and n2 must expose
   the same committed class and observation sets.

## Implementation sequence

1. Refactor the worker loop to produce one movable `VerificationResult` per task and prove native
   aggregate equality without enabling regions.
2. Add token-tagged queue submission, outstanding accounting, stale-task rejection, and deterministic
   unit tests for commit/revoke races.
3. Add the native entry snapshot and fail-open replay, initially with quotient disabled, and prove
   exact native equality.
4. Enable one non-nested supported region and run the future-write/own-source/provenance oracle.
5. Add nested token stacks, loop epochs, error buffering, and one/two-worker exhaustive oracles.
6. Run the production activation census. Only nonzero class/work reduction advances to resource
   experiments.

## Stop gates

- Any lost or duplicated baseline observation rejects the design.
- Any task result published before region commit rejects the design.
- Any stale descendant executed after revocation rejects the design.
- Zero production activation or zero reduction in offered/queued work, popped work, realized
  prefixes, or representatives stops the performance branch.
- A timing improvement with higher peak memory, or a memory improvement with slower terminal work,
  is not retained.
