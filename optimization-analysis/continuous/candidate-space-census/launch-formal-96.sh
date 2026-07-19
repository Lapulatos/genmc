#!/bin/bash
set -euo pipefail
export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat
root=/data3/sujie/experiments/caat-optimization/candidate-space-census
binary="$root/build-serverpath/bin/genmc"
out="${1:-$root/formal-96-v2}"
mkdir -p "$out"
printf 'model\tcores\tstatus\tfinished\n' > "$out/manifest.tsv"
models=(sc tso pso)
cores=(0-15 20-35 40-55)
pids=()
for index in 0 1 2; do
  model="${models[$index]}"
  mkdir -p "$out/$model"
  env GENMC_BINARY="$binary" benchexec --no-container --numOfThreads 16 --allowedCores "${cores[$index]}" --outputpath "$out/$model/" "$root/definitions/census-caat-$model.xml" > "$out/$model/console.log" 2>&1 &
  pids+=("$!")
done
failed=0
for index in 0 1 2; do
  status=0
  wait "${pids[$index]}" || status=$?
  printf '%s\t%s\t%s\t%s\n' "${models[$index]}" "${cores[$index]}" "$status" "$(date -Iseconds)" >> "$out/manifest.tsv"
  ((status == 0)) || failed=1
done
((failed == 0))
date -Iseconds > "$out/complete.txt"
