#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat
export GENMC_BINARY=/data3/sujie/experiments/caat-optimization/preventive-census/build-serverpath/bin/genmc

root=/data3/sujie/experiments/caat-optimization/preventive-census
out="$root/formal-96"
mkdir -p "$out"
benchexec --no-container --numOfThreads 48 --outputpath "$out/" \
  "$root/definitions/preventive-caat-pso.xml" > "$out/console.log" 2>&1
printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
