#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
pipeline="$root/endpoint-pipeline"
binary="$root/build-gcc13-release/bin/genmc"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/core-direction-review-20260718/endpoint-smoke.xml"
out="${1:?pass a new endpoint-smoke result directory}"

[[ "$out" == "$root/formal-results/endpoint-smoke-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
mkdir -p "$out"
printf 'configuration\tstatus\tfinished\n' >"$out/manifest.tsv"
sha256sum "$binary" "$wrapper" "$definition" "$pipeline/tools/rewrite_sources.py" \
  >"$out/input-sha256.txt"

export PYTHONPATH="$pipeline"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"
for configuration in baseline control rvf; do
  result="$out/$configuration"
  rewrite="/workspace/experiments/caat-optimization/endpoint-smoke-rewrite/$(basename "$out")/$configuration"
  mkdir -p "$result" "$rewrite"
  status=0
  env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
    GENMC_EXPERIMENT_MODE="$configuration" GENMC_REWRITE_ROOT="$rewrite" \
    benchexec --no-container --numOfThreads 3 --allowedCores 0-2 \
    --outputpath "$result/" "$definition" >"$result/console.log" 2>&1 || status=$?
  printf '%s\t%s\t%s\n' "$configuration" "$status" "$(date -Iseconds)" \
    >>"$out/manifest.tsv"
  ((status == 0)) || exit "$status"
done
date -Iseconds >"$out/complete.txt"
