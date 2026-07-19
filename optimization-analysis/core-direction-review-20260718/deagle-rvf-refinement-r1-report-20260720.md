# Deagle RVF source-refinement r1 decision (2026-07-20)

## Decision

Retain the correctness framework on `genmc-caat-opt-dev`, but stop this refinement
algorithm and do not run a broad workload. Choosing a value class and then enumerating
its concrete RF members does not remove the dominant repeated SC-completion work.

## Correctness result

`FiniteRfRefiner` exhaustively enumerates the Cartesian product of active members and can
skip combinations proved equivalent by an exact RF core. A stronger incremental variant
adds concrete member selectors only for classes reached by the current Deagle model.
On a bounded two-read fixture with three sources per read, both variants reproduce all
nine concrete RF combinations and exactly match direct concrete encoding's SC-completion
status. The incremental oracle passes under ASan+UBSan.

Abstract assignments remain unfit for CAT/SC/replay. Only a member-refined assignment can
reach those consumers, and the materialization boundary continues to enforce this.

## Resource result

The first external Cartesian panel is preserved at
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-refinement-panel-20260720-r1`.
Concrete-SC completes only two of 15 tasks within 120 seconds (45.58 and 102.51 seconds),
while Cartesian value refinement times out on all 15. The candidate is rejected; this is
not a time-for-memory tradeoff.

After replacing Cartesian enumeration with incremental selector activation, the decisive
single-task diagnostic is
`/data3/sujie/experiments/caat-optimization/finite-rvf-incremental-safe010-20260720-r3`:

| Mode | Concrete candidates | Solver check (us) | SC completion (us) | Approx. wall |
|---|---:|---:|---:|---:|
| concrete-SC panel row | 310 | 390,238 | 44,792,557 | 45.58 s |
| incremental value→source | 305 | 357,281 | 43,577,652 | 45.00 s |

The five-candidate reduction and roughly 1% wall difference are below the project's
continuation threshold. No repetition or broader run is justified.

## Root cause and next algorithm

First-model value abstraction removed 25.94% of RF selectors, but exact SC completion
still receives one concrete RF graph at a time. Delaying selector creation changes when
that work is represented, not how many source/order cases must be decided.

The next implementation should encode the existential source condition directly in the
Deagle ordering theory. For a selected same-value class and load `r`, require that some
active class member `w` is the latest same-location write before `r` in the candidate SC
order. The solver can then return that latest member as a concrete witness without
enumerating every class member. This condition is exact for supported finite SC programs;
unsupported RMW/lifecycle cases must fail open until their ordering rules are encoded.

## Infrastructure failures and prevention

- The panel launcher accepted parameterized expected task counts but still hard-coded
  exactly three result XMLs. The two-mode run therefore lacked `complete.txt` despite an
  independently verified 15+15 XML rows and 30 logs. The launcher now takes
  `GENMC_FIRST_MODEL_EXPECTED_MODES`, checks `expected = modes × tasks`, and validates that
  number of XMLs.
- Direct diagnostic r1 invoked `/usr/bin/time` inside an image where it is absent; r2
  supplied the formal wrapper without its two required environment variables. Neither
  started GenMC and neither is evidence. r3 uses host `/usr/bin/time` and explicitly sets
  `GENMC_REAL_BINARY` and `GENMC_EXPERIMENT_MODE`.
- An incremental-model extraction regression routed concrete assignments through an
  unallocated abstract-member table. ASan identified the exact access; the concrete path
  is now explicit. A second oracle failure showed that adding constraints may move Z3 to
  a different uninstantiated class; refinement now loops until every selected class in
  the current model is concrete.
