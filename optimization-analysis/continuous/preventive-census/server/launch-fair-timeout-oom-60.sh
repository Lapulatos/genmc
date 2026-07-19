#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/experiments/caat-optimization/preventive-census/pipeline
export GENMC_BINARY=/data3/sujie/experiments/caat-optimization/preventive-census/build-serverpath/bin/genmc
export GENMC_BENCHMARK_ROOT=/data3/sujie/svcomp2026-caat/sv-benchmarks/c
export GENMC_REWRITE_ROOT=/data3/sujie/experiments/caat-optimization/preventive-census/fair-rewritten
export GENMC_COMPAT_HEADER=/data3/sujie/experiments/fair-coverage/pipeline/include/svcomp_genmc_compat.h
export GENMC_MODEL_ROOT=/data3/sujie/svcomp2026-caat/source/genmc-caat/models/cat

root=/data3/sujie/experiments/caat-optimization/preventive-census
out="$root/fair-timeout-oom-60"
mkdir -p "$out"
benchexec --no-container --numOfThreads 48 --outputpath "$out/" \
  "$root/definitions/fair-preventive-timeout-oom-pso.xml" > "$out/console.log" 2>&1
printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
