# P0.7e product-state EOG decision

Date: 2026-07-16  
Baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe` (P0.7d2)  
Decision: **reject P0.7e1, P0.7e2, and P0.7e3; restore the baseline**

## Intended optimization

P0.7e compiles an analyzer-certified positive CAT relation expression into an acyclic
epsilon-NFA, adds an exit-to-entry reset, and searches the exact product graph
`(control state,event)`. This shares a composition suffix reached from multiple source
events without materializing the root relation. The product-cycle/reset theorem and
projected witnesses are exact; unsupported expressions and oversized products use the
retained P0.7d2 evaluator.

This is a generic CAT/CAAT consistency algorithm. It never calls a built-in SC/TSO
checker, branches on a model/task name, or borrows a built-in verdict.

## P0.7e1: recursive product traversal

All correctness gates pass: Release 161/161, focused ASan+UBSan 7/7, mutation
39/5,441, and broad differential 852 matches + 12 mutually unsupported + 0 mismatch.

The two-repetition pilot rejects it:

- TSO fib CPU improves to 0.9107/0.9166.
- TSO queue CPU regresses to 1.2243/1.1761 and RSS to 1.3662/1.3651.
- PSO queue CPU regresses to 1.0316/1.0293.

The native product stack and exact deep-product fallback are too expensive on the
large queue product, so no formal matrix is run for e1.

## P0.7e2: resumable heap frames

P0.7e2 preserves the automaton semantics but replaces product recursion and base-edge
callbacks with explicit frames and `Relation::nextSuccessor` cursors. Correctness again
passes: Release 161/161, sanitizer 7/7, mutation 39/5,441, broad 852/12/0.

It removes the RSS regression but raises per-transition dispatch cost:

- TSO fib CPU is 1.2778/1.2802; one baseline correct result becomes a timeout.
- TSO queue CPU is 1.3298/1.3331.
- PSO queue CPU is 1.1666/1.1316.

The identical 1,776,716,915-transition TSO fib offline workload rises from about
25.65 seconds in e1 to 42.60 seconds in e2. P0.7e2 is rejected before formal testing.

## P0.7e3: 32 KiB cache-resident selector

P0.7e3 restores e1 traversal and admits a product only when its one-byte `Q*N` color
array fits 32 KiB. The rule is fixed before testing and is independent of model/task
identity. Larger products fall back exactly to P0.7d2.

Correctness passes:

- Release unit/property tests: 162/162.
- Focused ASan+UBSan tests: 8/8.
- Mutation oracle: 39 rows and 5,441 full recomputations, zero mismatch.
- Broad differential: 288 programs × SC/TSO/PSO; 852 matches, 12 mutually
  unsupported, zero mismatch.
- Formal: 1,152 cells per variant; 716 true, 360 false and 76 TIMEOUT in each; zero
  status, terminal-verdict, or log-derived execution-count mismatch.

The selector fixes the pilot: TSO fib CPU is 0.9110/0.9161, TSO queue
1.0133/1.0052, PSO queue 0.9827/0.9829, and all pilot RSS ratios are at most 1.003.

## P0.7e3 formal result

| Metric | SC | TSO | PSO | All |
|---|---:|---:|---:|---:|
| Common-terminal CPU | 0.98230 | 1.00955 | 1.00780 | 0.99930 |
| CPU task-bootstrap 95% CI | [0.97391, 0.99048] | [0.99639, 1.02336] | [0.99360, 1.02328] | [0.99216, 1.00667] |
| Common-terminal RSS | 0.99998 | 0.99999 | 1.00032 | 1.00009 |

The frozen retention gate requires the aggregate CPU upper confidence bound below
1.0. The observed upper bound is 1.00667, so P0.7e3 fails. TSO and PSO point estimates
also regress by about 0.96% and 0.78%. There is no coverage gain to provide an
independent reason to retain it.

Large-task snapshot-equivalent state is exactly 1.0. Large process RSS is 1.00152
overall; the PSO point is 1.01139. These pass the 1.02 point bound but do not compensate
for the failed CPU gate.

## Why macro-edge reduction did not become speedup

The candidate reduces root macro candidates from 5,694,936,384 to 2,349,253,524
(`-58.75%`), but executes 18,831,879,352 product transitions. Thus product control
transitions alone are 3.31 times the baseline macro-candidate count; candidates plus
product transitions are about 3.72 times that count. Sharing is real, but the NFA
introduces too many epsilon/control-state operations.

By stage:

1. **Analyzer/model state:** immutable automata add small shared metadata.
2. **Root relation production:** 58.75% fewer macro-candidate callbacks and no
   materialized root relation.
3. **Consistency traversal:** 18.83B product transitions dominate the saved work.
4. **Query-local memory:** the 32 KiB cap removes the e1 RSS failure; snapshot state is
   unchanged because colors do not survive a query.
5. **Rollback:** verdicts and execution counts remain exact; no product state is
   checkpointed.

The next attempt, if resumed, needs epsilon-closure compression or a specialized
transition program that fuses multiple automaton steps per relation edge. Another
threshold on the same interpreter is not justified by this experiment.

## Artifacts

- Protocols: `experiment-protocol.md`, `cache-selector-protocol.md`.
- e1/e2/e3 pilots: `server-results/pilot-e1`, `pilot-v2`, `pilot-v3`.
- Formal XML/logs: `server-results/formal-v3`.
- Formal analysis: `server-results/analysis-v3/comparison.json` and
  `product-analysis.json`.
- Correctness/build logs: `server-results/gates`.
- Local/remote formal SHA-256 audit: all 131 files match.

No P0.7e production or test code is retained or committed.
