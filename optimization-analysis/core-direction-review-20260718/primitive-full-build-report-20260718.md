# Exact primitive full-build experiment — 2026-07-18

## Decision

Keep the implementation behind `--cat-fast-primitive-build` and continue the combined core
optimization on the fixed 15-task panel. Do not promote it to default or expand to all 283
`pthread-wmm` tasks yet: completed-task CPU improves 22.49%, but the five fixed 61-second
TIMEOUTs are unchanged and dilute all-task CPU improvement to 7.90%.

## Algorithm

The experiment preserves a complete from-scratch primitive materialization on every changed
query. It makes two exact construction changes:

1. For dense relations with at most 512 stable events, insert edge bits directly. Sorting and
   uniquing is redundant because bit insertion is idempotent. The >512 CSR path is unchanged.
2. Derive `fr = rf^-1 ; co` directly from each address's existing ordered store vector, indexed
   by RF source. The baseline first expands every CO edge into a hash table of later writes.

Neither change removes an execution, changes a CAT predicate, or owns TruSt exploration.

## Correctness evidence

- Local unit: 219/220 passed; one expected Z3-availability skip.
- Server Release: 219/220 passed; one expected skip.
- Server GCC ASan+UBSan focused: 18/18.
- Online mutation oracle: 39 rows and 5,466 full evaluator oracle checks, zero mismatch.
- Broad differential: 288 programs × SC/TSO/PSO = 864 comparisons; 852 matches, 12 mutually
  unsupported, zero mismatch.
- Fixed panel: baseline and candidate both have ten correct terminal results and five TIMEOUTs;
  no incorrect or aborted result.

## Paired fixed-15 result

Simultaneous disjoint-core run: baseline cores 0–3, candidate cores 4–7.

| Metric | Baseline | Candidate | Change |
|---|---:|---:|---:|
| CPU time, all 15 | 470.52 s | 433.35 s | -7.90% |
| CPU time, 10 completed | 165.56 s | 128.33 s | -22.49% |
| CPU time, 5 TIMEOUTs | 304.96 s | 305.02 s | +0.02% |
| Concurrent suite wall time | 138.37 s | 134.33 s | -2.92% |
| CAT consistency | 89.571 s | 51.528 s | -42.47% |
| materialization | 37.747 s | 19.308 s | -48.85% |
| offline evaluation | 49.106 s | 26.897 s | -45.23% |
| primitive coherence group | 25.393 s | 5.773 s | -77.27% |
| relation packing | 14.444 s | 1.242 s | -91.40% |
| FR derivation | 7.721 s | 2.361 s | -69.42% |
| aggregate RSS | 398,757,888 B | 398,737,408 B | -0.01% |
| maximum task RSS | 27,041,792 B | 27,082,752 B | +0.15% |

The Benchexec summary reports summed task CPU first and the four-job concurrent suite wall time
second. An earlier draft inverted those columns; the table above is the corrected attribution.
The 37.23-second completed-task CPU reduction closely matches the 38.04-second CAT-consistency
reduction, so there is no evidence of a compensating cost increase elsewhere.

The candidate also records the previously validated exact-cache boundary: 146,360 exact hits and
292,784 changed-query rebuilds. The large CAT-phase reduction but small end-to-end CPU reduction
shows that primitive construction is no longer the sole limiting component. The next measured
target is the complete offline fixed-point evaluator; no new searcher, SMT encoding, or
relation-specific revisit patch is justified by this result.

## Artifacts

- Paired raw result: `server-results/primitive-fast-paired-15-20260718a/`
- Broad table: `server-results/direct-fr-broad-20260718.tsv`
- Pre-change phase census: `server-results/primitive-cache-paired-15-co-phase-20260718a/`
