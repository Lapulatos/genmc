# Optimization 04: measured online/offline cost selector

## Decision

Reject and remove the selector. A sequential full-corpus run initially suggested an
8.8% wall-time improvement, but the controlled simultaneous held-out experiment showed
a 6.3% wall regression and 3.5% CPU regression. The initial signal was server-load/order
bias, not a reproducible algorithmic gain.

## Prototype

For exact certified SC/TSO/PSO models and at most 512 stable events, the selector used
power-of-two event buckets, began with the retained static offline policy, sampled online
on the 16th changed query, compared EWMA cost, and periodically refreshed the losing arm.
An experiment-only switch selected the old static policy in the same binary. Selection
changed only the exact evaluator path, never candidate generation or CAT acceptance.

## Correctness evidence

- Server Docker prototype suite: 142/142.
- 288-run oracle matrix: 237 completed records and 39,879 full Phase 2 comparisons,
  with zero mismatch.
- Full 1,728-cell and held-out 1,440-cell comparisons: zero common-solved verdict or
  safe-task execution-count mismatch.
- After removing the selector and CLI switch: server Docker tests 141/141.

## Training/profile evidence

| Model | Offline | Online | Online probes | Profiled queries |
|---|---:|---:|---:|---:|
| SC | 3,415 | 56 | 56 | 5,207 |
| TSO | 3,415 | 56 | 56 | 5,207 |
| PSO | 430 | 62 | 8 | 511 |

SC/TSO never promoted online beyond probes. PSO did, agreeing with the first full run's
PSO CPU regression.

## Performance evidence

The sequential 96-task run reported overall wall ratio 0.9124, but placed every baseline
batch before every selector batch during changing shared-server load. The controlled
held-out run contradicted it; it is retained only as evidence of order bias.

The authoritative held-out half contained 48 tasks not used by the profile. Each model
had five repetitions. Static and selector ran simultaneously as separate 24-worker
queues pinned to `0-23` and `28-51`, with CPU sets swapped on even repetitions. Configured
task parallelism was 48; XML measured peak overlap 45--48 in every pair.

Ratios are selector/static geometric means of each task's five-run median.

| Metric | SC | TSO | PSO | Overall |
|---|---:|---:|---:|---:|
| Wall ratio | 1.1208 | 1.0445 | 1.0272 | 1.0634 |
| Wall 95% CI | [1.0715, 1.1822] | [0.9832, 1.1158] | [0.9691, 1.0882] | [1.0293, 1.1001] |
| CPU ratio | 1.0235 | 1.0407 | 1.0419 | 1.0353 |
| CPU 95% CI | [1.0130, 1.0346] | [1.0274, 1.0546] | [1.0261, 1.0587] | [1.0274, 1.0435] |
| RSS ratio | 0.9602 | 0.9989 | 0.9998 | 0.9861 |

Correct cells changed from 566/720 to 565/720 because one TSO safe cell crossed the
timeout boundary. The RSS reduction does not offset slower execution and lower completion.

## Operational errors retained

- The first profile used stale `/work/sv-benchmarks` paths and executed zero tasks.
- The first dual queue used `24-47`, which crossed NUMA sockets asymmetrically; one queue
  was rejected. Valid queues used `0-23` and `28-51`.
- The original sequential held-out attempt was stopped and quarantined when simultaneous
  dual queues were adopted.

## Artifacts

- Full raw XML/logs: `server/wide-before/`, `server/wide-after/`
- Held-out XML/logs: `server/heldout/`
- Oracle/profile logs: `server/oracle/`, `server/profile24/`
- Parsed rows/bootstrap/concurrency: `analysis/`
- HTML: `html/cost-selector-heldout.table.html`

The worktree retains successful static adaptive-offline Optimization 01 and no measured
selector state or experimental CLI flag.
