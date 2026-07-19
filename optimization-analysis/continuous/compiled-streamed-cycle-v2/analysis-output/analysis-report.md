# P0.7g2 strict analysis report

## Analysis question

Does replacing the retained generic lazy-root stream with a minimal immutable ordered
stream plan reduce end-to-end CPU without changing GenMC+CAAT semantics, coverage, or
memory under SC, TSO and PSO?

## Result

No general speedup is established. Across 269 task-model clusters with four terminal
repetitions, the candidate/baseline CPU geometric-mean ratio is 0.999423 (95% clustered
bootstrap CI [0.994756, 1.004137]). The point estimate is a 0.0577% improvement, but the
interval includes both improvement and regression, so the frozen upper-CI-below-1 gate
rejects the candidate.

The implementation is semantically exact on the measured matrix: 2,304 cells / 1,152
pairs produce zero terminal-verdict, execution-count, candidate-count, lazy-check,
snapshot or base-state mismatch. Both variants have 716 TRUE, 360 FALSE and 76 TIMEOUT
cells. There are zero coverage gains, zero coverage losses and zero new OOMs.

Memory is not the reason for rejection. The paired RSS cell P90 ratio is 1.002677,
P90-of-levels is 0.999844, and the large-task maximum is 1.003172; all satisfy the
predeclared 1.02 limits. This confirms that removing P0.7g's statistics/layout expansion
eliminated its observed memory regression.

## Model breakdown

| Model | Task-model clusters | CPU ratio | Bootstrap 95% CI | Decision signal |
|---|---:|---:|---:|---|
| SC | 95 | 0.994100 | [0.989014, 0.999137] | positive in this matrix |
| TSO | 90 | 1.004601 | [0.994941, 1.014564] | uncertain, point regression |
| PSO | 84 | 0.999926 | [0.991432, 1.008710] | neutral/uncertain |
| Overall | 269 | 0.999423 | [0.994756, 1.004137] | reject generic default |

The heterogeneous direction matters: retaining an SC-only path selected by model name
would be Optimization 6.1-style specialization, which this experiment explicitly
disallows. The generic mechanism must stand on its overall result.

## Claim candidates

- Claim: P0.7g2 preserves measured semantics and state while removing P0.7g's memory
  regression.
  - Source evidence: 1,152 exact pairs; zero mismatch/loss/new OOM; all frozen RSS gates
    below 1.02.
  - Allowed wording: "P0.7g2 was exact and memory-neutral on the formal matrix."
  - Forbidden stronger wording: "P0.7g2 is proven equivalent for every CAT model."
  - Uncertainty: test coverage cannot replace a general proof for unsupported future CAT
    forms.
  - Next check: none; rejection is already determined by CPU.
  - Decision: keep.

- Claim: ordered stream compilation is not a worthwhile generic default in its current
  form.
  - Source evidence: overall CPU ratio 0.999423, CI upper 1.004137.
  - Allowed wording: "The formal matrix did not establish a general CPU improvement."
  - Forbidden stronger wording: "Compiled streams always slow GenMC down."
  - Uncertainty: SC improved, while TSO/PSO did not establish improvement.
  - Next check: move to a mechanism that removes relation/product work rather than more
    dispatch overhead.
  - Decision: keep.

## Decision

Reject and restore the retained baseline. Do not tune thresholds or add model-name
selection after observing this result. The next experiment should target graph/exploration
work that dominates end-to-end time, not the already-small stream-dispatch component.
