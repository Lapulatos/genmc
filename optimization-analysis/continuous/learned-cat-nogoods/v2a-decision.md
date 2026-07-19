# P0-A V2a bounded explanation: pilot rejection

## Decision

V2a is a positive refinement over V1 but remains a net regression and does not advance
to the formal matrix. Keep its 16-attempt safety bound as a backstop for the next
admission experiment; do not retain V2a by itself.

## Evidence

- Correctness: Release 164/164, ASan+UBSan 4/4, mutation 39/5,441, and broad
  differential 852/12/0 all pass.
- Core: zero new terminal tasks; eight tasks still TIMEOUT at 60 seconds.
- Hit-rich common-terminal CPU ratios: SC 1.0560, TSO 1.0974, PSO 1.1407, all models
  1.0957 over 23 pairs.
- PSO `fib_safe-5` remains a useful TIMEOUT-to-TRUE transition at 56.049 seconds.
  `triangular-2`, `fib_unsafe-5`, and `reorder_2` run at 0.7076, 0.8410, and 0.7467 of
  baseline CPU.
- PSO `queue`, `circular_buffer_bad`, and `szymanski` still regress to 2.2484, 1.8199,
  and 1.2811 of baseline CPU.

## Interpretation

The cap reduces PSO `queue` from V1's 176 explanations and 9.721 seconds to 16
explanations and 2.858 seconds, proving explanation reconstruction was the dominant V1
regression. But the first 16 conflicts are not necessarily reusable: queue records only
20 hits and 2,939 offline evaluations after spending the full budget. A fixed prefix of
conflicts is therefore not a sufficient admission policy.

V2b should wait for evidence of recurrence before invoking `Reasoner`: select one
explanation only on the second observation of the same stable violation witness. The
16-attempt bound remains as a fail-safe. Matcher ordering/indexing stays unchanged until
V2c so costs remain attributable.
