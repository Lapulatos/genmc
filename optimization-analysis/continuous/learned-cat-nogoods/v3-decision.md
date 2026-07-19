# P0-A V3 lazy-preserving provenance: formal result

## Decision

Do not retain V3 by itself. It fixes V2f's global lazy-cycle regression, gains coverage,
and passes every semantic/space gate, but the four-repetition task-clustered CPU gate
still shows a small systematic regression. Use V3 as the base for V4 warm-up admission.

## Correctness and coverage

- Release 165/165; focused tests 17/17; focused ASan+UBSan 17/17.
- Mutation oracle 39 rows / 5,441 checks; broad differential 852 matches, 12 mutual
  unsupported pairs, zero mismatch.
- Formal matrix: 2,304/2,304 cells and 1,152/1,152 pairs.
- Zero terminal-verdict mismatch and zero complete-execution mismatch.
- Six cell-level coverage gains and zero losses: PSO fib_safe-5 terminates in all four
  repetitions; PSO triangular-1 terminates in two of four.

The full sanitized queue path reports the same two null-reference UBSan diagnostics in
`lli/Runtime` with learned nogoods both enabled and disabled. They are preserved as a
pre-existing interpreter differential, not attributed to V3. The focused CAT sanitizer
suite has no error.

## Performance and space

- Pilot common-terminal CPU ratio: 0.9552; SC/TSO/PSO 1.0206/0.9931/0.8469.
- Formal task-model median CPU geometric ratio: 1.01032; task-clustered 95% CI
  [1.00151, 1.01866]. SC/TSO/PSO ratios are 1.00533/0.99909/1.02823.
- Cell-level CPU/wall geometric ratios are 1.00757/1.00721.
- Candidate/baseline RSS P90 ratio is 1.00242; memory-ratio P90 is 1.00316.
- Completed candidate logs contain 624 Reasoner calls, 556 full provenance
  materializations, 6,144,462 learned hits, and 78,751,315 literal checks. Full
  materialization costs 0.112 s total; Reasoner costs 19.506 s total.

## Mechanism

Large PSO tasks retain strong gains: triangular-2 0.6232, reorder_2 0.7308,
fib_unsafe-5 0.8091, and butterfly 0.8268. The primary regressions are sub-0.12-second
PSO tasks that pay one explanation per run but expose too little remaining exploration
to amortize it. No >1-second common-terminal task regresses by 20%.

## Next step

V4 delays the first optional explanation until 1,024 snapshot match queries. The
threshold is derived from existing generic bounds (`64` maximum literals times `16`
maximum attempts), not from model/task names. Subsequent attempts retain the 64-hit
credit. Skipping or delaying learning cannot change CAT semantics or completeness.
