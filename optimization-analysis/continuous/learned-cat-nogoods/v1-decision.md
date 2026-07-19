# P0-A V1 learned CAT nogoods: pilot rejection

## Decision

Reject the V1 implementation as a default or retained optimization. Keep the sound
positive-conflict contract, but do not run the four-repetition 96-task formal matrix
for this implementation. The predeclared progressive pilot failed both focused advance
conditions: it produced no new terminal core task and regressed the hit-rich cohort.

## Correctness evidence

- GCC 13 Release: 163/163 tests pass.
- GCC 13 ASan+UBSan: 5/5 focused tests pass.
- Mutation oracle: 39 rows / 5,441 checks pass.
- Broad differential: 852 matches, 12 mutual unsupported pairs, zero mismatch over
  864 SC/TSO/PSO pairs.

## Performance evidence

- Strict core: baseline and candidate both terminate only `exponential-4`; the other
  eight tasks TIMEOUT at 60 seconds. New candidate terminals: zero.
- Hit-rich common-terminal CPU geometric-mean candidate/baseline ratios:
  SC 1.03591, TSO 1.13745, PSO 1.54683, all models 1.20904 (23 pairs).
- One useful resource transition exists: PSO `fib_safe-5` changes from TIMEOUT to TRUE
  in 47.408 seconds.
- Useful PSO reductions include `triangular-2` at 0.6311 and `fib_unsafe-5` at 0.8422.
  They are outweighed by `queue` at 7.4527, `circular_buffer_bad` at 4.2493, and
  `szymanski` at 1.7146.

## Root cause

V1 pays for a full `Reasoner` reconstruction on every inconsistent query until the
database happens to subsume the result. At PSO `queue`, synchronization falls from
1.181 to 1.007 seconds and offline evaluations fall from 2,959 to 109, yet total CPU
rises from 1.304 to 9.721 seconds after 176 explanation attempts. The dominant cost is
therefore outside the synchronizer in explanation reconstruction.

The second cost is linear clause/literal matching. TSO `butterfly` removes 38,784 of
152,057 offline evaluations but performs 24,280,918 literal checks; synchronization
rises from 12.272 to 16.385 seconds and total CPU rises by 24.1%.

## Next bounded experiments

1. V2a: cap costly explanation attempts independently of database capacity. This can
   only remove pruning opportunities, so it cannot change accepted executions.
2. V2b: if V2a passes, reorder/index clause probes using model-agnostic snapshot data.
   Measure literal checks separately so matching and explanation effects remain
   attributable.

The WMM/program-shape-specific Optimization 6.1 is not used in either refinement.
