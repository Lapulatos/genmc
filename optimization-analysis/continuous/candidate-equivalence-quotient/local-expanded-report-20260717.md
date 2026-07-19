# Local SC-RVF Expanded Differential Report (2026-07-17)

## Scope

- Server and Docker execution remained paused.
- Input manifest: `local-expanded-cases.txt` (40 additional loop-free litmus variants).
- Matrix: baseline/RVF × one/two workers = 160 GenMC invocations.
- Every GenMC invocation had a 20-second timeout; the enclosing run had a 900-second timeout.
- Binary: local LLVM 20 RelWithDebInfo build in `.codex-build-rvf-tests2`.

## Correctness gates

- 160/160 rows completed; zero timeout.
- Zero exit-status or semantic-verdict mismatch between same-worker baseline and RVF.
- Zero one/two-worker RVF mismatch.
- Zero late fail-open on all 35 enabled cases; 5 cases used native fallback.

These checks establish differential safety-verdict and worker determinism for this
sample. They do not by themselves prove preservation of every reachable observation;
the dedicated four-outcome SB suite remains the stronger outcome-completeness probe.

## Initial search and cost observations

- Enabled cohort: baseline 294 complete executions, RVF 298 (net growth 4, -1.36%).
- RVF processed 584 reduced loads.
- Diagnostic median local one-worker elapsed ratio: 1.0080×.
- Largest reduction: `IRIWish`, 27 → 16 complete executions.
- Other reductions: `WRC+dep`, 7 → 5; `MP+rels+acq`, 3 → 2;
  `S+rels+acq`, 3 → 2.
- Largest growth: `LB3`, 7 → 18; `TC2`, 4 → 7; five LB variants each 3 → 4.

Conclusion: this expanded local cohort supplies correctness evidence but still does not
support a performance claim. The LB-family representative growth is the next local
algorithmic diagnosis target.

## Local merge-opportunity gate

The growth was caused by applying RVF to singleton value classes. `LB3` had 18 visible
sources in 18 value groups, but still queued 18 witnesses and 16 parent continuations.
The driver now keeps first-visit reads on native RF-DPOR unless at least one value class
contains multiple sources. Native reads are reconstructed from the current graph as
singleton `GoodW` constraints when a later RVF problem is built; they are not persisted
in the frame, so an RF revisit cannot leave a stale source constraint.

After the gate, the same 160-cell matrix reports:

- zero timeout and zero strict invariant violation;
- enabled complete executions 294 → 279 (15 fewer, 5.10%);
- 11 loads with a real merge opportunity, instead of labeling 584 loads as reduced;
- `IRIWish` 27 → 16, `WRC+dep` 7 → 5, and two 3 → 2 reductions;
- every former growth case restored exactly to baseline (`LB3` 7, `TC2` 4, five LB
  variants 3 each);
- diagnostic median local elapsed ratio 1.0072×, still too noisy and too close to 1 to
  claim wall-clock improvement.

An exhaustive observation probe then encoded the five loads in `IRIWish` and checked
all 32 Boolean outcomes. Baseline finds 16 reachable outcomes; RVF with one and two
workers matches the reachability of every outcome (96 invocations), with zero fail-open.
The same matrix passes under local ASan+UBSan. This directly strengthens the preservation
evidence for the cohort's largest 27→16 quotient, beyond safety-verdict equality alone.

The remaining reduced cases also have exhaustive encoded-outcome comparisons:

- `WRC+dep`: 5/8 reachable, `[0,1,4,5,7]`;
- `MP+rels+acq`: `[3,4]` across six encodings, including a sentinel for the skipped load;
- `S+rels+acq`: `[2,5]` across six `(seen-y, final-x)` encodings.

Baseline, RVF/n1 and RVF/n2 agree on all 20 cells (60 GenMC invocations). Together with
IRIWish, every program contributing to the measured 15-execution reduction now has a
full finite observation-set comparison. These probes pass under Release, ASan+UBSan and
TSan. This is strong bounded evidence, not a replacement for the general RVF proof.

## Error-reporting UB follow-up

The sanitizer compiler identified a generic nullable-ownership bug in DOT error output:
`if (&*errView)` dereferenced a possibly empty `unique_ptr` merely to test it. The code
now uses `if (errView)`. Release and ASan+UBSan RVF hard-error probes both exit 42 and
write a valid 1,703-byte DOT, with no sanitizer report. A fresh 40-case/160-cell matrix
is unchanged at 294→279 complete executions, 11 reduced loads, zero timeout and zero
invariant violation. This fix improves error-path definedness and is not counted as an
RVF performance gain.

## Evidence

- Raw logs and TSV: `local-results/differential-40-expanded-20260717a/`
- Strict analyzer output: `local-results/differential-40-expanded-20260717a/analysis.json`
- Post-gate evidence: `local-results/differential-40-local-merge-gate-20260717a/`
- Post-UB-fix evidence: `local-results/differential-40-errview-fix-20260717a/`
- Reproducer: `run-rvf-differential.sh` with `RVF_CASE_FILE=local-expanded-cases.txt`
