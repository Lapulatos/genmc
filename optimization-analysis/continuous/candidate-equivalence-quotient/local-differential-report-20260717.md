# SC RVF local differential checkpoint — 2026-07-17

## Scope

- Host: local macOS, LLVM 20.1.7.
- Matrix: 30 programs × baseline/RVF × 1/2 workers = 120 runs.
- Per-run timeout: 20 seconds.
- Model: `models/cat/recursive-sc.cat`.
- Raw bundle: `local-results/differential-30-final-20260717a/`.
- Machine-readable summary: `local-results/differential-30-final-20260717a/analysis.json`.

## Correctness and completeness gates

- 120/120 runs completed; zero timeouts.
- 17 programs passed the whole-program RVF gate; 13 used native fallback.
- Exit status and semantic safety/warning verdict matched the same-worker baseline for
  every program.
- Successful one-worker and two-worker RVF runs produced identical execution counts.
- Every enabled run reported `rvf-fail-open=0`; fallback runs did not enter RVF.
- Focused solver/state/adapter unit tests passed 11/11.
- Four explicit SC-SB outcome probes confirmed `(0,0)` unreachable and `(0,1)`,
  `(1,0)`, `(1,1)` reachable under baseline and RVF with one/two workers.
- The outcome suite passed 30 consecutive Release repetitions (360 GenMC invocations),
  plus 10 ASan+UBSan and 10 TSan repetitions of the formerly crashing two-worker case.

## Cost and benefit

- Enabled programs processed 173 RVF loads.
- Baseline complete executions across enabled programs: 101.
- RVF complete executions across enabled programs: 108 (net +7, so no aggregate
  execution-count reduction in this sample).
- Median local RVF-one-worker/baseline elapsed ratio: 1.0084×. Compilation dominates
  these short runs, so this is a diagnostic measurement, not a server performance claim.
- `fib_bench` exposed severe RVF expansion (more than 100,000 processed loads and a
  20-second timeout versus a roughly 1.5-second baseline) before the loop gate was added.
  It now falls back before exploration and preserves the one-worker native result exactly.

## Changes driven by the experiment

- Causal cutoffs now use stable GenMC event identities instead of prefix-local dense
  adapter IDs, eliminating unsafe late causal-map fallback.
- Loop-bearing transformed programs use native RF-DPOR under the current performance
  gate.
- RVF-only configuration changes are applied only after the whole-program gate succeeds;
  native fallback preserves native reductions and the direct one-worker path.
- Hard errors in certified RVF witness graphs are reported directly from that graph,
  avoiding an RF-DPOR replay restriction that could delete the selected error event.

## Remaining gate

Server Docker Release/sanitizer repetition and broad performance experiments remain
required after the server recovers. Local timing must not be merged with server results.
