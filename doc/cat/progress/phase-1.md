# Phase 1 Progress Log

This is an append-only record. Each substage entry must be completed before
the substage commit is pushed.

## Entry template

### Phase 1.X: Title

- Date:
- Starting commit:
- Ending commit:
- Remote ref pushed:
- Constraint/plan pre-read:
- GenMC development rules pre-read:
- Initial dirty files:

#### Contract

- Inputs:
- Outputs:
- Unsupported cases:
- Expected changed files:
- Expected untouched files:

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|

#### Implementation

- Changed files:
- Untouched files:
- Key decisions and invariants:
- Comment/documentation audit:
- GenMC convention audit:

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|

#### Risks and next target

- Known risks:
- Next substage target:

#### Git delivery

- Commit message:
- Commit SHA:
- Push command/result:

### Phase 1.0: Baseline, asset inventory, and specification freeze

- Date: 2026-07-14
- Starting commit: `5f71a10d617bb6ca99ac77b7a522a428535a381a`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Inputs: current GenMC source/tests, local CAT/CAAT papers, official
  herdtools7 frontend/models/license, and Kater paper/artifact metadata.
- Outputs: reproducible baseline, frozen Phase 1 grammar/type/include contract,
  clean-room SC/TSO/PSO models, and feature-to-test fixture manifest.
- Unsupported cases: no parser/evaluator/CLI implementation in Phase 1.0; no
  full CAT compatibility or Kater/CAAT source import.
- Expected changed files: project records, `doc/cat/` specification records,
  and three `models/cat/` acceptance inputs.
- Expected untouched files: all existing C++/headers, CMake files, tests,
  manual pages, scripts, and generated checkers.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| GenMC | v0.17.0 at `5f71a10` | Apache-2.0/MIT | build/test conventions, graph/checker oracle | direct reuse in later substages |
| herdtools7 | official master frontend/library models | CeCILL-B | syntax, semantics, differential behavior | reference only; no source copied |
| CAT semantics | local 2016 paper | publication | type/operator/check semantics | normative Phase 1 language basis |
| CAAT | local OOPSLA 2022 paper | publication | phase boundary and later fixed-point design | defer implementation to Phase 2 |
| Kater | paper/artifact instructions and generated checkers | mixed; artifact code outside GenMC GPLv2 | generated behavior and design evidence | no 3.6 GB runtime/source dependency |

#### Implementation

- Changed files: `doc/cat/supported-cat.md`, `doc/cat/phase-1-fixtures.md`,
  `doc/cat/baseline.md`, `models/cat/{sc,tso,pso}.cat`, `task_plan.md`,
  `notes.md`, and this progress log.
- Untouched files: production code, build configuration, existing tests,
  scripts, manual, and generated checkers.
- Key decisions and invariants: C++23 remains the runtime path; model files are
  clean-room; includes have no implicit herd path; four compatibility gates
  distinguish syntax, typing, supported surface, and GenMC executability.
- Comment/documentation audit: model comments explain semantic intent and
  provenance; no production comments were changed.
- GenMC convention audit: documentation remains under `doc/`, acceptance
  inputs under `models/cat/`, and no parallel `docs/` hierarchy was created.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake configure/build with LLVM 20 and hwloc link path | pass | all targets; GenMC v0.17.0 |
| `GenMC=... ./scripts/fast-driver.sh` | pass | exit 0; expected; 50.65s reported |
| SC/TSO smoke commands | pass | SB 3/4; LB+ctrl 3/3; WWR+2WR 0/0 complete |
| focused registered unit/property tests | pass | 40/40; 0 failed; 0.21s |
| `ctest --test-dir RelWithDebInfo ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 76.72s |
| feature/model cross-check against supported table | pass | all tokens/operators/names in three models are listed |
| `git diff --check` and model manifest/path audit | pass | no whitespace errors; all 3 acceptance paths present |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| build/test baseline | `doc/cat/baseline.md` | environment: fresh fetch remains network-sensitive | retain documented source override when needed |
| SC/TSO fixed smoke results | baseline and fixture manifest | none | preserve as differential oracle |
| herd/Kater inventory and licenses | baseline reuse table and notes | compatibility: no local herd executable | add herd differential runner when available |
| exact SC/TSO/PSO models | `models/cat/*.cat` | correctness: PSO has no built-in oracle | validate against herd/Nidhugg litmus corpus in 1.8 |
| frozen grammar/types/includes/diagnostics | `supported-cat.md` | none for selected surface | implement without silent expansion |
| fixture feature mapping | `phase-1-fixtures.md` | implementation: tests not created yet | realize incrementally in 1.1-1.5 |

#### Risks and next target

- Known risks: TSO equivalence must be demonstrated beyond the initial smoke
  set; PSO's SC-access policy needs external outcomes; unit dependencies are
  network-sensitive; full herd library models intentionally exceed this subset.
- Next substage target: Phase 1.1, validated `--model-file` CLI/config plumbing
  with no claim that the file already affects consistency.

#### Git delivery

- Commit message: `docs(cat): freeze phase-one CAT subset and baselines`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verified after commit



### Phase 1.1: CLI and configuration plumbing

- Date: 2026-07-14
- Starting commit: `5cd8e54412397b09178f8eb54f401a7879a754e4`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Inputs: one optional `--model-file=<path.cat>`, the existing mutually
  exclusive `--sc`/`--tso`/`--ra`/`--rc11` model options, and a program input.
- Outputs: canonical model path in `Config`; early stable diagnostics for
  conflict, missing/non-regular path, duplicate option, and the explicit
  Phase-1.1 execution boundary; unchanged legacy configuration otherwise.
- Unsupported cases: model parsing, type checking, checker selection, and CAT
  consistency evaluation remain Phase 1.2+ work.
- Expected changed files: `lli/main.cpp`, `Config.hpp/.cpp`, focused tests and
  test CMake, `doc/manual/cli.md`, project plan/notes, and this progress log.
- Expected untouched files: checkers, execution graph, driver exploration,
  LLVM passes other than their existing `LLIConfig` input, and CAT semantics.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| LLVM CommandLine | LLVM 20 API already used in `lli/main.cpp` | Apache-2.0 with LLVM exceptions | option parsing, occurrence count, duplicate rejection, help | add one `cl::opt`; no new dependency |
| GenMC `Config::validate` | current branch | Apache-2.0/MIT | aggregated errors/warnings and pre-compilation validation | store and canonicalize path here |
| GenMC MM detector | `adjustConfig` and `LLIConfig::mmDetector` | Apache-2.0/MIT | existing automatic strengthening | disable it for an explicit CAT file |
| GenMC CTest/GoogleTest | `tests/CMakeLists.txt`, unit target | Apache-2.0/MIT | config unit tests and executable CLI test | extend neighboring targets/scripts |

#### Implementation

- Changed files: `lli/main.cpp`, `genmc/genmc/Verification/Config.hpp`,
  `genmc/genmc/Verification/Config.cpp`, `tests/CMakeLists.txt`,
  `tests/unit/CMakeLists.txt`, `tests/unit/ConfigTest.cpp`,
  `tests/cli/model-file.sh`, `doc/manual/cli.md`, `task_plan.md`, `notes.md`,
  and this progress log.
- Untouched files: all consistency checkers, execution-graph code, LLVM
  transformations, driver exploration algorithms, existing test fixtures,
  and the frozen CAT models/specification.
- Key decisions and invariants: a valid file is canonicalized before the
  Phase-1.1 boundary; CAT input never reaches compilation or exploration yet;
  an explicit CAT file disables automatic model detection and dependency-model
  inference; estimation and multithreading options remain accepted but cannot
  run before parsing is integrated. LLVM's scalar option parser accepts
  repeated values, so the occurrence count is retained for an explicit stable
  duplicate diagnostic instead of silently using the last path.
- Comment/documentation audit: new configuration fields, validation blocks,
  test helpers, and CLI interactions document their purpose and boundary; the
  public manual and generated help describe the new option.
- GenMC convention audit: new C++ and shell files carry the dual license;
  production comments use GenMC's block-comment convention; source placement,
  CMake integration, naming, and formatting follow neighboring code.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake configure and `cmake --build RelWithDebInfo -j4` | pass | all targets; existing LLVM/libc++ deprecation warnings only |
| `RelWithDebInfo/bin/unit_tests` | pass | 46/46 tests from 7 suites; 0 failed; 12 ms |
| `ctest ... -R '^cli-model-file$'` | pass | 1/1; 0 failed; 0.96s |
| help/conflict/missing/directory/duplicate/boundary/default probes | pass | all six new diagnostics/help paths plus legacy SC execution |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 76.83s |
| repository `clang-format`, dry-run, and `git diff --check` | pass | all changed C++ files; no whitespace errors |
| `clang-tidy ... -p RelWithDebInfo` | environment-blocked | compile DB generated, but local tidy cannot resolve libc++ C++23 `<format>`; no actionable new-code error obtained |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| help exposes `--model-file` | `lli/main.cpp` and CLI test | none | retain spelling in parser integration |
| canonical path and readable-file validation | `Config::validate` plus 4 focused unit tests | none | pass canonical path to the loader in 1.2 |
| duplicate and built-in conflicts | occurrence/config tests and CLI test | none | replace temporary boundary only after compiled model exists |
| model-detector/dependency interaction | guarded `LLIConfig` and `adjustConfig` paths | compatibility: estimation/thread execution not yet observable | exercise both after evaluator integration |
| unchanged legacy CLI | SC CLI probe and full fast-driver | none | continue differential regression |
| CAT consistency execution | explicit boundary diagnostic | deferred scope | implement parser in 1.2, typed IR in 1.3, evaluator/adapter in 1.4-1.6 |

#### Risks and next target

- Known risks: the temporary boundary must not be mistaken for CAT execution
  support; local `clang-tidy` needs a corrected libc++ resource/sysroot setup;
  configuration-only tests cannot yet measure estimation or threaded execution.
- Next substage target: Phase 1.2 lexer/parser after all Phase 1.1 gaps close.

#### Git delivery

- Commit message: `feat(cli): add validated CAT model file option`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verified after commit

### Phase 1.2: Lexer, parser, source spans, and diagnostics

- Date: 2026-07-14
- Starting commit: `0ba98bc91cec7a73a3f603630f9e20a26e168acb`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Inputs: one canonical UTF-8 root model, its relative/absolute includes, and
  the exact syntax frozen in `doc/cat/supported-cat.md`.
- Outputs: an owned syntax-only model with source spans and includes expanded
  in place, or categorized diagnostics with path/line/column before LLVM
  execution.
- Unsupported cases: name resolution, type checking, immutable relational IR,
  graph evaluation, and consistency checking remain Phase 1.3+ work.
- Expected changed files: a new C++ CAT frontend, library/test CMake entries,
  parser/config tests, CLI boundary text, manual, project records, and this log.
- Expected untouched files: consistency checkers, execution graph, exploration
  algorithms, LLVM transformations, and bundled model semantics.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| frozen Phase 1 grammar | `doc/cat/supported-cat.md` | project documentation | tokens, precedence, AST surface, include rules | implemented exactly as local contract |
| GenMC CMake/GoogleTest | current branch | Apache-2.0/MIT | library source placement and discovered unit tests | extended existing targets; no new dependency |
| herdtools7 frontend | official OCaml lexer/parser surveyed in 1.0 | CeCILL-B | behavioral syntax reference only | no source copied; local recursive-descent parser avoids runtime/license cost |
| C++ standard library | C++23 toolchain | implementation license | filesystem, streams, ownership, containers | sufficient for linear dependency-free frontend |

#### Implementation

- Changed files: `genmc/genmc/CAT/Frontend.hpp`,
  `genmc/genmc/CAT/Frontend.cpp`, `genmc/CMakeLists.txt`,
  `genmc/genmc/Verification/Config.cpp`, `tests/unit/CatFrontendTest.cpp`,
  `tests/unit/ConfigTest.cpp`, `tests/unit/CMakeLists.txt`,
  `tests/cli/model-file.sh`, `doc/manual/cli.md`, `task_plan.md`, `notes.md`,
  and this progress log.
- Untouched files: built-in/generated consistency checkers, execution graph,
  driver exploration, LLVM passes, existing litmus fixtures, bundled CAT
  models, and the frozen grammar/type contract.
- Key decisions and invariants: the lexer validates UTF-8 before tokenization;
  AST ownership uses `unique_ptr`; every node retains a half-open canonical
  file span; included files are statement fragments resolved beside their
  parent; the active include stack records paths and include sites; binary
  operators fold left at the frozen precedence; top-level recovery restarts
  only at unambiguous statement keywords. Config parses before LLVM execution
  and retains an explicit typed-execution boundary.
- Comment/documentation audit: public types and methods document ownership,
  lifetime, thread safety, errors, and semantics; private lexer/parser/loader
  blocks document their invariants; tests name the protected behavior.
- GenMC convention audit: both new C++ files carry the dual license, use include
  guards and block/Doxygen comments, follow class declaration order and naming,
  and are formatted with the repository configuration.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake build | pass | all targets; only pre-existing LLVM/libc++ deprecation warnings |
| focused CAT/config tests | pass | 24/24 after final include/negative-surface additions |
| complete `unit_tests` | pass | 64/64 from 8 suites; 0 failed; 33 ms |
| `ctest ... -R '^cli-model-file$'` | pass | 1/1; 0 failed; 1.09s |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 77.61s final run |
| parser benchmark on bundled PSO model | pass | 300 parses; 20,631 files/s; 6.75 MiB process peak RSS |
| repository clang-format dry-run and `git diff --check` | pass | all changed C++ files; no whitespace errors |
| `clang-tidy Frontend.cpp -p RelWithDebInfo` | environment-blocked | command ran and exposed style suggestions; analysis terminated because local tidy cannot find standard header `<cstddef>` |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| tokens/comments/UTF-8/strings/spans | lexer plus lexical-detail and negative tests | none | preserve spans during lowering |
| frozen operators and precedence | syntax AST, complete-surface and precedence tests | none | assign types/node IDs in 1.3 |
| deterministic includes/cycles | canonical relative/absolute and cycle-site tests | none | retain source provenance during lowering |
| diagnostics/recovery | stable categories, exact-location and two-error recovery tests | none | add name/type categories in 1.3 |
| SC/TSO/PSO parse | bundled-model test | none | lower all three to stable summaries |
| parser performance record | XML property and `/usr/bin/time -l` | measurement: process RSS includes GoogleTest | add allocation-level benchmark only if parser cost becomes material |
| CAT execution | explicit typed-model boundary | deferred scope | implement resolver/type checker/immutable IR in 1.3 |

#### Risks and next target

- Known risks: `clang-tidy` remains toolchain-blocked; the parser intentionally
  accepts only the frozen subset; syntax trees are currently parsed during
  validation and discarded until Config owns the typed model in 1.3.
- Next substage target: Phase 1.3 name resolution, complete operator typing,
  immutable typed IR with stable IDs, and golden summaries for SC/TSO/PSO.

#### Git delivery

- Commit message: `feat(cat): parse the phase-one CAT language subset`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verified after commit

### Phase 1.3: Name resolution, typing, and immutable relational IR

- Date: 2026-07-14
- Starting commit: `d073b3c163b97386fc1cb19fec97ff2e6ff9c6c3`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Inputs: an error-free Phase 1 syntax model with includes already expanded.
- Outputs: a topologically ordered, typed, immutable `ModelIR` with stable
  node IDs, bindings, checks, source spans, and a platform-independent summary.
- Unsupported cases: relation values, graph primitives, consistency
  evaluation, and checker integration remain Phase 1.4+ work.
- Expected changed files: CAT IR/compiler, Config ownership, focused/golden
  tests, CMake, CLI boundary/manual, project records, and this log.
- Expected untouched files: graph/checker/exploration code, LLVM passes,
  existing litmus tests, and bundled CAT model text.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| Phase 1 syntax AST/spans | Phase 1.2 | Apache-2.0/MIT | owned syntax, expanded source order, diagnostics | lower directly; do not reparse |
| `Config` shared ownership pattern | current GenMC | Apache-2.0/MIT | immutable configuration shared by workers | store `shared_ptr<const ModelIR>` |
| CAT type rules | frozen contract/2016 semantics paper | publication/project | set/rel operator constraints | encode explicitly in local compiler |
| GenMC RapidCheck/GoogleTest | existing unit target | compatible existing dependency | DAG/source invariants and examples | reuse without new dependency |

#### Implementation

- Changed files: `genmc/genmc/CAT/Model.hpp`,
  `genmc/genmc/CAT/Model.cpp`, `genmc/genmc/CAT/Frontend.hpp/.cpp`,
  `genmc/genmc/Verification/Config.hpp/.cpp`, `genmc/CMakeLists.txt`,
  `tests/unit/CatModelTest.cpp`, `tests/unit/cat-golden/{sc,tso,pso}.ir`,
  `tests/unit/ConfigTest.cpp`, `tests/unit/CMakeLists.txt`,
  `tests/cli/model-file.sh`, `doc/manual/cli.md`, `task_plan.md`, `notes.md`,
  `doc/cat/supported-cat.md`, and this progress log.
- Untouched files: all consistency checkers, graph/driver/exploration code,
  LLVM transformations, litmus fixtures, and bundled CAT model definitions.
- Key decisions and invariants: operands always reference lower node IDs;
  identifiers disappear during lowering; bindings are sequential/nonrecursive;
  primitive nodes and derived aliases are cached; `M` and conventional aliases
  lower to generic operations; `mo` resolves to `co` with a deprecation note;
  no model is published after any name/type error; Config owns one immutable
  compiled model for read-only worker sharing.
- Comment/documentation audit: public IR types document ownership,
  immutability, topology, thread safety, and stable IDs; every compiler block
  explains its resolution/type invariant; tests state each protected rule.
- GenMC convention audit: new source/header carry dual licenses, use block and
  Doxygen comments, follow class/member ordering, and pass repository format.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake build | pass | all targets; no new-source compiler warnings in normal build |
| focused Config/model tests | pass | 18/18 plus CLI 1/1 |
| complete `unit_tests` | pass | 75/75 from 10 suites; 0 failed; 53 ms final run |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 76.80s |
| SC/TSO/PSO golden summaries | pass | exact 17/43/45-node DAGs; no paths or pointers |
| RapidCheck typed-IR property | pass | random supported expressions; resolved spans and backward-only operands |
| clang-format dry-run and `git diff --check` | pass | all changed C++ files; no whitespace errors |
| `clang-tidy Model.cpp -p RelWithDebInfo` | environment-blocked | local tidy again cannot resolve standard `<cstddef>`; actionable initialization/style findings were fixed manually |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| built-ins/includes/nonrecursive binding resolution | compiler symbol/prelude maps and name tests | none | evaluator supplies primitive values in 1.4/1.5 |
| complete set/relation typing | example matrix, check tests, RapidCheck | none | use node `ValueType` for evaluator dispatch |
| immutable stable IR and source provenance | `ModelIR`, DAG property, exact goldens | none | retain as Phase 2/3 semantic frontend |
| duplicates/undefined/forward/reserved diagnostics | focused name tests and Config pre-execution type test | none | preserve error categories |
| `mo` portability note | compiler notes and alias test | none | surface note through CLI warnings |
| graph consistency execution | typed execution boundary | deferred scope | implement pure relation storage/evaluator in 1.4 |

#### Risks and next target

- Known risks: lazy built-in node IDs depend on first source use (but remain
  deterministic for identical input); no common-subexpression elimination is
  attempted; `clang-tidy` remains environment-blocked.
- Next substage target: Phase 1.4 word-packed `EventSet`/`Relation`, all pure
  relational operations, memoized typed DAG evaluation, checks, and witnesses.

#### Git delivery

- Commit message: `feat(cat): add typed relational model IR`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verified after commit

### Phase 1.4: Relation storage and pure evaluator

- Date: 2026-07-14
- Starting commit: `4dad6fb8374e5d6a5cd156ccef2ba1de008900e2`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Inputs: an immutable typed `ModelIR`, one dense event count, and synthetic
  primitive set/relation values with the same universe.
- Outputs: pure word-packed values, memoized binding/check evaluation,
  structured errors, and named violations with source spans and witnesses.
- Unsupported cases: no `ExecutionGraph` dependency or checker integration;
  graph primitive construction remains Phase 1.5/1.6.
- Expected changed files: CAT values/evaluator, library/test CMake, focused
  property/benchmark tests, CLI phase-boundary documentation, records/log.
- Expected untouched files: Config semantics, typed IR, checkers, execution
  graph, driver/exploration, LLVM passes, models, and litmus fixtures.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| Phase 1.3 `ModelIR` | current branch | Apache-2.0/MIT | typed DAG, node IDs, checks/spans | interpret directly without syntax/model dispatch |
| GenMC `VSet`/`View` | current branch | Apache-2.0/MIT | representation ideas | not reused: vector clocks cannot represent arbitrary binary relations |
| C++ bit operations | C++23 standard library | implementation license | `popcount`, `countr_zero`, packed row algebra | reuse; no dependency added |
| RapidCheck/GoogleTest | existing unit target | compatible existing dependency | algebra/oracle properties and examples | extend existing target |

#### Implementation

- Changed files: `genmc/genmc/CAT/Value.hpp/.cpp`,
  `genmc/genmc/CAT/Evaluator.hpp/.cpp`, `genmc/CMakeLists.txt`,
  `tests/unit/CatEvaluatorTest.cpp`, `tests/unit/CMakeLists.txt`,
  `doc/manual/cli.md`, `task_plan.md`, `notes.md`, and this progress log.
- Untouched files: Config/frontend/typed IR semantics, all checkers,
  `ExecutionGraph`, driver/exploration code, LLVM passes, CAT models, and
  existing litmus fixtures.
- Key decisions and invariants: sets use one packed bit vector; relations use
  contiguous packed rows; all values have one fixed dense universe and zero
  tail bits; composition unions rhs rows for lhs edges; closure uses in-place
  bitset Warshall; evaluator state is per call and memoizes both success and
  failed primitive attempts; compiler types justify variant access; all
  bindings and checks are evaluated; witnesses use event, pair, diagonal, or
  closed DFS-cycle forms.
- Comment/documentation audit: value layouts, complexity, ownership,
  thread-safety, evaluator lifetime, memoization, closure/composition
  algorithms, parameters/errors, and witness formats are documented.
- GenMC convention audit: four new production files carry dual licenses,
  block/Doxygen comments, class ordering/naming, and repository formatting.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake build | pass | all targets; existing external/generated warnings only |
| focused value/evaluator tests | pass | 10/10 example/property/evaluator/benchmark tests |
| complete `unit_tests` | pass | 85/85 from 13 suites; 0 failed; 65 ms final run |
| `ctest ... -R '^cli-model-file$'` | pass | 1/1; 0 failed; 0.77s |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 76.61s |
| RapidCheck vs `std::set` oracle | pass | set Boolean algebra; relation Boolean/composition/closure; algebraic identities |
| 64/128/256/512 sparse+dense benchmark | pass | 2,093 us; 85 KiB packed inputs; 6.67 MiB process peak RSS |
| clang-format dry-run and `git diff --check` | pass | all changed C++ files; no whitespace errors |
| clang-tidy Value/Evaluator with compile DB | environment-blocked | local tidy cannot find standard `<cstddef>`; actionable nodiscard/initialization findings applied manually |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| word-packed set/relation storage | `Value` classes and cross-word tests | none | feed adapter values in 1.5 |
| all frozen pure operations | examples plus RapidCheck reference comparison | none | keep evaluator as from-scratch oracle |
| memoized typed DAG evaluation | `Evaluator` and per-node count test | none | cache only per graph snapshot |
| named checks and witnesses | set/pair/diagonal/cycle violation test | none | map dense IDs back to graph events in adapter/checker |
| generic bundled-SC interpretation | synthetic primitive SC test | none | compare real graph executions in 1.6 |
| performance/memory record | increasing sparse/dense benchmark | measurement: RSS includes GoogleTest | add adapter/end-to-end measurements later |
| GenMC graph execution | deliberately absent dependency | deferred scope | implement read-only graph adapter in 1.5 |

#### Risks and next target

- Known risks: dense relations require O(events²) bits; inverse remains a
  scalar O(events²) transpose; evaluator recomputes from scratch by design;
  `clang-tidy` remains environment-blocked.
- Next substage target: Phase 1.5 stable graph indexing and exact construction
  of event predicates plus `po/rf/co/fr/rmw/loc/int/ext/tc/tj` primitives.

#### Git delivery

- Commit message: `feat(cat): evaluate typed CAT relations and checks`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verified after commit
