#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/experiments/fair-coverage/pipeline
export GENMC_BINARY=/data3/sujie/experiments/caat-optimization/rejection-kernel-cache/build/bin/genmc
export GENMC_BENCHMARK_ROOT=/data3/sujie/svcomp2026-caat/sv-benchmarks/c
export GENMC_COMPAT_HEADER=/data3/sujie/experiments/fair-coverage/pipeline/include/svcomp_genmc_compat.h
export GENMC_MODEL_ROOT=/data3/sujie/experiments/caat-optimization/rejection-kernel-census/model-roots/pso

root=/data3/sujie/experiments/caat-optimization/rejection-kernel-cache
out="$root/timeout-oom-pso/r01"
mkdir -p "$out/baseline" "$out/cache"

export GENMC_REWRITE_ROOT="$root/rewritten-timeout-oom/baseline"
benchexec --no-container --numOfThreads 24 --allowedCores 0-23 \
	--outputpath "$out/baseline/" "$root/definitions/fair-baseline-timeout-oom-pso.xml" \
	> "$out/baseline.console.log" 2>&1 &
baseline_pid=$!

export GENMC_REWRITE_ROOT="$root/rewritten-timeout-oom/cache"
benchexec --no-container --numOfThreads 24 --allowedCores 28-51 \
	--outputpath "$out/cache/" "$root/definitions/fair-cache-timeout-oom-pso.xml" \
	> "$out/cache.console.log" 2>&1 &
cache_pid=$!

baseline_status=0
cache_status=0
wait "$baseline_pid" || baseline_status=$?
wait "$cache_pid" || cache_status=$?
printf 'baseline_status=%s cache_status=%s finished=%s\n' \
	"$baseline_status" "$cache_status" "$(date -Iseconds)" > "$out/status.txt"
if ((baseline_status != 0 || cache_status != 0)); then
	exit 1
fi
