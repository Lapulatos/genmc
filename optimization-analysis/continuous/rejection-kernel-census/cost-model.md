# P0.1 rejection-kernel census cost model

## Expected measurement overhead

For each rejected query with a positive explanation of `k` literals:

- provenance reconstruction: current dense reasoner cost;
- deletion minimization: up to `k + 1` complete offline CAAT evaluations;
- lookup: current antichain size times average kernel size;
- storage: at most 16,384 unique and 16,384 antichain kernels per worker.

This build is intentionally not a performance optimization. CPU and RSS may increase;
the flag-off path must remain unchanged. Capacity drops make reuse estimates a lower
bound and prevent a positive P0.2 decision under the frozen protocol.

## P0.2 predicted savings if opportunity is confirmed

An exact hit before returning from a consistency query would initially save only the
full evaluator call. Moving the same proved lookup to RF/CO/revisit decision points can
then remove:

- tentative graph mutation and synchronization;
- packed derived-relation evaluation;
- explanation construction for the repeated conflict;
- queued forward/backward revisits;
- cloned prefixes, checkpoints, and retained base-history state below that choice.

The time benefit should therefore track avoided queries and interpreter work. The space
benefit should track avoided revisit entries, retained prefixes, history-base bytes, and
active candidates—not merely the kernel store size.

The SC large-task measurement adds an important zero-benefit case: a task can execute
hundreds of thousands of consistency queries with no rejected candidates. No rejection
kernel can reduce that valid-execution enumeration. Such tasks require stronger
equivalence reduction, dependence/value abstraction, or symbolic loop/path reasoning;
P0.2 must not be presented as a general solution to their TIMEOUT/OOM.

## Soundness boundary

A positive kernel is monotone under additional base facts: once its exact sufficient
conjunction is present, retaining it cannot repair the already witnessed `empty`,
`irreflexive`, or `acyclic` violation. Negative/difference-derived facts are not monotone
under insertion and are excluded. Event removal, ID reuse, model changes, and unsupported
CAT fragments require version invalidation or the existing generic fallback.
