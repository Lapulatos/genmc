# Candidate-space census before generic CAT exploration reduction

## Purpose

Measure where the CAT/CAAT extension expands GenMC's search before changing exploration
semantics. Native GenMC already implements TruSt's sound, complete and optimal RF-DPOR
and enables SPORE symmetry reduction by default. The relevant hypothesis is narrower:
the generic CAT fallback conservatively enumerates RF/CO/revisit candidates because it
cannot yet derive the generated checkers' candidate constraints from arbitrary CAT
structure.

## Instrumentation

Under the existing opt-in `--cat-stats`, aggregate per task:

- RF and CO choices offered after existing generic optimizations;
- forward RF and CO alternatives actually queued;
- backward revisits offered and queued after maximality filtering;
- all work items added/popped and maximum retained work items across the single-worker
  execution stack;
- partial candidate-validity queries;
- revisit prefixes actually materialized and those rejected as inconsistent.
- at every post-filter RF choice point, the number of distinct
  `(value, provenance)` classes, candidates beyond one representative per class,
  repeated-value choice points, and the largest repeated class.

The value counters are an optimistic opportunity bound, not an equivalence proof.
Reads-value-from equivalence additionally requires the same event set, read values and
causal ordering between reads. Under a weak CAT model, synchronization and model checks
add further proof obligations. No candidate is removed from a repeated-value class.

No graph, hash set or trace is retained. The counters do not reorder, add or remove a
candidate. Formal tasks use one GenMC exploration thread and 48 BenchExec task workers,
so `maximumRetainedWorkItems` is an exact per-process peak rather than a sum of
asynchronous worker peaks.

## Correctness gates

1. local macOS compilation only; no local tests or benchmarks;
2. server Release unit/property suite;
3. focused GCC 13 ASan+UBSan suite;
4. mutation oracle 39 rows / every full recomputation;
5. 288-program SC/TSO/PSO broad differential, requiring zero status, signature,
   complete-execution or unsupported mismatch;
6. counter accounting invariants on every terminal log:
   queued-by-kind <= offered-by-kind where the kinds are comparable,
   popped <= added, inconsistent-prefix <= realized-prefix, and nonnegative peaks.
7. same-value accounting: repeated-value candidates <= RF offered candidates and
   repeated-value choice points <= RF multi-candidate choice points.

## Broad run

After correctness, run directly on the server with 48 BenchExec task workers:

1. one complete 96-task x SC/TSO/PSO census for comparison with earlier formal logs;
2. the full 725-task adapted SC/TSO/PSO dataset at 60 seconds where the existing
   transformation supports that model;
3. retain all XML and log archives and classify terminal, TIMEOUT and OOM groups.

No selected-task performance gate is used. Instrumentation timing is descriptive only;
the build is not a candidate speedup.

## Decision outputs

For each model and outcome class report:

- choices offered and queued per completed execution and per CAT query;
- fraction of realized revisit prefixes rejected immediately;
- peak retained work items versus RSS/OOM;
- whether RF, CO, backward revisits or local program/event growth dominates;
- the measured upper bound available to generation-time CAT propagation;
- the optimistic RF reduction available to RVF/value-centric exploration, separated
  from the subset for which causal-order and CAT-model preservation can be certified;
- whether exact generic preventive propagation can plausibly reduce at least 20% of
  candidate prefixes, or whether the next mechanism must instead be symbolic
  guard/data abstraction or stronger property-preserving equivalence.

The instrumentation patch is never a retained optimization. It is archived and removed
after the evidence is pulled.
