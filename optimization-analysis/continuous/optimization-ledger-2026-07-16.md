# CAT/CAAT optimization ledger through P0.7e

Date: 2026-07-16  
Retained code baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe`  
State: paused after the second EOG exploration round

## Counting rule

- 24 implementation/performance rounds were completed.
- They cover 23 distinct optimization ideas because acyclic closure slicing was tested
  twice: Optimization 12 on the old packed baseline and P0.7a on the later CSR/sparse
  baseline.
- P0.1 rejection-kernel census, P0.3 opportunity census, P0.5 storage census,
  Optimization 02a instrumentation, and Optimization 05 profiling are measurement
  studies, not additional production optimization ideas.
- P0.7d1/d2 and P0.7e1/e2/e3 are refinements inside one optimization round each.

## Legend

- `+++`: statistically supported time improvement, large space reduction, or repeatable
  terminal-coverage gain; retained unless correctness/scope failed.
- `+`: positive but not statistically decisive on the aggregate metric.
- `0`: neutral or failed the frozen retention gate.
- `-`: measured regression.
- `X`: inadmissible research scope or runtime/correctness failure.

## Round-by-round result

| # | Optimization | Mark | Main measured effect | Decision |
|---:|---|:---:|---|---|
| 1 | Opt01 PSO-certified adaptive offline | `+++` | wall 0.937 [0.901,0.968], CPU 0.938, RSS 1.000; +5 solved cells | retained |
| 2 | Opt02b exact semantic snapshot cache | `0/-` | wall 0.9904 but median 1.0000; RSS 1.0066 | rejected |
| 3 | Opt03 incremental violation/cycle frontier | `---` | v1 wall/CPU 1.110/1.121; fallback screen wall 1.045 | rejected |
| 4 | Opt04 online/offline cost selector | `---` | controlled wall 1.063, CPU 1.035; one correct result lost | rejected |
| 5 | Opt05a moved base + small-history removal | `-` | CPU 1.00568 [1.00129,1.01023], RSS 1.00035 | rejected/split |
| 6 | Opt05b adaptive-offline history elision | `0` | CPU 1.00250 [0.99779,1.00734]; internal history reduced but RSS unchanged | rejected |
| 7 | Opt06 shared dependency adjacency | `--` | CPU 1.00879 [1.00416,1.01357], TSO 1.02149 | rejected |
| 8 | Opt07 bounded ring worklist | `0` | CPU 1.00121 [0.99548,1.00682], RSS 0.99999 | rejected |
| 9 | Opt08 scalar direct `fr` construction | `--` | CPU 1.00835 [1.00164,1.01510], wall 1.01984 | rejected |
| 10 | Opt09 packed functional `fr` | `0/+` | all CPU 0.99996; SC 0.98671 significant, PSO point 1.00590 | rejected |
| 11 | Opt10 built-in certified checker bypass | `X` | correctness passed, but bypassed generic CAT/CAAT consistency | rejected as inadmissible |
| 12 | Opt11 generic closure-check lowering | `+++` | CPU 0.9901 [0.9832,0.9959], wall 0.9896; +15 correct cells; offline time 0.624 | retained |
| 13 | Opt12 old-baseline closure slicing | `+ / 0` | CPU 0.99584 [0.98957,1.00131], +9 correct cells, offline 0.905 | rejected by old gate |
| 14 | P0.2 raw-positive rejection-kernel cache | `---/+` | CPU 1.04343, PSO 1.13116, RSS 1.01808; net +4 correct FALSE but +8 OOM | rejected |
| 15 | P0.3 exact PSO preventive-order pruning | `+` | CPU 0.95369 [0.87527,1.04274]; five repeatable TIMEOUT-to-terminal gains | retained opt-in |
| 16 | P0.4 preventive-history suppression | `0` | internal state 0.2061 and history search 0.1658, but CPU 0.99528 CI crosses 1 and RSS 0.99992 | rejected |
| 17 | P0.6a grouped packed primitive construction | `+++` | large materialization 0.2596 [0.2049,0.3302]; +7 correct terminal cells; CPU neutral | retained |
| 18 | P0.6b exact structural primitive views | `++/-` | large RSS 0.87544, base 0.70217; CPU 1.00337 and missed standalone 0.5 base gate | rejected standalone; absorbed into P0.6c |
| 19 | P0.6c adaptive sparse edge primitives | `+++` | large base 0.14139, RSS 0.61512, materialization 0.79688; CPU 1.00209 neutral | retained combined design |
| 20 | P0.7a closure slicing on CSR baseline | `+++` | +22 correct cells; large offline 0.48197, RSS 0.95984; CPU 0.99575 CI crosses 1 | retained by coverage gate |
| 21 | P0.7b certified lazy cycle EOG | `+++` | CPU 0.97950 [0.95888,0.99714]; large snapshot 0.57732, RSS 0.66807 | retained |
| 22 | P0.7c per-plan selector | `+ / ---` | CPU 0.99326 [0.98708,0.99937], but large RSS 1.11401 and snapshot 1.20292 | rejected |
| 23 | P0.7d streaming lazy traversal | `X -> +++` | d1 caused 20 segfault cells; bounded d2 CPU 0.98593 [0.97878,0.99306], RSS 0.97676 | d1 rejected; d2 retained |
| 24 | P0.7e product-state EOG | `0/---` | e3 CPU 0.99930 [0.99216,1.00667], no coverage gain; e1/e2 large regressions | rejected |

## Current retained mechanisms

Nine mechanisms are present in the retained code or retained opt-in path:

1. PSO-certified adaptive offline evaluation.
2. Generic linear-recursion closure lowering.
3. Exact PSO preventive-order pruning, opt-in only.
4. Grouped packed primitive materialization.
5. Exact structural primitive views, retained as part of the combined sparse design.
6. Size-adaptive sparse edge primitives.
7. Cycle-only closure slicing.
8. Certified lazy cycle-EOG evaluation for composition-heavy cones.
9. Depth-bounded streaming lazy traversal with exact iterative fallback.

## End-to-end cumulative effect

The authoritative cumulative comparison is the previous corrected-census CAAT-SC
binary versus the latest P0.7d2 CAAT-SC binary on the same 725 adapted C.Concurrency
tasks, with 60 CPU seconds, 4 GiB and one core per task. It must be preferred over
multiplying per-round ratios because the round matrices use changing baselines and
partly different common-terminal task sets.

| Metric | Previous | Latest | Cumulative change |
|---|---:|---:|---:|
| Correct tasks | 388 | 398 | +10, or +2.58% |
| All terminal true/false results | 418 | 428 | +10, or +2.39% |
| TIMEOUT | 224 | 240 | +16, or +7.14% |
| OOM | 57 | 31 | -26, or -45.61% |
| TIMEOUT + OOM | 281 | 271 | -10, or -3.56% |
| Other unknown/error | 26 | 26 | unchanged |

For the 418 tasks on which both binaries terminate with the same verdict:

- CPU ratio is 0.915446 [0.894591, 0.934561]: an 8.46% geometric-mean reduction,
  with a bootstrap-supported reduction of about 6.54%--10.54%.
- Wall ratio is 0.985597 [0.949612, 1.02079]: a 1.44% point reduction, but the
  confidence interval crosses 1, so no aggregate wall-time improvement is established.
- RSS ratio is 0.994343 [0.986828, 1.00024]: a 0.57% point reduction, also not
  statistically decisive at the frozen interval.
- No old terminal verdict changes. The ten new terminal results are five old OOM and
  five old TIMEOUT tasks; another 21 old OOM tasks become TIMEOUT. Therefore the 45.61%
  OOM reduction is real memory-pressure relief, but most of it is not yet solved
  coverage.

Large-event mechanism experiments show much larger localized savings than the 725-task
aggregate RSS number:

- P0.6c versus its P0.6a baseline: large base storage 0.14139 (-85.86%), process RSS
  0.61512 (-38.49%), and materialization time 0.79688 (-20.31%).
- P0.7b versus P0.7a: large snapshot-equivalent state 0.57732 (-42.27%) and process RSS
  0.66807 (-33.19%).
- P0.7d2 versus P0.7b: aggregate CPU 0.98593 (-1.41%) and large RSS 0.97676 (-2.32%).

These large-task ratios are stage-local results and must not be multiplied into one
claimed global memory ratio without a fresh oldest-versus-latest formal matrix.

## Highest-value untried directions

1. **Compiled epsilon-closure/fused transition program.** Replace P0.7e's interpreted
   NFA epsilon/control steps with precomputed epsilon closures and fused base-edge
   transitions. P0.7e reduced macro candidates by 58.75%, but 18.83 billion product
   transitions erased the gain; another size threshold on the same interpreter is not
   justified.
2. **Rollback-safe incremental topological/EOG state.** Maintain exact order/cycle
   certificates across insert, rollback and RF/CO replacement instead of rebuilding a
   per-query traversal. The difficult unimplemented part is deletion/rollback repair,
   not cycle detection itself.
3. **Exploration-level abstraction and refinement.** EOG/CEGAR, property-directed event
   abstraction, and counterexample-guided graph refinement have been studied but not
   integrated into GenMC's execution-class exploration. Current retained changes mostly
   reduce the consistency cost of one candidate graph; they do not substantially reduce
   the number or size of candidate executions.
4. **Generic candidate pruning beyond exact PSO.** P0.3 proves preventive pruning can
   add coverage, but the implementation is limited to one certified recursive-PSO
   fingerprint. Sound generic CAT-derived rejection certificates and subsumption remain
   unimplemented; P0.2 shows that a naive snapshot kernel cache is too expensive.
5. **Delta checkpoint/undo representation.** P0.4 removed unused history but did not
   replace full evaluator snapshots with compact relation deltas or persistent
   copy-on-write blocks. This remains a direct target for large online CAAT states.
6. **Search-level equivalence/POR integration.** TruSt-family/Awamoche/Mixer/Spore-style
   reduction of explored executions has not been combined with the generic CAT/CAAT
   backend. It potentially addresses TIMEOUT more directly than another evaluator
   micro-optimization, but requires a separate completeness proof for the admitted CAT
   fragment.
7. **Cross-worker sharing and checker-internal parallelism.** Task-level 32--48-way
   BenchExec parallelism and GenMC exploration threads were measured, but no exact
   shared immutable plan cache, duplicate-state table, or parallel consistency kernel
   has been implemented across workers.

## Pause decision

Do not start P0.7f automatically. If work resumes, the first candidate should be item 1
with a pre-implementation transition-count model and a pilot that rejects any design
whose fused-transition count is not below the retained P0.7d2 macro-candidate count.
