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
