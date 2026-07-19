# P0.7f compiled epsilon-closure decision

Decision: **reject as a general default optimization; do not commit**.

## Exactness and coverage

The balanced formal matrix contains 2,304 cells: 1,152 before/after pairs over four
repetitions. Both variants report exactly 716 TRUE, 360 FALSE and 76 TIMEOUT cells.
There are zero status changes, opposite terminal verdicts, complete-execution-count
mismatches, coverage losses or new OOM results.

The preceding gates also pass: Release 163/163, focused GCC 13 ASan+UBSan 10/10,
mutation oracle 39 rows / 5,441 comparisons, and broad differential 852 matches +
12 mutually unsupported + 0 mismatch over 864 SC/TSO/PSO pairs.

## Frozen performance result

| Metric | SC | TSO | PSO | All |
|---|---:|---:|---:|---:|
| Common-terminal CPU | 1.00556 | 0.98786 | 1.00561 | 0.99962 |
| CPU task-bootstrap 95% CI | [0.99851, 1.01280] | [0.97699, 0.99836] | [0.99579, 1.01601] | [0.99407, 1.00521] |
| Common-terminal RSS | 0.99979 | 0.99994 | 0.99926 | 0.99968 |

The pre-registered retain rule requires the aggregate CPU confidence-interval upper
bound below 1.0. The observed upper bound is 1.00521, so P0.7f fails even though its
point estimate is 0.99962. TSO has a statistically supported roughly 1.21% reduction,
but SC and PSO each regress by roughly 0.56%. Enabling the implementation only for a
named model would turn this into the model-specific specialization rejected under the
Optimization 6.1 applicability decision; that is not a general optimization.

Snapshot-equivalent state is exactly unchanged at 1.0. Large-task process RSS is
0.99603 overall and at most 1.00175 by model, so the space gates pass. P0.7f is not a
time-for-space trade: it changes only immutable shared program metadata and bounded
query-local product control state.

## Mechanism

Against measured P0.7e3, the fused representation keeps exactly the same 3,517,116
product checks and 144 size fallbacks, while:

- product-state visits fall 5,223,464,456 -> 904,134,964 (-82.69%);
- product transitions fall 18,831,879,352 -> 14,296,911,636 (-24.08%);
- root macro candidates remain 2,349,253,524;
- PSO transitions fall 2,477,566,236 -> 718,503,096 (-71.00%);
- TSO transitions fall 16,354,313,116 -> 13,578,408,540 (-16.97%).

The implementation therefore succeeds at its local target: epsilon/control-state
interpretation is substantially reduced. It does not establish a general end-to-end
speedup because the remaining relation-transition and product-DFS work is still large,
and the saved control visits are cheap relative to relation enumeration on many PSO
queries. The candidate executes 3.52 million checks with a mean 19.25 compiled states
and 31.25 actions per check; no query state survives rollback.

## Next general direction

Do not tune a model-name or benchmark-name selector. The next experiment should return
to the retained baseline and target one of these model-independent boundaries:

1. compile the existing streamed macro-successor evaluator into direct bytecode while
   preserving its event-level DFS, avoiding the product graph entirely; or
2. pre-register a generic, observable amortization rule based on repeated suffix work,
   with exact streamed fallback and separate correctness/performance gates.

Option 1 is the cleaner next test because it removes interpreter dispatch without
changing which event-level candidates are explored. A dynamic selector should not be
implemented until its cost signal predicts both TSO gains and PSO losses independently
of model identity.

Raw XML/log archives are under `server-results/formal/`; strict outputs are
`server-results/analysis/comparison.json` and `product-analysis.json`. The exact
rejected source diff is archived as `rejected-source.patch`.
