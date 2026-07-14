# Phase 2 plan: offline CAAT backend

## 1. Objective and completion boundary

Phase 2 extends the Phase 1 CAT path with an offline CAAT backend. Given one
complete GenMC execution graph, the backend must:

1. normalize the supported consistency model into one-operation predicate
   equations;
2. validate dependencies, polarity, stratifiability, domain independence, and
   semi-positivity;
3. compute recursive set/relation predicates as the canonical stratified least
   fixed point;
4. check `empty`, `irreflexive`, and `acyclic` axioms; and
5. return a source-located violation plus base-predicate explanations.

The backend remains a from-scratch, full-graph checker. Incremental updates,
backtracking, trail validity, and early pruning belong to Phase 3. Phase 1
models and results must remain unchanged.

“Supports CAAT” in this phase means the normalized, domain-independent,
stratifiable and semi-positive consistency language defined by Sections 2--4
of the CAAT paper, mapped to GenMC's available base predicates. It does not
mean all herd CAT commands, functions, scopes, or candidate-generation
constructs. Non-semi-positive models require an explicit cut; automatically
encoding cut predicates into an SMT layer is not available in GenMC Phase 2
and must not be simulated silently.

## 2. Evidence and reuse decision

| Source | Revision/license | Reused idea | Decision |
|---|---|---|---|
| Phase 1 typed relational IR/evaluator | this repository, Apache-2.0/MIT | values, operators, graph adapter, witnesses | extend in place and keep Phase 1 evaluator as differential oracle |
| CAAT paper and artifact | OOPSLA 2022 / paper CC BY 4.0 | normalization, SCC stratification, least fixed points, explanations, semi-positivity | normative algorithm/semantics reference |
| Dat3M | `a7e3e4843359dde3a0e29500a821030e2433e316`, MIT | predicate hierarchy, recursive placeholders, worklist propagation, derivation-based reasoner | design and test oracle; implement idiomatic dependency-free C++23 rather than add Java/JavaSMT |
| herdtools7 | external CeCILL-B tool | CAT parsing/results on aligned models and litmus tests | behavioral oracle only; no source copied |
| generated GenMC checkers | this repository | accepted execution counts and diagnostics | SC/TSO differential oracle |

Dat3M's MIT license is compatible, but direct source translation would import
Java object/layout assumptions and make review harder. Any substantially copied
algorithm or test will be identified explicitly; otherwise the implementation
is based on the paper and existing GenMC representations.

## 3. Supported CAAT consistency language

Phase 2 targets the paper's predicate language over finite execution domains:

- set and relation base predicates supplied by `CATGraphAdapter`;
- union, intersection, difference, composition and inverse;
- identity restricted to a set (`[S]`), Cartesian product, `domain`, and
  `range`;
- transitive and reflexive-transitive closure;
- forward references and `let rec ... and ...` mutually recursive groups;
- `empty` over sets/relations, `irreflexive`, and `acyclic` over relations.

The frontend must diagnose, with source spans:

- undefined, duplicate, or ill-typed predicates;
- recursive definitions outside a declared recursive group;
- negative dependency inside an SCC (non-stratifiable recursion);
- a derived right operand of normalized difference (non-semi-positive), with
  the predicate that would need cutting;
- an axiom that fails the paper's syntactic domain-independence analysis; and
- CAT features outside the declared Phase 2 language.

## 4. Architecture

```text
CAT source / Phase 1 AST
    -> symbol collection and forward-reference resolution
    -> normalized predicate equations (one operator per equation)
    -> signed dependency graph + Tarjan SCCs
    -> stratification/admissibility report
    -> offline CAAT evaluator
         base values from CATGraphAdapter
         strata in topological order
         worklist least fixed point inside recursive SCCs
    -> axiom violations
    -> derivation DAG / base-literal explanations
    -> CATChecker consistency result + optional CLI explanation
```

Normalized predicate IDs are stable within a compiled model. Each derived
membership stores a shortest known derivation (operator and antecedent
memberships). Explanations recursively project a violation to positive or
negative base set/relation literals and remove duplicates. This is offline
provenance; no derivation may refer to a graph revision other than the snapshot
being evaluated.

## 5. Substages and atomic delivery

### Phase 2.0: specification and baseline

- Freeze this plan, Dat3M revision/license, CAAT semantic boundary, and test
  matrix.
- Record a clean Phase 1 build and existing 288-program differential result.
- Deliver documentation only, then commit and push.

Acceptance: reviewers can decide exactly what Phase 2 accepts, rejects, and
defers without reading the implementation.

### Phase 2.1: recursive frontend and normalized IR

- Parse forward references and `let rec ... and ...` groups without weakening
  duplicate/reserved-name diagnostics.
- Add CAAT operators `domain`, `range`, Cartesian product, and identity/base
  domain forms needed by the paper grammar.
- Lower every complex expression and axiom operand into one-operation typed
  equations with stable source provenance.
- Retain the original Phase 1 DAG for non-recursive models until equivalence is
  demonstrated.

Acceptance: golden normalized models, malformed-group tests, type tests, and
normalization property tests pass; SC/TSO/PSO summaries and CLI behavior remain
unchanged.

### Phase 2.2: dependency, polarity, and admissibility analysis

- Build signed dependencies and deterministic Tarjan SCCs.
- Produce strata and reject negative recursion.
- Implement syntactic domain-independence and normalized semi-positivity
  checks, including actionable cut diagnostics.
- Publish an immutable analysis report shared by evaluator workers.

Acceptance: positive/mutually recursive, negative-recursive,
non-semi-positive, and domain-dependent fixtures agree with hand proofs and
CAAT paper examples.

### Phase 2.3: stratified least-fixed-point evaluator

- Evaluate non-recursive strata once and recursive SCCs with a delta worklist.
- Support recursive sets and relations over existing packed values.
- Track convergence statistics and enforce finite-domain invariants without an
  arbitrary iteration limit.
- Keep the Phase 1 evaluator callable as an oracle for acyclic models.

Acceptance: property tests compare against an independent naive Kleene
oracle; recursive transitive-closure, single-recursion, mutual-recursion, empty
fixed-point, and nested-operator fixtures converge to exact expected values.

### Phase 2.4: violation explanations and CLI diagnostics

- Record derivations for derived set elements and relation edges.
- Explain `empty` witnesses, irreflexive loops, and acyclic cycles using base
  literals, including negative literals for semi-positive difference.
- Deduplicate/dominance-reduce explanations deterministically.
- Render a concise source-located explanation when requested, while preserving
  default GenMC output compatibility.

Acceptance: replaying every explanation's base literals reproduces the
violation in an independent evaluator; malformed or stale explanations fail
tests rather than being printed.

### Phase 2.5: GenMC integration and model corpus

- Select the offline CAAT backend for admissible recursive models; keep
  non-recursive Phase 1 models behaviorally stable.
- Add representative recursive models derived independently from paper/public
  CAT definitions, subject to GenMC base-predicate availability.
- Fail before exploration for unavailable base predicates or inadmissible
  models.

Acceptance: real concurrent programs exercise recursive models end to end;
worker counts 1 and 2 agree; SC/TSO/PSO Phase 1 differential tests remain exact.

### Phase 2.6: broad validation and closure

- Run all unit/property/integration tests and the frozen 288-program Phase 1
  differential suite.
- Build a recursive-model corpus with at least 200 distinct program/model
  pairs spanning safe/error outcomes, recursion shapes, RMW, lifecycle,
  dynamic memory, and larger data structures.
- Compare aligned cases with herd and, where executable semantics match, Dat3M;
  isolate and explain every mismatch before accepting or fixing it.
- Record parser/evaluator throughput, fixed-point iterations, peak memory, and
  end-to-end time against Phase 1 baselines.
- Complete a requirement-by-requirement gap audit, report, atomic commit, and
  push.

Acceptance: zero unexplained mismatch, no Phase 1 regression, clean worktree,
and local/remote commit equality.

## 6. Test oracle hierarchy

1. Independent unit/property oracle for normalized values and fixed points.
2. Phase 1 evaluator equivalence for non-recursive models.
3. Generated GenMC SC/TSO checkers for end-to-end regression.
4. herd results for aligned CAT/litmus semantics.
5. Dat3M lazy/eager agreement for models/programs that both tools can parse
   with the same base-event meaning.
6. Manual derivation replay for every anomaly and every explanation fixture.

Aggregate verdict agreement alone is insufficient: exact execution counts,
diagnostic classes, fixed-point memberships, and explanation literals are
checked where the oracle exposes them.

## 7. Risks and explicit non-goals

| Risk | Required control |
|---|---|
| negative recursion has no canonical monotone fixed point | reject by signed-SCC analysis; never guess |
| automatic cutting changes the outer search problem | report required cut; defer SMT/on-demand cutting unless a GenMC-compatible contract is designed |
| provenance can explode | shortest derivations plus deterministic reduction; benchmark memory |
| dense full-graph evaluation can be slow | correctness first, retain measurements; incremental optimization is Phase 3 |
| herd/Dat3M event semantics differ from GenMC C/LLVM | use only aligned cases and document mapping |
| broad CAT includes functions/scopes/procedures/flags | keep explicit unsupported diagnostics; do not call this arbitrary CAT support |

## 8. Phase completion checklist

- [x] Phase 2.0--2.6 each have a progress entry, gap analysis, commit, and push.
- [x] Normalized recursive semantics match the CAAT stratified least fixed point.
- [x] Semi-positivity and domain-independence gates are source-located.
- [x] All three axiom types return replayable base explanations.
- [x] At least 200 recursive program/model pairs have zero unexplained mismatch.
- [x] Frozen 288-program Phase 1 suite still has zero mismatch.
- [x] Performance and memory deltas are recorded.
- [x] User/manual/developer documentation states exact support and limits.
- [x] Final branch is clean and equals `origin/genmc-caat`.
