# Optimization 12 / P0.7a decision: retain cycle-only closure slicing

## Decision

Retain and push the candidate. All correctness gates pass, the aggregate CPU bound
passes, and predeclared benefit Gate B passes: five SC tasks change from TIMEOUT to a
correct false result in all four repetitions. The candidate does not pass Gate A's 10%
large-state/RSS reduction threshold, so it is retained for repeatable coverage and
closure-evaluation savings, not for a claimed 10% process-memory reduction.

## What changes

During normalization, a non-reflexive transitive closure `P = R+` is removed only when
no predicate consumes `P` and all of its check consumers are `acyclic` or
`irreflexive`. Those checks become `acyclic R`. For finite execution graphs this is an
exact equivalence. `empty`, downstream use, reflexive closure, mixed consumers and
unused closure all fail closed and retain the original predicate.

The bundled recursive SC/TSO/PSO models therefore check `order` directly and no longer
construct/store/checkpoint the dense `reach = order+` value. CAT/CAAT remains the
verdict authority; no built-in GenMC checker is used for consistency decisions.

## Correctness evidence

- Server Release tests: baseline 150/150; candidate 152/152.
- New independent oracle: random finite relations are checked by a separate DFS for
  both `acyclic(R+)` and `irreflexive(R+)` rewrites.
- Near neighbours retain closure for `empty`, predicate use, reflexive closure, mixed
  check use and no closure check.
- Exact transformed SC/TSO/PSO semantic fingerprints pass; mutated model/check-kind
  certificate negatives remain fail-closed.
- Mutation suite: 39 rows, 5,441 full-offline oracle checks, zero mismatch.
- Broad differential: 288 programs, 864 SC/TSO/PSO pairs, 852 matches, 12 mutual
  unsupported and zero mismatch.
- Formal matrix: 2,304 cells, 24/24 runsets exit zero, zero opposite common-terminal
  verdict, zero common-terminal execution-count mismatch and no new OOM.

The XML did not retain the configured custom execution column with this BenchExec tool
adapter. The strict analyzer therefore reads `Number of complete executions explored`
from all compressed logs and compares every pair whose before/after categories are both
correct; the mismatch count is zero.

## Formal performance result

The balanced design is 4 repetitions × 96 tasks × 3 models × 2 variants, with 48
BenchExec workers, 1 core/4 GB/60 s per cell and odd/even core-range swaps.

- Aggregate common-terminal CPU after/before: **0.99575**, task-bootstrap 95% CI
  **[0.98138, 1.00687]**. The frozen upper bound of 1.02 passes.
- Aggregate common-terminal RSS: **0.99909** [0.99824, 0.99975].
- Large completed-task offline evaluation time: **0.48197**
  [0.24931, 0.82600]. By model: SC 0.22362, TSO 0.85983, PSO 0.70350.
- Large peak snapshot-equivalent state: **0.95338**
  [0.93190, 0.97527]. Large process RSS: **0.95984**
  [0.94200, 0.97495]. These reductions are real but below Gate A's 10% threshold.
- Primitive base storage is exactly **1.0**, as expected: this optimization changes a
  derived closure, not graph materialization.
- Status totals change from 712 true / 338 false / 60 TIMEOUT / 2 partial-false
  TIMEOUT / 40 OOM to 712 true / 360 false / 40 TIMEOUT / 40 OOM.
- Five SC `28-race_reach_*` tasks move TIMEOUT → correct false in **4/4** repetitions
  (20 cells). PSO `fib_unsafe-5` additionally terminates correctly in 2/4 repetitions.

The large-task materialization counter is noisy and rises to 1.16898 overall even
though its code and base-byte count are unchanged; the wide interval is
[0.96754, 1.53863]. No materialization speedup is claimed. TSO's common-terminal CPU
is 1.01364 [1.00205, 1.02493], a small model-specific regression hidden by the aggregate
gain. This is the main retained limitation and motivates replacing repeated generic
acyclic scans with an exact sparse EOG/topological state in P0.7b.

## Gate audit

- Correctness gates: **pass**.
- Aggregate CPU upper CI ≤ 1.02: **pass** (1.00687).
- No new OOM: **pass** (40 before, 40 after).
- Gate A, ≥10% snapshot and RSS reduction: **fail**.
- Gate B, repeatable resource failure → correct terminal result: **pass** (five SC
  tasks, every repetition).
- Required normalized closure removal: **pass**, covered by exact model summaries and
  focused tests.

## Evidence paths

- Protocol: `experiment-protocol.md`
- Core analysis: `analysis/comparison.json`, `analysis/rows.tsv`
- Derived-state/execution audit: `analysis/derived.json`,
  `analysis/derived-rows.tsv`
- Full formal XML/logs: `server-results/formal/`
- Pilot XML/logs: `server-results/pilot/`
- Unit/mutation/broad logs and 864-row TSV: `server-results/`

Server launchers, definitions, source/build trees and paths remain outside Git. A Git
commit may contain only `Normalized.cpp` and `CatEvaluatorTest.cpp`.

## Post-push preventive-pruning compatibility correction

The P0.7b audit found that opt-in recursive-PSO preventive pruning was an extra-model
consumer of the old `reach` predicate. P0.7a's normalizer correctly proved that the CAT
checks did not consume the complete closure, but the checker constructor still looked
up `reach` by name. Default formal runs did not enable the option, so the 2,304-cell
result above is unaffected; `--cat-preventive-pruning` itself would have asserted.

The correction retains the sliced default path and, only when the exact PSO certificate
and preventive option are active, reconstructs `order+` from the published exact
`order` value. This is semantically the same finite closure previously published by the
model. A checker-construction regression test now covers the option.

Correction verification:

- server Release tests 152/152;
- real PSO preventive smoke: 4 prefix queries, 3/10 RF candidates pruned, 4 complete
  executions and a safe verdict;
- mutation 39 rows / 5,441 oracle checks, zero mismatch;
- broad differential 852 matches / 12 mutual unsupported / zero mismatch;
- three paired repetitions on RMWFix and fcombiner preserve exit/verdict, execution
  count and every preventive candidate/pruned counter relative to pre-slice `reach`.

The correction adds `CATChecker.cpp`, `CATChecker.hpp` and `ConfigTest.cpp` to the
production/test-only follow-up commit. It does not change any default-path formal result
or relax the model certificate.
