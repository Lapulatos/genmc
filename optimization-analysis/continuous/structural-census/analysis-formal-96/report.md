# Optimization 12 structural storage census

- Cells: 288; profiles recovered: 273.
- TIMEOUT/OOM cells: 27; checkpoint profiles recovered: 12.
- Profiled cells with at least 512 stable events: 9.
- Sparse projection is a conservative 32-bit CSR payload: 4*(N+1)+4*E bytes; it excludes allocator and index metadata.
- Structural projection is not generic CSR: po/co use O(N) chains, loc/int/ext are attribute views, fr is derived on demand, normalized Base nodes alias primitives, and reach uses direct order edges plus topological ranks.
- Per-value maxima need not occur simultaneously; `*_peak_sum` is an opportunity upper bound, while the main packed-byte counters are exact observed aggregate peaks.

## Opportunity gate

- Target relations occupy >=30% of relation peak sums: True.
- For N>=512, target packed/sparse median is >=2x: False.
- For N>=512, target packed/structural-view median is >=2x: True.

## By model

| Model | Profiled | Resource profiled | Median events | Max events | Median base bytes | Median predicate bytes | Median target share | Large packed/sparse | Large packed/structural |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| SC | 91/96 | 1/6 | 17 | 8029 | 1088 | 2448 | 0.692 | 0.27x | 1.76x |
| TSO | 91/96 | 2/7 | 17 | 8029 | 1256 | 6304 | 0.364 | 0.28x | 2.58x |
| PSO | 91/96 | 9/14 | 24 | 8033 | 1760 | 9448 | 0.345 | 0.22x | 3.66x |

## Predicate operation-time hotspots

### SC

| ID | Name | Kind | Type | Evaluation ns | Attempts | Maximum bytes |
|---:|---|---:|---|---:|---:|---:|
| 2 | reach | TransitiveClosure | relation | 16291721682 | 223610 | 8093232 |
| 16 | $n3 | Composition | relation | 5592515436 | 223610 | 8093232 |
| 14 | $n1 | Union | relation | 229562583 | 223610 | 8093232 |
| 12 | coe | Intersection | relation | 92311287 | 223610 | 8093232 |
| 17 | $n4 | Intersection | relation | 74044439 | 223610 | 8093232 |
| 15 | $n2 | Union | relation | 71736556 | 223610 | 8093232 |
| 11 | fre | Intersection | relation | 68470449 | 223610 | 8093232 |
| 13 | $n0 | Union | relation | 67560280 | 223610 | 8093232 |
| 1 | order | Union | relation | 63099759 | 223610 | 8093232 |
| 0 | com | Union | relation | 61006184 | 223610 | 8093232 |
| 3 | rf | Base | relation | 0 | 0 | 8093232 |
| 4 | fr | Base | relation | 0 | 0 | 8093232 |

Kind totals: TransitiveClosure=72.0%, Composition=24.7%, Union=2.2%, Intersection=1.0%.

### TSO

| ID | Name | Kind | Type | Evaluation ns | Attempts | Maximum bytes |
|---:|---|---:|---|---:|---:|---:|
| 4 | reach | TransitiveClosure | relation | 12224172806 | 216750 | 8093232 |
| 42 | $n21 | Composition | relation | 11800301385 | 216750 | 8093232 |
| 38 | $n17 | Composition | relation | 11695173539 | 216750 | 8093232 |
| 28 | $n7 | Composition | relation | 11508316820 | 216750 | 8093232 |
| 25 | $n4 | Composition | relation | 11492145331 | 216750 | 8093232 |
| 46 | $n25 | Composition | relation | 4748180821 | 216750 | 8093232 |
| 23 | $n2 | Composition | relation | 4664942897 | 216750 | 8093232 |
| 36 | $n15 | Composition | relation | 4617951437 | 216750 | 8093232 |
| 40 | $n19 | Composition | relation | 4614446253 | 216750 | 8093232 |
| 44 | $n23 | Composition | relation | 4607629871 | 216750 | 8093232 |
| 33 | $n12 | Composition | relation | 4590575334 | 216750 | 8093232 |
| 29 | $n8 | Composition | relation | 4563810717 | 216750 | 8093232 |

Kind totals: Composition=84.3%, TransitiveClosure=13.1%, Optional=1.0%, Union=0.7%, Identity=0.5%, Intersection=0.5%.

### PSO

| ID | Name | Kind | Type | Evaluation ns | Attempts | Maximum bytes |
|---:|---|---:|---|---:|---:|---:|
| 5 | reach | TransitiveClosure | relation | 37992635572 | 1013810 | 8097264 |
| 44 | $n22 | Composition | relation | 36078177623 | 1013810 | 8097264 |
| 40 | $n18 | Composition | relation | 35899854808 | 1013810 | 8097264 |
| 31 | $n9 | Composition | relation | 35166402243 | 1013810 | 8097264 |
| 26 | $n4 | Composition | relation | 24212885011 | 1013810 | 8097264 |
| 48 | $n26 | Composition | relation | 15780315179 | 1013810 | 8097264 |
| 24 | $n2 | Composition | relation | 15634408316 | 1013810 | 8097264 |
| 28 | $n6 | Composition | relation | 15569935055 | 1013810 | 8097264 |
| 42 | $n20 | Composition | relation | 15311704890 | 1013810 | 8097264 |
| 38 | $n16 | Composition | relation | 15284214908 | 1013810 | 8097264 |
| 46 | $n24 | Composition | relation | 15272764591 | 1013810 | 8097264 |
| 36 | $n14 | Composition | relation | 15222821983 | 1013810 | 8097264 |

Kind totals: Composition=84.4%, TransitiveClosure=12.6%, Optional=1.1%, Union=0.8%, Identity=0.6%, Intersection=0.5%.


## Largest observed graphs

| Model | Task | Status | Events | RSS | Base bytes | Predicate bytes | Target share | Packed/sparse | Packed/structural |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| PSO | queue_ok_longer.yml | true | 8033 | 1077948416 | 72879408 | 396770976 | 0.345 | 0.22x | 4.13x |
| PSO | queue_ok_longest.yml | TIMEOUT | 8032 | 1266487296 | 72870336 | 396721584 | 0.345 | 0.28x | 3.66x |
| SC | queue_ok_longest.yml | true | 8029 | 596942848 | 64745856 | 145678176 | 0.692 | 0.27x | 1.77x |
| TSO | queue_ok_longest.yml | true | 8029 | 1189212160 | 72843120 | 372294720 | 0.364 | 0.28x | 2.60x |
| SC | queue_ok_longer.yml | true | 4029 | 160489472 | 16244928 | 36551088 | 0.692 | 0.27x | 1.76x |
| TSO | queue_ok_longer.yml | true | 4029 | 309256192 | 18277560 | 93411360 | 0.364 | 0.28x | 2.58x |
| SC | queue_longer.yml | TIMEOUT | 2153 | 577339392 | 4684928 | 10541088 | 0.692 | 0.22x | 1.49x |
| TSO | queue_longer.yml | TIMEOUT | 2153 | 454569984 | 5271632 | 26939968 | 0.364 | 0.17x | 2.04x |
| PSO | queue_longer.yml | TIMEOUT | 1648 | 102117376 | 3085888 | 16797456 | 0.345 | 0.18x | 1.69x |
| PSO | queue_ok.yml | true | 433 | 26267648 | 218456 | 1188432 | 0.345 | 0.22x | 3.54x |
| PSO | fib_unsafe-7.yml | TIMEOUT | 319 | 26112000 | 115000 | 625440 | 0.345 | 0.26x | 4.21x |
| SC | fib_unsafe-7.yml | false(unreach-call) | 319 | 25821184 | 102080 | 229680 | 0.692 | 0.25x | 2.38x |
| TSO | fib_unsafe-7.yml | TIMEOUT | 319 | 26017792 | 115000 | 587200 | 0.364 | 0.27x | 3.42x |
| PSO | fib_safe-6.yml | TIMEOUT | 281 | 26128384 | 101320 | 550960 | 0.345 | 0.30x | 4.61x |
| PSO | fib_unsafe-6.yml | TIMEOUT | 281 | 26025984 | 101320 | 550960 | 0.345 | 0.30x | 4.61x |
