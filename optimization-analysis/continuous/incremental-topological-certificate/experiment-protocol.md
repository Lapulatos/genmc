# P0.8a rollback-safe incremental topological certificate protocol

## Question

Can an exact, reusable topological-order certificate remove repeated event-level DFS
work from retained lazy CAT/CAAT acyclicity checks without storing the root relation,
changing explored executions, or exceeding the memory saved by P0.7b/P0.7d2?

The baseline is retained commit `d963c49e45b82066ca9dcd036232c7384f6a95fe`.
Optimization 6.1 model/task-name specialization, host-profile selection and built-in
checker verdicts are not permitted.

## Exact semantic contract

For an analyzer-certified lazy root relation `R`, a certificate is a vector `rank` over
the stable event universe. It certifies acyclicity exactly when every extensionally
enumerated edge `(u,v)` in `R` satisfies `rank[u] < rank[v]`. A directed cycle would
imply a strict rank chain returning to its start, which is impossible.

1. A complete existing lazy DFS produces a certificate only when it finds no cycle.
2. A later query first streams the exact current `R`. If every edge respects the cached
   ranks, return acyclic without DFS.
3. A missing/short certificate or the first non-forward edge has no semantic meaning;
   run the retained complete DFS and replace the certificate only if that result is
   acyclic.
4. A cyclic result clears its certificate. A rollback to an earlier acyclic state may
   therefore lose reuse but never reuse an invalid cyclic certificate.
5. A certificate valid for an insertion descendant remains valid after rollback to a
   subgraph: deleting edges cannot violate any retained rank inequality. Replacement
   epochs are validated from scratch or use complete fallback, and GenMC clears their
   old checkpoints.
6. Unsupported CAT operators/checks continue through the retained generic evaluator.
   No certificate is used to produce a positive cycle witness.

## Time and space effects

Expected reductions on a validation hit:

- no color/parent initialization and no recursive or buffered event DFS;
- no repeated DFS tree traversal or cycle-witness bookkeeping;
- no checkpoint or rollback work for the certificate.

Costs and risks:

- every hit still enumerates the complete extensional root relation once;
- a validation miss enumerates a prefix and then reruns the complete exact DFS;
- one persistent `uint32_t` rank per stable event per admitted lazy check, plus vector
  metadata; worst-case certificate storage is `4 * N * C` bytes;
- certificates are evaluator-local and are deliberately excluded from undo trails and
  snapshot-equivalent state, but they contribute to process RSS;
- no claim of reducing candidate executions, root-edge enumeration, TIMEOUT coverage,
  or asymptotic exploration complexity is allowed.

## Correctness gates before performance

1. Unit tests cover empty graphs, forward-order validation, invalid-order fallback,
   cycle creation, universe growth, checkpoint/rollback and replacement/reinitialize.
2. Randomized exact relation tests compare certificate validation/full fallback with the
   retained lazy cycle result and materialized evaluator, including duplicate derivations.
3. Release suite passes in the server image.
4. Focused GCC 13 ASan+UBSan tests pass in the server image.
5. Mutation oracle passes all 39 rows / 5,441 complete comparisons.
6. Broad differential passes 288 programs x SC/TSO/PSO with zero mismatch; mutually
   unsupported pairs remain mutually unsupported.

Any verdict, witness-validity, rollback, oracle or sanitizer failure rejects the
candidate regardless of performance.

## Performance evidence boundary

Do not use a selected fib/queue or other small-program pilot as a performance
advancement gate. Such a sample can overstate a local hit pattern and miss large-task
TIMEOUT/OOM behavior. Existing counters remain mechanism evidence: a full DFS increments
`lazyCycleChecks`; certificate validation candidates contribute to
`lazyEdgeCandidates`; a successful certificate hit does not increment
`lazyCycleChecks`. Do not add production statistics fields solely for the experiment.

The stopped pilot artifacts are audit-only and excluded from retain/reject conclusions.
Correctness authorization comes only from the complete gates above; performance is
decided directly by the formal matrix below.

## Formal retain gate

Run four repetitions over the established 96-task set, SC/TSO/PSO and before/after:
2,304 cells / 1,152 pairs, one core and 4 GiB per cell, 60 CPU seconds, using 48
BenchExec workers.

Retain only if all hold:

- zero terminal-verdict and completed-execution mismatch;
- zero coverage loss and zero new OOM;
- four-repetition model-task CPU geometric-mean bootstrap 95% CI has upper bound below
  1.0;
- each SC/TSO/PSO CPU point ratio is at most 1.01;
- paired RSS P90, P90-of-levels and large-task maximum are at most 1.02;
- snapshot-equivalent and current-base storage remain exact; and
- activation is structural and independent of model/task name, expected answer, host,
  seed, timeout history and built-in checker result.

Candidate-edge counts are work metrics and are not required to match because a failed
validation deliberately adds a streamed prefix before the exact fallback. Semantic
equivalence is instead guarded by full oracle, witness, verdict, execution, rollback and
broad-differential checks.

If any formal gate fails, archive the exact source/test patch, restore the retained
baseline, keep all XML/log evidence, and do not retune the gate after observing results.
