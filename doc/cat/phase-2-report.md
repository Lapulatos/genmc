# Offline CAAT backend: Phase 2 report

## Outcome

Phase 2 is complete for the declared offline CAAT fragment.  GenMC can parse
forward references and `let rec ... and ...` groups, normalize them into typed
one-operation equations, reject inadmissible dependencies with source
locations, compute stratified least fixed points over complete graph snapshots,
and explain `empty`, `irreflexive`, and `acyclic` violations with base facts.

This is not arbitrary CAT support and it is not the incremental CAAT solver
planned for Phase 3.  The command-line checker recomputes from scratch for each
candidate graph.  Functions, procedures, candidate generation, scopes, negative
recursion, non-domain-independent axioms, and non-semi-positive difference are
outside this phase.  A recursive model containing difference is also rejected
at GenMC's growing-prefix boundary even when the offline equation is
semi-positive, because a negative base fact is not prefix-monotone.

## Implemented path

```text
CAT source
  -> recursive parser and source spans
  -> stable typed one-operation predicates
  -> signed dependencies and deterministic SCC strata
  -> domain-independence / stratifiability / semi-positivity gates
  -> from-scratch delta-worklist least fixed point
  -> empty / irreflexive / acyclic checks
  -> optional shortest base-literal explanation
  -> GenMC consistency verdict
```

Ordinary bundled `sc.cat`, `tso.cat`, and `pso.cat` retain the Phase 1 DAG and
evaluator.  Declared recursion or an acyclic forward reference selects the
normalized backend.  The choice is immutable model metadata; filenames and
model names do not participate in dispatch.

## Semantic and diagnostic evidence

| Requirement | Evidence | Result |
|---|---|---|
| one-operation normalization | golden mutual-recursion and projection/product tests | stable IDs, typed operands, source provenance |
| stratified recursion | deterministic signed Tarjan SCC tests | positive mutual recursion accepted; negative recursion rejected |
| domain independence | guarded and unguarded recursive fixtures | source-located acceptance/rejection agrees with syntactic analysis |
| semi-positivity | derived difference-right operand fixture | diagnostic names the predicate requiring a cut |
| least fixed point | recursive closure, mutual, empty, set-projection tests | exact expected memberships |
| independent oracle | RapidCheck random four-event reachability | worklist equals naive Kleene recurrence |
| explanations | direct base-literal replay fixtures | all three axiom kinds replay; stale explanations rejected |
| worker independence | 3 models, 8 programs, workers 1 and 2 | 48/48 recursive results equal Phase 1 |

The explanation reasoner emits deterministic shortest-known positive base
literals and, for offline semi-positive difference, negative base literals.
The replay tests reconstruct the violation with direct relation algebra rather
than trusting the reasoner that produced the explanation.

## Broad differential validation

`tests/cat/recursive-broad-differential.sh` freezes the same six corpus roots as
Phase 1.  It holds the program and flags constant, runs each ordinary model and
its recursive counterpart, and compares exit status plus externally observable
verdict, error/warning class, blocked/complete execution counts, and bounds.

| Measurement | Result |
|---|---:|
| distinct programs discovered | 288 |
| correct-program model pairs | 696 |
| wrong-program model pairs | 168 |
| total recursive program/model pairs | 864 |
| matching pairs | 864 |
| unexplained mismatches | 0 |
| unsupported before verification | 0 |
| actual GenMC invocations | 1,728 |

The final rerun repairs `psc-base-notin-ar0.c` to use GenMC's `<genmc.h>`
interface instead of declaring `__VERIFIER_assume` as an external function.
Ordinary and recursive SC, TSO, and PSO each complete one safe execution, so all
three formerly unsupported rows are now valid matches. Machine-readable results,
including source hashes and exact arguments, are stored in
`doc/cat/phase-2-broad-results.tsv`.

The same frozen Phase 1 corpus selection was rerun: 288 programs, 576 model pairs,
576 valid matches, zero mismatch, and zero unsupported rows.
Thus the new recursive path introduced no observed Phase 1 regression.

## External tool evidence

- herdtools7 7.56+03 passes the repository's aligned TSO SB/MP oracle and PSO
  MP oracle.  These cases compare matching plain memory-event meanings; herd
  source is not copied or linked into GenMC.
- Dat3M revision `a7e3e4843359dde3a0e29500a821030e2433e316` was built from
  source with Java 17 and Maven.  The build compiled 699 production and 159
  test source files and produced `dartagnan.jar` in 60 seconds with tests
  skipped.  Dat3M informed the predicate/worklist/reasoner design, but no
  aggregate GenMC-versus-Dat3M verdict is claimed: their C/LLVM transformations
  and base-event mappings are not identical enough for a sound row-by-row
  oracle in this corpus.  The independent Kleene oracle and aligned herd cases
  cover the executable semantic comparisons available in Phase 2.

## Performance and memory baseline

Measurements used the existing Apple arm64 RelWithDebInfo build.  They are
smoke baselines for later Phase 3 comparison, not scaling claims.

| Measurement | Phase 2 result | Comparison |
|---|---:|---|
| parser, 300 PSO parses | 17,289 files/s | Phase 1 recorded about 20,631 files/s; both are startup-free microbenchmarks |
| relation operations, 64--512 events | 2.166 ms, 85 KiB packed input | Phase 1 recorded 2.09 ms; no material change |
| recursive chain fixed points, 32/64/128 events | 2.974 ms | new Phase 2 baseline |
| fixed-point work | 442 evaluations, 439 value changes, 442 pushes | deterministic convergence counters |
| fixed-point result storage | 2,816 packed bytes | three final reachability relations |
| SC SB, 20 invocations | ordinary 1.00 s; recursive 0.96 s | no measured recursive penalty above run noise |
| TSO SB, 20 invocations | ordinary/recursive 0.97/0.97 s | equal at 10 ms resolution |
| PSO SB, 20 invocations | ordinary/recursive 0.97/0.97 s | equal at 10 ms resolution |
| peak RSS for the six 20-run batches | 52.63--52.69 MB | maximum spread below 0.1 MB |

Process startup, source compilation, and LLVM transformation dominate the SB
measurements.  The reproducible GoogleTest benchmark records algorithmic
counters so later optimization is not judged from wall time alone.

## Verification commands

The closing run used:

```bash
cmake --build RelWithDebInfo -j4
./RelWithDebInfo/bin/unit_tests
ctest --test-dir RelWithDebInfo --output-on-failure -R '^(cat-|fast-driver$)'
bash tests/cat/recursive-differential.sh \
  "$PWD/RelWithDebInfo/bin/genmc" "$PWD/models/cat" "$PWD/tests"
bash tests/cat/recursive-broad-differential.sh \
  "$PWD/RelWithDebInfo/bin/genmc" "$PWD/models/cat" "$PWD/tests" \
  "$PWD/doc/cat/phase-2-broad-results.tsv" 4
bash tests/cat/broad-differential.sh \
  "$PWD/RelWithDebInfo/bin/genmc" "$PWD/models/cat" "$PWD/tests" \
  "$PWD/doc/cat/phase-1-broad-results.tsv" 4
bash tests/cat/herd-tso-oracle.sh "$(opam exec -- which herd7)" \
  "$PWD/tests/cat/herd"
bash tests/cat/herd-pso-oracle.sh "$(opam exec -- which herd7)" \
  "$PWD/tests/cat/herd/MP.litmus"
```

Results: 120/120 unit/property tests, fast-driver, three Phase 1 CAT CTests,
the 48-case focused recursive comparison, both herd scripts, and both broad
suites passed.  `fast-driver` took 78.14 seconds.  Shell syntax, changed-line
formatting, and `git diff --check` are part of the final audit.

The standalone `clang-tidy` environment still cannot resolve the configured
Apple libc++ headers, as recorded in Phase 1 and Phase 2.4.  The normal LLVM 20
CMake build succeeds; no clean lint result is claimed.

## Requirement audit and Phase 3 boundary

Every Phase 2 completion item has direct evidence: substages 2.0--2.6 have
progress records; normalized recursion matches an independent least-fixed-point
oracle; admissibility diagnostics retain source locations; all axiom explanation
kinds replay; 864 broad pairs have zero unexplained mismatch; the Phase 1 suite
remains exact; and time/memory measurements plus public support limits are
recorded.

No open Phase 2 P0 correctness gap remains for the declared fragment.  The
following are intentionally Phase 3 or later:

- persistent predicate state across graph additions;
- push/pop, backtracking, deletion invalidation, and trail-valid explanations;
- sound online handling of negative literals/difference;
- proof-backed early pruning and performance-specialized propagation;
- broader herd CAT functions, procedures, scopes, and candidate generation.
