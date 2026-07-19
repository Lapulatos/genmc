#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat

root=/data3/sujie/experiments/caat-optimization/preventive-history
wrapper=/data3/sujie/experiments/caat-optimization/preventive-census/preventive-genmc-wrapper.sh
definition=/data3/sujie/experiments/caat-optimization/preventive-census/definitions/pruning-caat-pso.xml
out="$root/formal-96"
mkdir -p "$out"
env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$root/build/bin/genmc" \
	benchexec --no-container --numOfThreads 48 --outputpath "$out/" "$definition" \
	> "$out/console.log" 2>&1
printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
