# P0.2 exact positive rejection-kernel cache

## Conclusion

Decision: **reject and remove the P0.2 prototype** under the frozen gate. Aggregate CPU cache/baseline is
1.04343 with task-bootstrap 95% CI
[1.00788, 1.09055]; PSO CPU is
1.13116 [1.01913, 1.29104].

The four-repetition matrix contains 2,304 cells, zero opposite
terminal verdict mismatch, and 0 safe
execution-count mismatches. PSO cache logs report 6,568,688/7,368,584
hits (89.14%), 3,092 learned kernels, and
0 capacity drops.

## Mechanism and scope

- A hit removes graph synchronization, evaluator propagation/rebuild, and checkpoint
  retention after stable materialization.
- A rejected miss pays Reasoner once; a later matching candidate returns before evaluation.
- Certified SC/TSO candidate profiles never construct the cache.
- The mechanism does not reduce valid executions, interpreter paths, or pre-query OOM.

## Correctness

- Release tests: 145/145.
- Mutation oracle: 39 rows and 5,441 full recomputations, including cache-hit validation.
- Broad differential: 852 comparable matches, 12 mutual unsupported, zero mismatch.
- Opposite terminal verdict mismatches: 0.
- Safe execution-count mismatches: 0.

## Limits

The 96-task corpus estimates solved-task overhead and some resource-bound changes. It does
not establish a broad 725-task coverage gain. A separate historical TIMEOUT/OOM run is
required before claiming that P0.2 reduces the full GenMC/Deagle coverage gap.
