# CAT model-certificate check-kind fix

Date: 2026-07-16

## Conclusion

Keep this correctness fix separate from rejected Optimization 12. A normalized-model
certificate previously fingerprinted the predicate DAG and check target, but omitted
whether each check was `acyclic`, `irreflexive`, or `empty`. Two semantically different
models could therefore share a closed-world certificate and incorrectly enable a
specialized candidate profile or adaptive-offline path.

## Change

- Include the check kind in `NormalizedModel::summary()`.
- Update the exact recursive SC/TSO/PSO certificate strings.
- Add a negative TSO neighbor that changes only the coherence check from `acyclic` to
  `irreflexive`; both closed-world certifications must then be absent.

This does not use GenMC's built-in SC/TSO/PSO checker and does not alter CAT evaluation
semantics. It only prevents an inexact model fingerprint from selecting a certified
specialized path.

## Server verification

| Gate | Result |
|---|---:|
| Release unit/property tests | 144/144 passed |
| Online mutation rows | 39 |
| Full Phase-2 oracle comparisons | 5,441, zero mismatch |
| Broad programs | 288 |
| SC/TSO/PSO broad pairs | 864 |
| Comparable matches | 852 |
| Mutually unsupported pairs | 12 |
| Broad mismatches | 0 |

Complete logs and the 864-row broad matrix are under `server/`. The isolated test
container was removed after exit 0. The server source/build tree remains outside Git.
