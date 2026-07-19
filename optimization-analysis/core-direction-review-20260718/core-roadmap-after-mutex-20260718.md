# Core optimization roadmap after the mutex experiment

## Conclusion

Reject mutex support as a standalone performance candidate for the 725-task workload.
Retain its solver, adapter, replay, and concurrency correctness infrastructure because it
is needed by any future quotient that crosses locks, but do not extend another isolated
operation gate next.

The next core question is no longer “which unsupported instruction should be whitelisted?”
It is whether SC-RVF can coexist soundly with native assume/load-annotation IPR. The actual
run stops 513/725 tasks at that semantic boundary, while the completed mutex work receives
zero quotient traffic. This boundary must be solved architecturally or SC-RVF should be
paused as a full-workload optimization direction.

## Evidence from the actual 725-task run

The endpoint-normalized server/Docker matrix ran baseline, quotient-disabled control, and
RVF on all 725 actual tasks with 60 s and 4 GiB limits. The frozen result is
`server-results/mutex-full-20260718a/`.

- All three configurations have identical category totals: 398 correct, 297 error/resource,
  and 30 wrong.
- Control and RVF have zero status differences across all 725 tasks.
- No completed execution count differs. For this zero-activation run that is an expected
  diagnostic, not the correctness rule for a future active quotient.
- Only two tasks pass the whole-program RVF gate, and neither reaches an RVF-reducible load.
- Aggregate RVF work is exactly zero: zero attempted/reduced loads, solver calls, states,
  representatives, parent continuations, and fail-open events.
- The first recorded blockers are 513 assume/load-annotation IPR, 57 non-atomic stores,
  32 non-atomic loads, 32 loops, 4 malloc calls, and 1 remaining generic RMW. Eighty-four
  logs do not record a gate because compilation/tool termination occurs earlier.
- On the 398 common-correct tasks, RVF/baseline CPU is 1.00056 with 95% bootstrap CI
  [0.99665, 1.00461], wall is 0.99971 [0.99510, 1.00447], and RSS is 1.00010
  [0.99991, 1.00038]. These ratios measure zero-activation drift only.
- RVF/control CPU is 1.00246 [0.99883, 1.00642]. There is no demonstrated benefit or
  meaningful overhead because the candidate does no quotient work.

The bounded mutex oracles remain positive correctness evidence: the solver/adapter tests,
ASan+UBSan runs, contended mutex outcomes, and mutex+IRIWish 16/32 outcome set all pass.
They establish that lock-aware witnesses are feasible; they do not establish workload
effectiveness.

## Why the current optimization flow failed

The flow optimized the second blocker before making the first blocker compatible with the
quotient. A diagnostic census had shown 326 mutex tasks only after conservatively bypassing
some assume cases, but the production-safe configuration correctly restored native IPR and
therefore stops 513 tasks before mutex support can matter.

This is a dependency error, not a need for another micro-optimization:

```text
actual task
  -> assume/load annotation requiring native IPR (513)
  -> mutex/RMW semantics (hidden in many tasks)
  -> reducible same-value RF class (unknown until both boundaries pass)
  -> fewer queued classes/work (required success signal)
```

Implementing mutex first was still useful as a feasibility and correctness probe, but the
full run demonstrates that further isolated gate expansion has near-zero expected value.

## P0 decision: IPR-compatible quotient or stop SC-RVF

The only justified next SC-RVF core project is a design spike for native IPR compatibility.
It must answer these invariants before production implementation:

1. When an annotated assume invalidates a load choice, which quotient class owns the
   alternative sources removed by IPR?
2. Can the quotient's representative witness and native IPR revisit share one class
   frontier without duplicating or losing ancestor alternatives?
3. If IPR discovers an unsupported suffix after a class was merged, how are covered-class
   marks, queued descendants, and parallel-worker ownership revoked or reconstructed?
4. What complete signature includes load value/provenance, assume outcome, causal order,
   and the future writes that can make the annotation succeed?
5. Can a bounded independent enumerator compare all maximal outcomes and error kinds for
   annotated loads, with and without intervening mutex operations?

There are two acceptable designs:

- **Unified class enumeration:** RVF directly enumerates annotated-read value/source
  classes and treats assume success/blocking as part of witness realizability. Native IPR
  is not separately active inside the certified suffix.
- **Owned-region handoff:** native IPR and RVF operate in disjoint, explicitly owned
  regions with a complete frontier/revocation protocol. This is larger and riskier because
  the current task pool cannot withdraw already covered ancestor classes.

Prefer unified enumeration for the first proof attempt. Reject the design before coding if
it cannot state a finite class signature and a mechanically checkable parent-continuation
rule.

### P0 acceptance gate

Before another 725 run:

- generated assume/load-annotation oracle covers success, block, future-write enablement,
  own/other-thread source, repeated annotations, mutex before/after assume, and hard errors;
- baseline and quotient have identical reachable observation/error sets, not necessarily
  identical raw execution counts;
- one- and multi-worker quotient runs cover the same structural class set;
- every fail-open proves no prior class was marked covered;
- the gate census shows a nonzero cohort passing both IPR and mutex boundaries.

Then run the actual 725 tasks directly. Retain only if at least one actual task records
nonzero reduced loads and a downstream reduction in RF queued, work added/popped, realized
prefixes, or resource failures. Runtime without changed work is not sufficient.

## P1: memory-scalability track, independent of SC-RVF

The 32 OOM and 239 TIMEOUT rows show that coverage remains the dominant product outcome.
Prior adaptive CAT storage reduced relation memory but did not solve the hard cohort; many
failures occur before CAT state dominates. The separate core memory track should measure:

- bytes retained by execution labels and graph history;
- queued task/revisit bytes and peak retained work;
- duplicate prefixes/classes across workers;
- OOM-to-timeout and timeout-to-terminal transitions.

Candidate mechanisms are persistent/shared graph prefixes, compact reversible graph deltas,
and bounded/deduplicated revisit storage. Do not mix this track with evaluator cursor or
cache changes; its retain criterion is lower peak retained exploration state or improved
terminal coverage on the same 725 tasks.

## Deferred work

Until P0 produces actual class/work reduction or is rejected:

- additional mutex variants, trylock, init/destroy, or isolated operation whitelists;
- relation cursors, sparse-intersection tuning, CAT evaluator caches, and statistics-path
  optimizations;
- timing refinements on the current zero-activation RVF run;
- requiring raw complete/search-count equality for an active coarser quotient;
- TSO/PSO quotienting without a model-specific observation/class theorem.

## Immediate next step

Perform a source-level IPR/RVF invariant audit only. The deliverable is a small state
machine and a bounded oracle specification, with a go/no-go decision. Do not implement
another gate expansion until that audit proves how ancestor alternatives remain complete.
