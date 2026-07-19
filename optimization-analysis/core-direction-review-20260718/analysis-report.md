# Core optimization direction analysis

## Primary question

Given the retained implementations, rejected prototypes, and full 725-task evidence,
which optimization mechanisms can materially improve GenMC/CAAT scalability, and which
work should be deferred as evaluator micro-optimization?

The primary unit is a task within a memory model. Results from different experiment
families are not pooled into one statistical test because their task cohorts, repetitions,
and baselines differ. Each confidence interval retains the unit and protocol of its source
decision artifact.

## Main conclusion

The next core implementation should reduce generated *consistent* RF alternatives and
their descendant revisits before they enter the worklist. The best supported route is a
correct SC RVF-style quotient with a broader but proved activation scope. The second core
route is generation-time CAT conflict propagation that prevents a decision subtree from
being queued. Persistent exploration/history compression is a separate memory route for
OOMs that occur before CAT evaluation.

Do not spend another implementation round on relation cursors, evaluator caches,
candidate-level rejection reuse, or faster recomputation of an unchanged candidate set.

### Post-mutex actual-workload update

The direct baseline/control/RVF 725-task matrix now rejects mutex support as a standalone
performance candidate. Only two tasks pass the production whole-program gate, and both
have zero reducible loads; all RVF mechanism counters are zero. The dominant first blocker
is assume/load-annotation IPR on 513 tasks, so further isolated operation whitelisting is
deferred. The next SC-RVF work is limited to an IPR-compatible quotient design audit; if
it cannot prove ancestor-class completeness, pause this direction and prioritize
exploration/history memory compression. Detailed decision and acceptance gates are in
`core-roadmap-after-mutex-20260718.md`.

## Evidence synthesis

### 1. Search-space reduction is the only repeatably useful mechanism class

- V9 preventive pruning changes the search before descendant exploration. Against V8 it
  reduces offered RF+CO by 67.82%, queued RF+CO by 81.74%, work added by 77.53%, and work
  popped by 73.97%. CPU is 0.96105 [0.93854, 0.98130], with four repeatable coverage gains
  and zero losses in the balanced formal matrix.
- V10 records 14,193,035 learned-core hits but avoids zero direct checks. It preserves the
  same completed search and regresses CPU to 1.049916 [1.025769, 1.077275].
- V11 removes 1,205,405 full-root checks and cuts emitted internal reach candidates from
  2.94 billion to 489 million, but does not change GenMC search counters. CPU regresses to
  1.108462 [1.067238, 1.154924] and 18 tasks lose terminal coverage.
- V11.1 cuts base cursor visits from 16.12 billion to 2.73 billion but leaves the search
  unchanged. CPU is 1.099922 [1.056791, 1.148091].
- V12's two-run per-task median CPU ratio is 0.998008 [0.988150, 1.009684], with unchanged
  search and a small positive RSS point estimate. It supplies no reproducible benefit.

The evidence does not imply that every search-space reduction is fast. It shows the
necessary condition: an optimization that cannot lower RF/CO alternatives, queued work,
realized prefixes, complete executions, or resource failures should not be a core project.

### 2. The opportunity is before candidate realization, not after rejection

Across the full 725-task census:

| Model | RF offered | optimistic same-value alternatives | share | realized prefixes | post-generation inconsistent |
|---|---:|---:|---:|---:|---:|
| SC | 67,978,223 | 12,868,426 | 18.93% | 22,090,308 | 207 |
| TSO | 57,718,147 | 9,775,191 | 16.94% | 16,927,328 | 207 |
| PSO | 23,005,964 | 2,787,706 | 12.12% | 5,243,489 | 67 |

The post-generation inconsistent fraction rounds to 0.00% in all three models. Filtering
after revisit realization therefore cannot remove the dominant work. A quotient must
prevent multiple equivalent consistent sources from being offered/queued, or a CAT
certificate must block a partial assignment before descendants are generated.

TIMEOUT rows expose an aggregate optimistic same-value share of 29.67%/29.55%/28.47% for
SC/TSO/PSO. The task-level association is large before search-size adjustment, but after
restricting to tasks with at least 10,000 RF offers, completed and timeout medians differ
by only +0.62/+0.37/-0.77 percentage points. Therefore the allowed claim is that the hard
cohort contains millions of merge opportunities—not that a 30% quotient will produce a
30% runtime reduction.

### 3. The first SC-RVF implementation proved local possibility but missed the workload

The repaired bounded SC-RVF prototype has unusually strong finite evidence:

- generated oracle: 20 shapes, one/two workers, 6,480 calls, zero reachable-state,
  verdict, worker, timeout, or late-fail-open difference;
- local enabled cohort: 1,308 to 1,283 complete executions after conservative class-key
  repair;
- expanded merge-opportunity cohort: 294 to 279 executions, with all reduced programs
  checked against their full finite observation sets;
- Release, ASan+UBSan, mutation, 185-case differential, and 864-pair broad gates pass.

The formal 725-task SC run nevertheless activates on zero tasks. Of 106 gate-bearing
executions, whole-program certification fails open for external `abort` (53), modeled
mutex lock (40), non-atomic access (9), loop (4), or another unsupported operation. Thus
the 1.00281 [0.99650, 1.00864] CPU ratio is zero-activation drift, not RVF performance.

The lesson is not “RVF has no opportunity.” It is that the present whole-program support
set covers none of this workload. A post-report source audit also ruled out generic
regional fallback: after earlier classes have been merged, native exploration cannot
reconstruct deleted ancestor alternatives. The sound near-term route is incremental
whole-program/whole-suffix support expansion, not an arbitrary region switch.

## Ranked core roadmap

### P0-A: certified regional SC-RVF quotient

**Post-report source audit:** the current implementation cannot safely enter and leave an
arbitrary region. The root whole-program gate is what ensures fallback occurs before any
class merge; later `rvf.reset()` can only continue from the current prefix and cannot
restore ancestor RF alternatives. `Frame` and `ThreadPool` also lack region ownership,
covered-class revocation, and descendant withdrawal. P0-A therefore remains the preferred
research direction but is not implementation-ready. Exact entry conditions are recorded
in `p0a-entry-audit.md`; P0-B becomes the active implementable core branch.

#### Goal

Enter quotient exploration only at a boundary whose complete descendant region can be
represented, and leave it only after emitting a structurally checked frontier. Unsupported
operations outside that region stay on native RF-DPOR. Unsupported behavior discovered
inside the region must invalidate the whole region before any class is considered covered.

#### Required design changes

1. Replace the whole-program gate with a **region certificate**, not a per-load heuristic.
   The certificate owns a dynamic event interval, supported effects, stable event IDs,
   and its exit frontier.
2. Make future events part of the recursion invariant. A current-prefix witness is
   insufficient because future writes may change visible sources and coherence placement.
   Preserve the always-backtrack parent continuation until the new-source signal has an
   independent proof/oracle.
3. Keep class identity at least `(event set, values + pointer provenance, read causal
   order, own/non-own source distinction)`. The own/non-own repair is required by the two
   previously lost generated outcomes.
4. Mark a class covered only after witness realization, graph replay, view rebuild, and
   recursive CAT acceptance. Hashes may index classes but structural equality decides.
5. Define shared ownership for parallel workers. One- and two-worker outcome equality is
   evidence, not a substitute for a partition invariant.
6. Add explicit rules for the dominant failed gates in this order: property/abort terminal,
   modeled mutex region boundary, loop iteration identity, then non-atomic access. Do not
   enable an operation merely to increase benchmark activation.

#### Completeness oracle

Raw execution-count equality is intentionally not required. The gate is:

- every bounded baseline maximal execution maps to a quotient class signature;
- every baseline reachable per-thread local observation appears under the quotient;
- every quotient witness replays to a baseline-valid execution;
- every baseline error remains reachable with the same error kind;
- future-write, nested-frontier, loop-iteration, own/non-own source, and provenance
  adversaries pass;
- one-worker and multi-worker class ownership cover the same structural class set;
- every fail-open ledger entry proves that no class was marked covered before fallback.

After these gates, run the actual 725 tasks directly. Effectiveness requires a nonzero
activation cohort plus reductions in RF offered/queued, work added/popped, realized
prefixes, or quotient representatives. CPU alone is not enough.

### P0-B: generation-time CAT subtree blocking

**Post-report source audit:** V10 already matches positive cycle cores at candidate
generation, before RF/CO lists are returned to the driver. A concrete rejected-candidate
core proves neither that the consistent parent prefix has no extension nor that its
siblings are equivalent. Rebranding the same matcher as backjumping would therefore save
invalid-candidate reconstruction at most, not remove consistent executions/classes. P0-B
is not implementation-ready without a stronger no-consistent-extension certificate; see
`p0b-entry-audit.md`.

This is complementary to RVF: it removes inconsistent classes rather than merging
consistent ones.

The previous V10/V3/nogood work learned useful conflicts too late. The next form must:

1. extract a sufficient positive violation clause from the certified CAT cycle;
2. project each literal to a stable RF/CO decision and earliest rollback-safe level;
3. watch the partial assignment while choices are generated;
4. prevent the violating alternative or descendant subtree from entering the worklist;
5. fail open when provenance, rollback mapping, or clause sufficiency is unavailable.

The early stop rule is mechanical: if `direct-checks-avoided`, `work-added`, `work-popped`,
and realized prefixes are unchanged on the activating actual-workload rows, stop before
another large formal repetition. Millions of cache hits are not success.

### P1: exploration/history compression for pre-CAT OOM

Exact structural and adaptive-CSR relations are already retained and reduce large-task
base bytes to 0.14139 and RSS to 0.61512. They convert 20 OOM cells to TIMEOUT but solve
no new task. Several historical 4-GiB failures have only 14--52 events and at most about
25 KiB of CAT state, or fail before the first CAT checkpoint.

Their core bottleneck is therefore GenMC exploration/history state. Relevant work is
prefix sharing, compact graph deltas, deduplicated revisits/classes, and bounded retained
work—not another CAT relation representation. This track should measure retained labels,
graph/history bytes, worklist bytes, and OOM-to-terminal transitions separately from CAT
snapshot bytes.

### Deferred until after P0/P1

- relation successor/predecessor cursors;
- base-intersection ordering;
- evaluator memoization and candidate-level rejection caches;
- dynamic selectors among equivalent evaluator paths;
- closure/composition micro-optimizations that preserve all candidate counts;
- TSO/PSO value quotienting before a model-specific observation signature and
  realizability theorem exist.

## Claim candidates

- Claim:
  - Source evidence: V9--V12 decisions and full 725 candidate census.
  - Allowed wording: Search-space-changing V9 is the only recent family with a
    reproducible end-to-end CPU reduction; evaluator-only V10--V12 variants do not improve
    the unchanged full search and often regress it.
  - Forbidden stronger wording: Any search-space reduction will improve CPU, or all
    evaluator optimizations are useless.
  - Uncertainty: protocols differ across families; this is a mechanism synthesis, not a
    pooled causal meta-analysis.
  - Next check: regional SC-RVF activation and candidate counters on the actual workload.
  - Decision: keep.

- Claim:
  - Source evidence: 2,175 task/model census rows and timeout-effect analysis.
  - Allowed wording: hard tasks contain millions of optimistic same-value RF alternatives,
    making pre-generation quotienting a relevant target.
  - Forbidden stronger wording: same-value grouping is complete by itself or will remove
    30% of runtime.
  - Uncertainty: same-value counts are only an upper bound and are confounded by search
    size.
  - Next check: measure actual class representatives, not same-value candidates.
  - Decision: keep with boundary.

- Claim:
  - Source evidence: bounded SC-RVF oracles and zero-activation formal run.
  - Allowed wording: the repaired RVF design preserves tested finite observation sets and
    can reduce executions locally, but its whole-program gate does not cover the actual
    workload.
  - Forbidden stronger wording: SC-RVF is generally complete in the current integration
    or already improves the full workload.
  - Uncertainty: arbitrary-program proof and regional fail-open invariant remain open.
  - Next check: region certificate plus future-write exhaustive oracle.
  - Decision: revise.

## What changed our belief

The project should no longer use “CAT internal work reduced” as a proxy for scalability.
V11.1 supplies the decisive counterexample: 83% fewer base cursor visits coexist with a
10% CPU regression and 18 coverage losses. The accepted evidence unit is now a reduced
candidate/class/subtree count on the actual workload, followed by end-to-end resource
measurement.
