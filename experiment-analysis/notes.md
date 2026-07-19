# Notes: GenMC / CAT / CAAT Performance Analysis

## Repository State
- Branch: `genmc-caat`
- Current commit at discovery: `0171e63`
- Existing untracked file before this task: `.DS_Store`
- Existing Phase 3 report measured offline CAAT vs online CAAT only; it did not include built-in GenMC or Phase 1 CAT in one three-way design.

## Discovered Entrypoints
- Binary: `RelWithDebInfo/bin/genmc`
- Built-in paths: `--memory-model=sc|tso`
- CAT paths: `--model-file=models/cat/sc.cat` and `tso.cat`
- CAAT paths: `--model-file=models/cat/recursive-sc.cat` and `recursive-tso.cat`
- Common controls: `--disable-estimation --disable-mm-detector --nthreads=1`

## Existing Evidence
- `doc/cat/phase-3-report.md` reports 27 offline and 27 online samples, three repetitions per cell.
- On that earlier dataset, online CAAT averaged 0.2004 s versus offline CAAT 0.0700 s; maximum peak RSS was effectively unchanged.
- This task will produce a fresh three-way comparison rather than reuse that result as the main conclusion.

## Smoke Test
- Command: `REPETITIONS=1 WARMUPS=0 RESULT=$PWD/experiment-analysis/smoke-results.tsv experiment-analysis/run-benchmark.sh`
- Result: 30/30 rows completed; status, complete executions and blocked executions matched across all three backends for every model/program cell.
- The first cold GenMC/SC/SB sample was 0.36 s and is not suitable for inference; the formal run includes one full warmup pass and rotates backend order.

## Formal Results
- SC/TSO three-way: 300 rows, 100 paired semantic groups, zero status/execution/blocked mismatch.
- Fixed-suite mean totals: GenMC 0.614 s, CAT 0.694 s, CAAT 2.056 s per repetition.
- PSO CAT-vs-CAAT: 100 rows, 50 paired semantic groups, zero mismatch; CAT 0.308 s vs CAAT 0.361 s per repetition.
- Process peak RSS stayed near 50--52 MiB for all paths; checker-specific memory is below the resolution of this end-to-end measurement.
