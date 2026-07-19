#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
pipeline="$root/endpoint-pipeline"
binary=/data3/sujie/svcomp2026-caat/build/genmc-caat-llvm15noble/bin/genmc
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/core-direction-review-20260718/decisive-diagnostic-15.xml"
out="${1:?pass the dedicated diagnostic result directory}"

[[ -f /.dockerenv ]] || { echo "must run inside the server Docker container" >&2; exit 2; }
[[ "$out" == "$root/formal-results/decisive-diagnostic-15-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
mkdir -p "$out/run"
sha256sum "$binary" "$wrapper" "$definition" >"$out/input-sha256.txt"
export PYTHONPATH="$pipeline"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT=/data3/sujie/svcomp2026-caat/source/genmc/models/cat
export GENMC_BINARY="$wrapper"
export GENMC_REAL_BINARY="$binary"
export GENMC_EXPERIMENT_MODE=baseline
export GENMC_REWRITE_ROOT="/workspace/experiments/caat-optimization/decisive-diagnostic-rewrite/$(basename "$out")"
mkdir -p "$GENMC_REWRITE_ROOT"
status=0
benchexec --no-container --numOfThreads 4 --allowedCores 0-3 \
  --outputpath "$out/run/" "$definition" >"$out/run/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\nrecursive-caat-observation-only\t%s\t%s\n' \
  "$status" "$(date -Iseconds)" >"$out/manifest.tsv"
((status == 0)) || exit "$status"
date -Iseconds >"$out/complete.txt"
