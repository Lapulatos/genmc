#!/usr/bin/env bash

set -euo pipefail

root=/workspace/experiments/caat-optimization/adaptive-offline-state
definitions="$root/definitions"
output="$root/formal"
before_binary=/workspace/experiments/caat-optimization/cost-selector/build-tests/bin/genmc
after_binary="$root/build-tests/bin/genmc"
before_cores=28-51
after_cores=0-23
repetition=6
tag=r06

cd /workspace/svcomp2026-caat
export PYTHONPATH=/workspace/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/workspace/svcomp2026-caat

for model in sc tso pso; do
  before_out="$output/before/$model/$tag"
  after_out="$output/after/$model/$tag"
  mkdir -p "$before_out" "$after_out"
  started=$(date --iso-8601=seconds)
  (
    export GENMC_BINARY=$before_binary
    benchexec --no-container -N24 --allowedCores "$before_cores" \
      --outputpath "$before_out" "$definitions/perf-caat-$model.xml"
  ) > "$before_out.console.log" 2>&1 &
  before_pid=$!
  (
    export GENMC_BINARY=$after_binary
    benchexec --no-container -N24 --allowedCores "$after_cores" \
      --outputpath "$after_out" "$definitions/perf-caat-$model.xml"
  ) > "$after_out.console.log" 2>&1 &
  after_pid=$!

  set +e
  wait "$before_pid"
  before_status=$?
  wait "$after_pid"
  after_status=$?
  set -e
  finished=$(date --iso-8601=seconds)
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$model" "$repetition" "$before_cores" "$after_cores" \
    "$before_status" "$after_status" "$started" "$finished" \
    >> "$output/progress.tsv"
  if (( before_status != 0 || after_status != 0 )); then
    exit 1
  fi
done
