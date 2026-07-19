# P0-A regional SC-RVF implementation-entry audit

## Decision

Do not implement a generic regional fallback in the current task decomposition. The
existing whole-program gate is not merely conservative benchmark filtering: it prevents
an unsound transition back to native RF-DPOR after an ancestor RVF frame has already
merged alternatives.

P0-A may resume only after either:

1. the full remaining program suffix is certified before the first quotient merge; or
2. a region protocol owns all descendant tasks, records covered classes, and can revoke
   every representative/parent-continuation task before native fallback.

Neither protocol exists in the current source.

## Authoritative code boundary

### Root certification

`lli/main.cpp::adjustConfig()` runs `findUnsupportedScRvfInstruction()` over the complete
transformed module. `ThreadPool` creates the root `rvf::Frame` only when
`scRvfProgramSupported` remains true. This means native fallback happens before any RF
class can be merged.

### Irreversible merge point

`GenMCDriver::handleRVFLoad()`:

1. clones the parent graph and choices;
2. groups viable sources by value, provenance, and own/non-own source;
3. creates one parent continuation and one accepted witness child per value group;
4. submits every generated execution to the global `ThreadPool`;
5. moots the current execution.

At step 4, concrete RF alternatives within one group are no longer separate native work
items. The fallback lambda used by later reads resets only the current execution's RVF
frame and enumerates sources visible at that current prefix. It cannot reconstruct the
deleted alternatives of an ancestor read.

### Missing regional protocol state

`rvf::Frame` currently stores only:

- `goodWrites`;
- `causalCutoffs`;
- `processedReads`.

It has no region identity, entry snapshot, exit frontier, covered-class set, ownership
epoch, revocation token, or fail-open ledger. `ThreadPool::submit()` immediately increments
global remaining work and pushes an independent task. There is no API to atomically
withdraw all descendants of one region.

## Why the dominant `abort` reason cannot simply be whitelisted

The formal gate reports external `abort` first in 53 tasks. In
`lli/Runtime/ExternalFunctions.cpp`, `lle_X_abort()` calls `raise(SIGABRT)`; it is not a
GenMC graph event or a normal `VerificationError::VE_Safety` endpoint. Adding it to
`safeInternal` would therefore broaden the static gate without supplying witness replay,
error-kind, or worker-pool termination semantics.

A valid repair must lower the benchmark property endpoint to the existing driver-managed
assert/safety path, or add an explicit terminal operation whose baseline and quotient
behavior is proved equal. It must then enumerate all remaining unsupported reasons rather
than assume the 53 tasks become enabled; the current scanner returns only the first reason.

## Required entry gates for future P0-A work

1. Property endpoint normalization has a baseline-only differential proving identical
   verdict, error kind, graph/error trace, and worker termination.
2. The static scanner reports all unsupported operation classes per task, so activation
   after normalization is known without repeatedly changing the whitelist.
3. The chosen scope is either whole-suffix certified or has an explicit reversible region
   protocol; per-load `rvf.reset()` is not accepted as a completeness argument.
4. Generated future-write/nested-frontier oracles map every baseline maximal execution to
   a class and compare all reachable observations.
5. Parallel ownership is proved structurally, not only by equal n1/n2 counts.

## Priority consequence

P0-A remains the preferred consistent-class reduction research direction, but it is not
implementation-ready. The active implementable core direction becomes P0-B generation-
time CAT subtree blocking, whose acceptance criterion is preventing work creation at an
earliest rollback-safe decision. No evaluator micro-optimization is promoted while P0-A
is design-blocked.

## Property-endpoint normalization result (2026-07-18)

The SV-COMP adapter now recognizes only exact `reach_error` endpoints backed by
`assert(0)` or direct `__assert_fail`. It removes an immediately following
`reach_error(); abort();` only after normalizing that endpoint to the driver-managed
assert-failure path. Four unit tests cover both accepted forms, an unmatched endpoint,
and an unrelated abort; all four pass.

An isolated server/Docker smoke used three real tasks previously classified as external
`abort`. Baseline, control, and RVF all returned `true`, and baseline/control search
counters stayed equal. The abort blocker disappeared, but the next gate reason was
non-atomic store, modeled `__VERIFIER_malloc`, or modeled mutex lock respectively.
Therefore this is a semantic enabler, not RVF activation or performance evidence.
