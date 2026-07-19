# Optimization 11 cost model

## Affected path

The structural rewrite changes one normalized recursive SCC

`X = R | (X ; R)` or `X = R | (R ; X)`

into the existing generic relation operation `X = R+`. It removes the generated
composition predicate but retains the named closure value `X`.

| Stage | Before | After | Expected effect |
|---|---|---|---|
| Parse/type checking | ordinary AST walk | one additional structural match | `O(AST)` one-time cost; negligible |
| Analysis | two-node recursive SCC (`X`, generated composition) | one acyclic closure node | one predicate/dependency/stratum removed |
| Offline fixed point | alternating composition and union until diameter is reached | one packed Warshall closure | removes repeated full relation allocations and SCC queue rounds |
| Online insertion | re-evaluate composition/union until the recursive SCC stabilizes | recompute one exact packed closure | fewer operation evaluations, queue pushes, and value publications |
| Transactional copy | copy `X` and generated composition state | copy `X` only | one dense relation less per insertion attempt |
| Undo/checkpoint | retain deltas/metadata for both predicates | retain only `X` | lower undo and snapshot-equivalent bytes |
| Violation checks | unchanged, read complete `X` | unchanged, read complete `X` | no witness/check weakening |
| Explanation | iterative recursive provenance for two predicates | existing shortest-path closure provenance | less reason-table state and fewer derivation rounds |
| Oracle | optimized offline and online share the canonical IR | same | requires a separate naive-Kleene property oracle to avoid common-mode errors |

## Time complexity

Let `n` be the event count, `w = ceil(n/64)`, and `d` the longest relevant path.

- Current recursive form can perform up to `d` fixed-point growth rounds. Each dense
  composition scans `n²` possible left edges and ORs `w` words for present edges, so
  the dense worst-case bound is `O(d · n² · w)`, plus union/equality/allocation rounds.
- Packed transitive closure performs one pivot/from scan with row ORs, bounded by
  `O(n² · w)`, with one result allocation.
- The intended reduction is therefore the fixed-point round factor and its allocation,
  equality, queue, and publication overhead. It does not make closure asymptotically
  sub-cubic in the dense worst case.

## Space complexity

Both paths retain `X`, but the recursive form also retains its generated composition.
The rewrite removes one packed `n × n` relation, approximately `n²/8` bytes, from:

- the steady evaluator value vector;
- transactional value copies;
- insertion undo entries when that predicate grows;
- explanation reason tables (whose per-pair objects are larger than one bit).

Packed relation storage removed per evaluator is approximately 512 B at 64 events,
8 KiB at 256, 32 KiB at 512, and 128 KiB at 1,024 events, excluding container overhead.

## Costs that may increase

- The normalizer performs a small recursive expression comparison once per recursive
  declaration.
- On very shallow sparse relations, packed Warshall still scans every pivot/from pair.
  The old composition also scans `n²`, but this must be measured rather than assumed.
- Canonical summaries and exact certificates change because one generated predicate is
  removed; certification must be updated and re-tested fail-closed.

## Measurable predictions

For structurally matched models, expect lower `offline-ns`, `eval-ops`, `queue-pushes`,
`value-changes`, transactional `copy-ns`, and retained/snapshot-equivalent bytes. Adapter,
materialization, candidate counts, verdicts, and complete exploration counts should not
change. A result that improves only standard model names but not structurally equivalent
renamed fixtures indicates an invalid dispatch dependency.

