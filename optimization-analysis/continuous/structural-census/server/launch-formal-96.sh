#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat

root=/data3/sujie/experiments/caat-optimization/structural-census
binary="$root/build/bin/genmc"
definitions="$root/definitions"
out="$root/formal-96"
mkdir -p "$out"
printf 'model\tcores\tstatus\tfinished\n' > "$out/manifest.tsv"

models=(sc tso pso)
cores=(0-15 20-35 40-55)
pids=()
for index in 0 1 2; do
	model="${models[$index]}"
	model_out="$out/$model"
	mkdir -p "$model_out"
	env GENMC_BINARY="$binary" benchexec --no-container --numOfThreads 16 \
		--allowedCores "${cores[$index]}" --outputpath "$model_out/" \
		"$definitions/census-caat-$model.xml" \
		> "$model_out/console.log" 2>&1 &
	pids+=("$!")
done

failed=0
for index in 0 1 2; do
	status=0
	wait "${pids[$index]}" || status=$?
	printf '%s\t%s\t%s\t%s\n' "${models[$index]}" "${cores[$index]}" "$status" \
		"$(date -Iseconds)" >> "$out/manifest.tsv"
	if ((status != 0)); then
		failed=1
	fi
done
((failed == 0))
printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
