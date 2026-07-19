# Optimization 12 structural storage census

- Cells: 60; profiles recovered: 12.
- TIMEOUT/OOM cells: 39; checkpoint profiles recovered: 12.
- Profiled cells with at least 512 stable events: 5.
- Sparse projection is a conservative 32-bit CSR payload: 4*(N+1)+4*E bytes; it excludes allocator and index metadata.
- Structural projection is not generic CSR: po/co use O(N) chains, loc/int/ext are attribute views, fr is derived on demand, normalized Base nodes alias primitives, and reach uses direct order edges plus topological ranks.
- Per-value maxima need not occur simultaneously; `*_peak_sum` is an opportunity upper bound, while the main packed-byte counters are exact observed aggregate peaks.

## Opportunity gate

- Target relations occupy >=30% of relation peak sums: True.
- For N>=512, target packed/sparse median is >=2x: False.
- For N>=512, target packed/structural-view median is >=2x: False.

## By model

| Model | Profiled | Resource profiled | Median events | Max events | Median base bytes | Median predicate bytes | Median target share | Large packed/sparse | Large packed/structural |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| SC | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0.000 | 0.00x | 0.00x |
| TSO | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0.000 | 0.00x | 0.00x |
| PSO | 12/60 | 12/39 | 340 | 3088 | 165216 | 898704 | 0.345 | 0.18x | 1.68x |

## Largest observed graphs

| Model | Task | Status | Events | RSS | Base bytes | Predicate bytes | Target share | Packed/sparse | Packed/structural |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| PSO | 13-privatized_04-priv_multi_true.yml | TIMEOUT | 3088 | 848846848 | 10896032 | 59316264 | 0.345 | 0.26x | 3.28x |
| PSO | stack_longest-1.yml | TIMEOUT | 1375 | 736694272 | 2178704 | 11858880 | 0.345 | 0.18x | 1.68x |
| PSO | rwlock.yml | TIMEOUT | 1056 | 59977728 | 1293088 | 7037864 | 0.345 | 0.15x | 1.34x |
| PSO | ttaslock.yml | TIMEOUT | 1052 | 58003456 | 1288192 | 7011208 | 0.345 | 0.15x | 1.33x |
| PSO | exponential-64.yml | TIMEOUT | 706 | 141512704 | 610368 | 3321504 | 0.345 | 0.24x | 4.45x |
| PSO | spaghetti.wvr.yml | TIMEOUT | 465 | 25653248 | 268096 | 1458560 | 0.345 | 0.17x | 1.75x |
| PSO | safestack_relacy.yml | TIMEOUT | 216 | 26341376 | 62336 | 338848 | 0.345 | 0.45x | 5.16x |
| PSO | pthread-demo-datarace-1.yml | TIMEOUT | 207 | 26087424 | 59744 | 324736 | 0.345 | 0.20x | 2.78x |
| PSO | rec_ticketlock.yml | OUT OF MEMORY | 52 | 3999997952 | 3776 | 20424 | 0.345 | 0.31x | 2.20x |
| PSO | exponential-4.yml | TIMEOUT | 47 | 26152960 | 3416 | 18464 | 0.345 | 0.27x | 2.24x |
| PSO | ticketlock.yml | OUT OF MEMORY | 35 | 3999997952 | 2552 | 13760 | 0.345 | 0.44x | 2.68x |
| PSO | parallel-min-max-1.wvr.yml | OUT OF MEMORY | 14 | 3999997952 | 1040 | 5528 | 0.345 | 0.73x | 2.90x |
