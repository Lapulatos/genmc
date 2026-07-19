# P0.7f: compiled epsilon closure and fused lazy-cycle program

## Baseline and scope

- Starting branch/SHA: `genmc-caat` at
  `d963c49e45b82066ca9dcd036232c7384f6a95fe`, equal to `origin/genmc-caat`.
- Production and test files are clean at the start. Planning and experiment artifacts
  remain intentionally uncommitted.
- Runtime experiments, builds and tests run only in the user-owned Docker environment
  on `server@frp-arm.com:36722`. macOS is limited to editing and light syntax checks.
- The implementation starts from the exact rejected P0.7e3 product-cycle prototype,
  but changes its compiled representation and traversal. P0.7d2 remains the exact
  fallback and performance baseline.

## Evidence and target

P0.7e3 reduced root macro-candidate callbacks by 58.75%, but its four-repetition
matrix executed 18,831,879,352 product transitions, about 3.72 times the retained
macro-candidate count. The NFA interpreter visits epsilon/control vertices and branches
on transition kind on every query. Its aggregate CPU ratio was 0.99930 with no coverage
gain, so the product construction was exact but not cost-effective.

P0.7f targets only this interpreter overhead:

1. compute epsilon closure once during immutable model analysis;
2. replace `vector<vector<transition>>` with one flat transition array and state
   offsets;
3. retain only fused-action states reachable from the entry/reset cycle;
4. remove runtime epsilon transitions and dispatch only relation, set-guard and reset
   actions;
5. preserve the existing 32 KiB query-local color selector and exact P0.7d2 fallback
   for the first experiment.

No model name, task name, verdict, built-in checker result, or benchmark-specific
constant participates in compilation or selection.

## Semantic contract

For every original control state `q`, let `E(q)` be its epsilon closure excluding the
sole reset edge. The fused outgoing actions of `q` are exactly all non-epsilon actions
leaving states in `E(q)`, with duplicate `(kind, predicate, target)` actions removed.
Action targets are interpreted through their own precompiled closure at the next
product step. Therefore fused paths and original epsilon-NFA paths accept exactly the
same event pairs.

Reset remains a labelled zero-event action. The compiled program records reset sources,
so witness projection does not infer reset boundaries from model names or state layout.
The control graph without reset is acyclic; every product cycle still contains a reset,
and splitting at reset actions still yields only accepted root-relation paths.

Any malformed graph, overflow, unsupported predicate, failed closure construction,
or selector miss runs P0.7d2 exactly. Fallback is a loss of optimization only.

## Expected time and space effects

- **Analysis time:** one bounded `O(Q(Q+E))` epsilon-closure compilation per immutable
  check, with `Q <= 256`; paid once and shared by all workers.
- **Query time:** remove all runtime epsilon product transitions, nested per-state
  vectors, and epsilon switch branches; expected reduction is proportional to measured
  epsilon/control transitions eliminated.
- **Query space:** color state changes from original `Q*N` to compact fused-state
  `Qf*N`; the same 32 KiB cap remains. DFS stack depth cannot increase because every
  fused action replaces one or more original product edges.
- **Persistent space:** flat offsets plus actions replace vector-object overhead. This
  is immutable model metadata and is shared across workers.
- **Rollback:** no product color/stack survives a query; immutable compiled programs
  remain valid across insertion and rollback exactly as P0.7e.

## Correctness gates

1. Unit/property comparison of fused product cycles against the materialized root
   relation and the existing streamed P0.7d2 evaluator.
2. Explicit epsilon-heavy union/optional/composition cases, duplicate epsilon paths,
   set guards, reset witness projection, deep path fallback and incremental rollback.
3. Full Release unit/property suite.
4. Focused GCC 13 ASan+UBSan suite.
5. Online mutation oracle: 39 rows and at least 5,441 exact comparisons.
6. Broad differential: 288 programs x SC/TSO/PSO, zero mismatch; mutually unsupported
   pairs remain classified separately.

## Performance gates

Before a formal matrix, run the same balanced two-repetition product pilot used by
P0.7e on TSO fib, TSO queue and PSO queue.

Advance only if all hold:

- product transitions fall by at least 30% on an activated representative;
- no status, verdict or complete-execution mismatch;
- no terminal-result coverage loss;
- every representative CPU median ratio is at most 1.05;
- every RSS ratio is at most 1.02.

If the pilot advances, run the same four-repetition 2,304-cell matrix. Retain only if:

- zero verdict and complete-execution mismatch;
- zero coverage loss and no new OOM;
- task-clustered CPU geometric-mean 95% CI upper bound is below 1.0;
- large-task RSS and snapshot-equivalent state are each at most 1.02;
- counters demonstrate that the measured gain comes from fewer product transitions,
  not from skipped checks or a narrower accepted model/task subset.

Otherwise reject, restore `d963c49`, and retain only protocol, XML/logs and analysis.
