# P0-B V1: conflict-directed forward-RF worklist pruning

## Starting state and scope

- Retained production baseline: `genmc-caat` at
  `d963c49e45b82066ca9dcd036232c7384f6a95fe`.
- Reuse the semantically validated P0-A V3 mechanism: lazy CAAT evaluation remains
  enabled, full provenance is materialized only for an admitted explanation, and the
  worker-local database retains only bounded positive sufficient CAT explanations.
- V4's fixed 1,024-query warm-up is not used because it increased total Reasoner time
  and lost two coverage gains. V3's 16-attempt cap and 64-hit credit remain.
- This version prunes only `ReadForwardRevisit`. Coherence, backward, optional, rerun
  and replay revisits are unchanged and therefore fail closed.
- No model name, task name, expected verdict, host profile, built-in SC/TSO checker or
  Optimization 6.1 selector may influence pruning.

## Exact mechanism

Before `GenMCDriver::done()` destructively restricts the current graph for one forward
read revisit `(read, source)`, the CAT checker performs two stages:

1. Resolve `source` and `read` to the checker worker's stable event keys. If no retained
   positive clause contains the exact base literal `rf(source, read)`, return
   `not blocked` without copying or materializing a graph.
2. Otherwise construct the same prefix selected by GenMC's
   `getViewFromStamp(read.stamp)`, set the copied read's RF to the copied source, and
   materialize that exact hypothetical prefix with the existing `StableGraphAdapter`.
   Drop the revisit only if every literal of a retained sufficient clause matches.

The implementation deliberately does not infer preservation of `po`, `co`, `fr`,
thread, location or set facts by hand. Exact prefix materialization remains the single
authority for all primitive CAT facts. Unsupported keys, absent events, initial-write
resolution failures, copy failures and non-read revisit kinds retain the work item.

## Correctness and completeness argument

Let `C = l1 /\\ ... /\\ ln` be a Reasoner-certified positive base-fact conjunction
sufficient to derive an `acyclic` or `irreflexive` violation. P0-A admits `C` only for an
online-admissible normalized CAT model without difference and only when every endpoint
has a stable worker identity. The P0-B exact hypothetical prefix matcher drops a
revisit only when that prefix entails every `li`. Therefore the revisited execution is
already CAT-inconsistent before interpreter suffix replay. Removing it cannot remove a
consistent execution. Failure to establish any premise keeps the revisit.

Database eviction, explanation admission and the first-stage RF index can only miss a
pruning opportunity. They cannot create a rejection. The full CAT/CAAT evaluator
remains the verdict authority; built-in checkers are not consulted for learned
rejections.

## Expected time and space effects

| Step | Possible saving | Added cost | Space |
|---|---|---|---|
| exact-RF clause prefilter | avoids prefix work for irrelevant revisits | stable-key lookup and indexed/linear clause probe | bounded RF clause index |
| exact hypothetical-prefix match | avoids destructive cut, dangling-read repair, interpreter replay, later CAT queries and descendants | one prefix copy, RF replacement, primitive materialization and clause scan | one temporary prefix snapshot |
| P0-A V3 learning | supplies cross-execution sufficient conflicts | bounded Reasoner/full-materialization calls | at most 4,096 clauses, 64 literals each |

The primary risk is paying a prefix copy for clauses that almost match but do not
reject. Counters must therefore separate: worklist read queries, RF-prefilter hits,
prefix materializations, pruned revisits, copied labels, materialization time, match
time, and retained database bytes. Existing evaluator-query, Reasoner, execution and
RSS counters remain authoritative.

## Correctness gates

1. Unit tests for stable RF candidate lookup, initial-write lookup, absent/evicted
   clauses, exact prefix match and fail-closed non-read revisits.
2. Randomized property test: every pruned hypothetical prefix is independently
   inconsistent under a fresh full `CaatEvaluator`.
3. Worklist differential tests: enabled and disabled runs have identical complete
   execution counts and verdicts; a constructed redundant inconsistent RF revisit is
   removed only when the exact sufficient clause is present.
4. Server Release unit/property suite.
5. Focused GCC 13 ASan+UBSan suite.
6. Mutation oracle: 39 rows / 5,441 exact full-recomputation comparisons.
7. Frozen 288-program SC/TSO/PSO broad differential: zero verdict, execution-count,
   error-category or unsupported mismatch across 864 pairs.
8. One- and multi-exploration-thread deterministic comparison where supported.

Any semantic, complete-execution, sanitizer or oracle mismatch immediately rejects and
restores the prototype before performance testing.

## Performance experiments

There is no selected-task performance pilot. After all correctness gates pass:

1. run the established 96-task x SC/TSO/PSO x before/after x four-repetition formal
   matrix: 2,304 cells, six simultaneous eight-worker BenchExec instances, 48 actual
   task workers;
2. if no correctness/coverage loss is observed, run the complete 725-task adapted set
   at 60 seconds to measure whether the mechanism converts prior TIMEOUT/OOM cases;
3. pull every XML and log archive and report CPU, wall, RSS, terminal coverage,
   complete executions, CAT queries, learned hits, RF-prefilter hits, exact-prefix
   checks, pruned revisits and time per successful prune.

The formal matrix is retained only with zero correctness/coverage loss and either a
task-clustered CPU 95% confidence upper bound below 1.0 or at least three deterministic
new terminal cells explained by nonzero revisit pruning without a common-terminal CPU
regression above 1%. Peak-RSS P90 may not increase by more than 5%. The 725-task run is
reported separately because fixed-seed nondeterministic inputs are not formal SV-COMP
proofs.

## Server and artifact boundary

- Build and test only on `server@frp-arm.com:36722` in task-owned containers derived
  from `genmc15noble:sujie`, mounting `/data3/sujie`.
- macOS is limited to source editing and lightweight compilation; no benchmark runs.
- Raw XML/logs, server paths, credentials, container definitions and benchmark
  transformations remain outside Git.
- Retained source is committed and pushed only after every frozen gate passes.
