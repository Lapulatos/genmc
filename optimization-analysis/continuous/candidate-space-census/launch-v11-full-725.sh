#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
binary="$root/build-gcc13-release/bin/genmc"
v9_wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/generic-preventive-wrapper.sh"
v11_wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/v11-genmc-wrapper.sh"
definition=/workspace/experiments/caat-optimization/rvf-opportunity-census/definitions/full-caat-pso.xml
out="${1:?pass a new full-725 result directory}"

[[ "$out" == "$root/formal-results/v11-full-725-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$v9_wrapper" && -x "$v11_wrapper" && -f "$definition" ]] || exit 2
"$binary" --help 2>&1 | grep -q -- '--cat-focus-reach' || exit 2
pgrep -af '[b]enchexec|build-gcc13-release/bin/[g]enmc' && exit 2
available_kib=$(df -Pk /data3/sujie | awk 'NR==2 {print $4}')
((available_kib >= 100 * 1024 * 1024)) || exit 2

export PYTHONPATH=/workspace/experiments/fair-coverage/pipeline
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"

mkdir -p "$out/v9" "$out/v11"
v9_rewrite=/workspace/experiments/caat-optimization/v11-full-rewrite/v9
v11_rewrite=/workspace/experiments/caat-optimization/v11-full-rewrite/v11
mkdir -p "$v9_rewrite" "$v11_rewrite"
printf 'variant\tcores\tstatus\tfinished\n' >"$out/manifest.tsv"
sha256sum "$binary" "$v9_wrapper" "$v11_wrapper" "$definition" >"$out/input-sha256.txt"
env GENMC_BINARY="$v9_wrapper" GENMC_REAL_BINARY="$binary" GENMC_REWRITE_ROOT="$v9_rewrite" \
	benchexec --no-container --numOfThreads 24 --allowedCores 0-23 \
	--outputpath "$out/v9/" "$definition" >"$out/v9.console.log" 2>&1 &
v9_pid=$!
env GENMC_BINARY="$v11_wrapper" GENMC_REAL_BINARY="$binary" GENMC_REWRITE_ROOT="$v11_rewrite" \
	benchexec --no-container --numOfThreads 24 --allowedCores 28-51 \
	--outputpath "$out/v11/" "$definition" >"$out/v11.console.log" 2>&1 &
v11_pid=$!
v9_status=0
v11_status=0
wait "$v9_pid" || v9_status=$?
printf 'v9\t%s\t%s\t%s\n' 0-23 "$v9_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
wait "$v11_pid" || v11_status=$?
printf 'v11\t%s\t%s\t%s\n' 28-51 "$v11_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
((v9_status == 0 && v11_status == 0)) || exit 1
date -Iseconds >"$out/complete.txt"
