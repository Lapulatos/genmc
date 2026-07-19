# P2 Gate A1: non-local CAT conflict/backjump census

## Decision

Reject the proposed CDCL/subtree-blocking implementation before Gate B. The measured workload
contains no post-install CAT conflict from which a non-local no-consistent-extension certificate
could be learned. Implementing watched clauses, rollback scopes, or queue filtering would therefore
add time and memory without an observed native work item to remove.

The observation-only implementation and strict launcher remain on `genmc-caat-opt-dev` as
reproducible negative evidence. They are not an effective optimization and must not be promoted to
`genmc-caat`.

## Mechanism observed

`CATDecisionState` records only genuine multi-alternative RF/CO choices. A changed choice for the
same subject replaces the prior entry and receives a fresh branch-local ordinal. State is copied
with a real execution, filtered by the same vector-clock prefix as a backward revisit, and allocated
only under `--cat-backjump-census`; event insertion stamps are never treated as decision levels.

When the complete CAAT checker rejects an installed graph, the census attempts to derive a lazy
cycle in the analyzer-certified checked root, maps exact RF and immediate-CO facts back to active
decisions, and records newest-only/non-local depth, recurrence, unresolved choice facts, and
potential backjump distance. It never changes candidate order, consistency results, work creation,
or revisit restoration.

## Correctness and infrastructure gates

- `CATDecisionState.*` passes 3/3 server GCC 13 Release tests, covering replacement ordinals,
  vector-clock holes, initial-address identity, and branch-copy isolation.
- The focused Config and state suite passes 37/37.
- A recursive-PSO smoke completes with the census enabled and unchanged verdict/search behavior.
- Both formal lanes contain exactly one log archive and one 15-row result XML. The launcher owns the
  cgroup mount, disjoint core sets, exact task count, and input SHA-256 manifest.
- The server sync script now rejects any modified code/test path absent from its explicit hash
  manifest, closing the earlier omitted-header failure mode.

## Fixed-15 paired result

Authoritative timeout-resilient run:

`/data3/sujie/experiments/caat-optimization/p2-backjump-census-15-20260719b`

Both lanes use recursive PSO with the existing preventive proof, 60 s, 4 GB, one task core, and the
same fixed 15-task easy/medium/hard panel. The candidate adds only `--cat-backjump-census`.

| Metric | Baseline | Census | Delta |
|---|---:|---:|---:|
| Correct terminal | 7 | 7 | 0 |
| TIMEOUT | 8 | 8 | 0 |
| All-task CPU (s) | 609.0046 | 609.0626 | +0.0095% |
| Common-terminal CPU (s) | 121.0397 | 121.0363 | -0.0028% |
| Sum of task peak RSS (bytes) | 402,706,432 | 402,735,104 | +0.0071% |

There are zero status/category differences. For every task where both lanes emitted the same latest
progress/final record, RF offered/queued, CO offered/queued, backward offered/queued, work
added/popped, realized prefixes, inconsistent prefixes, and validity queries are exactly equal.
Thus the observation path did not alter search.

Eleven tasks emitted a final or timeout-resilient census record. Their aggregate latest values are:

- installed conflict queries: 0;
- derived or unsupported conflict cores: 0 / 0;
- mapped/unresolved choice facts: 0 / 0;
- newest-only/non-local conflicts: 0 / 0;
- recurring/unique signatures: 0 / 0;
- maximum potential backjump distance: 0.

The other four timeout logs reach compilation and transformation but emit no 100k-activity record;
they are dominated before the proposed post-install conflict hook. P2 cannot improve that first
large-query region because it has not yet learned any clause or classified any descendant.

## Full-workload context and proof consequence

Gate A0 already measured the only earlier conflict source on all 725 tasks:

- V9 preventive reach rejected 5,814,007 RF and 12,899,587 CO proposals, but observed zero
  all-sibling-pruned fallbacks and zero inconsistent parent prefixes.
- V10 matched 14,193,035 positive cores, avoided zero additional direct checks, changed no
  common-correct search counter, and regressed CPU by 4.99%.

Gate A1 was the remaining hypothesis: a proposal accepted by preventive reach might fail only after
installation and expose a recurring non-local mixed RF/CO cause. The fixed representative panel
observes no such failure even in the five heavy tasks that emit timeout progress. Combined with
Gate A0, there is no measured clause that could reject a queued prefix before restore, no
resolution-derived parent UNSAT, and no domain-closed all-sibling conflict.

Therefore Gate B (shadow clause engine), Gate C (active filtering), the 283-task package run, and a
new 725-task run are not justified. Resuming P2 requires a new certificate source that first
demonstrates nonzero covered native work items in observation mode; retuning core storage or clause
matching is explicitly excluded.
