# Generic preventive V8 decision

V8 preserves V6/V7's exact candidate-space reduction and improves the candidate
implementation again, but it still does not satisfy the frozen default CPU/RSS gate.
The structural theorem remains valid and the implementation advances to V9 rather than
becoming the default.

Across four balanced 24+24-worker repetitions, V8 has the same four stable PSO coverage
gains, zero OOM, zero losses, zero safe-execution-count mismatch, and zero differences in
all 12 candidate-space counters. Against its simultaneous baseline:

- CPU ratio is 1.00796 with task-bootstrap 95% CI [0.94325, 1.09232];
- wall ratio is 1.00844 [0.94595, 1.09067];
- RSS ratio is 1.02600 [0.99979, 1.06664].

The isolated V8/V7 candidate comparison confirms that bidirectional CSR removed useful
work:

- candidate CPU ratio is 0.98477 [0.97280, 0.99650], a significant 1.52% reduction;
- candidate wall ratio is 0.98883 [0.97644, 1.00089];
- candidate RSS ratio is 0.99875 [0.99720, 0.99987];
- the corresponding baseline CPU ratio is 1.00582 [0.99839, 1.01339];
- every candidate-space counter remains exact.

The task-level attribution identifies the remaining avoidable cost. On
`pthread-C-DAC/pthread-demo-datarace-3.yml`, both configurations pop 41 work items and
find the same error, but baseline takes about 0.18 seconds while V8 takes about 2.14
seconds. V8 performs 1,719 preventive prefix queries, 1,759 full offline evaluations,
and about 1.27 seconds of offline evaluator work to suppress a large queued tail that
early error termination would never pop.

V9 therefore keeps the exact pre-enqueue reversal certificate but removes the full CAT
fixed-point/check/history path from preventive preparation. A separate stable-ID adapter
will materialize only the selected analyzer-certified lazy root's primitive cone. The
existing exact lazy interpreter will produce its acyclicity verdict and bidirectional
CSR directly. Unsupported or cyclic prefixes fail open. The authoritative consistency
query remains unchanged and continues to use the complete incremental/offline CAAT
backend.

