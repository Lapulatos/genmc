# P0 Closed-Prefix Regional RVF Re-entry Decision

Date: 2026-07-20

## Decision

Reject and remove the closed-prefix re-entry candidate. Its completeness certificate is stricter
than the previously rejected unconditional re-entry, but it has zero production activation on the
51 regional-eligible tasks. No paired resource run is justified for a mechanism that cannot reach
one supported RVF load.

The conservative non-atomic frontier remains in production. The versioned census launcher and this
report remain on the development branch; the temporary driver logic and counters are removed.

## Candidate and completeness argument

After a regional unsupported frontier dropped the RVF frame, the candidate considered starting a
fresh RVF suffix only at a supported ordinary atomic `Read`. Admission additionally required:

1. at least one concrete read in the native prefix; and
2. every prior read to have `isRevisitable() == false`.

`GenMCDriver::calcRevisits()` does not offer a prior read whose revisit flag is false. Consequently
no future write in that concrete descendant can add a new RF alternative to a prefix read frozen as
a singleton RVF constraint. Already queued native alternatives remain separate work items. This
directly avoids the missing-future-write error that invalidated unconditional suffix re-entry.

## Validation before census

- GCC 13.3 + LLVM 15.0.7 Release build: passed.
- Existing `sc-rvf-regional-na-load` integration gate: passed.
- Existing `sc-rvf-regional-loop` outcome/worker gate: passed.
- Local GCC 14 + LLVM 14 final executable build failed in LLVM's `IntervalMap` because LLVM 14's
  unqualified `bit_cast` is ambiguous with GCC 14's `std::bit_cast`. The candidate core library had
  compiled; the authoritative supported GCC 13/LLVM 15 build subsequently passed.

## Authoritative 51-task census

- Server root:
  `/data3/sujie/experiments/caat-optimization/p0-regional-rvf-closed-prefix-51-20260720-r2`
- Launcher: `launch-server-regional-rvf-closed-prefix-51.sh`
- Concurrency: 36 tasks.
- Per-task limit: 120 seconds and 12 GB.
- Container limit: 500 GiB; host reported 739 GiB available before launch.
- Acceptance: one compressed XML with exactly 51 `<run>` rows, one archive with exactly 51 task
  logs, and `complete.txt` present.

Final timeout-resilient statistics:

| Metric | Tasks with nonzero value | Sum |
|---|---:|---:|
| Native-only mergeable loads after the frontier | 37 | 63,899 |
| Closed-prefix checks at supported ordinary reads | 0 | 0 |
| Closed-prefix admissions | 0 | 0 |
| RVF loads attempted | 0 | 0 |
| RVF loads reduced | 0 | 0 |

The post-frontier opportunity census groups every native `ReadLabel`, including special waiting,
spin, and RMW-related read kinds. The RVF implementation intentionally accepts only ordinary
`EventLabel::Read`. All 63,899 mergeable post-frontier observations in this cohort occur on read
kinds outside that supported entry. Broadening RVF to those operations would change blocking/RMW
semantics and is not licensed by the ordinary-read RVF proof.

## Special-read and causal-class audit

Two additional observation-only runs used the same validated 51-task launcher and acceptance rules:

- read-kind census r3:
  `/data3/sujie/experiments/caat-optimization/p0-regional-rvf-closed-prefix-51-20260720-r3`;
- exact-HB CAS census r4:
  `/data3/sujie/experiments/caat-optimization/p0-regional-rvf-closed-prefix-51-20260720-r4`.

Both contain exactly 51 XML rows and 51 task logs. The type partition is exhaustive:

| Read family | Mergeable loads | Nominally removable sources | Tasks |
|---|---:|---:|---:|
| Ordinary `Read` | 0 | 0 | 0 |
| Pure special read (`BWait`, speculative, confirming, condvar) | 0 | 0 | 0 |
| CAS/lock/RMW read | 63,899 | 188,498 | 37 |
| FAI read | 0 | 0 | 0 |

The apparent opportunity therefore comes entirely from CAS/lock choices. For a successful lock
CAS, however, reading from unlock A versus unlock B changes the synchronizes-with edge and the
happens-before prefix even when both stores contain the same unlocked value. Such sources are not
RVF-equivalent.

The r4 audit conservatively refined each nominal CAS class by requiring the same value, provenance,
same-thread-source flag, and exact source HB view. Results:

| CAS class | Mergeable loads | Removable sources | Tasks |
|---|---:|---:|---:|
| Value-only/provenance class | 63,899 | 188,498 | 37 |
| Exact-HB-refined class | **0** | **0** | **0** |

Thus every nominal duplicate distinguishes a different causal/synchronization history. A value-only
CAS quotient would be unsound, while a causally valid quotient has zero opportunity on the measured
production cohort. Temporary classification counters were removed after the audit.

## Infrastructure rejection retained

The first launch root ends in `r1` and contains no accepted result. It requested 48 tasks at 12 GB
each inside a 300-GiB container; BenchExec rejected the aggregate 576-GB request before starting a
task. The corrected launcher uses 36 tasks and 500 GiB and explicitly checks the aggregate-memory
arithmetic before creating results. The r1 directory was not overwritten.

## Consequence for P0

`isRevisitable == false` remains a useful suffix-ownership lemma, but it cannot unlock the current
production cohort. Ordinary and pure-special reads have zero post-frontier classes; all apparent
CAS classes disappear after preserving the causal past. Regional P0 should therefore stop for this
workload rather than implement an unsound lock quotient or another suffix-entry condition. The
successful finite Deagle RVF+exact-SC-order lane remains the evidence-backed way to obtain RVF
search-space reduction where finite whole-program constraints are available.
