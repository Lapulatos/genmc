# V9 direct preventive-root correctness argument

## Scope

V9 changes only how an already analyzer-certified preventive order is evaluated before
RF/CO alternatives are queued. The authoritative CAT/CAAT consistency backend, candidate
certificate, host exploration algorithm, and all-pruned fallback remain unchanged.

The direct path is enabled only when all of the following hold:

1. `Analyzer::buildPreventiveOrders()` proves that every choice-dependent contribution
   to the checked order is exactly one of the recognized RF/FR/CO forms;
2. `Analyzer::buildLazyCyclePlans()` proves that the complete order expression is in the
   exact lazy grammar and that its non-base cone is exclusively observed by its acyclic
   check;
3. lazy evaluation is enabled and oracle/explanation modes are disabled;
4. every base predicate reachable from the selected order is materialized exactly.

Otherwise the code retains the previous complete evaluator path or fails open.

## Exact current order

`StableGraphAdapter` maps every active GenMC event and address-specific initial write to
a persistent ID and materializes the selected root's primitive cone. Stable IDs may
differ from the authoritative synchronizer's IDs, but the mapping is injective and all
root operations are invariant under this renaming.

`findLazyCycle()` interprets base, alias, union, composition, supported intersection,
identity and optional nodes extensionally. Its materialized bidirectional CSR therefore
equals the normalized model's selected order on the current prefix. The randomized
`LazyCyclePlanMatchesMaterializedRelation` property checks both the complete relation
and cycle witness against the fully materialized CAAT evaluator.

If the selected root is cyclic, V9 returns no preventive result. It never uses a partial
CSR or a cycle witness to remove a candidate.

## Sufficient rejection theorem

Let `O` be the exact acyclic selected order on the current prefix and let one proposed
RF/CO choice add an analyzer-reconstructed edge `(u,v)` to `O`. If V9 finds
`v O+ u`, then `O union {(u,v)}` contains the cycle

`u -> v O+ u`.

The checked CAT order is therefore cyclic for that candidate. Adding further events or
positive relation facts cannot remove this cycle, so no completion containing the
choice can satisfy the checked axiom. Rejecting the alternative before WorkList enqueue
preserves every CAT-consistent execution and every property violation reachable through
such an execution.

RF handling reconstructs direct RF and induced FR edges exactly as declared by the
certificate. CO handling reconstructs all earlier-to-new and new-to-later edges of the
strict per-location order. Unsupported seed forms never receive a certificate.

The proof does not require evaluating unrelated checks. If another check already rejects
the prefix, the proposed candidate is invalid independently; if it does not, the cycle
above is sufficient. Thus bypassing the remaining fixed point/check/history work changes
only cost, not the set of accepted candidates.

## Required empirical evidence

- Release and GCC 13 ASan+UBSan unit/property suites;
- 39-row / 5,441-query full-recomputation mutation oracle;
- 288-program x SC/TSO/PSO broad differential with preventive pruning on PSO;
- four balanced 24+24-worker repetitions over the formal 96-task PSO corpus;
- zero verdict, safe-execution-count, and candidate-space-counter difference from V8;
- direct-check count equal to preventive prefix count on the activated path;
- elimination of V8's per-prefix full-evaluator work and the early-error outlier.

