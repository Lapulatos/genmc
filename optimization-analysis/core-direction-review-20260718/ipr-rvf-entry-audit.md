# IPR-compatible SC-RVF entry audit

## Decision

Proceed to a bounded experimental prototype, not production admission.

The source supports a unified design in which RVF owns annotated reads and native IPR is
disabled inside the whole-program certificate. An annotation is a concretized predicate
of the current read value, so every source in an existing RVF `(value, provenance)` group
has the same assume result. A failing group is infeasible and need not have a representative;
the existing parent continuation must remain responsible for future writes that can create
a succeeding group.

The current implementation is not safe if the static gate is merely removed. It can route
a singleton annotated read through native RF-DPOR while `conf.ipr=false`, or create a
failing RVF child whose later native backward revisit changes RF without updating the
frame's `goodWrites`. Both violate single ownership of the read class.

## Authoritative source facts

1. `lli/main.cpp::adjustConfig()` forces `conf.ipr=false` whenever the supported SC-RVF
   path is selected. Native IPR and RVF therefore do not currently run together.
2. `LoadAnnotationPass` attaches an `Annotation` to an annotatable load when the path from
   that load to `assume` has no intervening load or side effect. At runtime the expression
   is concretized except for the current load value.
3. `getReadRetValue()` performs early assume blocking/reset only when `conf.ipr=true`.
   The explicit assume call remains in the program, so a false value still blocks later,
   but native IPR's future-write reconsideration is absent.
4. `filterValuesFromAnnotSAVER()` removes failing non-maximal sources but deliberately
   retains the maximal source even if it fails. This preserves a blocked read for native
   IPR to revisit when a future write arrives.
5. `tryOptimizeIPRs()` revisits a blocked annotated read in place when a new write makes
   the assume succeed. `revisitInPlace()` removes the block successor and changes RF.
6. RVF stores class constraints in `Frame::goodWrites`, submits a parent continuation for
   future event growth, and submits one witness child per value/provenance group.
7. Native reads are intentionally absent from `goodWrites` and reconstructed from the
   current graph. A read already owned by an RVF class is present in `goodWrites`.

## Required ownership state machine

For each annotated plain atomic read `r`:

```text
UNSEEN
  -> PARENT_WAITING(new-event cutoff recorded, r processed)
  -> for each currently visible group G:
       predicate(G.value) == false: no child
       predicate(G.value) == true: WITNESS_CHILD(goodWrites[r] = G)

PARENT_WAITING
  -> schedule writes/non-loads before r
  -> revisit r only through RVF on the enlarged event set

WITNESS_CHILD
  -> replay a successful witness and continue after assume
  -> never use native IPR/backward revisit to replace r outside goodWrites[r]
```

The parent continuation is not an optimization detail. It is the completeness owner for
future writes. A failing value group represents no feasible program execution after the
assume and is therefore not a quotient class that needs a witness.

## Minimal prototype rules

1. An annotated plain atomic read always enters `handleRVFLoad`, including singleton and
   zero-merge cases. The ordinary singleton/native bypass is forbidden for that read.
2. Evaluate the concretized annotation once per `(value, provenance)` group before solver
   construction. Do not create a child for a failing group.
3. Always submit the parent continuation, even when every current group fails.
4. A successful child records `goodWrites[r]` exactly as today and replays the explicit
   assume, which must succeed by construction.
5. Any native backward revisit targeting a read already present in `goodWrites` is an
   invariant violation in the experimental lane. It must be counted and fail open before
   production; silently changing RF is forbidden.
6. Do not admit CAS/RMW annotations in the first prototype. Mutex lock CAS remains on its
   separately verified native-singleton path.

## Static admission requirement

`containsScRvfAssume()` is deliberately coarse. Production admission needs evidence that
every relevant user assume is represented by exactly one supported plain atomic annotated
load, or that an unannotated assume cannot require future-write reconsideration.

Counting `ModuleInfo::annotInfo.annotMap` is insufficient: one assume may annotate several
loads, several assumes may overwrite one load's annotation, and some assumes have multiple
source loads or intervening side effects. Extend load-annotation analysis with per-assume
coverage metadata:

- total user assumes;
- exactly-one-supported-load assumes;
- zero/multiple-source or side-effect-rejected assumes;
- CAS/RMW-associated assumes;
- duplicate-load annotation conflicts.

The initial production gate may admit only when all user assumes are in the
exactly-one-supported-plain-load class and there are no conflicts. All other programs
remain native fallback.

## Bounded oracle specification

Compare baseline native IPR against experimental RVF by reachable observation/error set,
blocked maximal count as a diagnostic, and worker-independent class coverage. Raw complete
execution equality is not required after quotient activation.

Required generated shapes:

1. current source fails, future write succeeds;
2. two same-value failing sources plus one future succeeding source;
3. two same-value succeeding sources (actual quotient activation);
4. mixed succeeding values and provenance-distinct equal bits;
5. own-thread and other-thread sources;
6. two consecutive annotated reads;
7. annotated read before and after uncontended/contended mutex;
8. a failing assume before a hard error (error must be infeasible);
9. a succeeding assume before a hard error (error must remain reachable);
10. one- and two-worker RVF ownership equality.

For every cell require zero late fail-open and zero native-revisit-of-owned-read invariant
violations. Run ASan+UBSan after the Release oracle.

## Go/no-go boundary

Go to implementation only for the bounded experimental lane described above. Do not
remove the production static gate globally.

After the generated oracle passes, measure the new conservative admission cohort on the
725 inputs. If exactly-one-supported annotations still yield zero reduced loads, reject
this branch before another full performance matrix. If the cohort activates, run the
actual baseline/control/RVF 725 matrix directly and require downstream class/work or
coverage improvement.

## Frozen implementation and workload result (2026-07-18)

The bounded implementation passes the local and server semantic gates, including future
writes, failing annotation groups, two annotations, mutex/RMW cases, native IPR warning
and error preservation, one/two workers, and conservative multi-source fallback. Server
Release is 190/190; Linux ASan+UBSan with leak detection passes 16 focused unit tests and
all 35 end-to-end invocations.

The direct 725-task census rejects the optimization before timing. Per-assume metadata
reduces the assume/IPR fallback population from 513 to 342, but only two tasks are
enabled. Neither reaches an RVF-owned read: aggregate attempted loads, reduced loads,
rejected annotated groups, suppressed owned revisits, and fail-open are all zero. The
activation retain gate is therefore false, and no baseline/control/candidate performance
matrix is warranted. Frozen evidence is under
`server-results/annotated-rvf-census-20260718b/`.
