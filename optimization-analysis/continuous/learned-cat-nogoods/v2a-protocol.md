# P0-A V2a: bounded conflict explanation

## Change

Keep at most 16 `Reasoner`-backed learning attempts per worker. Once the budget is
exhausted, continue normal CAT/CAAT evaluation and matching of already learned clauses,
but do not reconstruct another explanation. Database capacity and matcher behavior stay
unchanged so this experiment isolates explanation cost.

Sixteen is a fixed power-of-two implementation budget, not a benchmark/model selector.
It applies identically to every online-admissible CAT model and program. A counter must
report inconsistent queries whose optional explanation was skipped.

## Correctness argument

Skipping an explanation only declines to add a performance hint. It does not reject a
candidate, change an existing clause, alter evaluator state, or call a built-in memory
model checker. Existing matched clauses retain the V1 positive-explanation proof.

## Expected effects

- Time: cap full `Reasoner` reconstructions at 16, targeting the observed 54--176
  attempts in `circular_buffer_bad`, `szymanski`, and `queue`.
- Space: at most 16 newly learned clauses per worker in V2a, below the existing 4,096
  database bound.
- Possible loss: later conflicts may not be learned, increasing evaluator calls versus
  V1. Verdict and exploration completeness are unchanged.
- Unchanged cost: clause matching remains linear; TSO `butterfly` may still regress and
  motivates the separately gated V2b.

## Gates

1. Unit test that attempt 17 is not requested and normal evaluation remains active.
2. Full Release, focused ASan+UBSan, mutation oracle, and 864-pair broad differential.
3. Repeat the same core and hit-rich paired pilot.
4. Advance to the formal matrix only if there is no opposite verdict/status regression,
   the all-model common-terminal CPU ratio is at most 1.01, and no individual task above
   one second regresses by more than 20% without a resource-status improvement.
