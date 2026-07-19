# Optimization 02b: exact semantic snapshot cache

## Decision

Reject and remove the cache prototype. It preserved observed semantics, but its small,
tail-heavy speedup did not justify the additional full-graph signature, eight retained
snapshots, memory regression, and correctness-sensitive cache key.

## Prototype

The prototype keyed a bounded LRU by an exact execution-graph signature covering event
kind/order/address, `rf`, `rmw`, thread create/join, and per-location `co`. Hashes were
only an index filter; equality compared the complete semantic key. Previously unseen
graphs used the existing direct materializer.

## Correctness gates

- 141/141 local unit/property tests passed with the cache and again after removal.
- 864/864 local broad SC/TSO/PSO differential cells matched.
- The server-wide comparison had 0 common-solved verdict mismatches and 0 safe-task
  execution-count mismatches.
- Completed cells changed from 777/864 to 778/864. The only net gain was one PSO run
  crossing the 60 s timeout boundary; another pair changed between timeout forms.

## Performance gates

The authoritative server experiment used 96 C.Concurrency tasks, SC/TSO/PSO, three
repetitions, 16 BenchExec workers, one CPU and 4 GB per task, and `--nthreads=1`.
Ratios use each model-task's three-run median and report after/before geometric means;
the confidence interval is a 20,000-sample model-task bootstrap.

| Metric | SC | TSO | PSO | Overall |
|---|---:|---:|---:|---:|
| Wall ratio | 0.9994 | 0.9901 | 0.9819 | 0.9904 |
| Wall 95% CI | [0.9861, 1.0131] | [0.9785, 1.0017] | [0.9674, 0.9963] | [0.9828, 0.9979] |
| CPU ratio | 1.0024 | 0.9950 | 0.9789 | 0.9920 |
| RSS ratio | 1.0061 | 1.0038 | 1.0098 | 1.0066 |

The overall wall improvement was 0.96%, while median wall ratio was 1.0000 and RSS
increased 0.66% with a confidence interval excluding no change. A smaller micro-suite
was neutral/slower (overall wall ratio 1.014), which further weakens evidence that the
cache is a general improvement.

## Artifacts

- Raw XML/log archives: `server/wide-before/`, `server/wide-after/`
- Parsed rows: `analysis/wide-before.tsv`, `analysis/wide-after.tsv`
- Machine-readable comparison: `analysis/wide-comparison.json`
- Reproducible analysis: `analyze_wide.py`
- BenchExec HTML: `html/semantic-snapshot-cache.table.html`
- Earlier micro rows: `local/server-micro.tsv`

The direct stable-ID materializer and opt-in mutation-density/timing instrumentation
remain. Only the semantic snapshot cache, cache counters, and cache-specific assertions
were removed.
