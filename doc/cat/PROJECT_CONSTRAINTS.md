# CAT/CAAT Extension Project Constraints

This file defines persistent constraints for all CAT/CAAT work on the
`genmc-caat` branch. It must be read before planning or implementing every
substage. GenMC repository rules take precedence over this file; these rules
only fill gaps that `doc/development.md`, repository configuration, and nearby
code do not cover.

## Required pre-substage check

Before starting a substage, run:

```bash
sed -n '1,300p' doc/development.md
sed -n '1,300p' doc/cat/PROJECT_CONSTRAINTS.md
sed -n '1,480p' doc/cat/phase-3-plan.md
git status --short
git branch --show-current
```

Record the check and current commit in the substage progress entry before
editing implementation files.

## Fixed project sequence

1. Phase 1: load and execute CAT models through
   `--model-file=<model.cat>`, initially targeting SC, TSO, and PSO.
2. Phase 2: implement an offline CAAT backend with normalization, recursive
   fixed-point evaluation, and check explanations.
3. Phase 3: implement incremental/online CAAT integration with GenMC
   exploration, backtracking, and early consistency propagation.

Later phases may extend, but must not silently change, the accepted semantics
of an earlier phase.

## Reuse-before-reimplementation rule

Every substage starts with a focused survey of relevant existing engineering.
The progress record must list what was inspected, what will be reused, and why
anything is being reimplemented.

Priority order:

1. Reuse GenMC data structures, graph APIs, configuration, diagnostics,
   CMake targets, tests, and generated-checker behavior.
2. Reuse Kater algorithms and generated-checker structure when source and
   license permit; otherwise treat the generated GenMC checkers and Kater
   papers/artifact as design references.
3. Use herdtools7 as the CAT syntax/semantics reference, source of model and
   litmus test cases, and differential-testing oracle.
4. Consider maintained C++ libraries only after checking license,
   maintenance state, runtime cost, binary/dependency cost, and fit with
   GenMC's C++23 build.
5. Implement locally only when reuse would add incompatible licensing,
   language-runtime, architecture, or performance constraints.

The production runtime path should use C++23 unless a measured experiment
shows that another systems language provides a material benefit without
making GenMC deployment or review harder. Python and shell may be used for
test orchestration or one-off analysis, not for the consistency hot path.

Do not copy herdtools7 source into GenMC without an explicit license review.
herdtools7 is distributed primarily under CeCILL-B, while GenMC is dual
Apache-2.0/MIT.

## Correctness and performance order

1. Preserve soundness and completeness before optimizing.
2. Keep a from-scratch full-graph evaluator as the internal correctness
   oracle for later optimized and incremental implementations.
3. Conservative over-enumeration followed by consistency filtering is
   acceptable during Phase 1; pruning that can remove executions requires
   a proof argument and differential tests.
4. Record baseline and post-change measurements for parser throughput,
   relation evaluation, execution counts, peak memory, and end-to-end time
   when the substage affects them.
5. Avoid adding a runtime dependency unless a benchmark or substantial
   correctness/maintenance benefit justifies it.

## Documentation and code-comment standard

Implementation must be reviewable months later without reconstructing the
original conversation.

- Follow GenMC's comment convention: source comments use `/* ... */` or
  Doxygen `/** ... */`, not `//` comments.
- Every new source/header begins with GenMC's existing dual Apache-2.0/MIT
  license banner and uses the neighboring include-guard convention.
- Every new class has a Doxygen comment describing responsibility,
  ownership/lifetime, invariants, and thread-safety.
- Every public function and every non-trivial private function documents its
  purpose, parameters, return value, errors, preconditions, side effects, and
  relevant complexity.
- Parser AST nodes and relational operators document their CAT meaning.
- Each logical implementation block has a short comment explaining the
  algorithmic intent or invariant. Comments should explain *why* and the CAT
  semantics, not restate obvious C++ syntax.
- Non-obvious representations document indexing, invalidation, and memory
  layout.
- Optimizations cite the baseline operation they replace and state why they
  preserve semantics.
- Tests state the behavior or regression they protect.
- Public-facing behavior changes update the manual and CLI help in the same
  substage.

## GenMC repository conventions

The following rules are taken from `doc/development.md` and repository
configuration and are mandatory:

- Run the repository `.clang-format`; it uses tabs of width 8, a 100-column
  limit, and the checked-in include-order policy.
- Run relevant `.clang-tidy` checks when a substage introduces production C++.
- Use CamelCase except for STL-like utilities; names start lowercase and data
  members end in `_`.
- In classes, order declarations public → protected → private, and within a
  section use: types/aliases, static constants/factories, constructors and
  assignment/destructor, other functions, then static and non-static data.
- Use `VERIFY`, `ASSERT`, and `UNREACHABLE` from
  `genmc/genmc/Support/Error.hpp` according to their documented release/debug
  semantics; do not introduce ad-hoc assertion behavior.
- Submit changes as atomic, easily reversible commits.
- Put public user documentation under `doc/manual/` and development/project
  documentation under `doc/`; do not create a parallel top-level `docs/`
  hierarchy.
- Follow neighboring source placement and CMake organization under
  `genmc/genmc/`, `lli/`, and `tests/`.

## Substage execution loop

Every substage follows this loop:

1. **Read constraints and plan.** Record starting commit and dirty files.
2. **Survey reuse candidates.** Record source, license, reusable part, and
   decision.
3. **State the substage contract.** Name inputs, outputs, unsupported cases,
   files expected to change, and verification commands.
4. **Implement the smallest coherent target.** Do not mix later-stage CAAT
   work into Phase 1 unless required by the current contract.
5. **Verify.** Run unit tests, focused integration tests, relevant regression
   tests, format/lint checks, and differential tests where available.
6. **Analyze the gap.** Compare delivered behavior against the current phase
   plan. Classify gaps as correctness, compatibility, performance,
   documentation, or deferred scope.
7. **Update records.** Record commands, counts, timings, changed and untouched
   files, known risks, and the next target.
8. **Commit and push.** Use one Conventional Commit for the coherent substage,
   push `genmc-caat`, and record the commit SHA and remote result.

A substage is not complete until its implementation, tests, documentation,
gap analysis, commit, and push have all succeeded.

## Git and change-scope rules

- Work only on `genmc-caat` unless the user explicitly requests another
  branch.
- Before staging, inspect `git status --short` and the full diff.
- Stage only files belonging to the current substage. Preserve unrelated user
  changes.
- Use Conventional Commits and do not add co-author footers unless requested.
- Never use destructive reset/checkout commands to clean user changes.
- If push is rejected, inspect the remote change before rebasing; resolve
  conflicts without discarding either side.
- Record the commit SHA and pushed remote ref in the progress log.

## Required records

The project keeps these durable artifacts:

- `task_plan.md`: cross-phase status and current target.
- `notes.md`: research evidence and architectural findings.
- `genmc-caat-feasibility.md`: feasibility and three-phase rationale.
- `doc/cat/PROJECT_CONSTRAINTS.md`: persistent execution constraints.
- `doc/cat/phase-1-plan.md`: Phase 1 work breakdown and acceptance criteria.
- `doc/cat/progress/phase-1.md`: append-only Phase 1 substage results, gap
  analyses, commit SHAs, and push status.
- `doc/cat/phase-2-plan.md`: Phase 2 offline CAAT scope and acceptance criteria.
- `doc/cat/progress/phase-2.md`: append-only Phase 2 substage results, gap
  analyses, commit SHAs, and push status.
- `doc/cat/phase-3-research-question.md`: Phase 3 hypothesis, evidence and
  falsification contract.
- `doc/cat/phase-3-plan.md`: Phase 3 incremental/online scope, proof boundary,
  substages and acceptance criteria.
- `doc/cat/progress/phase-3.md`: append-only Phase 3 substage results, gap
  analyses, commit SHAs, and push status.

## Stop conditions

Stop and request a decision before:

- changing the accepted CAT semantics to accommodate an implementation
  shortcut;
- copying code with uncertain or incompatible license terms;
- introducing a large runtime dependency;
- weakening GenMC completeness or existing model behavior;
- deleting or rewriting user-owned work;
- weakening the Phase 3 monotonicity gate to make a non-monotone model appear
  incrementally supported.
