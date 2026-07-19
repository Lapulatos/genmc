# Optimization 12 / P0.7a: Cycle-only closure slicing on the CSR baseline

## Objective

Remove a transitive-closure predicate only when its complete value is unobservable and
all of its consumers ask only whether the closure contains a cycle. The retained CAT
or CAAT evaluator remains the sole consistency authority; this optimization changes the
normalized relational program, not the memory-model semantics.

The experiment baseline is commit `7d405d7` (exact structural views plus adaptive CSR
edge primitives). The candidate differs only in normalization and its tests.

## Exact semantic rule

For every finite relation `R`, `acyclic(R+)` iff `acyclic(R)`, and
`irreflexive(R+)` iff `acyclic(R)`.

A predicate `P = R+` may therefore be erased only when:

1. no predicate consumes `P`;
2. at least one check consumes `P`;
3. every such check is `acyclic` or `irreflexive`;
4. `P` is a non-reflexive transitive closure with exactly one relation operand.

Every consuming check is redirected to `R` and normalized to `acyclic`. `empty`, a
downstream predicate, reflexive-transitive closure, no-check use, or any mixed check set
must retain the closure unchanged. Predicate IDs and all operands/check references are
renumbered after an erase. Repeating the rule permits exact nested dead-closure slicing.

## Expected time and space effects

- **Normalization:** one linear consumer scan and predicate renumbering per sliced
  closure; paid once when loading the CAT model.
- **Materialization/synchronization:** unchanged; the same primitive graph relations are
  produced.
- **Offline CAAT:** avoids constructing, publishing, comparing and scheduling the dense
  `reach = order+` value. This removes closure iterations and one relation-sized value
  from each full evaluation.
- **Online CAAT:** removes insertion-delta propagation into `reach`, its stored value,
  checkpoint copies, rollback copies and oracle comparison. Cycle detection executes
  directly on `order`.
- **Process RSS:** should fall on large-event recursive SC/TSO/PSO tasks because `reach`
  is dense even after primitive `rf/co/fr/...` became CSR. Small tasks may show no RSS
  change because allocator/runtime overhead dominates.

## Frozen correctness gates

The candidate is rejected and restored if any gate fails:

1. independent random-graph DFS property for both `acyclic(R+)` and
   `irreflexive(R+)` rewrites;
2. activation tests plus fail-closed near neighbours (`empty`, downstream consumer,
   reflexive closure, mixed consumers, unused closure);
3. exact bundled-model semantic certificates remain closed-world and their mutation
   negatives still fail;
4. all Release unit/property tests pass in `genmc15noble:sujie`;
5. all 39 mutation rows match the full offline oracle on every query;
6. the 288-program SC/TSO/PSO broad differential has zero comparable verdict mismatch;
7. the formal matrix has zero opposite terminal verdict, zero safe execution-count
   mismatch and no new OOM relative to the fixed baseline.

Unsupported tasks must remain mutually unsupported. A fixed-seed SV-COMP true result
is never treated as a proof over all nondeterministic inputs.

## Frozen performance design and decision gate

1. First run paired pilots on `queue_ok_longer` and `fib_unsafe-7`, with identical
   source, model, seed, limits and isolated rewrite roots.
2. If correctness and the pilot mechanism agree, run four balanced repetitions over
   96 tasks × SC/TSO/PSO × baseline/candidate: 2,304 cells.
3. BenchExec uses 48 task workers, one CPU core, 4 GiB memory and 60 s per cell. Baseline
   and candidate are interleaved; task-level paired bootstrap uses a fixed seed.
4. Preserve complete XML and compressed logs. Report common-terminal CPU, wall and RSS;
   large-event materialization/offline/snapshot-equivalent metrics; terminal-result
   transitions; model-specific ratios; and closure/predicate-count evidence.

Retain only if all correctness gates pass and one predeclared benefit gate passes:

- **A:** common-terminal CPU 95% CI upper bound is at most 1.02, and on completed
  large-event pairs peak snapshot-equivalent bytes fall by at least 10% while process
  RSS falls by at least 10%; or
- **B:** the same CPU bound holds and at least one previously TIMEOUT/OOM cell becomes a
  correct terminal result in every comparable repetition, without increasing OOM.

The normalized bundled models must contain no `reach` transitive-closure predicate;
otherwise neither benefit gate is accepted even if process measurements improve.

## Artifact boundary

Local protocol, analysis and pulled XML/log archives remain under
`optimization-analysis/continuous/cycle-closure-slicing-csr/`. Server builds,
BenchExec definitions, launchers, rewritten tasks and raw results remain under
`/data3/sujie/experiments/caat-optimization/cycle-closure-slicing-csr/`. None of these
artifacts, paths or server configuration may be committed. A retained Git commit may
contain only production source and focused tests.
