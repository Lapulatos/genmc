# Phase 1 CAT Fixture Manifest

This manifest maps every frozen language feature to a future unit or
integration fixture. Test paths are contracts for substages 1.2-1.9; they may
be adjusted to the neighboring GoogleTest layout when the test sources are
added, but coverage must not be removed silently.

## Acceptance models

| Model | File | Primary oracle | Required distinction |
|---|---|---|---|
| SC | `models/cat/sc.cat` | built-in `--sc`, then herd litmus outcomes | SB has 3 complete executions |
| TSO | `models/cat/tso.cat` | built-in `--tso`, then herd litmus outcomes | SB has 4 complete executions |
| PSO | `models/cat/pso.cat` | herd official PSO ppo plus model-only GenMC test | W→W on different locations may relax |

Model provenance and semantic limits are recorded in
`doc/cat/supported-cat.md`. The models are clean-room project inputs, not
copies of the CeCILL-B herdtools7 library.

## Lexer and parser coverage

| Feature | Positive fixture | Negative fixture or assertion |
|---|---|---|
| identifier/string model header | all three models; `header-string.cat` | missing header, invalid escape |
| whitespace and all comment forms | `comments.cat` | unterminated nested comment |
| `include` | `include/root.cat` | missing file, directory, cycle chain |
| non-recursive `let` | all three models | missing `=`, `let rec` unsupported |
| `acyclic`/`irreflexive`/`empty` | `checks.cat` | missing expression/name |
| optional `as` name | all three models | duplicate check name |
| `|`, `&`, `\`, `;` | all three models; `precedence.cat` | missing left/right operand |
| `[S]`, Cartesian `*` | TSO/PSO; `set-rel.cat` | unmatched bracket/invalid type |
| `^-1`, `?`, `+`, postfix `*` | `closures.cat` | malformed inverse/postfix chain |
| `0`, `_`, parentheses | `constants.cat` | unmatched parenthesis |
| source spans | every negative fixture | exact path, line, and column |
| unsupported keyword recognition | `unsupported.cat` | category is `unsupported`, not `parse` |

## Resolver and type coverage

| Contract | Positive assertion | Negative assertion |
|---|---|---|
| built-in sets and relations | every name in supported table resolves | unknown and reserved names fail |
| conventional aliases | `rfe`, `fre`, `coe`, `po-loc`, `mo` | aliases cannot be rebound |
| definition order | earlier binding is visible | forward reference and duplicate fail |
| set operations | union/intersection/difference preserve `set` | mixed set/relation fails |
| relation operations | composition/closures preserve `rel` | set composition/closure fails |
| product and identity restriction | `R * W`, `[R]` produce `rel` | relation product and `[po]` fail |
| check operands | relation checks and set/relation emptiness | `acyclic R` and `irreflexive W` fail |
| stable IR | golden node/type/source summary | no pointer values or platform paths |

## Evaluator coverage

Every operation receives example tests and RapidCheck comparison against a
simple `std::set` reference implementation.

| Operation/check | Required cases |
|---|---|
| set/rel union, intersection, difference | empty, disjoint, overlapping, equal |
| product and `[S]` | empty, singleton, multiple events |
| composition and inverse | empty, chain, fork/join, non-symmetric |
| optional and closures | empty universe, singleton, cycle, disconnected graph |
| `acyclic` | DAG, self-loop, multi-node cycle, named witness |
| `irreflexive` | diagonal absent/present |
| `empty` | empty/non-empty set and relation |
| memoization | shared subexpression evaluated once per graph version |

## Graph-adapter coverage

Synthetic graphs must cover reads, writes, initial writes, fences, RMWs,
same/different thread, same/different location, thread create/start and
finish/join. Each built-in set/relation is compared with direct
`ExecutionGraph` queries. Debug checks cover endpoint membership, functional
`rf`, per-location `co`, and `fr = rf^-1 ; co`.

## Differential smoke set

The fixed initial smoke set uses generated variants already committed in the
GenMC test tree:

| Test | Path | SC baseline | TSO baseline | Purpose |
|---|---|---:|---:|---|
| SB | `tests/correct/litmus/SB/variants/sb0.c` | 3 | 4 | TSO W→R relaxation |
| LB+ctrl | `tests/correct/litmus/LB+ctrl/variants/lb+ctrl0.c` | 3 | 3 | preserved R-originating order |
| WWR+2WR | `tests/correct/litmus/WWR+2WR/variants/wwr+2wr0.c` | 0 complete, 8 blocked under driver | same | assume/blocking behavior |
| WW+RR | `tests/cat/programs/WW+RR.c` | 3, safe | 3, safe | PSO alone reaches Ry=1/Rx=0 and violates the assertion |

Phase 1.6-1.8 expand this to litmus classes for `po`, `rf`, `co`, `fr`, RMW
atomicity, fences, same-location order, cross-location W→W, SC accesses, and
thread lifecycle. Differential comparison includes exit status, complete and
blocked execution counts, reported errors, and allowed outcomes.

## CLI and include fixtures

- readable root path reaches the current substage boundary;
- missing root and include paths fail before compiling the program;
- `--model-file` repeated or combined with an explicit built-in model fails;
- default invocation without `--model-file` is unchanged;
- relative include resolution is independent of the current working directory;
- absolute includes work; implicit herd library lookup does not occur;
- include cycles print every file and include site in order.

## Performance records

- parser: repeated parse/lower of all three acceptance models, files/s and
  peak resident memory;
- evaluator: sparse and dense synthetic relations at increasing event counts;
- adapter: construction time and memory by graph size;
- end-to-end: built-in versus CAT SC/TSO on the fixed smoke set and selected
  larger tests.
