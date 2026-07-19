# P0.7f balanced pilot decision

Decision: **advance to the frozen four-repetition formal matrix; do not retain yet**.

## Correctness and coverage

- 12 paired cells completed across two repetitions, SC/TSO/PSO and two tasks.
- Status mismatches: 0; result-category mismatches: 0.
- Both PSO `fib_unsafe-7` cells remain the same 60-second unknown in before and after;
  there is no pilot coverage loss or unsupported-result change.

## Frozen advancement gates

| Model/task | CPU median after/before | Maximum RSS after/before |
|---|---:|---:|
| SC `fib_unsafe-7` | 0.993104 | 1.000158 |
| SC `queue_ok_longer` | 1.039699 | 0.998660 |
| TSO `fib_unsafe-7` | 0.842033 | 1.000471 |
| TSO `queue_ok_longer` | 0.989924 | 1.001994 |
| PSO `fib_unsafe-7` | 0.999995 | 1.000471 |
| PSO `queue_ok_longer` | 0.992588 | 1.017279 |

Every representative CPU median is at most 1.05 and every observed RSS ratio is at
most 1.02. Relative to the measured P0.7e3 interpreter, PSO `queue_ok_longer` reduces
product transitions from 143 to 28 (80.42%) and product-state visits from 137 to 25
(81.75%). TSO `fib_unsafe-7` reduces transitions from 1,776,716,915 to 1,593,488,234
(10.31%) and state visits from 215,828,825 to 42,966,528 (80.09%). Thus at least one
activated representative exceeds the frozen 30% transition-reduction gate, and the
large TSO case confirms that epsilon fusion primarily removes control-state visits.

The candidate performs 17,786 TSO product checks with 355,720 summed program states
and 569,152 summed program actions; these immutable program totals are recorded once
per check and do not add per-transition subtype dispatch.

## Boundary

This pilot only authorizes the 2,304-cell formal experiment. Retention still requires
zero verdict/execution mismatch and coverage loss, no new OOM, task-clustered CPU
geometric-mean 95% CI upper bound below 1.0, and RSS/snapshot ratios at most 1.02.

Raw BenchExec XML and log archives are under `server-results/pilot/`.
