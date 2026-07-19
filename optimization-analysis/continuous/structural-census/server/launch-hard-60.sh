#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/structural-census
fair_root=/data3/sujie/experiments/caat-optimization/preventive-census
export PYTHONPATH="$fair_root/pipeline"
export GENMC_BINARY="$root/build/bin/genmc"
export GENMC_BENCHMARK_ROOT=/data3/sujie/svcomp2026-caat/sv-benchmarks/c
export GENMC_REWRITE_ROOT="$root/hard-rewritten"
export GENMC_COMPAT_HEADER=/data3/sujie/experiments/fair-coverage/pipeline/include/svcomp_genmc_compat.h
export GENMC_MODEL_ROOT=/data3/sujie/svcomp2026-caat/source/genmc-caat/models/cat

out="$root/hard-60"
mkdir -p "$out"
benchexec --no-container --numOfThreads 48 --outputpath "$out/" \
	"$fair_root/definitions/fair-preventive-timeout-oom-pso.xml" \
	> "$out/console.log" 2>&1
printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
