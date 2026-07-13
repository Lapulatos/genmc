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

### Phase 1.7: TSO compatibility and differential validation

- Date: 2026-07-14
- Starting commit: `c8c208e5de760da74392f3df0a9e2dffa37c5112`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Input: any supported CAT file, optionally declaring a leading
  `(* @genmc host-profile sc|tso *)` comment, plus a GenMC execution graph.
- Output: filename-independent selection of an SC/TSO causal-view host and
  consistency decisions made exclusively by the generic relational evaluator.
- Compatibility default: files without metadata retain the Phase 1.6 SC host.
- Acceptance boundary: bundled TSO must match built-in TSO on aligned C/LLVM
  fixtures and official herd x86tso outcomes on separate X86 fixtures.
- Unsupported: unknown/duplicate/late metadata and metadata in include
  fragments fail before program execution; arbitrary host profiles remain out
  of Phase 1.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| generated `SCChecker`/`TSOChecker` | current GenMC/Kater output | Apache-2.0/MIT in GenMC | causal/prefix views and language diagnostics | template the existing CAT wrapper; never call generated consistency predicates |
| Phase 1 CAT frontend/IR | current branch | Apache-2.0/MIT | source spans, immutable worker-shared model | retain typed host metadata through the existing pipeline |
| CAT block comments | CAT syntax/herd parser behavior | specification behavior only | herd-compatible metadata carrier | exact root-only directive avoids a private non-CAT token |
| built-in `TSOChecker` | current branch | Apache-2.0/MIT | C/LLVM differential oracle | compare status, counts, warnings, and errors on seven fixtures |
| herdtools7 `x86tso.cat` | herdtools7 7.56+03 | CeCILL-B | independent X86 semantic oracle | execute installed tool; copy no herd source or model into GenMC |

#### Implementation

- Changed files: CAT `Frontend` and `Model` headers/sources; CAT checker and
  factory/config sources; bundled SC/TSO models; IR goldens; config/frontend
  unit tests; CMake and CAT differential/herd fixtures/scripts; CLI/supported
  language docs; `task_plan.md`, `notes.md`, and this progress log.
- Untouched files: generated SC/TSO checker sources, evaluator/value/graph
  adapter semantics, execution-graph ownership, LLVM transformations, existing
  program fixtures, PSO equations/profile, and all Phase 2/3 code.
- `HostProfile` is a typed enum in syntax and immutable IR. SC is the default;
  exact leading metadata selects TSO, preserves its source span, and appears in
  deterministic IR summaries.
- `BasicCATChecker<HostChecker>` has explicit SC and TSO instantiations. The
  factory switches only on `ModelIR::hostProfile()`; an arbitrary model header
  and filename select TSO in unit coverage.
- The TSO differential script covers SB, dependency order, message passing,
  same-location order, RMW atomicity, unordered-write warnings, and safety
  errors. A separate herd script checks standard X86 SB and MP observations.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake configure/build | pass | `genmc` and `unit_tests`; only pre-existing generated/LLVM warnings |
| focused CAT/config/integration set | pass | 43/43; includes SC/TSO differential and CLI |
| complete unit/property set | pass | 96/96; 0 failed; 0.63s |
| TSO GenMC differential | pass | 7 fixtures; status/count/verdict category identical |
| herd 7.56 official x86tso oracle | pass | SB `Sometimes 1 3`; MP condition `Never 0 3` |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 76.67s |
| warmed 10-run SB benchmark | pass | built-in/CAT both 0.48s; 52,625,408/52,641,792-byte peak RSS |
| clang-format, shell syntax, `git diff --check` | pass | all changed C++/shell and whitespace checks |
| clang-tidy changed production TUs | environment-blocked | local tidy cannot locate libc++ `<algorithm>`/`<cstddef>`; new uninitialized-enum finding fixed; remaining output is prior/style noise |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| all selected TSO constructs | bundled model parse/type/evaluate and golden | none | preserve surface in 1.9 matrix |
| sound TSO-family host profile | explicit typed metadata and TSO base instantiation | none | PSO may reuse declared TSO host only with oracle evidence |
| no filename/model-name dispatch | arbitrary-name config test and enum-only factory switch | none | repository-search again in 1.8/1.9 |
| built-in TSO comparison | seven-case CTest differential | none | broaden only when a mismatch-risk feature is identified |
| herd comparison | two standard X86 outcomes with official x86tso.cat | semantic boundary | do not equate X86 assembly state counts with transformed C/LLVM counts |
| generic violation details | checker still consumes evaluator result as boolean | deferred compatibility | audit whether Phase 1 CLI needs witness display in 1.9 |
| generic host-profile derivation | explicit metadata required beyond SC default | intentional compatibility | Phase 2 admissibility analysis may infer/prove profiles later |

#### Risks and next target

- Known risks: full evaluation and conservative candidate enumeration remain
  correctness-first; generated host views are available only for declared
  SC/TSO families; herd and GenMC input event semantics differ outside the
  assembly-aligned oracle; clang-tidy remains locally misconfigured.
- No P0 correctness, regression, or documentation gap remains for Phase 1.7.
- Next substage target: Phase 1.8 PSO model-driven proof, including a
  SC/TSO/PSO-distinguishing write-to-write litmus and external oracle.

#### Git delivery

- Commit message: `feat(cat): support TSO CAT verification`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verify after commit

### Phase 1.6: Full-graph checker and SC vertical slice

- Date: 2026-07-14
- Starting commit: `74e8dd449ce22f30c44cf14938ab9fc943b7ba79`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Input: Config containing a validated immutable `ModelIR` selected through
  `--model-file` and worker-local execution graphs.
- Output: actual program exploration whose candidate graphs are accepted only
  when the generic evaluator satisfies every CAT check.
- Host profile: reuse SC's established view/prefix/error infrastructure, but
  conservatively enumerate current same-location `rf/co` choices and retain
  revisits instead of applying generated SC consistency pruning.
- Dispatch invariant: checker selection observes only the presence of a typed
  model; it never inspects the path, filename, model header, or check names.
- Acceptance boundary: SC equivalence is required here. TSO/PSO use the same
  evaluator but receive their complete compatibility proof in 1.7/1.8.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| `ConsistencyChecker` factory/API | current branch | Apache-2.0/MIT | worker ownership and driver integration points | add one ordinary checker selected by typed model presence |
| generated `SCChecker` | current branch/Kater output | Apache-2.0/MIT in GenMC | views, prefix, race/error/warning handling | inherit host infrastructure; do not call its consistency predicate |
| `ExecutionGraph` co/rf APIs | current branch | Apache-2.0/MIT | all current same-location stores/positions | enumerate conservatively; retain RMW adjacency only |
| Phase 1.4/1.5 evaluator/adapter | current branch | Apache-2.0/MIT | generic checks and graph primitives | rebuild from scratch for every candidate as correctness oracle |
| built-in SC checker | current branch | Apache-2.0/MIT | execution/error differential oracle | run side-by-side on fixed programs |

#### Implementation

- Changed files: `genmc/genmc/Execution/Consistency/CATChecker.hpp/.cpp`,
  `ConsistencyChecker.cpp`, `Config.cpp`, `genmc/CMakeLists.txt`,
  `tests/cat/sc-differential.sh`, `tests/CMakeLists.txt`,
  `tests/cli/model-file.sh`, `tests/unit/ConfigTest.cpp`,
  `doc/manual/cli.md`, `task_plan.md`, `notes.md`, and this progress log.
- Untouched files: generated checker sources, graph/label representation,
  frontend/IR/evaluator/adapter semantics, LLVM passes, CAT model equations,
  and existing program/litmus fixtures.
- `Config::validate` now publishes a typed model without the former phase
  boundary error and selects the SC host profile used by transformations and
  views. The factory creates `CATChecker` based only on `catModel` presence.
- Both consistency entry points construct a fresh `GraphAdapter`, evaluate the
  immutable DAG, treat violations as inconsistency, and treat evaluator shape
  errors as internal invariant failures.
- Generic rf candidates contain Init plus every current same-location write;
  ordinary co candidates contain Init plus every current co predecessor;
  RMW writes remain immediately after their rf source; revisit filtering is a
  documented no-op.
- CLI coverage now runs a valid model through two workers and compares its SC
  execution count to built-in SC instead of expecting a phase-boundary error.
- Differential coverage compares successful, blocked/warning, and safety-error
  executions using existing fixtures without modifying their source.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake build | pass | `genmc` and `unit_tests`; new-source compile succeeds |
| Config/factory and CLI focus | pass | 10/10 Config+CLI checks; CAT factory selected; two-worker SB=3 |
| SC differential CTest | pass | 1/1; four fixtures; status/count/verdict markers identical |
| complete unit set | pass | 90/90; 0 failed; 0.59s final run |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 77.30s final run |
| end-to-end SB benchmark | pass | 10 runs: built-in 0.48s/50.2 MiB, CAT 0.48s/50.2 MiB |
| clang-format, shell syntax, and `git diff --check` | pass | all changed C++/shell and whitespace checks pass |
| clang-tidy CATChecker with compile DB | pass with reviewed notes | only false-positive static suggestions for required virtual overrides and a required direct Config definition include |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| CAT-backed checker selection | Config/factory test and successful CLI execution | none | retain one generic path |
| full-graph CAT consistency | adapter/evaluator call at both checker entry points | none | keep as later oracle |
| conservative candidate handling | all current rf/co positions; no revisit filtering | none | specialize only with proof/differential evidence |
| SC model vertical slice | four differential fixtures plus two workers | none | broaden SC corpus during final phase audit |
| built-in SC unchanged | built-in side of differential and fast-driver | none | retain as oracle |
| detailed CAT violation reporting | evaluator witnesses exist but checker returns boolean | deferred compatibility | map witnesses into diagnostics in 1.9 unless needed earlier |
| TSO/PSO host-profile proof | execution path exists but not fully validated | deferred scope | Phase 1.7/1.8 |

#### Risks and next target

- Known risks: full reconstruction/evaluation occurs at every candidate;
  conservative enumeration may increase work; host views still come from the
  SC generated checker; evaluator violations currently prune without printing
  their named witness during ordinary successful verification.
- Next substage target: Phase 1.7 TSO differential compatibility, explicit
  generic host-profile justification, and TSO performance comparison.

#### Git delivery

- Commit message: `feat(cat): verify programs with an SC CAT model`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verify local/remote SHA

### Phase 1.5: GenMC execution-graph adapter

- Date: 2026-07-14
- Starting commit: `dbccb7699d38690e6947df7472398201ca1c2929`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Input: one read-only `ExecutionGraph` snapshot whose labels outlive the
  adapter.
- Output: stable insertion-order real-label IDs followed by sorted per-address
  virtual initial writes, reverse event/location lookup, and every Phase 1
  primitive required by `Evaluator`.
- Snapshot boundary: every graph mutation invalidates the adapter. The graph
  has no relation-generation counter, so structural checking cannot prove
  stability after an edge-only `rf/co` update.
- Event policy: expose real labels except internal `EmptyLabel` and expand the
  address-polymorphic `InitLabel` into virtual per-location events.
  Unsupported/model-neutral kinds remain in `_`, `po`, `int`, and `ext`
  rather than failing construction.
- Expected untouched files: graph/label implementation, Config, evaluator,
  checkers, driver/exploration, LLVM passes, models, and litmus fixtures.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| `ExecutionGraph` traversal/query APIs | current branch | Apache-2.0/MIT | insertion order, po, rf pointers, co lists, lifecycle links | reuse read-only; do not duplicate graph ownership |
| `EventLabel` RTTI and accessors | current branch | Apache-2.0/MIT | read/write/fence/init/RMW/lifecycle classification | reuse GenMC's canonical label semantics |
| Phase 1.4 packed values | current branch | Apache-2.0/MIT | fixed-universe sets/relations and composition | materialize evaluator-ready primitives directly |
| herdtools7 event representation | upstream semantic oracle | CeCILL-B | per-location initial-write semantics only | no source copied; GenMC Init is expanded by address at the adapter boundary |

#### Implementation

- Changed files: `genmc/genmc/CAT/GraphAdapter.hpp/.cpp`,
  `genmc/CMakeLists.txt`, `tests/unit/CatGraphAdapterTest.cpp`,
  `tests/unit/CMakeLists.txt`, `doc/cat/supported-cat.md`, `task_plan.md`,
  `notes.md`, and this progress log.
- Untouched files: `ExecutionGraph`/`EventLabel`, Config/frontend/IR/evaluator,
  every consistency checker, driver/exploration code, LLVM transformations,
  bundled CAT models, and existing litmus fixtures.
- Real-label IDs follow `ExecutionGraph::labels()` insertion order and
  round-trip through `Event`; virtual IW IDs use sorted addresses and map to
  `InitLabel + SAddr` for witness reporting.
- Event sets classify emitted NA accesses normally, split RMW read/write
  labels normally, and append one virtual `IW`/`W` per tracked address.
- `po/int/ext/loc` are constructed over the complete exposed universe;
  `rf/rmw/tc/tj` follow direct label links; `co` expands stored per-location
  lists into strict order and adds conceptual init-to-store edges.
- `fr` is built as `rf^-1 ; co` and checked again during validation. Validation
  also checks dense round trips, primitive universe sizes, functional `rf`,
  strict `co`, and cross-location `co` endpoints.
- Comment/documentation audit: ownership, lifetime, invalidation, complexity,
  initializer encoding, RMW pairing, relation construction, and validation
  limits are documented in source and the supported-profile contract.
- GenMC convention audit: new production/test files carry the dual license,
  use block/Doxygen comments, pass repository format, and introduce no new
  dependency.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake focused build | pass | `unit_tests` rebuilt with new adapter source |
| focused adapter tests | pass | 5/5: index, predicates, memory/order, lifecycle, benchmark |
| complete unit set | pass | 90/90; 0 failed; 0.58s final run |
| `ctest ... -R '^cli-model-file$'` | pass | 1/1; 0 failed; 0.19s final run |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 77.04s final run |
| 64/128/256/512 graph benchmark | pass | 4,925 us; 522,960 packed bytes (~511 KiB); ~7.47 MiB process peak RSS |
| debug invariant validation | pass | dense/endpoints, functional rf, co location/strictness, exact fr equation |
| clang-format dry-run and `git diff --check` | pass | all changed C++ files; no whitespace errors |
| clang-tidy GraphAdapter with compile DB | environment-blocked | local tidy cannot find standard `<cstddef>`; helper decomposition and explicit initialization were reviewed manually |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| stable dense graph index | round-trip and post-mutation structural test | none | map evaluator witnesses back through labels in 1.6 |
| `R/W/F/IW/SC` | exact predicate test including NA/init/RMW | none | use directly in CAT checker |
| all primitive relations | exact synthetic graph assertions and direct graph links | none | differential-check real SC graphs |
| initializer/lifecycle/unsupported policy | source contract and supported-profile documentation | none | preserve per-address virtual Init mapping in checker |
| snapshot invalidation | explicit lifetime contract plus structural detector | accepted limitation | rebuild per full-graph check; Phase 3 adds generations/deltas |
| debug invariants | `validate()` plus focused graph fixture | none | invoke in debug CAT checker construction |
| construction benchmark | increasing graph/property record | measurement: RSS includes GoogleTest | add end-to-end CAT/built-in comparison in 1.6 |
| CAT checker execution | deliberately not connected in adapter substage | deferred scope | implement SC vertical slice in 1.6 |

#### Risks and next target

- Known risks: each generic relation remains O(events²) bits; virtual initial
  writes increase the universe by the number of tracked locations; freshness
  cannot detect edge-only graph mutation; from-scratch construction is
  correctness-first and will be replaced/supplemented by Phase 3 deltas.
- Next substage target: Phase 1.6 CAT-backed full-graph checker integration and
  SC differential equivalence without filename/model-name dispatch.

#### Git delivery

- Commit message: `feat(cat): expose GenMC graphs as CAT base relations`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verify local/remote SHA

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

### Phase 1.8: PSO model and new-model proof

- Date: 2026-07-14
- Starting commit: `b4e08c32bb1713378e2d3e887b6124df9c37538f`
- Ending commit: recorded by the atomic commit named below
- Remote ref pushed: `origin/genmc-caat` (to be verified after this atomic commit)
- Constraint/plan pre-read: `doc/cat/PROJECT_CONSTRAINTS.md` and
  `doc/cat/phase-1-plan.md` read before implementation edits
- GenMC development rules pre-read: `doc/development.md` read before edits
- Initial dirty files: none

#### Contract

- Input: the existing bundled PSO CAT equations and a distinguishing
  cross-location `WW+RR` C program.
- Output: PSO verification through the same generic evaluator/checker used by
  SC/TSO, with only the selected model file changing the observed outcome.
- Host boundary: PSO explicitly reuses the TSO causal-view host justified in
  1.7; no PSO profile enum, generated checker, or C++ consistency path exists.
- Oracle boundary: compare plain R/W ordering with herd's official x86 TSO
  model and official MIPS model whose CAT source explicitly selects PSO ppo.

#### Reuse survey

| Candidate | Version/source | License | Reused part | Decision/rationale |
|---|---|---|---|---|
| bundled clean-room `pso.cat` | Phase 1.0/current branch | Apache-2.0/MIT project input | selected PSO equations | add only explicit existing TSO host metadata |
| `CATTSOChecker` | Phase 1.7 | Apache-2.0/MIT | sound causal/prefix host | reuse unchanged; consistency remains generic |
| evaluator/adapter | Phase 1.4/1.5 | Apache-2.0/MIT | all PSO set/relation operations | no new primitive or production implementation needed |
| GenMC litmus corpus | current branch | Apache-2.0/MIT | po-loc, RMW, SC-fence regression cases | prove PSO preserves its declared ppo edges |
| herd `x86tso.cat`/`mips.cat` | herdtools7 7.56+03 | CeCILL-B | independent ordering oracle | execute official installed models; copy no source |

#### Implementation

- Changed files: `models/cat/pso.cat`, its IR golden, PSO model/herd scripts,
  the new `tests/cat/programs/WW+RR.c`, `tests/CMakeLists.txt`, manual and CAT
  compatibility/fixture docs, `task_plan.md`, `notes.md`, and this progress log.
- Untouched files: all production C++, generated checkers, CAT frontend/IR/
  evaluator/adapter semantics, graph/driver/LLVM code, SC/TSO models, and
  existing test programs.
- `pso.cat` now declares `host-profile tso`. Its equations remain unchanged:
  cross-location W→W is absent from `ppo`, while read-originating order,
  same-location W→W, fence order, and SC access pairs remain.
- `cat-pso-model-proof` keeps the program and flags fixed. It changes only
  `sc.cat`/`tso.cat`/`pso.cat`, checks distinct status/verdict/counts, then
  checks po-loc/RMW/SC-fence cases common to TSO and PSO.
- `herd-pso-oracle.sh` runs the same X86 MP event structure through official
  TSO and PSO ppo equations. Architecture checking is disabled for the latter;
  no architecture-specific instruction or fence participates.

#### Verification

| Command | Result | Counts/timing/output |
|---|---|---|
| CMake configure/build | pass | no new production target; existing LLVM warnings only |
| complete unit/property set | pass | 96/96; 0 failed; 0.62s |
| SC/TSO/PSO/CLI integration | pass | 4/4; 0 failed; 1.91s |
| model-only WW+RR distinction | pass | SC safe/3, TSO safe/3, PSO safety error/2; PSO exit 42 |
| PSO preserved-order regressions | pass | po-loc 3, RMWFix 4, SB+scfs 3 under both TSO/PSO |
| two-worker PSO | pass | same safety violation, 2 complete executions, exit 42 |
| herd 7.56 external oracle | pass | TSO MP `Never 0 3`; PSO ppo MP `Sometimes 1 3` |
| `ctest ... -R '^fast-driver$'` | pass | 1/1; 0 failed; 76.70s |
| warmed 10-run WW+RR benchmark | pass | TSO 0.50s/52,576,256 B; PSO 0.48s/52,609,024 B |
| shell syntax, repository search, `git diff --check` | pass | production PSO/name/path-dispatch matches: 0 |
| clang-tidy | not applicable | no production C++ changed in this substage |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap class | Next action |
|---|---|---|---|
| provenance-recorded PSO model | clean-room model retained; metadata only change | none | include in final compatibility matrix |
| generic capabilities only | no C++ change; TSO host plus generic IR/evaluator | none | preserve zero-PSO-code invariant |
| SC/TSO/PSO distinction | one fixed WW+RR program; only model path changes | none | retain as final acceptance test |
| PSO external outcome | official herd TSO versus official PSO ppo | none for plain R/W ordering | document architecture/fence boundary |
| PSO preserved constraints | po-loc, RMW, SC-fence cases | none | include counts in closure report |
| absence of hidden dispatch | zero production matches for PSO/name/path patterns | none | repeat repository audit in 1.9 |
| broader SPARC/MIPS instruction compatibility | not a GenMC C/LLVM contract | deferred compatibility | do not expand Phase 1 into architecture frontend support |

#### Risks and next target

- Known risks: the PSO host is conservative TSO infrastructure rather than a
  generated PSO specialization; external oracle equivalence is restricted to
  plain R/W ordering; generic evaluation remains correctness-first.
- No P0 correctness, regression, or documentation gap remains for Phase 1.8.
- Next substage target: Phase 1.9 complete matrix, code/comment/convention
  audit, final performance/compatibility report, and Phase 2 input list.

#### Git delivery

- Commit message: `feat(cat): add model-driven PSO verification`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verify after commit

### Phase 1.9: closure, safety-boundary audit, and final report

- Starting commit: `c951418d420bb06defff538b0f93014c5305f197`
- Pre-check: re-read `doc/development.md`, `PROJECT_CONSTRAINTS.md`, and the
  Phase 1 plan; branch was `genmc-caat` and the worktree was clean.
- Contract: run the complete closure matrix, fix any P0 issue, publish exact
  compatibility/performance limits, and turn only non-blocking gaps into Phase
  2/3 inputs.

#### Reuse survey

| Candidate | Reused part | Decision |
|---|---|---|
| immutable typed DAG | operand graph and stable source spans | add a linear reverse reachability audit; do not add a second model representation |
| `Config::validate` diagnostics | pre-execution validation and canonical model ownership | enforce online/Relinche boundaries before publishing the model |
| generated SC/TSO hosts | views and language-error infrastructure | continue to exclude their model-specific Relinche coherence from arbitrary CAT |
| existing unit/integration/herd tests | semantic, graph, CLI, differential, and external-oracle layers | rerun in one clean build rather than create overlapping closure tests |

#### Audit findings and implementation

- P0 fixed: relation difference is non-monotone in its right operand. The IR
  now reports the first difference reachable from any check; Config rejects it
  before exploration while unused/offline difference remains evaluable.
- P0 fixed: CAT plus Relinche could ask the generated host for a coherence
  predicate belonging to the wrong model. Config now rejects both Relinche
  collection and checking combinations in Phase 1.
- Added four regression tests covering reachable/unused difference, bundled
  model admissibility, and Relinche rejection.
- Published `doc/cat/phase-1-report.md` with the runtime path, compatibility
  matrix, differential/herd evidence, performance baselines, reproducible
  commands, and prioritized Phase 2/3 inputs.
- Changed production files: `CAT/Model.hpp`, `CAT/Model.cpp`, and
  `Verification/Config.cpp`. Changed tests/docs: `CatModelTest.cpp`,
  `ConfigTest.cpp`, CAT compatibility/manual/report, cross-phase plan/notes,
  and this append-only progress entry.
- Untouched: CAT parsing/value/evaluator/graph-adapter semantics, generated
  checkers, graph mutation/exploration algorithms, bundled model equations,
  LLVM transforms, and all existing program fixtures.

#### Verification

| Command/layer | Result |
|---|---|
| clean CMake configure and `cmake --build Phase1Audit -j4` | pass; existing LLVM/generated-checker warnings only |
| unit/property selection | 100/100 pass; 0.65s |
| CLI + SC/TSO/PSO integrations | 4/4 pass; 1.94s |
| herd TSO and PSO oracle scripts | both pass with herdtools7 7.56+03 |
| `ctest -R '^fast-driver$'` | 1/1 pass; 76.90s |
| `bash -n`, `git diff --check`, changed-line `clang-format` | pass |
| license/comment/name/path-dispatch repository audit | pass; no new `//` comments or hard model-file dispatch |
| `clang-tidy -p Phase1Audit` | environment-blocked: Apple standard header `cstddef` not found; normal build passes |

#### Plan-to-implementation gap

| Planned item | Delivered evidence | Gap/next action |
|---|---|---|
| complete verification matrix | clean build, 100 unit/property, 4 integration, fast-driver, herd | none |
| compatibility/performance publication | final report with exact subset, commands, and measurements | none |
| comment/convention audit | formatting, banners, comments, naming/order/manual review | clang-tidy environment issue recorded, not a source P0 |
| compare every Phase 1 requirement | report and this table cover CLI through PSO | none for declared scope |
| fix P0 findings | online difference and Relinche fail early | none |
| carry remaining gaps forward | normalized recursion/explanations to Phase 2; deltas/backtracking/pruning to Phase 3 | deferred by fixed project sequence |

#### Git delivery

- Commit message: `fix(cat): close phase one verification boundaries`
- Commit SHA: resolved by the Git commit carrying this entry
- Push command/result: `git push origin genmc-caat`; verify local and remote
  refs after commit
