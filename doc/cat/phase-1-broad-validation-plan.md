# Phase 1 broad false-positive/false-negative validation plan

## Objective

Search for false positives and false negatives in the Phase 1 CAT-backed
checker with at least 200 distinct concurrent programs. Every discrepancy must
be reproduced, classified, and explained before it can be accepted or fixed.

## Frozen local corpus

The primary corpus is repository-owned and therefore exercises GenMC's actual
C/LLVM transformation and execution path. At starting commit
`de9c60f10606c1d2038737e6a4c0e42490050d7d` it contains 288 distinct selected
variant source files:

| Class | Directory | Programs | Purpose |
|---|---|---:|---|
| litmus | `tests/correct/litmus` | 157 | weak-memory cycles, fences, RMW, dependencies, lifecycle edges |
| infrastructure | `tests/correct/infr` | 47 | pthread/runtime primitives and larger control flow |
| data structures | `tests/correct/data-structures` | 28 | queues, stacks, locks, barriers, and larger executions |
| expected safety failures | `tests/wrong/safety` | 5 | assertion/error detection |
| expected races | `tests/wrong/racy` | 28 | non-atomic race reporting |
| expected memory failures | `tests/wrong/memory` | 23 | invalid memory behavior and error-path preservation |

Each source is checked under both SC and TSO. Each model runs the built-in
checker and the corresponding CAT model with identical compiler and GenMC
arguments, producing at least 1,152 verifier invocations.

## Oracle hierarchy

1. Compare built-in SC/TSO with CAT SC/TSO for process status, complete and
   blocked execution counts, error category, warning category, and bound
   statistics.
2. For SC correct tests, compare the built-in result with the repository's
   `expected.sc.mo.in` count when present. This prevents two implementations
   from agreeing on a newly introduced common regression.
3. Re-run every mismatch in isolation and inspect both complete outputs.
4. Reduce memory-model mismatches to an aligned litmus and query herd when its
   event semantics match. Do not accept a herd result without checking the
   actual cycle/outcome against the CAT equations and GenMC graph.
5. Treat compiler/language-error differences separately from memory-model
   consistency differences.

## Classification

| Classification | Meaning | Required action |
|---|---|---|
| `match` | semantic signatures and statuses agree | retain as regression evidence |
| `oracle-invalid` | built-in SC result contradicts its exact repository SC expectation | fix invocation or exclude with a recorded reason; never count as a pass |
| `unsupported-baseline` | both paths fail before producing verification semantics | record and exclude from the validated-program count |
| `status-mismatch` | one checker accepts/terminates differently | P0 investigation |
| `signature-mismatch` | execution/error/warning summaries differ | P0 investigation |
| `timeout` or crash | either checker does not complete normally | P0 investigation or recorded corpus limitation |

The target is zero unexplained mismatches and at least 200 distinct programs
whose oracle invocations are valid. Equal aggregate counts alone are not a
proof for arbitrary programs, but a mismatch is always actionable evidence.

## Reproducibility

The harness is `tests/cat/broad-differential.sh`. It discovers only the six
frozen roots above, sorts canonical repository-relative paths, records the
source SHA-256, uses the first SC argument configuration where metadata exists,
and writes one TSV row per source/model pair. Full output is retained only for
discrepancies to keep the report reviewable.
