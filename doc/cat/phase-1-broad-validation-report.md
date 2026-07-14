# Phase 1 broad differential validation report

## Result

The frozen corpus contains 288 distinct concurrent C programs. One program
cannot reach verification because it calls the unsupported external function
`__VERIFIER_assume`; the remaining 287 programs produce valid SC and TSO
differential evidence. Across 574 source/model pairs (1,148 successful GenMC
invocations), the built-in and CAT-backed checkers have identical process
statuses, verdict and diagnostic classes, complete/blocked execution counts,
and bound summaries. The final mismatch count is zero.

This result supports the implemented SC/TSO subset on this corpus. It is not a
proof that every program or arbitrary CAT model is handled correctly.

## Defects found and fixed

The initial run exposed 94 result mismatches. Investigation reduced them to
three implementation/model defects:

| Defect | Observable risk | Fix and permanent regression |
|---|---|---|
| Conservative CAT coherence candidates decided GenMC's `Unordered writes` warning. | False-positive warnings; warning-as-error configurations could reject a valid execution. | Keep conservative candidates for CAT exploration, but use the host checker's diagnostic placement range for the warning. `assume-ctrl0.c` covers no warning; `WWR+2WR0.c` preserves a real warning. |
| CAT offered `Init` as a read-from candidate for dynamically allocated addresses. | A dynamic read could assert or acquire a fabricated static initializer. | Offer `Init` only for static addresses; retain real coherence writes for dynamic storage. `atomic-min-max0.c` covers this path. |
| TSO/PSO equations used bare thread-create/join edges without surrounding program order. | Missing lifecycle order could become safety false positives around nested create/join. | Define lifecycle order as `(po? ; tc ; po?) | (po? ; tj ; po?)`. `W+JW0.c` and `nested-create.c` cover the repaired cases. |

After these fixes, every former status and signature mismatch disappeared.

## Corpus and oracle

The exact selection and oracle hierarchy are defined in
`doc/cat/phase-1-broad-validation-plan.md`. The checked-in
`doc/cat/phase-1-broad-results.tsv` records the source path and SHA-256,
arguments, SC expectation where available, both statuses, classification, and
normalized semantic signatures for every source/model pair.

The excluded source is
`tests/correct/litmus/psc-base-notin-ar/variants/psc-base-notin-ar0.c`. Both
checker paths exit before exploration with the same unknown-external-function
diagnostic under SC and TSO. Its two rows are `unsupported-baseline`; they are
not counted as matches.

Repository SC expectations, focused exact-count tests, the model-only PSO test,
and the existing herd SB/MP scripts complement the differential oracle.

## Reproduction

```bash
bash tests/cat/broad-differential.sh \
  "$PWD/RelWithDebInfo/bin/genmc" "$PWD/models/cat" "$PWD/tests" \
  "$PWD/doc/cat/phase-1-broad-results.tsv" 8
```

Final summary:

```text
discovered=288 validated=287 model-pairs=576 mismatches=0 unsupported=2
```
