# Supported CAT/CAAT Language

This document is the compatibility contract for the Phase 1 CAT frontend. It
freezes the syntax, types, built-ins, include behavior, and diagnostics needed
by the bundled SC, TSO, and PSO models. Constructs not listed here are not part
of Phase 1 even when herdtools7 accepts them.

Phase 2 additionally accepts forward references and explicitly declared
`let rec ... and ...` groups after normalization and stratification. Phase 3
maintains positive normalized predicates incrementally across GenMC graph
growth and retained-ancestor rollback. This later backend changes evaluation
strategy, not the grammar, base-event mapping, or consistency axioms below.

The normative language reference is *Syntax and Semantics of the Weak
Consistency Model Specification Language Cat* (2016). The herdtools7 lexer,
parser, and library models are behavioral references only. No herdtools7
source is copied into GenMC: herdtools7 is primarily CeCILL-B licensed, while
GenMC is dual Apache-2.0/MIT.

## Compatibility levels

A model passes four distinct gates:

1. **Lexically and syntactically valid:** all tokens and productions belong to
   the grammar below.
2. **Well-typed:** every operator and check has the required operand type.
3. **Phase-1 supported:** the model uses no construct explicitly deferred
   below.
4. **GenMC executable:** every primitive can be built and every check passes
   the conservative online-admissibility gate described below.

Diagnostics identify the failed gate. A syntactically valid full-CAT program
can therefore still receive an `unsupported` diagnostic.

## Source text and model header

- Input is UTF-8. Phase 1 identifiers and keywords use the ASCII subset.
- The first non-comment token is the model name. It is one identifier or one
  quoted string. Bundled models use the identifiers `SC`, `TSO`, and `PSO`.
- Whitespace separates tokens but is otherwise insignificant.
- Newlines are tracked for diagnostics but are not statement terminators.
- `# ...` and `// ...` comments extend to the end of the line.
- `(* ... *)` comments may span lines and nest.
- A quoted string supports `\"`, `\\`, `\n`, `\r`, and `\t`. Other escapes
  are rejected instead of being interpreted differently across platforms.
- Identifiers match `[A-Za-z_][A-Za-z0-9_.-]*` with an optional trailing
  apostrophe. Reserved keywords and built-in names cannot be rebound.

### GenMC host-profile metadata

An optional leading block comment declares which existing GenMC causal-view
profile supports exploration around the generic full-graph evaluator:

```cat
(* @genmc host-profile sc *)
(* @genmc host-profile tso *)
```

The declaration must appear before the root model header, may occur once, and
is forbidden in included fragments. A file without it defaults to `sc` for
compatibility with Phase 1.6. Unknown profiles fail before LLVM execution.

This is explicit GenMC execution metadata, not a consistency axiom: the CAT
checks still decide whether a candidate graph is consistent. It is encoded as
a CAT block comment so herdtools7 ignores it and can read the same model. The
checker factory dispatches on the typed metadata enum only; model paths,
filenames, headers, binding names, and check names are never inspected.

## Grammar

The following EBNF describes the accepted Phase 1 grammar. `IDENT`, `STRING`,
and `EOF` are lexer tokens.

```ebnf
model         = model_name, statement*, EOF ;
model_name    = IDENT | STRING ;

statement     = include_stmt | let_stmt | check_stmt ;
include_stmt  = "include", STRING ;
let_stmt      = "let", IDENT, "=", expression ;
check_stmt    = check_kind, expression, [ "as", IDENT ] ;
check_kind    = "acyclic" | "irreflexive" | "empty" ;

expression    = union_expr ;
union_expr    = compose_expr, { "|", compose_expr } ;
compose_expr  = difference_expr, { ";", difference_expr } ;
difference_expr
              = intersection_expr, { "\\", intersection_expr } ;
intersection_expr
              = product_expr, { "&", product_expr } ;
product_expr  = postfix_expr, { "*", postfix_expr } ;
postfix_expr  = primary_expr, { "^-1" | "?" | "+" | "*" } ;
primary_expr  = IDENT
              | "0"
              | "_"
              | "[", expression, "]"
              | "(", expression, ")" ;
```

The lexer treats `^-1` as one inverse operator even if the implementation uses
multiple internal tokens. A binary `*` is Cartesian product and requires two
sets. A postfix `*` is reflexive-transitive closure and requires a relation.
The parser resolves the role from position; typing confirms it.

### Precedence and associativity

From highest to lowest precedence:

| Level | Operators | Associativity | Meaning |
|---:|---|---|---|
| 1 | `^-1`, `?`, postfix `+`, postfix `*` | postfix | inverse and closures |
| 2 | binary `*` | left | Cartesian product of event sets |
| 3 | `&` | left | intersection |
| 4 | `\` | left | difference |
| 5 | `;` | left | relation composition |
| 6 | `|` | left | union |

Parentheses are recommended in model files whenever two levels occur in one
check. These precedence choices agree with the expressions used by the
bundled models and avoid relying on herd's right-associative implementation
details for associative operators.

## Types and operator rules

Phase 1 has two value types:

- `set`: a finite set of events;
- `rel`: a finite binary relation over events.

There are no implicit conversions except `[S]`, which explicitly converts a
set to an identity relation.

| Expression | Required operands | Result | CAT meaning |
|---|---|---|---|
| `A | B` | same type | same type | union |
| `A & B` | same type | same type | intersection |
| `A \ B` | same type | same type | set or relation difference |
| `A * B` | `set`, `set` | `rel` | Cartesian product `A × B` |
| `r ; s` | `rel`, `rel` | `rel` | relational composition |
| `r^-1` | `rel` | `rel` | relational inverse |
| `r?` | `rel` | `rel` | `id | r` over the graph event universe |
| `r+` | `rel` | `rel` | transitive closure |
| `r*` | `rel` | `rel` | reflexive-transitive closure |
| `[S]` | `set` | `rel` | `{(e,e) | e in S}` |
| `domain(r)` | `rel` | `set` | sources of edges in `r` |
| `range(r)` | `rel` | `set` | targets of edges in `r` |

`acyclic` and `irreflexive` require a relation. `empty` accepts a set or a
relation. A check without `as` receives a stable generated name based on its
source location. Named checks must have unique names after includes expand.

Bindings are non-recursive and are visible only after their definition.
Bindings cannot shadow another binding, a built-in, or a check name. Phase 1
does not infer or iterate recursive definitions.

### Online-admissibility gate

The parser, typed IR, and from-scratch evaluator support set/relation
difference. GenMC's Phase 1 exploration path does not accept a difference node
that can reach a consistency check. Difference is antitone in its right
operand: as a graph prefix gains events or edges, `A \ B` can shrink, so a
violation observed at a prefix may disappear later. Pruning that prefix would
therefore be unsound without the polarity/fixed-point analysis planned for
offline CAAT.

The immutable IR computes reachability backward from checks and reports the
first reachable difference before LLVM execution. An unused binding may still
contain difference for offline evaluator use. The bundled SC, TSO, and PSO
models contain no reachable difference and pass this gate. This restriction
changes online admissibility, not CAT parsing, typing, or from-scratch value
semantics.

Phase 1 also rejects `--model-file` with Relinche collection/checking options.
Relinche asks the checker for a refinement-specific coherence predicate; the
available implementation belongs to the generated SC/TSO host rather than the
arbitrary CAT model. Failing early avoids silently applying the wrong model.

Phase 3 admits normalized recursion only when every equation is positive under
insertion. Difference remains excluded from the online boundary, including
semi-positive offline uses: inserting an RHS fact can remove a derived fact and
repair a prefix violation. Such a model is rejected before exploration rather
than being used for unsound pruning. Positive `empty`, `irreflexive`, and
`acyclic` witnesses persist until rollback and may reject a prefix immediately.

The online checker gives real events stable `(thread,index)` identities and
virtual initial writes stable address identities. It classifies each query as
initialize, unchanged, insertion, rollback, rollback-plus-insert, or rebuild;
mixed deletion/replacement such as an unsupported `rf`/`co` mutation uses the
Phase 2 full evaluator. `--cat-stats` exposes these transitions and
`--cat-oracle` checks every incremental result against a fresh Phase 2 fixed
point. Both diagnostics are disabled by default.

## Built-in event sets

The event universe `_` contains the execution-graph labels exposed by the
Phase 1 graph adapter. The exact inclusion policy is tested in the adapter
substage; internal bookkeeping objects that are not `EventLabel`s are never
events.

The adapter assigns dense IDs to real labels in `ExecutionGraph::labels()`
insertion order, then appends virtual initial writes in sorted-address order.
Every concrete label except the address-polymorphic `InitLabel`, including
thread lifecycle, block, allocation, and other model-neutral labels, belongs
to `_`; internal `EmptyLabel` placeholders do not. Labels not classified
below remain usable through `_`, `po`, `int`, and `ext` instead of causing
model-specific rejection. Emitted non-atomic `ReadLabel`/`WriteLabel` objects
are ordinary members of `R`/`W`.

| Name | Type | Phase 1 meaning |
|---|---|---|
| `_` | `set` | all exposed events |
| `M` | `set` | memory accesses, `R | W` |
| `R` | `set` | read labels, including the read side of an RMW |
| `W` | `set` | write labels, including initial writes and the write side of an RMW |
| `F` | `set` | fence labels |
| `IW` | `set` | initial write labels |
| `SC` | `set` | events whose GenMC ordering is sequentially consistent |

An RMW event can belong to both `R` and `W`. `IW` is a subset of `W`.

GenMC represents a successful RMW with adjacent read-side and write-side
labels, so the two labels respectively belong to `R` and `W`, with an `rmw`
edge between them. GenMC uses one `InitLabel` to represent every address, but
CAT relation composition must retain the address. The adapter therefore
expands it into one virtual `IW`/`W` event per tracked location. Each virtual
event maps back to `InitLabel` plus its address, reads-from only reads at that
address, precedes only same-address writes in `co`, and is `loc`-related only
to same-address memory events. Virtual initial writes share a synthetic
initialization thread: they are `int` with each other, `ext` with real events,
and are outside `po`.

## Built-in relations and aliases

| Name | Type | Phase 1 meaning |
|---|---|---|
| `0` | `rel` | empty relation |
| `id` | `rel` | identity over `_` |
| `po` | `rel` | strict program order exposed by GenMC |
| `rf` | `rel` | write-to-read reads-from relation |
| `co` | `rel` | per-location strict coherence order on writes |
| `fr` | `rel` | from-read, defined as `rf^-1 ; co` |
| `rmw` | `rel` | paired read-to-write RMW edges |
| `loc` | `rel` | pairs of memory events accessing the same location |
| `int` | `rel` | same-thread event pairs |
| `ext` | `rel` | different-thread event pairs |
| `tc` | `rel` | thread-create to child-start synchronization |
| `tj` | `rel` | child-finish to thread-join synchronization |

The following conventional CAT names are read-only aliases synthesized from
the primitives. They are included because the selected TSO/PSO models and
differential fixtures use internal/external communication explicitly.

```cat
let rfi = rf & int
let rfe = rf & ext
let coi = co & int
let coe = co & ext
let fri = fr & int
let fre = fr & ext
let po-loc = po & loc
```

`mo` is accepted as a deprecated spelling of `co` for model portability. A
diagnostic note identifies the canonical spelling; model semantics do not
change.

An adapter is valid for one immutable graph snapshot. Any graph change,
including an `rf` or `co` update that does not add/remove a label, requires a
new adapter. The debug structural check detects changed label identity/order,
but GenMC currently has no generation counter that could detect edge-only
mutation.

## Include resolution

Phase 1 deliberately avoids a hidden dependency on a system herd installation.

1. An absolute include names that exact file.
2. A relative include is resolved against the directory of the including
   file, not the process working directory.
3. The resolved path is made absolute and lexically normalized before cache
   lookup and cycle detection.
4. There is no implicit herdtools7 library search path and no network lookup.
5. Includes are expanded at their source position. The parser may cache a
   file's syntax tree, but name resolution behaves as textual inclusion.
6. A file already on the active include stack is an include cycle. The
   diagnostic prints every include site in the cycle.
7. Re-including a file after it has left the active stack is allowed; duplicate
   definitions are then diagnosed by the ordinary name rules.

Unreadable files, directories used as files, invalid UTF-8, and include cycles
fail before LLVM execution begins.

## Diagnostic categories

All model diagnostics use `path:line:column`, a concise category, a primary
message, and the relevant source line when available.

| Category | Examples |
|---|---|
| `io` | missing root file, unreadable include, directory path |
| `lex` | invalid byte, unterminated string/comment, unknown character |
| `parse` | missing operand, unmatched delimiter, malformed statement |
| `include` | include cycle or resolution failure |
| `name` | undefined, duplicate, or reserved name |
| `type` | composing sets, Cartesian product of relations, invalid check type |
| `unsupported` | valid full-CAT construct outside this contract |
| `model` | primitive unavailable for a GenMC graph/profile |
| `note` | accepted portability spelling such as deprecated `mo` |

The frontend reports multiple independent diagnostics from one file when
recovery is unambiguous, but never starts program exploration after any error.

## Explicitly unsupported after Phase 2 integration

- negative recursion and recursion split across incompatible declared groups;
- recursive/forward-reference models whose checked equations contain
  difference: the offline CAAT API accepts the semi-positive fragment, but
  Phase 2 cannot use its negative literals to prune a growing GenMC prefix;
- user functions, lambdas, function application, tuples, and pattern matching;
- procedures, `call`, `forall`, `with ... from ...`, and `linearisations`;
- enums, tags, scopes, instruction declarations, and architecture variants;
- `show`, `unshow`, `flag`, `assert`, negated checks, and `undefined_unless`;
- set literals, element insertion (`++`), complement (`~`), and conditionals;
- arbitrary communication-relation declarations or candidate generation;
- command-line include directories and environment-dependent include paths.

These constructs receive `unsupported`, rather than a misleading syntax or
name error, whenever the parser can recognize them safely.

## Bundled-model contract

The clean-room files under `models/cat/` are the Phase 1 acceptance models.
They are derived from published axiomatic definitions and written specifically
for this project; they are not copied from herdtools7.

- `sc.cat` requires global acyclicity of program order and communication, plus
  the standard RMW atomicity check.
- `tso.cat` preserves read-originating order, order into writes, full-fence
  order, and SC write-to-read order; ordinary write-to-read order may relax.
- `pso.cat` additionally relaxes write-to-write order across locations, while
  preserving same-location write order and fence order.

`tso.cat` is a GenMC-oriented axiomatic TSO profile, not a promise that every
architecture-specific `x86tso.cat` accepted by herd will parse. `pso.cat`
models the classic hardware PSO ordering profile; C/C++ memory-order tags other
than `SC` do not add language-level RC11 semantics.

The Phase 1.7 TSO differential suite uses existing GenMC C/LLVM fixtures for
SB, LB+ctrl, MP, same-location order, RMW atomicity, unordered-write warnings,
and safety errors. CAT TSO matches built-in TSO status, complete-execution
counts, and verdict categories for all seven. Direct herd execution is not
claimed for those C fixtures: herd's official TSO model consumes X86 litmus
events, while GenMC first transforms C/LLVM events. A separate assembly-aligned
oracle under `tests/cat/herd/` runs herd 7.56 with official `x86tso.cat`: SB's
weak outcome is `Sometimes` (1/4 states), while the MP anomaly is `Never` (0/3
states). `tests/cat/herd-tso-oracle.sh` makes that comparison reproducible.

Phase 1.8 adds no PSO-specific C++ path. `pso.cat` explicitly reuses the TSO
host profile and changes only its preserved-program-order equation. On the
same `WW+RR.c` input, changing only the model path gives SC/TSO three safe
complete executions, while PSO reaches the cross-location write-reordering
outcome and reports a safety violation after two complete executions.

The external plain-R/W oracle uses herd's official `x86tso.cat` and
`mips.cat`; the latter explicitly declares a PSO preserved-program-order
choice. With architecture checking disabled only to feed the same X86 event
syntax to both generic CAT models, the MP anomaly is `Never` under TSO and
`Sometimes` under PSO. No architecture-specific fence event participates.
`tests/cat/herd-pso-oracle.sh` reproduces the comparison.

## Phase 2 recursive backend

`let rec ... and ...` groups and acyclic forward references select the
normalized CAAT backend. GenMC computes signed dependencies and SCC strata once
at startup, then recomputes each candidate graph's stratified least fixed point
from scratch. This is the Phase 2 offline algorithm; it has no persistent
backtrackable solver state or delta propagation between graph revisions.

The clean-room `recursive-sc.cat`, `recursive-tso.cat`, and
`recursive-pso.cat` fixtures preserve the corresponding Phase 1 equations and
replace the final order acyclicity with a recursive reachability equation.
`tests/cat/recursive-differential.sh` compares the three pairs across eight
real C programs and both one- and two-worker exploration.

The closing broad comparison uses 288 distinct programs and all three model
pairs, for 864 program/model pairs and 1,728 GenMC invocations.  It records 864
valid matches, zero mismatch, and no unsupported rows.  See
`doc/cat/phase-2-report.md` and
`doc/cat/phase-2-broad-results.tsv` for commands, hashes, and classifications.

The offline evaluator and reasoner support semi-positive difference and
negative base literals. The command-line checker rejects difference in a model
that requires the CAAT backend because a negative fact can become false when a
later prefix adds an edge. Phase 3 must make such facts trail-aware before they
can participate in sound early pruning.
