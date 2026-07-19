#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
pipeline="$root/endpoint-pipeline"
binary="$root/build-gcc13-release/bin/genmc"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/definitions/full-caat.xml"
out="${1:?pass a new memory-attribution result directory}"

[[ "$out" == "$root/formal-results/memory-attribution-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
! pgrep -af '/bin/[g]enmc|[b]enchexec' || exit 2
mkdir -p "$out/baseline"
sha256sum "$binary" "$wrapper" "$definition" "$pipeline/tools/rewrite_sources.py" \
  >"$out/input-sha256.txt"

export PYTHONPATH="$pipeline"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"
rewrite="/workspace/experiments/caat-optimization/memory-attribution-rewrite/$(basename "$out")/baseline"
mkdir -p "$rewrite"
status=0
env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
  GENMC_EXPERIMENT_MODE=baseline GENMC_REWRITE_ROOT="$rewrite" \
  benchexec --no-container --numOfThreads 48 --allowedCores 0-23,28-51 \
  --outputpath "$out/baseline/" "$definition" >"$out/baseline/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\nbaseline\t%s\t%s\n' "$status" "$(date -Iseconds)" \
  >"$out/manifest.tsv"
((status == 0)) || exit "$status"
date -Iseconds >"$out/complete.txt"
