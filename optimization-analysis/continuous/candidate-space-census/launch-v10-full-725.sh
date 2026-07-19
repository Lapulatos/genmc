#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
binary="$root/build-gcc13-release/bin/genmc"
v9_wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/generic-preventive-wrapper.sh"
v10_wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/v10-genmc-wrapper.sh"
definition=/workspace/experiments/caat-optimization/rvf-opportunity-census/definitions/full-caat-pso.xml
out="${1:?pass a new full-725 result directory}"

[[ "$out" == "$root/formal-results/v10-full-725-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$v9_wrapper" && -x "$v10_wrapper" && -f "$definition" ]] || exit 2
pgrep -af '[b]enchexec|build-gcc13-release/bin/[g]enmc' && exit 2

export PYTHONPATH=/workspace/experiments/fair-coverage/pipeline
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"

mkdir -p "$out/v9" "$out/v10"
v9_rewrite=/workspace/experiments/caat-optimization/v10-full-rewrite/v9
v10_rewrite=/workspace/experiments/caat-optimization/v10-full-rewrite/v10
mkdir -p "$v9_rewrite" "$v10_rewrite"
printf 'variant\tcores\tstatus\tfinished\n' >"$out/manifest.tsv"
sha256sum "$binary" "$v9_wrapper" "$v10_wrapper" "$definition" >"$out/input-sha256.txt"
env GENMC_BINARY="$v9_wrapper" GENMC_REAL_BINARY="$binary" GENMC_REWRITE_ROOT="$v9_rewrite" \
	benchexec --no-container --numOfThreads 24 --allowedCores 0-23 \
	--outputpath "$out/v9/" "$definition" >"$out/v9.console.log" 2>&1 &
v9_pid=$!
env GENMC_BINARY="$v10_wrapper" GENMC_REAL_BINARY="$binary" GENMC_REWRITE_ROOT="$v10_rewrite" \
	benchexec --no-container --numOfThreads 24 --allowedCores 28-51 \
	--outputpath "$out/v10/" "$definition" >"$out/v10.console.log" 2>&1 &
v10_pid=$!
v9_status=0
v10_status=0
wait "$v9_pid" || v9_status=$?
printf 'v9\t%s\t%s\t%s\n' 0-23 "$v9_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
wait "$v10_pid" || v10_status=$?
printf 'v10\t%s\t%s\t%s\n' 28-51 "$v10_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
((v9_status == 0 && v10_status == 0)) || exit 1
date -Iseconds >"$out/complete.txt"
