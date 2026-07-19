# Optimization 12: structural relation and predicate census decision

## Decision

Retain no production behavior from Optimization 12. Keep the diagnostics only long
enough to complete the per-predicate timing census, then restore production/test source
to `05d7ff8` before implementing P0.6.

The next implementation should be exact structural relation views followed by a
model-certified sparse EOG cycle plan. It must not replace every packed relation with
CSR: the measured generic CSR projection is larger than the current packed matrices on
large graphs.

## Correctness evidence

- Release unit/property tests: 145/145.
- Final online mutation oracle: 39 rows, 12,784 complete Phase-2 comparisons, zero
  mismatch.
- Broad differential: 288 programs × SC/TSO/PSO = 864 pairs; 852 exact matches, 12
  mutually unsupported, zero mismatch.
- Diagnostic checkpoints are enabled only by `--cat-stats`, occur at power-of-two query
  counts, and never alter graph transitions or verdicts.
- The final timing run and space-only run have identical status in all 288 cells and
  identical complete-execution counts on every jointly safe cell.

## Formal 96-task census

- 288 model/task cells completed under 60 s, 4 GiB, one GenMC exploration thread and 48
  BenchExec task workers.
- 178 TRUE, 83 FALSE, 12 TIMEOUT and 15 OOM.
- Profiles were recovered from 273/288 cells. Checkpoints recovered partial state from
  12/27 TIMEOUT/OOM cells; the remaining 15 OOM logs contain no checker query.
- Nine cells reached at least 512 stable events. The maximum was 8,033 events.
- `queue_ok_longer` under PSO reached 8,033 events, 72,879,408 bytes of exact peak base
  storage, 396,770,976 bytes of exact peak published predicates and 1,069,854,720 bytes
  process RSS.
- Target structural/base-alias/order/reach values account for a median 36.4% of summed
  per-value relation maxima on the nine large cells.
- Generic 32-bit CSR fails: median packed/CSR is 0.288× on the large cells, so CSR would
  require roughly 3.5× the payload for the measured dense relations.
- Exact structural views plus direct-order EOG/topological ranks pass the opportunity
  gate: median packed/projected payload is 2.04× on the large cells. This projection
  reduces the targeted values by about 51%; across all relation peak sums the current
  conservative projection is about 25%, short of the later 30% production retention
  target. P0.6/P0.7 must therefore also avoid dense intermediate order expressions, not
  only `order` and `reach`.
- Predicate operation time identifies the exact hot operations. Under TSO and PSO,
  dense `Composition` accounts for 84.3% and 84.4% of measured predicate operation
  time, and `TransitiveClosure` accounts for another 13.1% and 12.6%. Under SC,
  `TransitiveClosure` accounts for 72.0% and `Composition` for 24.7%. Together they
  account for 96.7% of SC, 97.4% of TSO and 97.0% of PSO operation time.
- Relative to the otherwise identical space-checkpoint run, adding per-operation timing
  produced a 1.0020 CPU geometric-mean ratio and 0.9999 RSS ratio over 261 jointly
  terminated cells. This is diagnostic overhead, not a production speed claim.

## Historical hard-60 census

- Statuses: 28 OOM, 11 TIMEOUT and 21 compilation errors in the frozen historical
  adapted cohort.
- Twelve resource cells emitted checker checkpoints; five exceeded 512 stable events.
- `13-privatized_04-priv_multi_true` reached 3,088 events, 10,896,032 base bytes,
  59,316,264 predicate bytes and 848,846,848 RSS before TIMEOUT.
- Three OOM cells had only 14, 35 and 52 stable events and at most about 25 KiB of
  measured CAT base+predicate state while RSS hit 4 GiB. Their OOM is dominated by
  GenMC exploration/history state, not dense CAT relations.
- Twenty-seven other OOM/TIMEOUT cells emitted no checker checkpoint. For the five
  repeated formal OOM tasks, logs contain only the command line. These failures occur
  before the first CAT consistency query and cannot be fixed by P0.6/P0.7 alone.

## Concrete implications

1. P0.6 lowers adapter/materialization time and base/checkpoint space by replacing
   `po/int/ext/loc/rf/co/fr` matrices with exact chains, buckets, functional maps and
   iterators. Every view needs extensional dense-oracle tests.
2. P0.7 lowers worklist and published-predicate space by compiling the model's exact
   positive `order` dependency cone into direct EOG edges and incremental cycle state.
   It must fail closed for every uncertified model and leave atomicity/coherence on an
   exact generic or separately certified plan.
3. P0.6/P0.7 can materially help the large-graph queue family but cannot solve small-N
   4-GiB OOMs. Those require exploration-family compression, compact graph histories,
   blocker sharing, or scheduling-constraint CEGAR after the exact EOG foundation is
   validated.

## Evidence paths

- `analysis-formal-96/report.md`, `task-summary.tsv`, `value-summary.tsv`, `summary.json`
- `analysis-hard-60/report.md`, `task-summary.tsv`, `value-summary.tsv`, `summary.json`
- `server-results/formal-96/` and `server-results/hard-60/` contain complete XML and log
  archives; `formal-96-space/` is the space-only checkpoint run and
  `formal-96-schema-v1/` is retained but excluded because it lacks explicit base value
  types and resource checkpoints.
