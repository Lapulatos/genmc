# P0-A: cross-execution CAT learned nogoods

## Starting state and execution boundary

- Branch/commit: `genmc-caat` at
  `d963c49e45b82066ca9dcd036232c7384f6a95fe` locally and on
  `origin/genmc-caat`.
- Dirty files before implementation: existing `task_plan.md`, `notes.md`, and
  untracked analysis/experiment material. They are user/project evidence and must not
  be overwritten or staged with production code.
- Build/test host: only `server@frp-arm.com:36722`, inside a new container based on
  `genmc15noble:sujie`, with `/data3/sujie` mounted. No server path, credential,
  container configuration, XML, or log is committed.
- Verdict authority remains the normalized CAT/CAAT evaluator. This optimization does
  not call a built-in SC/TSO checker to decide consistency.

## Hypothesis

An explained violation is a conjunction of ground base facts sufficient to replay the
same CAT violation. For a positive normalized model, any later graph snapshot that
contains the same positive facts still contains that derivation. Retaining the
conjunction as a worker-local nogood can therefore reject repeated inconsistent
RF/CO/graph choices before another fixed-point evaluation.

The first version deliberately learns only when all of the following hold:

1. the normalized model is online-admissible and contains no difference;
2. the failed check is `acyclic` or `irreflexive`;
3. the Reasoner explains the violation entirely with positive base literals;
4. every literal endpoint maps to a worker-stable real-event or initial-write key;
5. the clause length is within the explicit bounded database limit.

Unsupported explanations are ignored. Ignoring a clause affects performance only;
it never changes the candidate result.

## Reuse decision

- Reuse `Reasoner` as the only provenance reconstruction authority. Do not implement a
  second CAT derivation engine.
- Reuse `StableGraphAdapter` identities so event IDs are never interpreted across
  unrelated worker lifetimes.
- Reuse `GraphSynchronizer` as the pre-evaluator interception point because it already
  owns the materialized `BaseValues` and classifies every graph mutation.
- Reuse `CaatEvaluator`/`--cat-oracle` as the correctness oracle. Oracle mode disables
  skip-on-hit unless it independently evaluates the skipped snapshot.
- Implement only a small C++23 bounded nogood database. Snapshot caching and external
  SAT dependencies are out of scope.

## Semantic contract

One learned nogood stores predicate name/type, polarity, and stable endpoint keys. A
snapshot matches only when every stored literal is present in its current primitive
map. V1 stores positive literals only, so universe growth and unrelated edge insertion
cannot invalidate the witnessed positive derivation.

On a match, `GraphSynchronizer` returns `knownInconsistent=true` without changing the
incremental evaluator or its history. The next non-matching query is classified against
the last evaluated state and may rollback or rebuild normally. `BasicCATChecker`
returns false directly and must not inspect a stale evaluator result.

The database is worker-local. It is cleared only with checker destruction; stable keys
are never reused during that lifetime. Capacity eviction may lose pruning power but
cannot remove an execution.

## Expected time and space effects

| Step | Expected time reduction | Added work | Space effect |
|---|---|---|---|
| learn on a new violation | none immediately | one Reasoner reconstruction, normalization and subsumption pass | one compact literal vector |
| preflight match | skip fixed-point initialize/rollback/rebuild, violation search, and history mutation on hit | scan indexed candidate clauses and ground literals | bounded database/index |
| future RF/CO prospective filter (not V1) | also skip graph construction and interpreter replay | prospective base-literal matcher | same database |

V1 may regress when violations rarely repeat because explanation and match scans are
new overhead. It is retained only if measured hits amortize that cost or if V1 provides
a necessary, low-overhead basis for the separately gated prospective filter.

## Bounded representation

- Default maximum learned clauses: 4096 per worker.
- Default maximum literals per clause: 64.
- Exact duplicates and supersets of an existing clause are not retained.
- A new strict subset removes stored supersets.
- FIFO eviction is acceptable in V1; activity/LBD retention is deferred until counters
  prove eviction pressure.
- Statistics: learn attempts, learned, duplicates/subsumed, unsupported, evicted,
  match queries, literal checks, hits, and maximum/current clauses/literals/bytes.

## Correctness gates

1. Pure unit tests:
   - explanation-to-stable-key conversion;
   - positive set/relation matching;
   - absent/inactive endpoints do not match;
   - duplicate and subset/superset behavior;
   - capacity eviction;
   - universe growth preserves a valid match;
   - negative, non-cycle, malformed and oversized explanations fail closed.
2. Randomized property test: for every learned positive explanation, a match must replay
   a violation under a fresh `CaatEvaluator`.
3. Server Release unit/property suite.
4. ASan+UBSan focused tests.
5. Mutation stress: all 39 rows and every per-query full oracle check, zero mismatch.
6. Frozen recursive broad differential: 288 programs × SC/TSO/PSO, zero verdict,
   execution-count, error-category, or unsupported mismatch.
7. One- and two-worker deterministic comparison where applicable.

## Performance gates

Performance comparisons use fresh baseline/candidate builds, simultaneous disjoint core
pools with socket/core swaps, at least four repetitions, one GenMC exploration thread per
task, and task-clustered bootstrap intervals. Raw XML/logs remain under
`/data3/sujie/experiments/caat-optimization/learned-cat-nogoods/` and are pulled to this
experiment directory before a decision.

Progressive workloads:

1. nine deterministic, complete TIMEOUT-vs-Deagle core tasks;
2. a same-input/seed diagnostic subset with repeated inconsistency;
3. the established 96-task × SC/TSO/PSO matrix;
4. the 725-task adapted set at 60 seconds only after focused evidence is positive.

Retain V1 only if all correctness gates pass and at least one predeclared condition holds:

- at least three of the nine core tasks newly terminate within 60 seconds; or
- common-terminal aggregate CPU upper 95% confidence bound is below 1.0 and nogood hits
  explain the saving; or
- a measured hit-rich cohort has at least 20% fewer evaluator queries and at least 10%
  lower CPU, without aggregate/common-terminal regression above 1%.

Peak RSS P90 must stay below 256 MiB on the core/diagnostic cohorts and may not increase
by more than 5% on the broad common-terminal matrix. Otherwise remove production/test
changes and retain only the protocol, raw evidence, and rejection decision.

## Expected production/test files

- `genmc/genmc/CAT/LearnedNogood.{hpp,cpp}`
- `genmc/genmc/CAT/StableGraphAdapter.{hpp,cpp}`
- `genmc/genmc/CAT/GraphSynchronizer.{hpp,cpp}`
- `genmc/genmc/Execution/Consistency/CATChecker.{hpp,cpp}`
- `genmc/genmc/Verification/Config.{hpp,cpp}`
- `lli/main.cpp`
- `tests/unit/CatEvaluatorTest.cpp`, `tests/unit/ConfigTest.cpp`
- focused CLI/CAT integration tests and CMake source registration as required.

No model file, built-in checker, accepted CAT semantics, or benchmark source is changed.

## Decision rule

The implementation remains experimental and uncommitted until the full frozen gate is
complete. A correctness mismatch immediately stops performance testing. A performance
failure removes the prototype; post-hoc task/model selection is not allowed.

