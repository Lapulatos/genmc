# Optimization 12: acyclic transitive-closure slicing

## Hypothesis

For every finite relation `R`, `acyclic(R+)` is equivalent to `acyclic(R)`. When a
named `TransitiveClosure` predicate has no predicate consumers and all of its check
consumers are `acyclic`, the CAT consistency query can point those checks at the seed
relation and remove the now-unobservable closure predicate. This avoids materializing
the full closure without changing candidate enumeration or any observable CAT axiom.

## Admissible structural slice

- The predicate kind is the generic normalized `TransitiveClosure`; model names,
  filenames, host profiles, and task identities are unavailable to the matcher.
- At least one check consumes the predicate, and every such check is `acyclic`.
- No normalized predicate consumes the closure value.
- `empty`, `irreflexive`, downstream predicate use, reflexive closure, and ambiguous
  recursive forms fail closed and keep the complete value.
- All qualifying checks are redirected to the seed before the dead closure predicate
  and its ID are removed. Remaining IDs, operands, checks, spans, and names are remapped
  deterministically.

This is dead-query slicing in CAT/CAAT. It must not call `SCChecker`, `TSOChecker`, or
any generated fixed-model consistency checker.

## Correctness gates

1. Independent random-relation property: `acyclic(R)` equals `acyclic(R+)` for finite
   relations, including self loops, multi-edge cycles, DAGs, and disconnected graphs.
2. Exact activation tests for one and multiple acyclic checks.
3. Near-neighbor tests proving no activation under downstream use, `empty`,
   `irreflexive`, or reflexive closure.
4. Explanation replay must use complete seed derivations and return a non-empty cycle.
5. Exact bundled certificate summaries must be updated and continue to fail closed for
   one-axiom neighbors.
6. Server Release unit/property suite, 39-row mutation/full oracle, and frozen
   864-pair SC/TSO/PSO differential.

The online/offline oracle shares the sliced normalized model and is therefore not an
independent proof of the algebraic rewrite; gates 1--4 are mandatory.

## Performance experiment

Use the same fresh simultaneous design as Optimization 11: 96 tasks × SC/TSO/PSO ×
six repetitions × before/after = 3,456 cells, alternating disjoint 24-core groups.
Keep only with:

- zero common-solved verdict and safe execution-count mismatch;
- aggregate CPU task-bootstrap 95% CI below 1;
- no model CPU point regression above 0.5%;
- no RSS regression above 0.5%;
- a paired `--cat-stats` profile showing unchanged query/candidate counts and reduced
  offline time or snapshot-equivalent state.

## Stop conditions

- Any missing or invalid explanation.
- Any activation when a consumer needs the complete closure value.
- Any oracle or broad differential mismatch.
- A performance gain caused only by disabling the certified candidate path.
- Aggregate improvement without the predicted reduction in closure state/work.
