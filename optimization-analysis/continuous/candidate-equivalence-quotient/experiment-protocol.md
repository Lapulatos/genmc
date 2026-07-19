# Candidate-Equivalence Experiment Protocol

## Compared configurations

- retained V9 RF-DPOR plus generic CAT/CAAT baseline;
- SC candidate-equivalence quotient with the same recursive-SC CAT model;
- quotient-disabled control (`--sc-rvf-exploration --sc-rvf-disable-quotient`): retain
  whole-program certification, RVF thread-pool/scheduler setup and per-read instrumentation,
  but delegate every read to complete native RF-DPOR before grouping/VerifySC/submission;
- TSO and PSO unchanged controls.

## Run policy

- Compile-only is allowed on macOS; all tests and experiments run in the owned server
  Docker image.
- BenchExec uses one dynamic queue per configuration and 48 task-level workers.
- No small performance pilot decides whether the implementation proceeds.
- Invalid launches are archived separately and never included in ratios.
- Raw BenchExec XML and complete log archives are retained and pulled locally.
- The quotient-disabled control must match the baseline verdict and native candidate/work
  counters on supported tasks, report a positive `rvf-quotient-disabled-loads` count when
  reads execute, and report zero VerifySC calls and queued representatives.

## Correctness comparison

For every task compare verdict, error category, explored local-state oracle on bounded
generated programs, and unsupported/fail-open reason. Any new FALSE is replayed on the
baseline; any lost FALSE or baseline-complete TRUE becoming quotient-complete TRUE with a
different reachable-state oracle rejects the implementation.

## Primary effectiveness measures

The quotient succeeds only when it lowers at least one exploration measure without a
correctness difference:

- `rf-offered`, `rf-queued`;
- `backward-offered`, `backward-queued`;
- `work-added`, `work-popped`, `max-retained-work`;
- completed candidates versus quotient representatives.

CAT query reduction is secondary. CPU, wall time and RSS determine whether a correct
reduction is usable, but do not substitute for candidate-count evidence.

## Decision rules

- Retain: no correctness difference and a reproducible candidate-space reduction on the
  full workload.
- Revise: correct reduction exists but realizability/closure overhead erases coverage gains.
- Reject: any unaccounted verdict/local-state difference, or the implementation only avoids
  evaluator work without reducing candidate exploration.
- Defer a model: proof obligations for that memory model are incomplete; use existing
  exploration rather than applying the SC rule heuristically.
