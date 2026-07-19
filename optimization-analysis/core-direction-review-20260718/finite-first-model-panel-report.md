# P1a abstract first-model panel report

## Decision

Retain the diagnostic representation change. Omitting first-query CO and replacing pairwise
RF at-most-one with Z3 native cardinality is necessary: it is the only tested representation
that produces an abstract model for all 15 fixed pthread-wmm tasks within 20 seconds.

This result does not establish a feasible execution or a verdict. Every abstract model still
requires exact ordering completion, generic CAT validation, and native error replay.

## Fixed panel result

| mode | models | easy | medium | hard | total CPU s | peak RSS MB |
|---|---:|---:|---:|---:|---:|---:|
| eager CO + pairwise RF | 9/15 | 5/5 | 4/5 | 0/5 | 165.064 | 169.2 |
| no CO + pairwise RF | 10/15 | 5/5 | 4/5 | 1/5 | 136.778 | 165.5 |
| no CO + native RF cardinality | 15/15 | 5/5 | 5/5 | 5/5 | 102.368 | 69.9 |

The five hard first-model checks under native cardinality were 2.40--7.68 seconds. Pairwise RF
still failed four of them, so CO omission alone is insufficient.

## Evidence

- Raw server result:
  `server-results/finite-first-model-panel-20260718a/`
- Analyzer: `analyze_first_model_panel.py`
- Frozen tasks: `pthread-wmm-15-panel.tsv`

