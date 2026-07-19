# Superseded standalone SC ordering theory design

> **Status (2026-07-18): superseded before implementation.**  This document records the
> proof obligations discovered during the P1b/P1c investigation, but its proposed
> standalone completion solver would preserve the architectural split between the finite
> SMT enumerator and GenMC.  The active direction is the real `ExecutionGraph` / CAAT
> preventive hook plus a persistent CDCL decision layer, documented in
> `genmc-caat-cdcl-integration-design.md`.  Do not implement this component as the next
> optimization.

## Decision

Replace the rejected global-rank SMT completion query with a dedicated, exact SC
ordering theory for one active-event/control assignment.  The theory receives fixed RF
choices as assumptions, introduces no integer or bit-vector ranks, and searches only the
ordering alternatives required by SC read visibility and successful-RMW atomicity.

This is a diagnostic P1 candidate.  It cannot return a program verdict.  A SAT result is
projected to CO and must pass the unchanged recursive-SC materializer/evaluator.  An
UNSAT result may refine the abstract encoder only when its proof has eliminated every
internal ordering choice and contains RF assumptions alone.

## Why the existing components are insufficient

- `CAT/LazyCycle` can find and explain a cycle in a positive CAT expression after its
  base relations are fixed.  It cannot represent an existential choice between two
  order orientations.
- `CAT/IncrementalEvaluator` transactionally inserts and rolls back already selected
  base facts.  It does not search Boolean RF choices or resolve conflicts across two
  alternative derived orders.
- `CAT/ConflictCore` stores positive ground facts for complete evaluations.  The earlier
  V1--V4 experiments measured a net regression and its cores do not prove that every CO
  completion of a fixed RF assignment is inconsistent.
- `SCGoodWritesSolver` is an exact and fast fixed-RF feasibility oracle, but its state
  search returns no proof.  A core from one dead-end prefix is not valid for the other
  possible prefixes, while repeated deletion/QuickXplain calls caused the measured P1b
  regression.

The new component may reuse dense event IDs and the final recursive-SC check, but not any
of those incomplete explanation mechanisms.

## Exact SC constraint system

For active events, let `<` denote the unknown global SC order.  Add mandatory edges for:

1. `po`, `tc`, and `tj`;
2. every selected RF edge `w -> r`;
3. every initial write before each non-initial write to the same address.

For a read `r` whose selected source is `w`, and every other write `x` to the same
address, add the disjunction:

```text
x < w  OR  r < x
```

For a successful RMW read `r` with write successor `u`, selected source `w`, and every
relevant external write `x` to the same address, add:

```text
x < w  OR  u < x
```

These constraints say that no competing write occurs between a source and its read, or
between the source and the successful RMW write.  Acyclic mandatory edges plus one chosen
orientation for every disjunction have a topological order.  Projecting that order onto
writes yields a CO for which `po | tc | tj | rf | fr | co` is acyclic.  Conversely, every
recursive-SC witness satisfies one orientation of every disjunction.  This establishes
equisatisfiability for the admitted finite SC subset; the exhaustive oracle remains the
executable guard against an implementation error.

## Incremental graph

Maintain direct edges, not a materialized transitive closure.

- Before adding `u -> v`, search deterministically for a path `v -> u`.
- If found, the new edge plus that path is a cycle conflict.
- Otherwise append the edge and its proof reason to an undo trail.
- A checkpoint is the trail length.  Rollback removes every later edge and restores the
  exact prior graph.
- Parallel edges are allowed only when their reasons differ; dominance/removal is not in
  the first implementation.

This avoids the rejected quadratic all-different/rank formula.  Work counters must expose
edge insertions, reachability visits, forced orientations, decision branches, rollbacks,
and maximum search depth.

## Proof and conflict contract

A reason is a sorted set of literals of two kinds:

- `RF(read)`: the selected source assumption for one active read;
- `Branch(id, side)`: an internal orientation decision.

Mandatory lifecycle/order edges have an empty reason.  A selected RF edge and every
visibility/RMW disjunction derived from it depend on `RF(read)`.

At a disjunction `A OR B`:

1. If adding `A` immediately cycles, its cycle reason (with the trial literal removed)
   justifies forcing `B`; symmetrically for `B`.
2. If neither side is forced, create one branch ID and solve both orientations.
3. If either side has a witness, return it.
4. If both sides conflict, resolve the two conflict reasons on the opposite branch
   literals.  The result is their RF/outer-branch union with the current branch literals
   removed.
5. A root UNSAT result is refinement-safe only if no `Branch` literal remains.  Otherwise
   fail open and do not block an encoder assignment.

An empty root core is valid only when mandatory RF-independent edges are cyclic.  For the
strictly admitted finite skeleton this means the active control/lifecycle assignment is
itself inconsistent; the first integration will fail open rather than add a new
control-core API.

The first version deliberately does not minimize the proof core.  It must eliminate
internal choices in one graph search; repeated completion calls are forbidden on the
performance path.

## Search and propagation

Run to a fixed point before branching:

- satisfied: one orientation is already reachable;
- forced: one orientation would immediately close a cycle, so add the other with the
  corresponding proof reason;
- conflict: both orientations immediately close cycles;
- unresolved: neither orientation is implied or forbidden.

Choose the unresolved disjunction with the largest current reachability impact; use a
stable index as the tie breaker.  This is ordering only: it never removes a candidate.
Property-directed ordering is deferred until the exact mechanism passes the panel.

## Witness construction and independent checking

On SAT:

1. topologically sort the final graph;
2. project non-initial writes into per-address CO order;
3. materialize the completed `FiniteAssignment` through the production CAT adapter;
4. run the exact recursive-SC base checks, including RMW atomicity.

Failure at steps 3--4 is `witnessRejected`, never SAT and never a refinement clause.

## Admission and fallback

- Use only after the existing strict finite-skeleton admission and a fixed source for
  every active read.
- Unsupported identity/address/RMW shapes return `invalidInput` and fall back to the
  unchanged native verifier.
- Solver budget exhaustion returns an inconclusive status and falls back; it must not be
  translated into UNSAT.
- The default verifier and non-SC CAT models remain unchanged.

## Verification gates before a server performance run

1. Unit tests for insertion, cycle reasons, rollback, forcing, branch resolution, and
   residual-branch fail-open.
2. Exhaustive comparison against every CO permutation for small fixed-RF graphs,
   including successful RMW cases.
3. Differential comparison with `SCGoodWritesSolver` for generated small proper event
   sets.
4. Local Release and server Docker Release plus ASan+UBSan focused tests.
5. Only then run the frozen 15-task `pthread-wmm` panel in the server Docker container.

Promotion requires identical supported/fail-open classification at correctness gates and
a material improvement over P1b/P1c in completed panel rows, CPU, and peak RSS.  If the
panel does not improve, reject the mechanism without a 283-task or 725-task run.
