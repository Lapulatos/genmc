# SV-COMP 2026 combined experiment analysis

## Scope

- Full Stage-1 inventory: 3,520 property rows from 1,060 YAML files. Dynamic
  10 s censuses cover 725 `unreach-call` and 1,030 `no-data-race` rows; the
  other 1,765 property rows are explicitly `property_adapter_unsupported`.
- Formal paired performance: 96 `unreach-call` tasks, five repetitions, 60 s,
  4 GB, one core. SC and TSO compare GenMC, GenMC+CAT, and GenMC+CAAT. PSO
  compares CAT and CAAT because native GenMC has no PSO backend.
- Scale diagnostics: the same 96 tasks, one separate `--cat-stats` repetition;
  diagnostic timing is not reused as performance evidence.
- External compatibility: 725 `unreach-call` tasks, 10 s, 4 GB, one core.
  Deagle and CBMC use ILP32. TruSt, Awamoche, Mixer, and Spore use RC11/LP64
  and form a separate semantic comparison family.

## Main paired-performance findings

| Model | Contrast | Common tasks | Wall-time ratio | 95% task-bootstrap CI | Memory ratio | Coverage evidence |
|---|---:|---:|---:|---:|---:|---|
| SC | CAAT / GenMC | 90 | 1.289x | 1.120–1.520 | 1.062x | 450/480 vs 480/480 solved runs |
| SC | CAT / GenMC | 85 | 1.387x | 1.141–1.742 | 1.076x | 425/480 vs 480/480 |
| SC | CAAT / CAT | 85 | 0.854x | 0.753–0.949 | 0.990x | 450/480 vs 425/480 |
| TSO | CAAT / GenMC | 89 | 1.350x | 1.143–1.631 | 1.085x | 445/480 vs 480/480 |
| TSO | CAT / GenMC | 82 | 1.308x | 1.096–1.630 | 1.047x | 410/480 vs 480/480 |
| TSO | CAAT / CAT | 82 | 0.855x | 0.745–0.952 | 0.996x | 445/480 vs 410/480 |
| PSO | CAAT / CAT | 79 | 1.075x | 1.028–1.132 | 1.001x | 395/480 vs 410/480 |

The ratios use only tasks solved correctly by both methods in all five
repetitions. Coverage is co-primary evidence: failures are not converted into
made-up times. Exploration-equivalence checks found zero verdict, complete
execution, or blocked-execution mismatches in 425 SC, 410 TSO, and 395 PSO
common solved task-repetition cells.

The strongest supported practical conclusion is model-dependent. CAAT is
slower than native GenMC overall, but improves over flat CAT on the common SC
and TSO tasks and solves more repeated runs. Under PSO, CAAT is 7.5% slower
than CAT on the common fully solved tasks and completes 15 fewer repeated runs.
The PSO sign-test result is not significant after the planned correction
(`p=0.1147`), so the directional claim should remain descriptive.

## Program scale and relation density

The separate diagnostic pass provides complete graph counters for 83 SC, 80
TSO, and 77 PSO task cells that also have complete formal CAT/CAAT timings.
CAT does not emit the incremental counters, so these measurements describe
CAAT's workload only.

| Model | Stable-events Spearman rho | 95% CI | Highest stable-event quartile CAAT/CAT wall ratio | Highest density quartile ratio |
|---|---:|---:|---:|---:|
| SC | -0.488 | -0.663–-0.278 | 0.555x | 0.759x |
| TSO | -0.441 | -0.632–-0.212 | 0.573x | 0.783x |
| PSO | +0.451 | +0.229–+0.632 | 1.297x | 1.199x |

SC and TSO therefore show the intended crossover: CAAT's relative advantage
over flat CAT grows with the stable event domain. PSO shows the opposite
association. In PSO, `profiled_queries` has rho 0.483 (95% CI 0.267–0.660)
with the CAAT/CAT wall ratio, making recursive-PSO query/rebuild work the first
profiling target. These associations do not identify a causal code path by
themselves.

## Memory behavior

On fully paired tasks, CAT and CAAT generally have similar peak RSS, while both
add memory over native GenMC. The geometric-mean CAAT/CAT ratios are 0.990x
(SC), 0.996x (TSO), and 1.001x (PSO). The aggregate hides large individual
cases: `pthread/queue_ok_longest.yml` uses a median 970.1 MB with CAT-SC and
613.3 MB with CAAT-SC; `pthread/queue_ok_longer.yml` uses 480.7 MB with CAT-TSO
and 313.3 MB with CAAT-TSO. On the small
`13-privatized_66-mine-W-init_true.yml` example, SC medians are 25.829 MB
(GenMC), 25.936 MB (CAT), and 25.944 MB (CAAT), so fixed process/runtime cost
dominates.

## External-tool compatibility

| Tool | Semantics / data model | Correct | Wrong | Unknown | Error/resource | Completed true/false | Median completed wall time | Median completed RSS |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Deagle 4.1.0 | SV-COMP / ILP32 | 596 | 2 | 20 | 107 | 598 | 0.214 s | 12.82 MB |
| CBMC 5.70 SV-COMP archive | SV-COMP / ILP32 | 389 | 25 | 0 | 311 | 414 | 0.379 s | 17.25 MB |
| TruSt 0.5.3 | RC11 / LP64 | 78 | 14 | 0 | 633 | 92 | 0.038 s | 9.69 MB |
| Awamoche 0.8 | RC11 / LP64 | 84 | 11 | 0 | 630 | 95 | 0.081 s | 10.13 MB |
| Mixer 0.10.1 | RC11 / LP64 | 85 | 15 | 0 | 625 | 100 | 0.152 s | 24.19 MB |
| Spore 0.10.1 | RC11 / LP64 | 90 | 15 | 0 | 620 | 105 | 0.154 s | 24.20 MB |

These are compatibility-census results, not a fair speed ranking. Deagle and
CBMC share ILP32 with the SV-COMP tasks but explore different verification
algorithms. The TruSt family uses LP64 and RC11, so it must not be ranked
directly against the ILP32 SC/TSO/PSO methods.

## Optimization priorities supported by the data

1. Profile recursive PSO first, separating predicate worklist, rebuild, and
   relation-update time. Its overhead grows with stable events, relation
   density, rebuilds, and query count, unlike SC/TSO.
2. Preserve the recursive SC/TSO path for larger event domains. The largest
   quartile is where CAAT shows its strongest advantage over flat CAT.
3. Add a cheap small-domain dispatch. At zero or very small diagnostic domains,
   CAAT/CAT ratios are near 1 and fixed setup cost dominates.
4. Replace the fixed history-count policy with a byte budget and record
   checkpoint hits. `max_history_base_bytes` currently tracks current-base
   growth and cannot yet separate useful history from copied state.
5. Keep compatibility and performance runs separate. Unsupported frontends,
   compiler errors, timeouts, and OOMs must not enter solved-only timing ratios.

## Claim boundary

Allowed: “On the selected compatible tasks, CAAT reduced CAT wall time for SC
and TSO, while the current PSO implementation showed the opposite scaling
trend.”

Not allowed: “CAAT is universally faster,” “Deagle is the fastest tool,” or
“relation density causes the PSO slowdown.” The experiment does not support
those claims.
