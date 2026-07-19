# Generic preventive V7 decision

V7 is a significant positive representation optimization, but still misses the frozen
default CPU/RSS gate. It keeps V6's exact exploration reduction and four stable coverage
gains with zero OOM, loss, or safe execution-count mismatch.

Against its simultaneous baseline, V7 CPU is 1.02416 with task-bootstrap 95% CI
[0.95902, 1.11356], wall is 1.02064 [0.95767, 1.10757], and RSS is 1.02792
[1.00015, 1.07104]. The default gate therefore still says reject.

The isolated V7/V6 candidate comparison shows that direct CSR rows worked:

- candidate CPU ratio 0.97601 [0.96429, 0.98706], a significant 2.40% reduction;
- candidate wall ratio 0.97797 [0.96588, 0.98941], a significant 2.20% reduction;
- candidate RSS ratio 0.99197 [0.97975, 1.00023], an estimated 0.80% reduction whose
  interval narrowly crosses no-change;
- the corresponding V7/V6 baseline CPU ratio is 0.99607 [0.98715, 1.00491], so the
  candidate improvement is not explained by a comparable machine-wide shift;
- all 12 candidate-space counters match exactly in all 384 pruning cells.

V8 removes the next measured redundant pass. The retained forward CSR is currently
scanned twice at every preventive preparation to count and fill a temporary inverse CSR.
V8 constructs an optional reverse CSR while flattening the already available successor
rows, then exposes a predecessor cursor. Preparation performs reverse BFS directly,
without two full edge scans or temporary inverse allocation. It does not alter relation
membership, cycle checking, reachability, certificates, or candidate filtering.
