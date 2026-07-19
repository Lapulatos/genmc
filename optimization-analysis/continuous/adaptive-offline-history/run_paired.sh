#!/usr/bin/env bash

set -euo pipefail

root=/workspace/experiments/caat-optimization/adaptive-offline-history
definitions="$root/definitions"
output="$root/formal"
before_binary=/workspace/experiments/caat-optimization/pso-adaptive-offline/build/bin/genmc
after_binary="$root/build-after/bin/genmc"
cores_a=0-23
cores_b=28-51

cd /workspace/svcomp2026-caat
export PYTHONPATH=/workspace/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/workspace/svcomp2026-caat
mkdir -p "$output"
printf 'model\trepetition\tbefore_cores\tafter_cores\tbefore_status\tafter_status\tstarted\tfinished\n' \
  > "$output/progress.tsv"

for model in sc tso pso; do
  for repetition in 1 2 3 4 5; do
    printf -v tag 'r%02d' "$repetition"
    if (( repetition % 2 == 1 )); then
      before_cores=$cores_a
      after_cores=$cores_b
    else
      before_cores=$cores_b
      after_cores=$cores_a
    fi
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
done
