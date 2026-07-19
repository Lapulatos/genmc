# Experiment Protocol

This protocol separates compatibility, correctness, and performance. Raw
BenchExec XML and log archives are immutable inputs; normalized TSV/JSON files
are derived artifacts.

## Stage 1: Compatibility census

- Benchmark pin: `sv-benchmarks` tag `svcomp26`, commit
  `7efe28dd29576b46927b7a34e8f742bd90966a75`.
- Census scope: all 3,520 property rows are retained transparently. GenMC can
  semantically check 725 `unreach-call` rows and 1,030 `no-data-race` rows;
  coverage, overflow, full SV-COMP memsafety, and undefined-behavior properties
  are labeled `property_adapter_unsupported`, not silently treated as executed.
  Dynamic censuses use ILP32, SC, 10 s CPU, 4 GB, and one core per run.
- Compatibility censuses may use up to eight BenchExec workers because their
  timings are not used for performance claims. Each run remains constrained to
  one core and 4 GB. Formal performance experiments remain single-worker runs.
  Scale-diagnostic runs may use eight workers because their wall/CPU values are
  discarded; only per-run graph counters are joined to the separate formal
  timing data.
- Entry source: use the same-basename `.c` where available because the
  preprocessed `.i` files remove the pthread declarations GenMC replaces.
- Compatibility shim: inject `svcomp_genmc_compat.h` only when the entry source
  or a recursively included quoted local header uses
  `__VERIFIER_atomic_begin/end`.
- Keep `unsupported external`, compilation error, timeout, OOM, abort,
  segmentation fault, wrong verdict, and correct verdict distinct.
- Corrected census result: 725 runs = 96 correct + 18 wrong + 611 error/resource
  outcomes. Error/resource outcomes include 320 aborted, 159 compilation
  errors, 78 unsupported externals, 47 timeouts, 5 segmentation faults, and 2
  OOMs.
- The 1,030-task `no-data-race` dynamic census is complete: 192 correct, 56
  wrong, and 782 error/resource rows. The final all-property assembly contains
  3,520 rows: 1,755 dynamically executed `unreach-call` or `no-data-race`
  rows and 1,765 rows explicitly classified `property_adapter_unsupported`.

## Stage 2: Paired performance experiment

- Primary comparison unit: one task/property/memory-model tuple. Only the 96
  tasks categorized correct in stage 1 enter the first SC performance batch.
- Backends: built-in GenMC SC, `models/cat/sc.cat`, and
  `models/cat/recursive-sc.cat`.
- Limits: 60 s CPU, 4 GB, one core, one BenchExec worker.
- Five paired repetitions; rotate backend order in each repetition.
- Primary metrics: CPU time, wall time, peak RSS, verdict, completed executions,
  and blocked executions. Timeout-aware performance profiles remain separate
  from solved-only ratios.
- Do not use the eight-worker, turbo-enabled census measurements for performance
  claims.
- The timing binary does not include the later scale-diagnostic patch. A
  separately built diagnostic binary enables `--cat-stats` and records maximum
  active/stable/inactive events, current/history primitive bytes, maximum base
  relation pairs and density, rebuild count, and profiled query count. Those
  runs explain scaling but are not substituted for the timing measurements.
- TSO repeats the same design after the SC batch. PSO is a CAT/CAAT comparison
  because the current built-in GenMC interface has no equivalent PSO checker.

## External tools

- Official SV-COMP packages: Deagle 4.1.0 and CBMC use their BenchExec tool-info
  modules and ILP32 task metadata. Their bounded/unwinding semantics are
  reported explicitly and are not treated as identical algorithms to DPOR.
- TruSt family artifacts available on the server: TruSt 0.5.3, Awamoche 0.8,
  Mixer 0.10.1, and Spore 0.10.1.
- TruSt and Awamoche expose RC11/IMM; Mixer and Spore expose SC/TSO/RC11. No
  available family artifact exposes a PSO command-line mode.
- The old family artifacts conflict with their runtime headers under ILP32.
  Therefore their comparison uses the same sources in LP64 and appears in a
  separate table. It must not be merged with the official ILP32 results.
- Deagle's official SV-COMP wrapper defaults to SC even though its underlying
  engine supports SC/TSO/PSO. Direct non-wrapper model experiments, if added,
  are labelled separately from the official wrapper result.
