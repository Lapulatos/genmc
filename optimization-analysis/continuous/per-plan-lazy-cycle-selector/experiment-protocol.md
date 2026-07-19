# P0.7c per-plan composition selector protocol

Date frozen: 2026-07-16
Baseline: `959f8697cfd3b20d47a4ca9d925e5e7e8ffdc508`

## Question

Does selecting lazy cycle evaluation independently for each `acyclic` cone remove the
union-only coherence DFS overhead in TSO/PSO without losing the retained
composition-heavy time, memory, or coverage gains?

## Structural rule

Compile each candidate cone with the retained P0.7b exact grammar and exclusivity rule.
Admit that check only if its own cone contains at least one `Composition` predicate.
Derived predicates are elided only for admitted per-check cones. Do not select by model
name, host profile, event count, runtime counter, verdict, or built-in checker result.

The TSO/PSO main order checks should remain lazy because their cones contain preserved
order and lifecycle compositions. Their separate coherence check should return to the
generic evaluator because its cone contains union/intersection but no composition. SC
should continue to admit no lazy plan.

## Expected time and space effects

- Remove one `LazyRelation` construction, colour/parent/seen allocation, source DFS,
  successor deduplication, sorting, and root-edge enumeration per TSO/PSO query for the
  union-only coherence check.
- Restore generic materialization and incremental maintenance of the coherence cone.
  P0.7b1 showed union-only lazy SC slower than generic, but the net TSO/PSO CPU effect
  remains measured rather than assumed.
- Restore persistent values, undo entries, and checkpoint-equivalent bytes for the small
  coherence cone. Peak snapshot/RSS may therefore rise slightly relative to P0.7b2; the
  frozen gate bounds that cost.
- The main composition cone and its coverage benefit remain unchanged. This prototype
  does not change successor enumeration inside that cone; product-state EOG traversal is
  a separate P0.7d candidate.

## Correctness and activation gates

1. Unit tests prove mixed plans in one model: a union-only check is generic while a
   composition-containing check is lazy, with exact elision flags for both cones.
2. Complete Release unit/property suite passes.
3. The 39-row mutation oracle and 288-program x SC/TSO/PSO broad differential have zero
   mismatch.
4. SC has zero lazy checks. TSO/PSO retain lazy checks but perform exactly one lazy
   cycle check per eligible offline evaluation in a focused smoke.

Any semantic or activation mismatch removes the candidate before performance testing.

## Performance matrix and retain gate

Use the same 96 tasks, SC/TSO/PSO, before/after, four repetitions, swapped core placement,
48 BenchExec task workers, one CPU, 4 GiB, and 60 seconds per cell. Preserve all XML and
compressed logs.

Retain only if all are true:

- no opposite terminal verdict, no common-safe execution-count mismatch, no lost correct
  terminal result, and no new OOM;
- aggregate common-terminal CPU bootstrap 95% CI upper bound <= 1.01;
- TSO and PSO CPU point ratios are each <= 1.02;
- large-event process RSS and peak snapshot-equivalent bytes are each <= 1.02;
- and either aggregate CPU point ratio <= 0.995, or large-event offline time <= 0.97
  with at least 10% fewer lazy root-edge candidates.

Otherwise restore all P0.7c production/test changes to baseline and retain only the
experiment record.

