# Generic CAT-derived generation-time preventive pruning

## Evidence and target

The complete 96-task census found that only 0.09% of realized SC/TSO revisit prefixes
were rejected by consistency, but 301,191 of 398,054 PSO prefixes (75.67%) were rejected.
PSO also retained a maximum of 51,601 queued work items. The next optimization therefore
targets RF/CO choice generation before enqueueing, not consistency-query memoization.

## Structural certificate

An optimization is admitted only when normalized CAT structure proves all of the
following, independently of model name, file path, host profile, or verdict:

1. an `irreflexive reach` check exists;
2. the least fixed point defining `reach` has exactly one of the forms
   `seed | reach ; seed` or `seed | seed ; reach` after normalized generated nodes;
3. `seed` is a union of choice-independent relations and an automatically recognized
   subset of `rf`, `rf & ext`, `fr`, and `co`;
4. no other seed operand depends on RF, FR, or CO.

Unsupported recursion, intersections, differences, closures, or choice-sensitive
derived leaves reject the certificate and retain generic enumeration.

## Pruning theorem

For a consistent prefix with exact current `reach = seed+`, an RF or CO choice may be
removed only if one concrete seed edge introduced by that choice is `(u,v)` and current
reach already contains `(v,u)`. The extension necessarily contains a cycle. This is a
sufficient rejection proof, not an approximation: absence of a reversal keeps the
candidate. If every choice has a proof, retain one known-invalid representative only to
drive GenMC's existing non-empty-candidate control path; do not enqueue the other
known-invalid choices.

This preserves every CAT-consistent execution. Certificate failure, missing stable IDs,
an inconsistent prefix, unavailable incremental state, or any unsupported form falls
back without pruning.

## Gates and metrics

1. structural positive/renamed/reordered and negative semantic-mutation unit tests;
2. Release and GCC 13 ASan+UBSan suites;
3. 39-row / 5,441-check mutation oracle;
4. 864-pair SC/TSO/PSO broad differential with zero mismatch;
5. direct 48-worker formal comparison, followed by the full adapted 60-second corpus;
6. compare candidates offered/queued, realized/rejected prefixes, complete executions,
   TIMEOUT/OOM, CPU, wall time, RSS, CAT queries, offline evaluations, and peak worklist.

Any status, verdict, complete-execution or oracle mismatch rejects the optimization.
