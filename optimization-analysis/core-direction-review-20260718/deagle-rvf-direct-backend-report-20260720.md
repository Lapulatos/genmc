# Deagle SC-RVF direct-backend decision (2026-07-20)

## Decision

Reject and remove the diagnostic direct-backend code. Even after eliminating the
previous sequential-prepass duplication, the exact finite SC-RVF solver consumes more
CPU, wall time, and peak memory than native GenMC and completes fewer panel tasks. Do not
expand this candidate to 283 tasks or merge it into the stable branch.

The exact value-class SC-order encoder remains useful as a research comparison against
concrete RF/SC encoding. This experiment falsifies the stronger claim that the current
SMT representation can efficiently replace production native exploration.

## Protocol

The direct lane builds the strict finite skeleton, requires an active error, encodes
value-class RF with exact SC order, evaluates each returned concrete graph with the CAT
model, and replay-confirms SAT errors. Solver UNSAT is recorded as diagnostic safe. The
process then stops, so its resources do not include a subsequent native fallback. The
native lane runs the ordinary CAAT-SC verifier.

Both lanes use the same fixed 15 tasks, one task CPU, 120 seconds, and 4 GB. The formal
server Docker root is
`/data3/sujie/experiments/caat-optimization/finite-rvf-direct-panel-20260720-r1`.
It contains two 15-row XML files and one archive with 30 logs. This is one deterministic
paired panel, so comparisons are descriptive; no p-value or confidence interval is
claimed.

## Correctness and completion

| Classification | Native | Direct SC-RVF |
|---|---:|---:|
| Reachable error | 6 | 4 |
| Safe | 4 | 3 |
| Timeout/unknown | 5 | 8 |

Seven tasks terminate in both lanes with zero verdict differences. The direct lane has
no unique terminal. All four direct errors are CAT-clean and replay-confirmed; fail-open
count is zero. Before the panel, 248/249 applicable server unit tests passed with the
remaining no-backend test skipped because Z3 is available. The finite safe/unsafe and
four two-create/two-join outcome oracles also passed.

## Resource result

| Metric | Native | Direct SC-RVF | Direct/native |
|---|---:|---:|---:|
| All-task CPU | 760.703 s | 1,193.340 s | 1.56873 |
| All-task wall | 760.954 s | 1,193.590 s | 1.56854 |
| Summed task peak RSS | 399,360,000 B | 2,853,576,704 B | 7.14537 |
| Common-terminal CPU | 73.791 s | 225.285 s | 3.05301 |
| Common-terminal wall | 73.829 s | 225.397 s | 3.05298 |
| Common-terminal peak RSS | 184,459,264 B | 660,082,688 B | 3.57847 |

The candidate violates both resource gates by large margins. One safe task improves from
43.34 seconds to 0.62 seconds, but other regressions remove three terminals and dominate
every aggregate. Selective activation cannot currently be proved without using a
task-specific oracle or first paying the solver-construction cost.

## Infrastructure record

- A parenthesized CTest regex was expanded by local zsh; Docker never started.
- The reduced server source copy omitted two existing test fixtures. After synchronizing
  them, both new CTests passed.
- The first ad-hoc parser raised `TypeError` after reading the complete artifacts. The
  corrected immutable-archive rerun produced the table above.

## Consequence

Do not invest in a production SC-RVF short-circuit, longer solver budgets, or a 283-task
direct run with this representation. Future Deagle work would need a fundamentally more
compact theory/solver representation. The diagnostic code and CTest are removed; the
definition, validated launcher, research question, and this negative report remain on
the development branch.
