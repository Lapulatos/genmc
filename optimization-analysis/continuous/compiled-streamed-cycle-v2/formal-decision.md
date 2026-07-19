# P0.7g2 formal decision

## Decision

Reject P0.7g2 as the generic default and restore the retained `d963c49` production and
test sources. The candidate is exact and memory-neutral under the frozen gates, but its
end-to-end CPU improvement is not statistically established.

## Frozen-gate evidence

- Matrix: 2,304 cells / 1,152 paired cells, four repetitions, 96 tasks, SC/TSO/PSO,
  before/after.
- Terminal verdict, execution, emitted-candidate and lazy-check mismatches: 0/0/0/0.
- Snapshot-equivalent and current-base mismatches: 0/0.
- Coverage gains/losses/new OOM: 0/0/0.
- Four-repetition task-model CPU geometric-mean ratio: 0.999423.
- Task-model bootstrap 95% CI: [0.994756, 1.004137]. The upper bound is not below 1,
  so the predeclared retain gate fails.
- SC/TSO/PSO point ratios: 0.994100 / 1.004601 / 0.999926.
- SC/TSO/PSO bootstrap 95% CIs: [0.989014, 0.999137] / [0.994941, 1.014564] /
  [0.991432, 1.008710].
- RSS cell P90 / P90-of-levels / large-task maximum ratios:
  1.002677 / 0.999844 / 1.003172, all below the frozen 1.02 threshold.

## Interpretation

Removing the avoidable statistics/layout expansion fixed P0.7g's memory regression,
but ordered stream dispatch removal does not deliver a general end-to-end speedup.
SC alone has a CI below 1, while TSO trends in the opposite direction and PSO is neutral.
Selecting the mechanism by model name would violate the Optimization 6.1 generality
boundary, so this result cannot be converted into a retained SC-only default.

## Evidence and restoration

- Raw XML, text summaries, console logs and BenchExec log archives:
  `server-results/formal/` (24 XML + 24 log ZIP files).
- Machine-readable analysis: `server-results/formal/analysis/`.
- Exact rejected source/test patch: `rejected-source.patch`.
- Patch SHA-256: `a3deb5a4cdb78846398fb458c6f0872b21250f955477b384f5130e51389af04a`.

No server configuration is part of the patch. The formal container may be removed only
after the pulled artifact counts and hashes are verified.
