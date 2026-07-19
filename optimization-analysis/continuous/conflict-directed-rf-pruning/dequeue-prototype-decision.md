# Dequeue-time RF nogood prototype: stop before server experiments

## Decision

Do not run the 2,304-cell formal matrix and do not retain this prototype. The mechanism
filters a learned-inconsistent `ReadForwardRevisit` before destructive cut/replay, but
the RF choice has already been enumerated and queued. It can reduce per-invalid-branch
work while leaving GenMC's candidate-generation algorithm and consistent execution
equivalence classes unchanged. The current priority is direct reduction of generated
candidates and decision subtrees.

## Implemented but deliberately unmeasured

- Reconstructed the previously validated P0-A V3 lazy-preserving positive nogood base,
  excluding V4's rejected 1,024-query warm-up.
- Added an exact RF clause index keyed by worker-stable source/read identities.
- Added a two-stage dequeue hook: cheap exact-RF prefilter, then exact copied cut-prefix
  materialization and complete clause match.
- Added counters for worklist queries, RF index hits, copied labels, prefix time and
  pruned revisits, plus focused RF and initial-write index tests.
- Local macOS compilation of `genmc` and `unit_tests` completed successfully. This is
  compile evidence only; no test or benchmark was run on macOS.

No Release, sanitizer, mutation, broad-differential or performance result is claimed.
Stopping before those runs avoids spending server resources on a mechanism that does
not address the newly clarified primary objective.

## Artifact and restoration

- Exact prototype patch: `rejected-dequeue-prototype.patch`
- Size: 77,394 bytes
- SHA-256: `29ea5633f5b001125bac00478b7b652d6ca2c082518ac22d50c096d48b8bd58d`
- Production/test/CLI paths were restored with `apply_patch`.
- `git diff --exit-code -- genmc tests lli` passes.
- The archived patch passes `git apply --check` against retained commit
  `d963c49e45b82066ca9dcd036232c7384f6a95fe`.

## Reusable design result

The stable RF index and exact hypothetical-prefix matcher remain useful as an oracle or
correctness test for generation-time propagation. They should not become the main
runtime path. The next implementation must prevent alternatives from being queued or
eliminate a certified decision subtree, and its primary metrics must count generated
RF/CO choices, queued revisits, realized prefixes and complete/equivalence-class
executions.
