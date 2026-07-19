# P0-B generation-time CAT subtree-blocking entry audit

## Decision

Do not reimplement the existing positive-cycle core matcher as a “generation-time”
optimization. V10 already runs before RF/CO candidate vectors are returned to the driver
and before revisits are enqueued. Its 14,193,035 hits do not reduce V9's surviving search
because every learned core proves only that the same concrete proposed candidate is
inconsistent—exactly the candidate V9 already removes.

No current proof justifies promoting such a candidate-level clause into a blocker for a
consistent parent prefix or its other descendants.

## Existing generation boundary

`BasicCATChecker::getCoherentStores()` and `getCoherentPlacings()` prepare the current
base prefix, add the proposed RF/CO delta, and match `ConflictCoreDatabase` before returning
the candidate list. `GenMCDriver::handleLoad()` enqueues only candidates returned by this
method. Therefore moving the same match “earlier” is not a new search-space mechanism.

The measured `conflict-core-direct-checks-avoided=0` means the learned database never
eliminated a root beyond what the retained preventive proof already established. Common-
correct execution and search counters remain equal, while CPU regresses 4.99%.

## Proof limitation

A learned positive clause `C` is sufficient to prove that one concrete graph plus one
proposed RF/CO delta violates a CAT check. It does **not** establish that:

- the current parent prefix is inconsistent;
- every remaining RF/CO continuation below that prefix is inconsistent; or
- two consistent continuations are behaviorally equivalent.

Non-chronological backjumping may avoid reconstructing the same invalid assignment when
its literals recur, but it cannot soundly delete other consistent extensions without a
stronger no-extension certificate. That is candidate-level rejection reuse, not the core
equivalence/subtree reduction sought by this project.

## What would make P0-B core work

P0-B may resume only with a certificate for **no consistent extension** of a partial
decision assignment, or a proof that a complete set of sibling choices is covered by
another representative. Such a certificate must identify its rollback scope and be
checked independently against exhaustive small decision trees. A concrete-cycle
explanation alone is insufficient.

## Priority consequence

The only evidence-backed core path currently remaining is SC-RVF/another complete
consistent-class quotient. Keep the safe whole-program/whole-suffix boundary and expand
its supported semantics with proofs, beginning with property endpoint normalization.
Do not substitute V10-style nogoods, dequeue filtering, or evaluator caches.
