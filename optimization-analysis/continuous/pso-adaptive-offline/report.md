# Optimization 01: PSO-certified adaptive offline backend

## Decision

**Keep.** The change separates candidate-pruning certification from backend-selection
certification and allows the exact bundled recursive PSO model to select the existing
from-scratch evaluator for graphs with at most 512 stable events. It deliberately does
not enable TSO candidate pruning for PSO.

## Correctness and completeness boundary

- `certifiedCandidateProfile()` remains SC/TSO-only. PSO uses the TSO causal-view host,
  but TSO's stronger candidate-pruning theorem is not assumed for PSO.
- `certifiedAdaptiveOffline()` recognizes the complete normalized PSO fingerprint.
  Any model name, relation, operand, recursive equation, or check change fails closed.
- Oracle mode disables adaptive selection and continues comparing incremental states
  against fresh offline evaluation.
- The selected online and offline evaluators implement the same normalized fixed point;
  the selector can change cost, not the intended accepted execution set.

## Changed source files

- `genmc/genmc/CAT/Normalized.hpp`
- `genmc/genmc/CAT/Normalized.cpp`
- `genmc/genmc/Execution/Consistency/CATChecker.cpp`
- `tests/unit/CatEvaluatorTest.cpp`

No server address, credential, mount path, Docker command, or server configuration was
added to tracked source files.

## Local verification

| Gate | Result |
|---|---:|
| Release build | passed |
| Unit/property tests | 141/141 passed |
| Recursive focused differential | passed |
| Broad repository differential | 864/864 pairs matched |
| Broad programs | 288 programs × SC/TSO/PSO |
| PSO offline evaluations exercised | 460,937 |
| Status/signature/exploration mismatches | 0 |

Broad differential output:
`local/recursive-broad.tsv`.

### Local timing probe

Five programs, nine repetitions, single GenMC worker, previous Stage 11c binary versus
the modified binary:

| Metric (after/before) | Result |
|---|---:|
| Wall-time geometric ratio | 0.859 |
| Task-bootstrap 95% CI | [0.761, 0.970] |
| CPU-time ratio over run cells | 0.867 |
| Peak-RSS ratio over run cells | 1.000 |

The local timer has 0.01-second resolution, so the server matrix is the authoritative
performance result.

## Server Docker experiment

### Valid matrix

- Image: the existing LLVM15-compatible GenMC image with the user's `sujie` tag.
- Isolation: one owned Docker container per build/BenchExec phase; all experiment
  containers were removed after completion.
- CPU parallelism: 16 BenchExec task workers; each GenMC process used `--nthreads=1`.
- Limits: 60 seconds, 4 GiB, one CPU core per run.
- Dataset: 96 selected SV-COMP 2026 C.Concurrency tasks.
- Repetitions: five; 480 before and 480 after runs.
- Exact raw XML and log ZIPs were pulled back under `server/before/` and `server/after/`.

### Server results

The statistical unit is one task. Each task's wall/CPU/RSS value is the median of its
commonly solved repetitions, followed by a geometric mean across tasks. The confidence
interval resamples tasks 20,000 times with seed 20260715.

| Metric | Before | After | After/before |
|---|---:|---:|---:|
| Correctly solved run cells | 395/480 | 400/480 | +5 |
| Wrong verdicts | 0 | 0 | unchanged |
| Common solved run cells | 395 | 395 | — |
| Common-task wall time | — | — | **0.937** |
| Task-bootstrap wall 95% CI | — | — | **[0.901, 0.968]** |
| Common-task CPU time | — | — | **0.938** |
| Common-task peak RSS | — | — | **1.000** |
| Tasks faster by >2% / within ±2% / slower by >2% | — | — | 40 / 25 / 14 |

There were no verdict differences and no complete-exploration-count differences on
common solved safe runs. Five previously timed-out unsafe run cells completed with
`false(unreach-call)` after the optimization. Two safe runs remained timeouts but emitted
a true result marker (`TIMEOUT (true)`); they are not counted as solved.

### Largest observed changes

Representative median wall ratios:

- `13-privatized_27-multiple-protecting2_true.yml`: 0.418
- `13-privatized_24-multiple-protecting_true.yml`: 0.492
- `pthread/queue.yml`: 0.579
- `pthread/reorder_2.yml`: 0.630
- largest regression, `pthread-ext/14_spin2003.yml`: 1.116

The aggregate gain is therefore not universal. The exact-model certificate and the
fallback path remain necessary.

## Invalid environment probe

An initial server probe was rejected: running the old binary without reproducing its
compile-time absolute runtime-header path caused 460/480 runs to report unsupported
`pthread_create`. Those outputs are retained only under the remote `results/before/`
diagnostic directory and are excluded from every performance/correctness statistic.
The valid matrix mounts the source at the binary's recorded runtime include path; it
restores the expected 395/480 baseline solved cells.

## Artifacts

- Parsed rows: `server/before.tsv`, `server/after.tsv`
- Parse summaries: `server/before-summary.json`, `server/after-summary.json`
- Raw XML/logs: `server/before/`, `server/after/`
- Table-generator output: `server/html-before/`, `server/html-after/`
- Local timing rows: `local/before.tsv`, `local/after.tsv`, `local/stats.tsv`
- Broad differential: `local/recursive-broad.tsv`

## Next optimization

Proceed to an `ExecutionGraph` mutation journal prototype. First add instrumentation and
a legacy-adapter differential mode; do not remove full materialization until append,
rf replacement, co move, rollback/cut, and non-LIFO revisit all match query by query.
