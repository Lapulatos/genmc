# Incremental/online CAAT integration: Phase 3 report

## Outcome

Phase 3 is complete for the positive normalized CAT/CAAT fragment declared in
`phase-3-plan.md`. Each `BasicCATChecker` worker owns persistent relational
state, incrementally propagates insertions, restores retained semantic
predecessors on backtracking, and safely rebuilds on mixed mutations. Positive
violations may reject a growing prefix immediately. The Phase 2 evaluator
remains both the unsupported-mutation fallback and an opt-in runtime oracle.

This is not arbitrary herd CAT, general incremental deletion, dynamic
difference, or a CDCL(T) integration. Functions, procedures, scopes, candidate
generation, negative recursion and difference affecting a checked online
predicate remain outside the supported boundary.

## Implemented path

```text
ExecutionGraph candidate/prefix
  -> immutable GraphAdapter snapshot
  -> persistent Event/SAddr stable IDs
  -> initialize / insert / rollback / rollback-insert / rebuild classification
  -> worker-local IncrementalCaatEvaluator
  -> positive recursive worklist to quiescence
  -> current empty / irreflexive / acyclic witnesses
  -> consistency verdict and optional Reasoner explanation
  -> optional fresh Phase 2 predicate-by-predicate oracle
```

Packed sets and relations grow across word boundaries without renumbering old
events. Checkpoints exactly retain primitive and derived values, violations and
witnesses. The synchronizer bounds retained checkpoints, compares every base
predicate, and never treats an `rf`/`co` replacement or deletion as insertion.

## Safety argument and unsupported boundary

For a positive normalized model, every base insertion can only add derived
memberships. A witnessed non-empty predicate, self-loop, or directed cycle
therefore remains a violation until rollback. GenMC may reject that extension
without losing a later consistent execution. Rollback restores an exact state;
mixed mutations rebuild from the Phase 2 oracle.

Difference is antitone in its right operand. A later RHS insertion may remove a
fact and repair a prefix, so offline evaluation followed by prefix rejection is
not a safe fallback. Recursive or forward-reference difference that reaches a
check is rejected before LLVM execution with a source-located diagnostic.

## Broad correctness evidence

The frozen Phase 2 corpus was rerun through the production online checker. It
contains 288 distinct programs selected from correct litmus/infrastructure/data
structures and wrong safety/racy/memory tests. Each program is checked under
recursive SC, TSO and PSO against its non-recursive CAT baseline.

| Measurement | Result |
|---|---:|
| distinct programs | 288 |
| recursive program/model rows | 864 |
| matching status, verdict, warnings and execution counts | 864 |
| unexplained mismatch | 0 |
| unsupported row | 0 |
| actual baseline/online GenMC invocations | 1,728 |

Machine-readable evidence is in `phase-3-online-broad-results.tsv`, including
source hashes, arguments, signatures and per-row online counters. Aggregate
transitions were:

| Counter | Total |
|---|---:|
| initialize | 780 |
| unchanged | 1,491 |
| insertion | 128,801 |
| exact rollback | 0 |
| rollback plus insertion | 620,887 |
| safe rebuild | 364,023 |
| checkpoint eviction | 0 |
| predicate evaluations | 31,219,757 |
| value changes | 27,936,189 |
| worklist pushes | 31,219,757 |
| offline initializations/rebuilds | 364,803 |

Some rows terminate before asking the CAT checker, which explains why
initializations are fewer than corpus rows. Exact rollback is covered by graph
and randomized push/pop tests; real GenMC exploration predominantly returns to
an ancestor and immediately adds a different fact, producing rollback-insert.

## Mutation and runtime-oracle evidence

`online-mutation-stress.sh` adds 39 deterministic rows over SC/TSO/PSO,
one/two workers and two fixed randomized schedules. The programs exercise RMW,
create/join, alternative `rf`/`co`, dynamic allocation, dynamic MS queue,
dynamic Treiber stack, asynchronous flat combining and a malloc-order error.
Every query runs `--cat-oracle`; 5,396 complete Phase 2 comparisons produced
zero predicate, diagnostic, violation or witness mismatch.

`--cat-oracle` is default-off. On mismatch it prints a deterministic query
number, named graph transition, universe size and first differing predicate or
diagnostic, then raises an invariant failure. `--cat-stats` reports graph and
worklist counters per worker; output is serialized to keep parallel records
parseable.

## Performance and memory

`online-performance.sh` compares the current online binary with a separately
built detached Phase 2/3.4 commit (`196d370`). Both are Apple arm64
RelWithDebInfo builds with the same LLVM 20 toolchain and run identical
recursive models, programs and flags. The 54 end-to-end samples cover SC/TSO/
PSO, SB, dynamic Treiber stack and fcombiner, three repetitions per cell.

| Backend | Runs | Mean end-to-end time | Maximum peak RSS |
|---|---:|---:|---:|
| Phase 2 from-scratch | 27 | 0.0700 s | 54,525,952 bytes |
| Phase 3 online | 27 | 0.2004 s | 54,493,184 bytes |

Execution counts match in all 27 paired cells. Peak RSS is effectively
unchanged at this scale. Mean latency is 2.86x higher, dominated by recursive
SC fcombiner (1.2933 s online versus 0.2200 s offline). The current checkpoint
representation copies complete predicate state and rebuild-heavy branches can
therefore outweigh insertion savings. This is an explicit performance limit,
not a correctness gap or a claimed speedup. The detailed 54 rows are stored in
`phase-3-performance.tsv`.

The unchanged Phase 2 microbenchmarks remain the parser/relation baseline:
17,289 model parses/s, 2.166 ms for packed relation operations at 64--512 events,
and 2.974 ms for the recursive 32/64/128-event chain. Phase 3's aggregate
worklist counters above provide the corresponding online operation evidence.

## Verification and environment boundaries

- Complete unit/property tests: 138/138 passed.
- Parallel CAT/CAAT/config/CLI/integration selection: 104/104 passed.
- ASan+UBSan focused checker/incremental/rollback/synchronizer tests: 15/15.
- Frozen online broad corpus: 864/864 match, zero unsupported.
- Mutation oracle: 5,396 comparisons, zero divergence.
- Formatter, shell syntax and `git diff --check`: passed.

The standalone Homebrew `clang-tidy` invocation still cannot locate the Apple
libc++ standard headers, as recorded in earlier phases. A full sanitized GenMC
binary also aborts in existing LLVM interpreter/`DepTracker` code before CAT
checking, while the sanitizer unit target covers all changed CAAT components.
TSO-hosted fcombiner with two workers hits the existing replay scheduler
assertion at `Scheduler.cpp:85`; fcombiner remains covered under every model
with one worker and five other mutation fixtures cover two workers.

## Requirement audit

- Phase 3.0--3.7 each has a precheck, reuse decision, contract, verification and
  gap record in `progress/phase-3.md`.
- Every positive normalized operator has staged and randomized insertion-oracle
  coverage.
- Checkpoint rollback restores values, violations and explanations exactly.
- Stable event/address IDs and full-base comparison classify all declared graph
  mutations or select an observable safe rebuild.
- Early pruning is restricted to the positive monotonicity certificate and has
  checker-level plus 864-row execution evidence.
- The final online corpus has 864 rows with zero mismatch and zero unsupported.
- Real programs produce insert and rollback-insert transitions; aggregate
  counters are stored with each result row.
- Phase 1/2 parsing, evaluation, explanation and differential tests pass.
- Runtime, peak memory, rebuild rate and the full-copy checkpoint limitation are
  recorded without claiming a performance win.
- User and developer documents state the exact supported semantics and
  non-goals.

No open Phase 3 correctness deliverable remains for the declared fragment.
Future work may replace full snapshots with support-aware timestamps, reduce
mixed-mutation rebuilds, or broaden CAT syntax, but those are new optimization
or language phases rather than incomplete Phase 3 requirements.
