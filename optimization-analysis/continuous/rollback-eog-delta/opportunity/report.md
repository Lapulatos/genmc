# Rollback-safe EOG delta opportunity census

- Baseline cells: 1152
- Cells with/without final CAT statistics: 1076/76
- The incremental-cell fractions are strict upper bounds: mixed cells also include initialization and rebuild checks.

| Model | insert | rollback | rollback-insert | replace | rebuild | incremental-cell check upper bound | candidate upper bound |
|---|---:|---:|---:|---:|---:|---:|---:|
| SC | 0 | 0 | 0 | 0 | 8 | - | - |
| TSO | 0 | 0 | 0 | 0 | 8 | 0.00% | 0.00% |
| PSO | 0 | 0 | 0 | 0 | 128 | 0.00% | 0.00% |
