# CAT model-file support: Phase 1 report

## Outcome

Phase 1 is complete for its declared scope. GenMC accepts
`--model-file=<model.cat>`, compiles the supported CAT subset into an immutable
typed relational IR, maps an execution graph into CAT primitives, and evaluates
SC, TSO, and PSO consistency through one generic C++23 evaluator. SC and TSO
remain differentially checked against GenMC's generated checkers; PSO is a new
model-driven result with no PSO-specific production checker or filename switch.

This is not a claim to accept arbitrary valid CAT. Unsupported syntax, missing
graph primitives, non-recursive Phase 1 bindings, and online-inadmissible checks
fail before exploration with source-located diagnostics.

## Runtime path

The implemented path is:

```text
model.cat -> lexer/parser/includes -> typed DAG -> online-admissibility gate
          -> Config/host profile -> ExecutionGraph snapshot -> pure evaluator
          -> BasicCATChecker<SCChecker|TSOChecker> -> GenMC exploration result
```

The host profile is explicit model metadata. It selects generated host views,
language-error checks, and transformation support; it does not select the CAT
consistency predicate. Candidate read-from/coherence enumeration is conservative
and the loaded CAT equations perform the consistency filtering.

## Compatibility matrix

| Area | Phase 1 support | Boundary |
|---|---|---|
| CLI | `--model-file=<path>`; canonical path; duplicate/conflict diagnostics | cannot combine with an explicit built-in model or Relinche options |
| Files | UTF-8, nested block comments, relative/absolute includes, cycle diagnostics | include search is deterministic and local; no system herd path |
| Bindings | sequential, non-recursive `let`; typed set/relation symbols | recursive bindings/functions are Phase 2 |
| Algebra | union, intersection, difference, sequence, inverse, optional, transitive and reflexive-transitive closure | difference may be evaluated offline but cannot reach an online check |
| Checks | `acyclic`, `irreflexive`, `empty` with structured evaluator witnesses | CLI currently reports GenMC's consistency result, not the full evaluator witness |
| Sets | `_`, `R`, `W`, `F`, `IW`, `SC`, `M` | only event classes represented by GenMC labels are available |
| Relations | `po`, `rf`, `co`, `fr`, `rmw`, `loc`, `int`, `ext`, `tc`, `tj`, plus documented aliases | no general CAT candidate-extension/scopes/classes machinery |
| Host profiles | `sc` and `tso`; SC is the documented default | profiles are explicit metadata, never inferred from a filename/model name |
| Models | bundled SC, TSO, PSO | PSO deliberately reuses the TSO host and differs only through CAT equations |

The full syntax and primitive definitions are maintained in
`doc/cat/supported-cat.md`. The online gate walks backward from checks and
rejects a reachable difference. This conservative rule is necessary because
`A \ B` may shrink when a prefix gains members of `B`; treating a transient
violation as final could otherwise prune a later consistent execution.

## Differential and oracle evidence

The broad post-closure validation adds 287 distinct programs and 574 valid
SC/TSO source/model comparisons, with zero built-in/CAT mismatch after three
discovered defects were fixed. The detailed report is
`doc/cat/phase-1-broad-validation-report.md`; machine-readable evidence is in
`doc/cat/phase-1-broad-results.tsv`.

| Target | Evidence | Result |
|---|---|---|
| SC CAT vs built-in SC | SB, LB+ctrl, WWR+2WR, safety error, two workers | equal status, verdict/error marker, and complete-execution count |
| TSO CAT vs built-in TSO | seven C/LLVM fixtures including SB, MP, po-loc, RMW, SC fence, blocking, safety error | equal status, verdict/error marker, and complete-execution count |
| herd x86 TSO | assembly-aligned SB and MP | SB `Sometimes 1 3`; MP anomaly `Never 0 3` |
| model-only SC/TSO/PSO | fixed `WW+RR.c`, fixed flags, only model path changes | SC safe/3, TSO safe/3, PSO safety error/2 |
| herd PSO ordering oracle | same plain-R/W MP structure, official TSO and PSO-ppo models | TSO `Never 0 3`; PSO `Sometimes 1 3` |
| PSO preserved constraints | po-loc, RMWFix, SB+SC fences | TSO and PSO both report 3, 4, and 3 executions respectively |

The herd comparisons are semantic oracles, not copied implementation. herdtools7
7.56+03 is CeCILL-B and runs externally; the GenMC runtime contains no herd or
OCaml dependency.

## Performance baseline

These measurements are correctness-phase smoke baselines on the development
Apple arm64 machine. Process/LLVM startup dominates the end-to-end samples.

| Measurement | Recorded result |
|---|---|
| parser, 300 bundled-model parses | about 20,631 files/s; 6.75 MiB test-process peak RSS |
| relation operations, 64-512 events | about 2.09 ms; 85 KiB packed input; 6.67 MiB peak RSS |
| graph adapter, 64-512 events | about 4.93 ms; 511 KiB materialized primitives; 7.47 MiB peak RSS |
| SC SB, ten warmed runs | built-in and CAT both 0.48 s; about 50.2 MiB peak RSS |
| TSO SB, ten warmed runs | both 0.48 s; 52,625,408 vs 52,641,792-byte peak RSS |
| TSO/PSO WW+RR, ten warmed runs | 0.50/0.48 s; 52,576,256/52,609,024-byte peak RSS |

Phase 1 deliberately rebuilds a dense graph snapshot and evaluates the model
from scratch on each consistency query. These figures do not establish scaling
for large executions; they establish a reproducible reference implementation
for Phase 2 and Phase 3.

## Reproducible verification

The Phase 1.9 closure used a clean `RelWithDebInfo` build:

```bash
cmake -S . -B Phase1Audit \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_EXE_LINKER_FLAGS=-L/opt/homebrew/opt/hwloc/lib \
  -DFETCHCONTENT_SOURCE_DIR_RAPIDCHECK="$PWD/RelWithDebInfo/_deps/rapidcheck-src"
cmake --build Phase1Audit -j4
ctest --test-dir Phase1Audit -R 'Test\.|PropertyTest\.' --output-on-failure
ctest --test-dir Phase1Audit \
  -R '^(cli-model-file|cat-sc-differential|cat-tso-differential|cat-pso-model-proof)$' \
  --output-on-failure
ctest --test-dir Phase1Audit -R '^fast-driver$' --output-on-failure
opam exec -- bash tests/cat/herd-tso-oracle.sh herd7 tests/cat/herd
opam exec -- bash tests/cat/herd-pso-oracle.sh herd7 tests/cat/herd/MP.litmus
```

Results: clean build passed; unit/property tests 100/100; focused integration
tests 4/4; fast-driver 1/1 in 76.90 seconds; both herd scripts passed. Shell
syntax, `git diff --check`, changed-line formatting, license banners, comment
style, and absence of filename/model-name dispatch were also checked.

`clang-tidy` was invoked with the generated compilation database, but the local
tool failed while resolving the Apple C++ standard library (`cstddef` not
found). Normal compilation with LLVM 20.1.7 succeeds, so this is recorded as an
environment-blocked audit rather than a clean lint result.

## Gap analysis and next-phase inputs

No open P0 correctness or documentation gap remains for the declared Phase 1
scope. Phase 1.9 fixed two audit findings before closure: reachable difference
is rejected from online checks, and CAT files cannot silently use a generated
host checker's Relinche predicate.

| Priority | Gap | Required follow-up |
|---|---|---|
| P1 compatibility | recursive definitions, broader CAT declarations, and general non-monotone checks are unsupported | Phase 2 normalization, dependency/polarity analysis, and least fixed points |
| P1 diagnostics | structured evaluator witnesses are not yet rendered as full CLI explanations | Phase 2 explanation/provenance output |
| P1 performance | every query rebuilds dense primitives and recomputes the typed DAG | keep as oracle; compare Phase 3 incremental results against it |
| P1 exploration | conservative rf/co enumeration and revisit retention may be expensive | add only proof-backed pruning after differential validation |
| P2 compatibility | Relinche has no arbitrary-CAT coherence contract | define a model-independent interface before enabling the combination |
| P3 online semantics | additions, removals, and backtracking need trail-valid fixed points/explanations | delta propagation, push/pop state, invalidation, and restart rules |

Phase 2 should begin with its own reviewed plan. It must preserve this
from-scratch evaluator as the semantic oracle rather than replacing the Phase 1
contract in place.
