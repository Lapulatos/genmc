# Deagle value-class SC-order encoding report (2026-07-20)

## Decision

Retain and commit the exact finite-SC value-class ordering encoder on
`genmc-caat-opt-dev`.  The final linear program-order formulation improves CPU, wall
time, memory, and terminal coverage simultaneously against the exact concrete-RF/SC
control.  It remains an opt-in finite-skeleton diagnostic and must not be promoted as a
complete weak-memory verifier or merged into the stable branch until end-to-end replay
integration and the remaining lifecycle rules are complete.

## Algorithm

For every active load `r`, the concrete encoder chooses one reads-from source from all
same-location writes.  The new encoder first groups sources with the same represented
value, selects one class `C`, and introduces an SC rank for every ordering event.  A
selected class is feasible exactly when

```text
exists w in C:
    active(w) and w <sc r and
    no active same-location write w' satisfies w <sc w' <sc r.
```

The model therefore does not enumerate each member of a same-value class as a separate
first-layer choice.  Model extraction takes the latest active write before each load,
checks that it belongs to the selected class, and returns concrete `rf` and per-location
`co`.  Every SAT witness is then materialized and evaluated by the CAT model.  The
ordering theory does not call GenMC's native consistency checker.

The first implementation emitted every active program-order pair, which was quadratic.
The final version sorts static event sites in each function and constrains adjacent
sites.  Inactive branch sites remain in the chain.  This is equisatisfiable for active
events, preserves every required strict program-order edge by transitivity, and reduces
the program-order part from O(n^2) constraints to O(n).

## Exactness boundary

The encoding is exact for the admitted finite SC skeleton fragment:

- loads, stores, fences, thread creation, locks, and unlocks are ordered;
- a successful lock is represented as one atomic read/write event and reads the latest
  preceding mutex write;
- same-location active events have distinct ranks; cross-location rank ties are allowed
  because every satisfying preorder has a total-order extension;
- concrete `rf/co` extraction is mandatory before CAT evaluation or replay;
- SC-order models cannot enter abstract-member refinement, concrete RF-core blocking, or
  CAT-explanation blocking.

Thread join is supported when its handle resolves through SSA phi/cast nodes to exactly
one `threadCreate` event.  The encoding requires the corresponding create to be active,
orders the create before the child entry, orders every active child return before the
join, and materializes the matching CAT `tj` edge.  Dynamic, missing, or ambiguous join
targets return `unresolved-thread-join-target` and remain fail-open.  Combining the
SC-order encoding with eager coherence ranks is also rejected.  These are fail-open
boundaries, not approximations.

The result is not full RVF-SMC, does not establish optimal exploration, and does not yet
encode TSO/PSO/POWER visibility and propagation constraints.  Native GenMC is used only
as an external correctness reference in the broad experiment.

## Correctness evidence

The post-lifecycle release C++ unit binary built in the GCC 13/Z3 Docker environment
runs 249 tests: 248 pass and the no-backend test is expectedly skipped because Z3 is
present.  The new
gates include:

- a nine-combination concrete-RF/refinement oracle whose reachable observations equal
  the value-class SC-order observations;
- an exhaustive 36-shape oracle covering both load positions, same/mixed write values,
  and both threads; every abstract-class observation equals direct concrete RF plus exact
  SC completion, and every extracted assignment is accepted by the independent exact SC
  base;
- atomic lock/unlock materialization;
- exact create/return/join ordering and explicit fail-open behavior for unresolved
  thread joins.

An end-to-end four-outcome store-buffering oracle exercises two creates and two joins.
Both lanes classify `00` as safe and `01`, `10`, and `11` as reachable errors.  Every
finite SAT witness is concrete and has zero CAT materialization, evaluation, or
violation errors.

The meaningful SC-order subset also passed ASan+UBSan before the linear-chain rewrite;
the rewrite changes only the transitive representation of program order.  The final
release source was rebuilt after the safety guards and all four SC-order-focused tests
passed again.

Across the broad candidate run, all 68 SAT models were concrete and all had zero CAT
materialization errors, evaluation errors, and violations.  Against the authoritative
native GenMC result set, 49 tasks were classified by both lanes with zero verdict
differences.  All 107 candidate classifications also agree with the benchmark expected
verdict.  These checks support first-error/UNSAT correctness for the admitted finite SC
fragment; they do not turn this first-model lane into an end-to-end verifier.

## Rejected quadratic formulation

The initial 15-task paired panel is preserved at
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-panel-20260720-r1`.
It classified five tasks instead of the control's two and had no verdict or witness
failure, but all-task memory was 1.36748 times the concrete control.  Although CPU was
0.78686 times the control, this violated the project's no time-for-memory-tradeoff rule.
The quadratic formulation was therefore rejected.

After replacing all active program-order pairs with static adjacent chains, the 15-task
candidate panel at
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-chain-panel-20260720-r1`
classified seven tasks (four SAT, three UNSAT) versus two control UNSAT results.  Total
CPU and memory ratios were 0.68429 and 0.26835 respectively.  All four SAT witnesses
passed concrete CAT validation.  This version passed the simultaneous resource gate and
was selected for the full run.

## Full 283-task exact-control experiment

The candidate root is
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-full-20260720-r1`.
The exact concrete RF plus SC-completion control is
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-concrete-sc-full-20260720-r1`.
Both contain exactly 283 XML/log rows, have zero manifest failures, use the same task set,
and enforce the same per-task limit.

| Metric | Concrete RF + exact SC | Value class + SC order | Candidate/control |
|---|---:|---:|---:|
| summed CPU | 26,891.814 s | 18,009.400 s | 0.66970 |
| summed wall | 26,909.903 s | 18,027.341 s | 0.66991 |
| summed per-task peak RSS | 99,848,499,200 B | 32,254,595,072 B | 0.32304 |
| SAT | 0 | 68 | -- |
| UNSAT | 5 | 39 | -- |
| unknown/timeout | 278 | 176 | -- |
| classified | 5 | 107 | 21.4x |

Thus the selected encoding reduces aggregate CPU and wall time by about 33.0%, reduces
summed per-task peak RSS by 67.7%, and produces 102 additional terminal classifications.
The five commonly classified tasks have zero verdict differences and zero per-task CPU,
wall, or memory regressions.  Their summed CPU, wall, and memory ratios are 0.00733,
0.00757, and 0.53032 respectively.

BenchExec labels stats-only terminal processes as `ERROR`, so its raw status is not a
verdict.  Verdict classification is derived from the emitted solver/assignment records:
the concrete lane has 63 `ERROR` and 220 `TIMEOUT` rows; the candidate has 165 `ERROR`
and 118 `TIMEOUT` rows.  The strict analyzer separately requires solver status,
assignment presence, and a clean concrete witness.

## Native reference experiment

Joining the same 283 candidate rows with the authoritative native results gives:

- candidate: 68 SAT, 39 UNSAT, 176 unknown;
- native: 90 SAT, 14 UNSAT, 179 unknown;
- common classifications: 49;
- common verdict differences: 0;
- candidate expected-verdict mismatches: 0;
- bad candidate witnesses: 0.

Native performance is not a promotion control for this finite algorithm: it explores a
different representation and is currently much faster on the commonly completed subset.
It is used here to detect unsound SAT/UNSAT answers, not to claim that the prototype is
already faster than production GenMC.

## Post-lifecycle 283-task regression

The join/return lifecycle implementation was rerun at
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-full-20260720-r5`
with the same 283 tasks, 120-second per-task time, and 4 GB per-task memory limit.  Exact
cardinality checks found 283 XML rows and 283 archived logs.  It produced 69 SAT, 39
UNSAT, and 175 timeout/unknown results, with zero bad concrete witnesses.  The original
run produced 68 SAT, 39 UNSAT, and 176 unknown results.  Twenty-three tasks changed only
whether they crossed the 120-second boundary; the net terminal gain is one.

Aggregate CPU and wall time ratios relative to the original candidate are 0.99090 and
0.99045; aggregate peak-memory ratio is 1.00109.  These sub-percent movements are
parallel-run noise, not an optimization claim.  The lifecycle patch is retained for
semantic completeness, not promoted as an additional performance improvement.

## Infrastructure failures and prevention

- A local CMake cache fixed to Clang 14 later picked up GCC 14 standard-library headers
  and failed in pre-existing `<format>`, ranges, and `source_location` code.  It was not
  used as evidence; final verification used the original GCC 13 Docker environment.
- Running all CTest entries against the server's reduced experiment source copy yielded
  260 passes, one expected skip, and ten invalid integration failures caused by absent
  scripts, variants, and `RelWithDebInfo/include/config.h`.  The standalone C++ unit
  binary is complete and passed 245/245 applicable tests.  The incomplete-source CTest
  result is recorded but is not presented as a code regression or a green integration
  gate.
- Direct host `/usr/bin/time` measures the Docker client rather than the task's peak RSS.
  Only BenchExec XML memory is accepted for resource claims.
- Every broad lane is accepted only after exact XML/log cardinality and manifest checks;
  a zero-task BenchExec exit code is not success.
- Three post-lifecycle launch attempts were rejected before running any task: r2 lacked
  the namespace capability, r3 used an unsupported nested overlay mode, and r4 made the
  workspace read-only.  r5 reuses the repository's established privileged outer Docker
  plus BenchExec `--no-container` configuration.  The validated configuration is now
  captured by `launch-server-finite-rvf-sc-order-full.sh`.

## Next optimization

The remaining 118 candidate timeouts are now dominated by the solver's order theory.
The next isolated candidate should compare the proven bit-vector rank formulation with a
Z3 integer/difference-logic rank formulation on the same panel.  It must retain concrete
witness extraction and all correctness gates, and it may replace this implementation
only if CPU, wall time, and memory improve together.  A later production integration can
use a validated SAT result as an error-witness prepass and fall back to native exploration
for unsupported or unknown cases.

That production-integration hypothesis was subsequently tested in two forms and rejected.
The five-second sequential prepass increased summed RSS about sevenfold without confirming
an error. A later direct-backend panel stopped after SC-RVF classification/replay and still
used 1.569x CPU, 1.569x wall time, and 7.145x summed task peak RSS while completing only
7 tasks versus native's 10. See `deagle-rvf-direct-backend-report-20260720.md`. Therefore
the SC-order encoder remains a diagnostic research result; no production integration is
currently justified.
