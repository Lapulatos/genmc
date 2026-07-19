# Deagle value-first RVF census and first-model report (2026-07-20)

## Decision

Continue on `genmc-caat-opt-dev`, but do not promote to stable yet. Conservative
same-value classes materially reduce first-model cost without increasing memory. The
current modes are diagnostic: a selected class is not a concrete reads-from witness and
is rejected by CAT materialization and SC completion until refined to a member.

## Mechanism and claim boundary

The concrete encoding chooses one RF source per active load. `value` first groups sources
with identical constant `(width,value)` or the same SSA value node. `value-provenance`
also requires the source function to agree and separates initial writes. This is
source-lazy RF search, not proof that all members are RVF-equivalent. Exact CAT/SC
validation, replay, and verdicts require class-to-source and ordering refinement. The
default concrete mode is unchanged. Results below establish representation opportunity
and first-model performance, not end-to-end verification speedup or completeness.

## Solver-free representation census

Authoritative run:
`/data3/sujie/experiments/caat-optimization/deagle-rvf-census-20260720-r2`.
It has 283 XML/log rows; 225 programs are admitted.

| Cohort / representation | Selectors | Change | Pair terms | Change |
|---|---:|---:|---:|---:|
| all admitted / concrete | 256,010 | baseline | 1,059,950 | baseline |
| all admitted / value | 189,609 | -25.94% | 436,143 | -58.85% |
| all admitted / provenance | 206,139 | -19.48% | 480,762 | -54.64% |
| baseline-TIMEOUT admitted / concrete | 183,250 | baseline | 824,229 | baseline |
| baseline-TIMEOUT admitted / value | 130,164 | -28.97% | 300,454 | -63.55% |
| baseline-TIMEOUT admitted / provenance | 143,348 | -21.77% | 337,397 | -59.07% |

All 225 admitted tasks, including all 156 admitted baseline-TIMEOUT tasks, have an
opportunity. Census overhead is 20.777 CPU seconds, 23.291 wall seconds, and at most
27,045,888 bytes RSS over 283 rows.

## First-model performance

The strict panel is
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-panel-20260720-r2`:
value changes summed CPU/peak RSS by -13.94%/-1.94%; provenance by -9.50%/-0.48%.

The full run is
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-full-20260720-r1`.
It has exactly 849 XML/log rows and no manifest failures.

| Mode | First models | Baseline-TIMEOUT models | CPU (s) | RSS sum (bytes) | Peak RSS (bytes) |
|---|---:|---:|---:|---:|---:|
| concrete | 223 | 154 | 861.598 | 10,656,956,416 | 70,406,144 |
| value | 225 | 156 | 648.083 | 10,505,744,384 | 69,148,672 |
| provenance | 225 | 156 | 709.524 | 10,548,400,128 | 69,816,320 |

On 223 common models, value CPU geometric-mean/summed ratios are 0.64561/0.74641
with zero CPU-regressed tasks; RSS ratios are 0.98306/0.98344 with one regressed task.
Provenance CPU ratios are 0.74143/0.81938 with two regressions; RSS ratios are
0.98787/0.98817. Solver status and assignment presence agree on common models. Value
also finds models for `mix008_tso.oepc.yml` and `mix023_tso.yml`, which time out in
concrete mode. Time and memory therefore improve together, but refinement cost remains
unmeasured.

## Correctness gates

- Release unit suite: 236 passed, one backend-availability test skipped.
- ASan+UBSan suite: the same result, with no sanitizer diagnostic.
- Post-boundary GCC 13 Docker gate: 18/18 encoder, CAT, and SC tests pass.
- `abstractReadsFrom` is rejected at the sole CAT materialization entrance, so CAT
  evaluation and both SC completion paths cannot treat a representative as concrete RF.

## Invalid/pre-evidence runs and prevention

- Census r1 used a 100-GiB container for 48 tasks capped at 4 GiB; BenchExec started zero
  tasks. Launchers now use 220 GiB and require exact XML/log cardinality.
- Panel r1 omitted `--finite-skeleton-stats-only`, so native continuation polluted CPU.
  It is not evidence; the launcher now supplies stats-only and checks every lane.
- A host rebuild reused a Docker-owned build tree and failed before compilation on
  permissions/toolchain regeneration. Rebuilding inside the original image passed 18/18;
  that tree must not be rebuilt with host CMake.

## Next experiment

Expose the selected class and member list, then refine only it by exact RF source and
required ordering constraints. Validate concrete witnesses through existing CAT/SC and
fail open to concrete enumeration. Promotion requires verdict/outcome/error equivalence,
witness replay, refinement coverage, and a broad paired CPU/RSS gate.
