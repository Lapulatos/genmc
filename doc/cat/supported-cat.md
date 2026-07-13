# Phase 1 Supported CAT Language

This document is the compatibility contract for the Phase 1 CAT frontend. It
freezes the syntax, types, built-ins, include behavior, and diagnostics needed
by the bundled SC, TSO, and PSO models. Constructs not listed here are not part
of Phase 1 even when herdtools7 accepts them.

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
4. **GenMC executable:** every primitive requested by the model can be built
   for the selected execution graph.

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

`acyclic` and `irreflexive` require a relation. `empty` accepts a set or a
relation. A check without `as` receives a stable generated name based on its
source location. Named checks must have unique names after includes expand.

Bindings are non-recursive and are visible only after their definition.
Bindings cannot shadow another binding, a built-in, or a check name. Phase 1
does not infer or iterate recursive definitions.

## Built-in event sets

The event universe `_` contains the execution-graph labels exposed by the
Phase 1 graph adapter. The exact inclusion policy is tested in the adapter
substage; internal bookkeeping objects that are not `EventLabel`s are never
events.

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

## Explicitly unsupported in Phase 1

- `let rec`, mutually recursive bindings, and fixed-point definitions;
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
