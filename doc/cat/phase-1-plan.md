# Phase 1 Plan: CAT-file-driven GenMC

## Outcome

Phase 1 is complete when GenMC can load a supported CAT model with
`--model-file=<path.cat>` and use that model during exhaustive verification.
SC and TSO must agree with GenMC's built-in checkers, and PSO must work without
a hard-coded `PSOChecker`.

Phase 1 builds the parser, typed IR, base-relation adapter, and from-scratch
evaluator that Phases 2 and 3 will retain as their semantic foundation and
correctness oracle.

## Non-goals

Phase 1 does not implement:

- general recursive `let` or mutually recursive relations;
- CAAT normalization, repairability, cutting, or explanations;
- incremental fixed-point maintenance or online propagation;
- full herdtools7 compatibility;
- architecture-specific scopes, `with ... from ...`, procedures,
  `linearisations`, or arbitrary user functions;
- performance parity with Kater-generated checkers.

## Engineering and reuse decisions

### Runtime language

Use C++23 for the parser, IR, relation storage, evaluator, and GenMC adapter.
This avoids a foreign-language runtime in the verification hot path and fits
the existing `genmc_lib` build.

### Components to reuse

| Source | Reuse in Phase 1 | Boundary |
|---|---|---|
| GenMC `ExecutionGraph` and labels | Event universe and primitive relations | Add a read-only CAT adapter; do not duplicate graph ownership |
| GenMC CMake and GoogleTest/RapidCheck setup | New unit and property tests | Add focused CAT test sources to the existing unit target or a dedicated target |
| Built-in `SCChecker`/`TSOChecker` | Differential oracle for execution counts and errors | Do not dispatch CAT files to these checkers as the final implementation |
| Existing GenMC `View`, `VSet`, graph utilities | Reuse where representation and semantics match | Do not force vector-clock types to represent arbitrary binary relations |
| herdtools7 CAT models and behavior | Grammar examples, test corpus, external oracle | Do not copy CeCILL-B implementation code without license approval |
| CAT semantics paper | Normative syntax/type/semantic reference | Record any intentionally unsupported construct |
| Kater-generated checkers and paper | Later specialization patterns and performance reference | Phase 1 retains a generic from-scratch evaluator |

### Components to implement locally

- A dependency-free C++ lexer and recursive-descent/precedence parser for the
  supported CAT subset. The restricted grammar is small, and a local parser
  avoids bringing an OCaml runtime or a parser-generator dependency into
  GenMC.
- A typed, immutable CAT AST/IR using explicit set/relation types and source
  spans.
- Dense event sets and binary relations backed by word-packed bit rows. This
  representation gives predictable allocation and enables word-parallel
  union/intersection/difference and graph traversal. Sparse alternatives must
  be benchmarked before replacing it.
- A read-only `ExecutionGraph` adapter that assigns dense indices for one
  evaluation and constructs primitive relations.
- A full-graph evaluator with memoization scoped to one graph version.
- A generic correctness-first `ConsistencyChecker` adapter.

## Supported CAT surface at Phase 1 completion

The final Phase 1 compatibility target includes:

- model name/header;
- comments and source locations;
- `include` with deterministic search rules and include-cycle diagnostics;
- event sets needed by bundled SC/TSO/PSO models, initially `R`, `W`, `F`,
  `IW`, and universe `_`;
- primitive relations needed by those models, including `po`, `rf`, `co` or
  its documented alias, `fr`, `loc`, `int`, `ext`, `id`, and `0`;
- set-to-identity relation `[S]` and Cartesian product `S * T`;
- relation union, intersection, difference, composition, inverse, optional,
  transitive closure, and reflexive-transitive closure;
- non-recursive `let` bindings and a small, documented set of helper
  functions/macros required by the selected model files;
- `acyclic`, `irreflexive`, and `empty` checks with optional check names;
- early parse, name-resolution, type, include, and unsupported-feature
  diagnostics with file, line, and column.

The exact grammar and type rules must be frozen in
`doc/cat/supported-cat.md` before evaluator implementation begins.

## CLI and configuration contract

Target usage:

```bash
genmc --model-file=models/cat/sc.cat program.c
genmc --model-file=models/cat/tso.cat program.c
genmc --model-file=models/cat/pso.cat program.c
```

Required behavior:

- `--model-file` accepts one path and is mutually exclusive with an explicitly
  supplied built-in `--model`.
- A CAT model is parsed and validated once before program exploration.
- Missing files, include failures, syntax/type errors, and unsupported
  constructs fail before LLVM execution begins.
- Automatic memory-model strengthening/detection must not silently replace a
  user-supplied CAT model. The first substage that integrates configuration
  must define and test this interaction explicitly.
- Existing invocations without `--model-file` remain byte-for-byte compatible
  in CLI meaning and keep their existing checker selection.

## Substages

Each substage must follow the loop in `PROJECT_CONSTRAINTS.md`, append a result
to `doc/cat/progress/phase-1.md`, and finish with a pushed commit.

### 1.0 Baseline, asset inventory, and specification freeze

Deliverables:

- Record build/test baseline and current SC/TSO results on a fixed smoke set.
- Inspect the Kater artifact metadata/generated checkers and relevant
  herdtools7 lexer/parser/model files; record licenses and reusable ideas.
- Select exact SC, TSO, and PSO CAT files or write clean-room minimal models
  derived from published definitions, with provenance recorded.
- Create `doc/cat/supported-cat.md` containing grammar, precedence, types,
  include rules, diagnostic categories, aliases, and unsupported constructs.
- Create a fixture manifest mapping each model feature to parser/evaluator
  tests and herd/built-in oracle tests.

Verification:

- Existing project config/build succeeds.
- Existing unit tests and `ctest -R fast-driver` baseline are recorded.
- Every syntax construct used by the selected SC/TSO/PSO files appears in the
  supported-subset table.

Suggested commit:

```text
docs(cat): freeze phase-one CAT subset and baselines
```

### 1.1 CLI and configuration plumbing

Deliverables:

- Add `--model-file` parsing and canonical path storage in `Config`/LLI
  configuration as appropriate.
- Add mutual-exclusion and file-readability validation.
- Define model-file interaction with model detection, dependency tracking,
  estimation, multithreading, and built-in model formatting.
- Add CLI/config tests and manual documentation.
- Do not yet claim that the file affects consistency; if a temporary
  unsupported diagnostic is used, state it explicitly.

Verification:

- Help text shows the option.
- Valid path reaches the explicit Phase-1.1 boundary.
- missing path, duplicate option, and explicit `--model` conflict fail with
  stable diagnostics.
- Existing CLI regression tests remain unchanged.

Suggested commit:

```text
feat(cli): add validated CAT model file option
```

### 1.2 Lexer, parser, source spans, and diagnostics

Deliverables:

- Implement tokenization, comments, identifiers, operators, strings/includes,
  and source spans.
- Implement the frozen grammar and precedence rules.
- Add syntax-only AST nodes and diagnostics; no graph evaluation yet.
- Add unit tests for every token/operator, precedence ambiguity, malformed
  input, include cycle, and exact error location.
- Add Doxygen and logical-block comments required by the project constraints.
- Format new C++ with the repository `.clang-format`; all comments use GenMC's
  `/* ... */` convention.

Verification:

- All parser fixtures pass.
- Selected SC/TSO/PSO model files parse successfully.
- Mutation/negative fixtures fail in the expected diagnostic category and
  source location.
- Parser benchmark records files/second and allocations or peak memory for
  repeated model loading.

Suggested commit:

```text
feat(cat): parse the phase-one CAT language subset
```

### 1.3 Name resolution, typing, and immutable relational IR

Deliverables:

- Resolve built-ins, includes, and non-recursive bindings.
- Type-check sets, relations, products, tests, and checks.
- Lower the AST to an immutable typed IR with stable node IDs and source
  provenance.
- Detect duplicate names, undefined names, cycles, invalid operands, and
  unsupported constructs before exploration.
- Document ownership, immutability, and future Phase-2 extension points.

Verification:

- Unit tests cover every operator's valid and invalid type combinations.
- Property tests ensure resolved IR contains no unresolved names and retains
  source spans.
- SC/TSO/PSO fixtures lower to stable golden summaries without pointer- or
  platform-dependent output.

Suggested commit:

```text
feat(cat): add typed relational model IR
```

### 1.4 Relation storage and pure evaluator

Deliverables:

- Implement documented word-packed `EventSet` and `Relation` values.
- Implement union, intersection, difference, product, identity restriction,
  inverse, composition, optional, and closures.
- Evaluate bindings with per-evaluation memoization and evaluate named checks.
- Return structured violations containing check name, source span, and a
  witness event/cycle where practical.
- Keep this module independent of `ExecutionGraph` so it can be unit-tested
  with synthetic graphs and retained as the later correctness oracle.

Verification:

- Example-based tests for all operations.
- RapidCheck properties for algebraic identities and comparison against a
  simple test-only reference representation.
- Closure/composition tests cover empty, singleton, cyclic, and disconnected
  graphs.
- Benchmarks record time and peak memory across increasing event counts and
  sparse/dense relations.

Suggested commit:

```text
feat(cat): evaluate typed CAT relations and checks
```

### 1.5 GenMC execution-graph adapter

Deliverables:

- Build a stable dense event index for one `ExecutionGraph` snapshot.
- Map event predicates and primitive relations: at minimum `R/W/F/IW`, `po`,
  `rf`, `co`, derived or primitive `fr`, `loc`, `int`, and `ext`.
- Specify treatment of initializer events, thread-start/join labels,
  non-atomic events, RMWs, and unsupported event kinds.
- Keep the adapter read-only and document graph-version/cache invalidation.

Verification:

- Synthetic graph unit tests compare every primitive set/relation with direct
  `ExecutionGraph` queries.
- Debug validation checks endpoints, event counts, `rf` functionality, and
  per-location `co` consistency.
- Adapter construction benchmark records cost by graph size.

Suggested commit:

```text
feat(cat): expose GenMC graphs as CAT base relations
```

### 1.6 Full-graph checker and SC vertical slice

Deliverables:

- Add a CAT-backed checker path selected by a validated model file.
- Implement the minimal correctness-first `ConsistencyChecker` hooks needed
  for exhaustive SC exploration.
- Use conservative candidate enumeration where a generic pruning rule is not
  yet justified.
- Bundle or test with the provenance-recorded SC model.
- Keep the built-in `SCChecker` available as the differential oracle.

Verification:

- Selected SC litmus and program tests have identical execution counts,
  allowed outcomes, errors, and termination with built-in and CAT paths.
- Existing built-in SC behavior is unchanged.
- Run focused unit/integration tests and `ctest -R fast-driver`.

Suggested commit:

```text
feat(cat): verify programs with an SC CAT model
```

### 1.7 TSO compatibility and differential validation

Deliverables:

- Support every declared Phase-1 construct used by the selected TSO model,
  including its include/helper boundary.
- Define a generic, sound causal/prefix-view profile for TSO-family models or
  explicit model metadata; do not infer unsafe pruning from arbitrary syntax.
- Compare against built-in `TSOChecker` and herd.

Verification:

- Fixed TSO litmus suite has identical allowed outcomes and execution counts
  across CAT, built-in, and herd where event semantics align.
- Differences caused by C/LLVM versus assembly event semantics are isolated
  and documented rather than normalized away.
- No TSO-specific checker dispatch is hidden behind the CAT model filename.

Suggested commit:

```text
feat(cat): support TSO CAT verification
```

### 1.8 PSO model and new-model proof

Deliverables:

- Add the provenance-recorded PSO CAT model.
- Add only generic relation/profile capabilities required by PSO; do not add a
  hard-coded `PSOChecker` or filename/model-name switch.
- Add litmus cases that distinguish SC, TSO, and PSO, especially relaxed
  write-to-write ordering across locations.

Verification:

- PSO outcomes match herd or another recorded authoritative oracle.
- Replacing only the model file changes the expected distinguishing outcomes.
- A repository search confirms no PSO-specific consistency algorithm or
  filename dispatch exists outside model fixtures/profile declarations.

Suggested commit:

```text
feat(cat): add model-driven PSO verification
```

### 1.9 Phase closure, compatibility matrix, and gap analysis

Deliverables:

- Run the complete Phase 1 verification matrix.
- Publish supported syntax/model compatibility and measured performance.
- Audit all new classes/functions/blocks against the comment standard.
- Audit naming, class declaration order, assertion macros, license banners,
  clang-format output, and relevant clang-tidy diagnostics against
  `doc/development.md` and repository configuration.
- Compare implementation against every Phase 1 requirement in this document.
- Fix P0 correctness/documentation gaps before closure; convert remaining P1
  performance or compatibility gaps into explicit Phase 2 inputs only when
  they do not violate the Phase 1 outcome.
- Update manual, progress log, `task_plan.md`, and cross-phase notes.

Verification:

- Clean configure/build and unit tests.
- `ctest -R fast-driver` plus focused SC/TSO/PSO suites.
- Differential result report and benchmark report are reproducible from
  recorded commands.
- Clean worktree after the closure commit and successful push.

Suggested commit:

```text
docs(cat): close phase one with compatibility and gap report
```

## Phase 1 verification matrix

| Layer | Required checks |
|---|---|
| Lexer/parser | positive, negative, precedence, includes, exact source spans |
| Resolver/types | undefined/duplicate names, type errors, stable IR |
| Relations | examples, algebraic properties, reference implementation comparison |
| Graph adapter | primitive relation equivalence and invariants |
| CLI | help, conflicts, missing/unreadable file, backwards compatibility |
| SC/TSO | CAT vs built-in execution counts, outcomes, and errors |
| SC/TSO/PSO | CAT vs herd outcomes on aligned litmus semantics |
| Regression | unit tests, focused integration tests, `fast-driver` |
| Performance | parser, relation operations, adapter, end-to-end time and memory |
| Documentation | CLI manual, supported subset, provenance, comments, progress/gaps |

## Gap-analysis rubric

After each substage, compare planned and delivered items line by line and use:

- `P0 correctness`: can accept/reject the wrong model or execution; fix now.
- `P0 regression`: changes existing GenMC behavior; fix now.
- `P0 documentation`: public behavior or non-obvious invariant is undocumented;
  fix before commit.
- `P1 compatibility`: supported subset is smaller than the current substage
  contract; becomes the next substage target.
- `P1 performance`: functionally correct but misses a recorded budget; measure,
  diagnose, and schedule the smallest optimization.
- `P2 deferred`: explicitly belongs to offline CAAT or online integration; keep
  out of Phase 1 and link it to the later phase.

No `P0` gap may be carried into the next substage.

## Reader questions this plan must answer

1. What exactly counts as Phase 1 completion?
2. Which CAT constructs are supported and where will they be frozen?
3. Which existing projects are reused, and why is parser code local?
4. How is license compatibility protected?
5. How can a reviewer distinguish a real model-driven PSO implementation from
   filename dispatch?
6. Which tests prevent the generic checker from missing executions?
7. What is committed and pushed after each substage?
8. How do Phase 2 and Phase 3 reuse Phase 1 instead of replacing it?
