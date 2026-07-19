#!/usr/bin/env bash
set -euo pipefail

root=/workspace/experiments/fair-coverage
output="${FAIR_OUTPUT:-$root/fair-census-run}"
definitions="${FAIR_DEFINITION_ROOT:-$root/definitions}"
export PYTHONPATH="$root/pipeline"
export GENMC_BINARY=/workspace/svcomp2026-caat/build/genmc-caat-llvm15noble/bin/genmc
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_REWRITE_ROOT="${FAIR_REWRITE_ROOT:-$root/rewritten-census}"
export GENMC_COMPAT_HEADER="$root/include/svcomp_genmc_compat_v2.h"

mkdir -p "$output/shard1" "$output/shard2"
benchexec --no-container -N24 --allowedCores 0-23 \
  --outputpath "$output/shard1" "$definitions/fair-census-shard1.xml" \
  > "$output/shard1.console.log" 2>&1 &
pid1=$!
benchexec --no-container -N24 --allowedCores 28-51 \
  --outputpath "$output/shard2" "$definitions/fair-census-shard2.xml" \
  > "$output/shard2.console.log" 2>&1 &
pid2=$!

status1=0
status2=0
wait "$pid1" || status1=$?
wait "$pid2" || status2=$?
printf 'shard1=%s\nshard2=%s\n' "$status1" "$status2"
if (( status1 != 0 || status2 != 0 )); then
  exit 1
fi
