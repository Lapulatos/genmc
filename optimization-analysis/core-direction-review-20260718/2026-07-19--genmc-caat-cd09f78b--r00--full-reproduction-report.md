# GenMC CAAT `cd09f78b` full-result reproduction

## Decision

The independent full 725-task paired rerun reproduces the retained CAAT result closely enough to
accept it.  The complete status/category matrices are identical to the historical run in both the
baseline and candidate lanes.  The rerun preserves all eight newly solved tasks, the same twenty
TIMEOUT-to-OOM resource transitions, zero resolved-case regression, and zero terminal-verdict
mismatch.

The predeclared closeness gate passes: common-terminal CPU improves by 27.44% (required at least
20%), all-task CPU and suite wall remain below baseline, and coverage/resource counts match the
historical result exactly.

## Reproduction boundary

- Retained commit: `cd09f78ba32ca3f4aa8e889506a2b57f3f6544bc`.
- Source: isolated server tree `genmc-cd09f78b`, populated from a clean detached worktree and
  verified by SHA-256 for 6,210 relative-path files.
- Toolchain: GCC 13.3.0, LLVM 15.0.7, Release, `genmc15noble:sujie` Docker image.
- Binary SHA-256: `25484549b39a525ffecc4340575fca537ad876e874e1b61fbf57ee9d89cb435a`.
- Workload definition SHA-256:
  `b2c65c6b5982f9ff06d05894ec05449cbce7ca789a376ef372ac37a1bbf2137d`.
- Workload set SHA-256:
  `9445fd446074c038a96b03e1c9c878db8b2342f9558bce73dc731e693913e468`.
- Per task limits: 60 CPU seconds, 4 GB, one core; 24 concurrent tasks per lane on disjoint cores.
- Run interval: 2026-07-19 04:59:40--05:11:37 UTC.

The candidate enables `--cat-primitive-cache`, `--cat-fast-primitive-build`,
`--cat-fast-checks`, `--cat-fast-composition`, and `--cat-fast-cycle-checks`.  The baseline uses
the same binary and workload without those switches.

## Correctness gates rerun before performance

| Gate | Result |
|---|---:|
| Release unit/property tests | 158/158 passed |
| Online mutation oracle | 39 rows; 5,441 checks; zero mismatch |
| Recursive broad differential | 852 match; 12 mutually unsupported; zero mismatch; 864 total |

## Full 725 result

| Metric | Baseline | Candidate | Change |
|---|---:|---:|---:|
| Correct terminals | 394 | 402 | +8 |
| TIMEOUT | 244 | 216 | -28 |
| OOM | 31 | 51 | +20 |
| TIMEOUT + OOM | 275 | 267 | -8 |
| Common-terminal CPU, 450 tasks | 1,015.699 s | 736.993 s | -27.44% |
| All-task CPU | 16,340.049 s | 15,615.592 s | -4.43% |
| Suite wall | 716.699 s | 689.878 s | -3.74% |
| Aggregate peak RSS | 251,752,890,368 B | 253,957,443,584 B | +0.88% |
| Maximum per-task RSS | 3,999,997,952 B | 3,999,997,952 B | unchanged |

All eight new correct terminals are pthread-wmm tasks that change from TIMEOUT to
`false(unreach-call)`.  All twenty new OOM rows are Goblint `28-race_reach_*` tasks that change
from TIMEOUT; they remain unresolved and are not counted as solved improvements.  No previously
non-resource row becomes TIMEOUT/OOM, and no terminal verdict differs.

## Historical comparison

| Metric | Historical claim | Independent rerun | Difference |
|---|---:|---:|---:|
| Correct terminals | 394 -> 402 | 394 -> 402 | exact |
| Hard failures | 275 -> 267 | 275 -> 267 | exact |
| Status transitions | 8 solved; 20 TIMEOUT->OOM | same task identities | exact |
| Common-terminal CPU change | -27.87% | -27.44% | +0.43 percentage points |
| All-task CPU change | -4.49% | -4.43% | +0.05 percentage points |
| Suite-wall change | -3.78% | -3.74% | +0.04 percentage points |
| Aggregate RSS change | +0.92% | +0.88% | -0.05 percentage points |
| Maximum per-task RSS | 3,999,997,952 B | 3,999,997,952 B | exact |

Baseline CPU is 1.70% lower and candidate CPU is 1.11% lower than in the historical run on the
450 common-terminal tasks.  Because both lanes moved in the same direction and the paired effect
differs by only 0.43 percentage points, this is consistent with ordinary run-to-run timing
variation under simultaneous capped execution rather than a change in optimization behavior.

## Evidence index

Raw artifacts are retained locally under
`server-results/primitive-fast-compose-cycle-paired-725-cd09f78b-rerun-20260719a/` and on the
server under the matching `formal-results/` directory.  The directory contains both compressed
BenchExec XML files, logs, SHA-256 inputs, a successful manifest, and `complete.txt`.

The generated table has 725 rows and two run sets.  Its difference table contains the exact 28
status changes described above:

- `html/primitive-fast-compose-cycle-cd09f78b-rerun-725.table.html`
- `html/primitive-fast-compose-cycle-cd09f78b-rerun-725.diff.html`

## Statistical scope

This is one paired deterministic benchmark rerun per configuration, not an independent-seed
sample.  Exact status matrices and aggregate accounting support a reproducibility claim about
direction and observed magnitude.  They do not justify population-level confidence intervals or
significance claims.
