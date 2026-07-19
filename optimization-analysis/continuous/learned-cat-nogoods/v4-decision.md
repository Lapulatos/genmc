# P0-A V4 decision: reject bounded-work warm-up admission

## Decision

Reject V4 as a general/default optimization. Do not commit or enable it by default.
The implementation remains an uncommitted audit prototype while this optimization
round is paused.

## Correctness and completeness evidence

- Release focused tests: 11/11 passed.
- Full Release unit/property tests: 166/166 passed.
- Focused GCC 13 ASan+UBSan tests: 11/11 passed.
- Mutation oracle: 39 rows and 5,441 exact full-recomputation comparisons passed.
- Broad differential: 852 exact matches, 12 mutually unsupported pairs, zero mismatch.
- Formal matrix: zero terminal-verdict mismatch and zero complete-execution mismatch.
- Formal coverage: four gains and zero losses; every gain is PSO `fib_safe-5`
  changing from baseline TIMEOUT to correct TRUE.

## Mechanism preflight

PSO `twostage_3` completed after 74 snapshot match queries. V4 recorded 54 warm-up
skips and zero Reasoner calls, provenance materializations, learned clauses, literal
checks, or hits. This confirms that small explorations no longer pay proof-generation
or retained-clause space.

## Pilot

The frozen 23-cell hit-rich pilot advanced:

- aggregate common-terminal CPU geometric-mean ratio: 0.99483;
- SC / TSO / PSO: 1.01067 / 1.02872 / 0.93638;
- one coverage gain, zero losses;
- no task above one baseline CPU second exceeded the 1.20 regression guard.

The pilot is an advancement check only, not retention evidence.

## Formal matrix

The balanced four-repetition matrix contains 2,304 cells and 1,152 pairs:

- common-terminal pairs: 1,076;
- task-model median CPU geometric mean: 1.01162;
- task-clustered bootstrap 95% CI: [1.00521, 1.01774];
- SC / TSO / PSO task-model ratios: 1.00719 / 1.02252 / 1.00504;
- cell-level CPU / wall geometric means: 1.00995 / 1.00649;
- candidate/baseline memory P90: 1.00219;
- memory-ratio geometric mean / P90: 1.01100 / 1.00331.

The frozen primary CPU gate requires a ratio no greater than 1.01. V4 fails it, and
the confidence interval lies entirely above 1.0.

## Why the generic query threshold fails

Compared with V3, V4 reduces Reasoner calls from 624 to 228 (-63.5%), provenance
materializations from 556 to 224 (-59.7%), literal checks from 78.751M to 42.246M
(-46.4%), and hits from 6.144M to 2.498M (-59.3%). However, total Reasoner time rises
from 19.506 s to 23.754 s (+21.8%). A fixed query count delays explanations until
execution graphs are larger; each admitted explanation can therefore be substantially
more expensive while early reusable conflicts are lost. V4 also retains only four of
V3's six formal coverage gains.

The result demonstrates that model independence is necessary but not sufficient for a
general optimization. Admission must be based on a bounded estimate of the work that
will actually be performed and the reusable benefit already observed, not a global
query-count constant.

## Evidence paths

- Protocol: `v4-protocol.md`
- Short preflight: `short-preflight-v4/`
- Correctness: `correctness-v4/`
- Pilot XML/logs: `pilot-v4/`
- Formal XML/logs and analysis: `formal-v4/`
- Machine-readable summary: `formal-v4/formal-summary.json`
