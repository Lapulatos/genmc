---
type: results-report
date: 2026-07-19
experiment_line: genmc-caat-optimization
round: 0
purpose: paper-evidence-report
status: active
source_artifacts:
  - analysis-report.md
  - stats-appendix.md
  - figure-catalog.md
  - caat-optimization-final-report-20260719.md
  - fast-cycle-check-report-20260719.md
  - ../../continuous/linear-recursion-closure/report.md
  - ../../continuous/grouped-primitive-materialization/decision.md
  - ../../continuous/sparse-edge-primitives/decision.md
  - ../../continuous/cycle-closure-slicing-csr/decision.md
  - ../../continuous/lazy-cycle-eog/decision.md
  - ../../continuous/streaming-lazy-cycle/decision.md
  - ../../continuous/candidate-space-census/v9-decision.md
linked_experiments: []
linked_results: []
---

# GenMC–CAAT Optimization / Round 0 / Paper Evidence Report / 2026-07-19

## 1. Executive Summary

This report consolidates the optimization history that is currently supported strongly enough to
inform a future paper. The retained work improves three different bottlenecks without changing CAT
semantics or using GenMC's built-in memory-model verdict as a shortcut:

1. normalize exact recursive relation equations and avoid unnecessary derived closures;
2. reduce primitive and derived-relation construction/storage through grouped, structural, sparse,
   and lazy representations;
3. remove repeated evaluator scans and, in the opt-in preventive path, avoid generating provably
   reversing RF/CO choices.

The strongest current end-to-end result is the final exact evaluator foundation: unchanged
primitive caching, small dense primitive construction, packed witness checks, successor-cursor
composition, and successor-cursor cycle checks. On the simultaneous actual 725-task comparison it
increases correct terminals from 394 to 402, reduces TIMEOUT+OOM from 275 to 267, reduces CPU by
27.87% on 450 common-terminal tasks, reduces total CPU by 4.49%, and reduces suite wall time by
3.78%. There is no terminal-verdict mismatch or resolved-case regression.

The foundation is not yet a universal default. Twenty Goblint tasks move from TIMEOUT to OOM under
the unchanged 60-second/4-GB limits because faster exploration reaches the memory limit sooner.
Aggregate RSS rises 0.92%, although peak RSS stays at the same 4-GB cap. The supported paper claim
is therefore an exact, workload-relevant improvement in solved coverage and evaluator cost—not an
unqualified reduction in every resource category.

## 2. Experiment Identity and Decision Context

This experiment line began with a complete online CAAT integration and then iteratively optimized
normalization, representation, cycle evaluation, preventive pruning, and repeated small-graph
evaluation. Each candidate had to preserve the real `ExecutionGraph`, TruSt exploration semantics,
complete CAT evaluation, and fail-closed behavior for unsupported structures.

The final round answered a specific question: after profiling showed that fixed pthread-wmm tasks
were dominated by repeated small-graph primitive materialization and offline relation evaluation,
could exact work-removal mechanisms improve hard resource outcomes without pruning exploration or
weakening safety reporting? The answer is yes for the combined cursor-based foundation, with the
operational TIMEOUT-to-OOM caveat above.

An important belief update occurred during this round. The 2026-07-18 direction analysis deferred
relation cursors because earlier evaluator-only variants did not change hard outcomes. The
2026-07-19 experiments isolated a different mechanism: `nextSuccessor()` skips absent adjacency
entries in composition and DFS. Once combined with primitive work removal, cycle cursors rescued
eight tasks at the 283 gate and the same eight at the actual 725 gate. The older blanket deferral is
therefore superseded; the narrower lesson is that internal work reductions require both a large
measured hotspot and an end-to-end hard-outcome gate.

## 3. Setup and Evaluation Protocol

### 3.1 Workloads and resource limits

- Historical formal matrices: 96 tasks under SC, TSO, and PSO, normally four or six paired
  repetitions, one GenMC thread, one CPU and 4 GB per task, 60-second task limit.
- Recovery funnel: frozen easy/medium/hard pthread-wmm panel of 15 tasks, followed by 283 tasks only
  after a hard resource outcome changed.
- Final workload: the actual 725-task suite, simultaneous baseline/candidate run sets under the
  same binary, resource limits, and BenchExec environment.

The final 15/283/725 experiments are deterministic paired benchmark observations, not independent
random seeds. Their aggregates and transitions are exact descriptive results. No p-value or
population confidence interval is claimed for those single paired runs. Earlier repeated matrices
use task-clustered bootstrap confidence intervals and treat the task/model cluster—not each run
cell—as the inferential unit.

### 3.2 Correctness and completeness gates

Retained stages passed the applicable sequence:

- local and server Release unit/property tests;
- server ASan+UBSan focused suites with halt-on-error and leak detection;
- 39-row mutation gates over SC/TSO/PSO, one/two workers, RF/CO/RMW/lifecycle and dynamic graph
  changes, with thousands of independent old-path evaluator checks and zero mismatch;
- 864 broad differential pairs: 852 exact matches, 12 mutually unsupported, zero mismatch;
- paired formal status, verdict, execution-count, and search-counter audits;
- 283 and 725 status-transition audits with no resolved terminal regression or verdict mismatch.

No retained optimization truncates exploration, guesses a bound, substitutes a built-in checker
verdict, or reports safety from an incomplete search.

### 3.3 Decision vocabulary

- **Retained/default-capable:** correctness gates and the predeclared end-to-end or resource gate
  passed without a material operational regression.
- **Retained opt-in:** exact and useful, but workload scope, variance, or resource tradeoffs prevent
  default enablement.
- **Retained as foundation component:** contributes to the best combined result even if its isolated
  incremental effect is small.
- **Stopped/rejected:** internal work may decrease, but end-to-end or resource gates fail; the result
  remains useful negative evidence, not a claimed contribution.

## 4. Effective Optimization Methods and Results

### 4.1 Exact linear-recursion closure canonicalization

The normalizer recognizes finite relation equations `X = R | (X ; R)` and
`X = R | (R ; X)` (including operand orientations) and lowers them to the exact non-reflexive
transitive closure `R+`. It fails closed for unequal seeds, mutual recursion, extra terms, or other
operators. This removes one generated composition predicate and repeated fixed-point work while
preserving the named value, checks, explanations, and oracle comparisons.

In the six-repetition, 3,456-cell formal matrix, aggregate common-task CPU ratio was 0.9901 with
95% CI [0.9832, 0.9959], wall ratio 0.9896 [0.9824, 0.9956], and RSS ratio 1.00002
[0.99985, 1.00019]. Fifteen previously timed-out run cells completed. The mechanism profile kept
query counts fixed while offline time fell to 0.624 and synchronization time to 0.750 of baseline.
This is a retained exact normalization contribution.

### 4.2 Grouped packed primitive materialization

The adapter builds `po`, `int`, `ext`, and `loc` from exact thread/location groups and inserts
packed rows word-wise instead of classifying every active-event pair. Persistent dense values stay
unchanged, so this is construction-time work removal rather than memory compression.

On completed tasks with at least 512 stable events, materialization ratio was 0.2596
[0.2049, 0.3302], a 74.0% reduction. Whole-process CPU ratio was 0.9954
[0.9871, 1.0036], so no universal CPU speedup is claimed. The alternative retention gate passed:
seven TIMEOUT cells became repeatable correct `false(unreach-call)` results, with unchanged OOM,
zero wrong verdict, and zero safe execution-count mismatch.

### 4.3 Structural views and size-adaptive CSR primitives

Large dense base relations are represented exactly according to structure and density:

- `po/int/ext/loc` use O(N) structural metadata above 512 events;
- `rf/co/fr/rmw/tc/tj` use CSR only when owned CSR storage is at most half the packed dense matrix;
- dense relations retain their representation-specialized operations when density makes them
  cheaper;
- `fr` remains the exact relation `rf^-1 ; co`.

For completed tasks with at least 512 stable events, total current/history base bytes fell to
0.14139 [0.10608, 0.16869] of baseline, process RSS to 0.61512
[0.53543, 0.70107], and materialization time to 0.79688 [0.70568, 0.89969]. CPU was neutral at
1.00209 [0.99644, 1.00783]. Twenty OOM cells became TIMEOUT; no additional task was solved. This is
a retained memory optimization and evidence that some remaining OOMs belong to GenMC
exploration/history rather than CAT relation storage.

### 4.4 Cycle-only closure slicing

For finite relations, `acyclic(R+)`, `irreflexive(R+)`, and `acyclic(R)` are equivalent. When a
non-reflexive closure has no value-observing consumer and all consumers are cycle checks, the
normalizer checks the seed relation directly and removes the dense closure value. Mixed consumers,
`empty`, reflexive closure, explanation-sensitive cases, and extra-model consumers fail closed or
reconstruct the exact closure when needed.

In the retained four-repetition matrix, aggregate CPU ratio was 0.99575
[0.98138, 1.00687], large offline evaluation fell to 0.48197, large snapshot-equivalent state to
0.95338, and large process RSS to 0.95984. The CPU interval did not prove a general speedup, but
five SC tasks moved from TIMEOUT to correct false in all four repetitions, satisfying the
predeclared coverage gate. A later narrower experiment with a stricter CPU-only rule was rejected;
the retained version is justified by repeatable solved coverage, not by a 10% memory claim.

### 4.5 Analyzer-certified lazy cycle evaluation

For exclusive, non-recursive, positive relation cones used only by `acyclic`, the evaluator avoids
materializing derived union/composition relations and enumerates the exact extensional graph
directly during deterministic three-colour DFS. A structural selector activates the path only when
the cone contains expensive composition; unsupported, shared, recursive, explanation, oracle, and
preventive cases fall back to the generic evaluator.

The initial retained lazy selector achieved aggregate common-terminal CPU ratio 0.97950
[0.95888, 0.99714], TSO 0.97303, PSO 0.94685, large snapshot bytes 0.57732, and large process RSS
0.66807. Forty OOM cells became TIMEOUT and one PSO task became repeatably correct.

The later depth-bounded streaming traversal removed successor-vector construction and stopped edge
enumeration as soon as a cycle was found. The first unbounded recursive version caused deterministic
stack-overflow segmentation faults and was rejected. The retained 2,048-depth version falls back
to an exact iterative heap-frame DFS. It improved aggregate CPU by about 1.41%, ratio 0.98593 with
95% CI [0.97878, 0.99306], reduced consumed derivations by 7.86%, introduced no new error/OOM, and
preserved identical 716 true / 360 false / 76 TIMEOUT status totals in the formal matrix.

### 4.6 Generic preventive RF/CO pruning (V9, opt-in)

The analyzer proves a sufficient acyclic order certificate and uses exact prefix reachability to
remove RF/CO choices that would reverse the certified order before they enter the worklist. V9
avoids the full fixed-point/history evaluator for preventive preparation and evaluates only the
selected certified root. Unsupported provenance fails open; the authoritative consistency query
remains unchanged.

Against V8, candidate CPU ratio was 0.96105 [0.93854, 0.98130], wall 0.95698
[0.93535, 0.97650], and RSS 0.98326 [0.95772, 1.00052]. Offered RF+CO choices fell 67.82%, queued
choices 81.74%, work added 77.53%, work popped 73.97%, and peak retained work from 51,601 to 801.
The simultaneous no-pruning comparison rescued the same four tasks in every repetition but had a
wide CPU interval crossing one and a small RSS increase. V9 is therefore retained as an exact
opt-in search-space optimization, not a default universal improvement.

### 4.7 Final exact small-graph evaluator foundation

The final foundation combines five independently gated mechanisms:

1. **Exact unchanged primitive cache.** A complete descriptor compares every primitive determinant;
   only an exactly unchanged query reuses the previous snapshot.
2. **Fast dense primitive build.** Small dense relations avoid redundant edge sort/unique; `fr` is
   constructed directly from ordered writes and RF sources.
3. **Packed witness checks.** `firstPair()` and `firstReflexive()` scan packed/CSR relation storage
   without copying every row.
4. **Successor-cursor composition.** `nextSuccessor()` enumerates present lhs edges instead of
   scanning the full universe for membership.
5. **Successor-cursor cycle checks.** DFS preserves root, successor, parent, colour, and witness
   order while skipping absent adjacency positions.

#### Isolated and incremental effects on the fixed 15

| Foundation stage | Main measured effect | End-to-end decision |
|---|---|---|
| Primitive cache + dense build | completed CPU -22.49%; materialization -48.85%; FR -69.42%; packing -91.40% | retain |
| Packed checks | ordinary checks -5.12%; only -0.15% CPU beyond prior foundation | retain exact component |
| Successor-cursor composition | composition -88.61%; incremental completed CPU -4.64%; offline evaluation -19.89% | retain |
| Successor-cursor cycle checks | all CPU -12.74%; completed CPU -35.49%; checks -77.08%; offline evaluation -83.25% | retain and expand |

The final fixed-15 cycle foundation preserves 10 terminal / 5 TIMEOUT and exact search counts
18,693 complete / 197,703 blocked / 0 exceeding bound. CPU falls 475.920 to 415.295 seconds and
completed CPU 170.957 to 110.293 seconds.

#### 283-task expansion

TIMEOUT falls 183 to 175. Eight tasks become BenchExec-correct `false(unreach-call)` with no reverse
transition or terminal-verdict mismatch. Common-terminal CPU falls 642.146 to 436.096 seconds
(-32.09%), all-task CPU falls 2.70%, suite wall 3.39%, aggregate RSS 0.02%, and peak RSS 0.12%.

#### Actual 725-task gate

| Metric | Baseline | Candidate | Change |
|---|---:|---:|---:|
| Correct terminals | 394 | 402 | +8 |
| TIMEOUT | 244 | 216 | -28 |
| OOM | 31 | 51 | +20 |
| TIMEOUT + OOM | 275 | 267 | -8 |
| Common-terminal CPU (450 tasks) | 1,033.224 s | 745.272 s | -27.87% |
| All-task CPU | 16,366.595 s | 15,632.316 s | -4.49% |
| Suite wall | 717.88 s | 690.74 s | -3.78% |
| Aggregate RSS | — | — | +0.92% |
| Maximum RSS | 3,999,997,952 B | 3,999,997,952 B | unchanged |

All eight new terminals are pthread-wmm `false(unreach-call)` results. The 20 new OOMs are Goblint
`28-race_reach_*` tasks that previously timed out; they remain unresolved. There are 28 status
changes in total, no terminal-to-resource regression, and no terminal-verdict mismatch.

## 5. Statistical Validation

The earlier repeated formal matrices provide task-clustered bootstrap intervals and support the
specific ratio claims reported above. Those intervals must not be pooled across families because
baselines, repetitions, cohorts, and activation scopes differ.

The final fixed-15, 283, and 725 results use simultaneous paired lanes but one completed run per
cohort. Valid claims are exact benchmark-set aggregates, per-task status transitions, and semantic
invariants. Invalid stronger claims include population significance, confidence intervals inferred
from benchmark rows as if they were independent random samples, or an assertion that the same
percentage improvement will generalize to other workloads.

Correctness evidence is stronger than timing inference: thousands of old-path oracle comparisons,
864 broad pairs, sanitizers, exact search-counter equality on the fixed panel, and terminal audits
all support semantic equivalence within the tested scope. They are extensive finite evidence, not
a proof for every possible CAT model or C program.

## 6. Figure-by-Figure Interpretation

### Figure 1: mechanism and end-to-end CPU evidence

Files: `figures/figure-01-mechanism-cpu.pdf` and `.png`.

The figure exists to compare earlier search-changing and evaluator-only mechanisms. The reader
should notice that V9's interval is below one, while V10/V11 variants that reduced internal work
without changing realized search often regressed. The original implication—require measured work
removal before promoting a core optimization—remains valid. The figure predates the final cursor
foundation and must be updated before publication; otherwise it would incorrectly imply that all
relation-cursor work was ineffective.

### Figure 2: 725-task search funnel

Files: `figures/figure-02-search-funnel.pdf` and `.png`.

The figure shows millions of RF merge opportunities and realized prefixes but only tens or
hundreds of post-generation inconsistencies. It supports moving quotienting or conflict blocking
before candidate realization. It does not estimate the speedup of same-value quotienting and does
not replace the separate evidence for the exact evaluator foundation.

### Required new paper figures

Before manuscript use, generate two updated figures from the final CSV and stage reports:

1. a cumulative ablation plot for fixed-15 CPU and internal phase time across primitive, checks,
   composition, and cycle stages;
2. a 725 status-transition/all-task/common-terminal summary that visually separates eight solved
   gains from 20 TIMEOUT-to-OOM movements.

## 7. Failure Cases, Negative Results, and Limitations

### Exact micro-optimizations stopped before expansion

| Direction | Strongest local effect | End-to-end result | Decision |
|---|---:|---:|---|
| Ordered dense coherence/FR | coherence -17.55%; materialization -5.02% | all CPU -0.08%; wall +0.11% | stop before 283 |
| Descriptor miss-build | scan -8.87%; materialization -2.68% | all CPU +0.02% | stop before 283 |
| Descriptor vector reuse | materialization -11.01%; CAT consistency -6.06% | all CPU -0.52%; no hard change | stop before 283 |
| Forced incremental small-graph path | none | common CPU +32.73%; one new TIMEOUT | reject |
| Larger checkpoint history | 51,486 entries examined | zero subset matches | reject |
| Changed-query primitive delta | intended delta materialization | missed FR edges; 4.8x local cost | reject |

These results show why internal phase improvement is necessary but insufficient. Once the phase is a
small share of end-to-end cost, even double-digit local reductions do not justify a 283/725 run.

### Other historically important failures

- Unbounded recursive streamed DFS caused deterministic segmentation faults; depth-bounded exact
  fallback was required before retention.
- Per-plan lazy selection reduced edge work but rematerialized derived state, increasing large RSS
  and snapshot bytes beyond the gate.
- Positive conflict-core reuse recorded millions of hits but avoided zero direct checks and
  regressed CPU; a concrete rejected candidate is not proof that the consistent parent subtree has
  no extension.
- The bounded SC-RVF prototype preserved tested finite observations but activated on zero tasks in
  the formal 725 SC workload. It is feasibility evidence, not a workload speedup.
- Sparse CAT relations turn some OOMs into TIMEOUTs but cannot fix small-event 4-GB failures that
  occur before the first CAT checkpoint; those require exploration/history compression.

### External-validity boundary

The workload is an adapted fixed suite under fixed 60-second/4-GB bounds. Results do not establish
official SV-COMP scores, unbounded-program performance, or universal benefit across arbitrary CAT
models. The final foundation remains experimental and workload-selective until the
TIMEOUT-to-OOM policy is resolved.

## 8. What Changed Our Belief

1. **Strengthened:** exact structural work removal can materially improve both evaluator cost and
   solved coverage without altering search completeness.
2. **Revised:** relation cursors are not inherently too small to matter. They matter when they skip
   a measured N²/full-universe scan and are combined with upstream primitive savings.
3. **Strengthened:** search-space-changing optimization remains the most direct route to large
   scalability gains; V9's large candidate/work reductions are qualitatively different from
   evaluator micro-optimizations.
4. **Strengthened:** representation and evaluation need separate objectives. Structural/CSR values
   solve relation-memory pressure, lazy plans solve derived-state pressure, and cursor primitives
   solve repeated scan cost.
5. **Unresolved:** faster exploration may trade TIMEOUT for OOM. A default policy needs explicit
   memory-aware scheduling or exploration/history compression rather than hiding this transition.

## 9. Paper Claim Candidates and Next Actions

### Supported claim candidates

- **Exact evaluator foundation:** “On the tested 725-task workload, the combined exact CAAT
  evaluator optimizations produce eight additional correct results, reduce common-terminal CPU by
  27.87%, and reduce total hard-resource failures by eight, with no resolved-case or verdict
  regression.”
- **Structural representation:** “On completed large-event tasks, exact structural and adaptive
  CSR primitives reduce retained base storage to 14.1% and process RSS to 61.5% of the dense
  baseline, converting 20 OOM cells to TIMEOUT without changing correct-result counts.”
- **Lazy cycle evaluation:** “Analyzer-certified lazy evaluation avoids materializing exclusive
  composition-heavy cycle cones and reduces large snapshot/RSS substantially while preserving the
  generic evaluator as a fail-closed fallback.”
- **Preventive pruning:** “A structurally certified opt-in order reversal filter reduces offered
  RF/CO choices by 67.8%, queued choices by 81.7%, and popped work by 74.0% in its activating PSO
  matrix.”

### Forbidden stronger wording

- Do not call the final foundation universally faster or universally lower-memory.
- Do not count TIMEOUT-to-OOM as a solved improvement.
- Do not claim statistical significance for the single final 725 paired run.
- Do not pool confidence intervals from heterogeneous experiment families.
- Do not describe SC-RVF, conflict cores, descriptor reuse, or ordered coherence as established
  end-to-end contributions.
- Do not imply that any optimization changes CAT semantics, truncates exploration, or substitutes a
  built-in checker verdict.

### Concrete next actions

1. Freeze `cd09f78b` as the implementation anchor for the final exact foundation.
2. Produce updated paper figures and a compact exact-results table from the 725 CSV plus stage
   reports; keep status transition categories separate.
3. Run at least three independent paired 725 repetitions if infrastructure budget permits, using
   task-clustered analysis, before making population-style timing claims.
4. Investigate memory-aware exploration/history compression for the 20 Goblint TIMEOUT-to-OOM
   movements and small-event pre-CAT OOMs.
5. Treat generation-time quotienting/subtree blocking as a separate research contribution that
   requires a completeness proof and nonzero actual-workload activation.

## 10. Artifact and Reproducibility Index

- Implementation commit: `cd09f78b` (`perf(cat): optimize exact CAAT relation evaluation`).
- Final decision: `caat-optimization-final-report-20260719.md`.
- Stage reports: `primitive-full-build-report-20260718.md`,
  `fast-check-recovery-report-20260719.md`, `fast-composition-report-20260719.md`,
  `fast-cycle-check-report-20260719.md`, `fast-coherence-build-report-20260719.md`,
  `fast-descriptor-build-report-20260719.md`, and
  `fast-descriptor-reuse-report-20260719.md`.
- Final raw result root:
  `server-results/primitive-fast-compose-cycle-paired-725-20260719a/`.
- Final BenchExec table: 725 rows, two run sets; difference table: 28 rows. HTML/CSV files are in
  the result root's `html/` directory and contain no `cputime-cpux` field.
- Historical strict bundle: `analysis-report.md`, `stats-appendix.md`, `figure-catalog.md`, and
  `figures/`.
- Historical retained decisions: `../../continuous/linear-recursion-closure/report.md`,
  `../../continuous/grouped-primitive-materialization/decision.md`,
  `../../continuous/sparse-edge-primitives/decision.md`,
  `../../continuous/cycle-closure-slicing-csr/decision.md`,
  `../../continuous/lazy-cycle-eog/decision.md`,
  `../../continuous/streaming-lazy-cycle/decision.md`, and
  `../../continuous/candidate-space-census/v9-decision.md`.

This repository is not bound to an Obsidian project knowledge base, so no Obsidian write-back was
attempted.
