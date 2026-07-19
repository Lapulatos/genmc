# Local SC-RVF Automatic Litmus Differential (2026-07-17)

## Scope

- Server and Docker remained paused.
- Automatic discovery selected all 185 C variants below `tests/correct/litmus` and
  `tests/wrong/racy`.
- Matrix: baseline/RVF × one/two workers, 740 rows.
- Per invocation timeout: 20 seconds; batch timeout: 1,800 seconds.
- Local LLVM 20 RelWithDebInfo binary.

## Correctness result

- 740/740 rows recorded; zero timeout and zero analyzer invariant violation.
- 184 runnable programs have paired status and safety-verdict equality.
- One fixture, `correct/litmus/wrw/variants/wrw0.c`, fails before the gate in all four
  modes because its required compile-time macro `N` is absent; all four statuses are 5.
- 108 programs enable RVF and have zero late fail-open; 76 use native fallback.
- Fallback reasons: 47 non-atomic store, 22 atomic RMW, 6 assume/IPR semantics, and
  1 modeled malloc call.

The first partial pass exposed `WWR+2WR`: baseline upgraded an unordered-write warning
to an IPR error, while an RVF request returned success because load annotations had been
disabled before the whole-program fallback decision. RVF now detects source-level
assume calls before transformation, retains native load-annotation lowering, and marks
the task unsupported before exploration. Baseline and requested-RVF n1/n2 now all exit
42 with the same IPR diagnostic.

## Search and local cost

- Enabled cohort complete executions: 1,308 baseline → 1,283 RVF, reduction 25 (1.91%).
- 17 loads have an actual multi-source value-group quotient.
- Diagnostic median RVF/n1 elapsed ratio: 0.9940×.

Compilation/transformation dominates these short local measurements, so 0.9940× is not
a wall-clock performance claim. It only shows no broad local regression at this scale.

## Raw value-group prefilter

Instrumentation showed that only 17 of 1,037 completed first-visit decisions entered a
multi-source quotient; 1,020 took the singleton/native path. Before the prefilter, those
native paths synthesized 1,875 prior-read constraints into provisional RVF problems.

The new first-stage check groups the raw store superset by value and provenance before
constructing the graph adapter. If no class is repeated, it immediately continues with
native RF-DPOR. This is exact because the later visible-write set is a subset of the raw
store set, and taking a subset cannot introduce a duplicate class.

Across the repeated 740-row matrix:

- native-read synthesis falls from 1,875 to 11, a reduction of 1,864 (99.41%);
- singleton bypasses remain 1,020 and actually reduced loads remain 17;
- complete executions remain exactly 1,308 baseline → 1,283 RVF;
- invariant violations and timeouts remain zero.

The diagnostic median elapsed ratio changes from 1.0060× before the prefilter to
1.0087× after it. These roughly 50 ms, compilation-dominated local runs do not establish
a wall-clock gain; only the removed construction work and unchanged search behavior are
claimed.

### Allocation-free small-class scan

A follow-up candidate replaces the raw prefilter's node-allocating `std::map` with the
repository's `SmallVector<ValueKey, 8>` and stops at the first duplicate class. The
predicate is unchanged: both implementations answer whether at least two raw sources
share value and provenance. The candidate only changes how that predicate is computed.

The repeated 740-row matrix again has zero timeout and zero invariant violation. All
aggregate mechanism/search values are identical: 1,308 baseline → 1,283 RVF complete
executions, 17 reduced loads, 1,020 singleton bypasses, and 11 synthesized native reads.
The diagnostic median is 1.0069× versus 1.0087× for the map implementation; this
difference is noise-level local evidence, not a demonstrated runtime benefit. The
candidate remains uncommitted and provisional pending direction review.

## Outcome completeness

All distinct structures contributing to the 25-execution reduction have finite encoded
outcome comparisons under baseline, RVF/n1 and RVF/n2:

- IRIWish: 16/32 reachable;
- WRC+dep: `[0,1,4,5,7]`;
- MP/S release-sequence variants: two reachable encodings each;
- cumul-release: `[0,1,4,5,7]`;
- rel-B-cumul-acq: `[0,1,4,5,7,8,9,12,13,15]`.

The extended outcome suites also pass under local ASan+UBSan and TSan. This is bounded empirical
evidence; the SC RVF proof obligations remain authoritative for general completeness.

The registered Release end-to-end suite passes 5/5 in 16.49 seconds after adding the
two new structures. The focused solver/state/adapter unit set remains 11/11.

## Evidence

- Raw matrix and logs: `local-results/differential-185-auto-litmus-final-20260717a/`
- Strict result: `local-results/differential-185-auto-litmus-final-20260717a/analysis.json`
- Counter baseline: `local-results/differential-185-overhead-counters-20260717a/analysis.json`
- Raw-prefilter result: `local-results/differential-185-raw-prefilter-20260717a/analysis.json`
- SmallVector-prefilter result: `local-results/differential-185-smallvector-prefilter-20260717a/analysis.json`
- Runner: `run-rvf-differential.sh` with `RVF_AUTO_LITMUS=1`
