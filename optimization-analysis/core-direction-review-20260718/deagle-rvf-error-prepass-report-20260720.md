# Deagle SC-RVF replay-confirmed error-prepass decision (2026-07-20)

## Decision

Reject the production error-prepass integration and remove its code.  A bounded SC-RVF
solver followed by CAT validation, constrained LLVM replay, and native fallback preserves
verdicts, but adds substantial CPU and memory without confirming an error on the strict
panel.  Increasing the solver budget would worsen the measured tradeoff and is not run.

The successful finite SC-order encoder remains a research/diagnostic result against the
exact concrete-RF/SC control.  This experiment shows that placing it before the current
highly optimized native GenMC verifier is not an end-to-end optimization.

## Conservative integration tested

The opt-in path used the proven value-class SC-order encoder with active-error required.
Only the following chain could return an error before native verification:

1. obtain a concrete SC-order `rf/co` assignment;
2. validate it with the configured CAT model;
3. clone the transformed LLVM module and constrain nondeterministic inputs and observed
   load values;
4. run the ordinary GenMC verifier on that clone;
5. accept only a replay-confirmed safety violation.

UNSAT, unsupported skeletons, Z3 unknown/timeout, CAT rejection, replay-transform errors,
safe replay, and candidate-budget exhaustion all fell back to unchanged native
verification.  A per-check Z3 timeout and a one-candidate default made fallback operational
rather than nominal.  Dedicated unsafe/safe/invalid-option tests passed before the panel:
an unsafe fixture was replay-confirmed, a safe finite UNSAT fell back, and selecting the
backend without the conservative prepass was rejected.

## Budget selection

The frozen 283-task first-model run contains 68 SAT assignments.  Under 48-way server
concurrency, the fastest SAT check took 5.43 seconds; none completed within five seconds.
Among the 29 SAT tasks classified only by SC-RVF while native was unknown, the fastest
check took 27.88 seconds.  Therefore a five-second budget was selected as a bounded-cost
gate.  Budgets of 30--120 seconds would impose large overhead on every admitted task to
reach only a small subset of native-unknown errors and were not justified without a
successful short-budget panel.

## Strict end-to-end panel

Authoritative root:
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-error-prepass-panel-20260720-r1`.
It contains two complete 15-row XML lanes and 30 logs.  Both lanes use the same CAAT-SC
backend, one CPU, 4 GiB, 120 seconds, disabled estimation/MM detection, and native
single-thread exploration.  The candidate adds a five-second per-check SC-RVF prepass
with at most one candidate.

| Metric | Native | SC-RVF prepass + fallback | Candidate/native |
|---|---:|---:|---:|
| summed CPU | 763.323 s | 805.369 s | 1.05508 |
| summed wall | 763.450 s | 805.568 s | 1.05517 |
| summed per-task peak RSS | 399,106,048 B | 2,812,370,944 B | 7.04668 |
| FALSE / TRUE / TIMEOUT | 6 / 4 / 5 | 6 / 4 / 5 | identical |

All ten commonly terminal tasks have the same verdict, but every one regresses in both
CPU and memory.  Common-task CPU/wall geometric-mean ratios are 4.02367/3.99849 and summed
ratios are 1.26547/1.26556.  Common-task RSS geometric-mean and summed ratios are
3.85958/4.74325.

The prepass records 12 `solver-incomplete` fallbacks and three `exhausted-fallback`
results; replay-confirmed errors are zero.  Thus no exploration result improves, while
the finite solver's retained state sharply increases peak memory.

## Infrastructure note

The first rebuild attempt combined restored headers with a stale max-rank oracle file in
the server's mutable experiment source copy.  Compilation correctly rejected the removed
field.  Re-synchronizing the committed baseline oracle fixed the copy and the build/tests
passed.  Future candidate teardown must synchronize every modified source and test file,
not only production headers, before starting the next build.

## Consequence

Do not use SC-RVF as a sequential prepass to native GenMC.  A useful end-to-end integration
would need to replace native exploration work directly or activate only after a reliable
hard-task signal, without retaining both representations.  The next optimization should
return to generation-time quotienting or exploration/history compression rather than add
another solver in front of native verification.
