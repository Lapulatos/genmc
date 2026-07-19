# P0 regional SC-RVF bounded-loop decision

## Question and corrected acceptance criterion

The first smoke comparison incorrectly treated a native/RVF complete-execution change from 2 to 1
as a completeness failure. Native RF-DPOR executions and SC-RVF read-value/witness classes are not
the same partition, so a quotient may legitimately reduce that counter. The corrected gate compares
reachable observable states, safety-error categories, one/two-worker results, and late fail-open;
complete-execution reduction is a mechanism metric rather than an equality oracle.

## Positive minimal result

A bounded-loop fixture with an initial write and a same-value write preserves its safe verdict and
reduces complete executions 2 -> 1. It also changes RF offered 4 -> 1, work popped 1 -> 0, and
realized revisit prefixes 1 -> 0. Regional n1 and n2 have identical counters, one reduced load, two
visible same-value sources, one value group, and zero fail-open. Thus the original 2 -> 1 observation
is a genuine local quotient, not evidence of missing work.

A separate two-outcome fixture preserves both reachable error outcomes under native, regional/n1,
and regional/n2. Those error descendants revoke their transaction and replay the untouched native
entry, so their durable statistics correctly contain quotient-disabled native reads rather than
speculative reduction counters.

## Decisive generated oracle

The existing canonical generated-state oracle was extended with two dynamic iterations of each
thread body. It enumerates 20 canonical access shapes and all 81 tuples of two read results plus two
final values. Each state is checked under native/RVF and one/two workers.

- Four-shape gate: 324 states, 1,296 invocations, 310 reduced RVF cells, zero violations.
- Full gate: 1,620 states, 6,480 invocations, 2,162 reduced RVF cells, but 12 reachable-state
  violations across six shapes. Every violation reproduces with both worker counts.
- Native reports 154 error cells and RVF only 130; the missing 24 cells are exactly 12 states times
  two worker counts. Aggregate complete executions are 374,785 native and 193,013 RVF, while RVF
  reports 16,988 reduced loads. The large reduction therefore cannot be credited because it removes
  reachable safety errors.
- First counterexample: shape `000100`, outcome `2121`. Native n1 reaches the error after 12
  complete executions; RVF n1 reports safe after 23 complete executions despite three reduced loads.
  RVF n1/n2 are identical, so this is a deterministic equivalence/future-write defect rather than a
  worker race.

Raw authoritative server artifacts:

- `/data3/sujie/experiments/caat-optimization/p0-regional-loop-20260719/loop2-4shape-r1/`
- `/data3/sujie/experiments/caat-optimization/p0-regional-loop-20260719/loop2-20shape-r1/`

## Decision

Reject and remove bounded-loop admission. Preserve the generated oracle and counterexamples on the
development branch. Do not describe complete-execution inequality itself as unsafe; the decisive
reason is the 12 missing observable error states. A future loop-capable SC-RVF must refine its class
or ancestor-continuation protocol for repeated dynamic reads and future coherence writes, then pass
this exact 6,480-invocation oracle before any workload or timing experiment.

## Superseding repair result

The required repair was subsequently implemented as a dynamic native-ancestor future-write
frontier plus durable/speculative transaction-result splitting. Re-enabling regional loop admission
after that repair passes the same full gate: 20 shapes, 1,620 states, 6,480 invocations, zero
violations, and 156 RVF-reduced cells. The original rejection remains the historical result for the
unrepaired implementation; loop admission is now retained on the development branch pending an
actual-workload activation and resource gate. Raw repaired evidence:
`/data3/sujie/experiments/caat-optimization/p0-regional-loop-20260719/loop2-20shape-native-frontier-r1/`.
