#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/learned-cat-nogoods
pipeline=/data3/sujie/svcomp2026-caat
definitions="$root/pilot-definitions"
output="$root/pilot-v4"

cd "$pipeline"
export PYTHONPATH="$pipeline"
export GENMC_EXPERIMENT_ROOT="$pipeline"
rm -rf "$output"
mkdir -p "$output/core/baseline" "$output/core/candidate"

run_benchexec()
{
	local variant="$1"
	local cores="$2"
	local definition="$3"
	local destination="$4"
	local binary="$root/build-baseline/bin/genmc"
	local real_binary=""
	if [[ "$variant" == candidate ]]; then
		binary="$root/run-with-nogoods.sh"
		real_binary="$root/build-gcc/bin/genmc"
	fi
	(
		export GENMC_BINARY="$binary"
		if [[ -n "$real_binary" ]]; then
			export GENMC_REAL="$real_binary"
		else
			unset GENMC_REAL || true
		fi
		benchexec --no-container --numOfThreads 8 --allowedCores "$cores" \
			--outputpath "$destination/" "$definition"
	) > "$destination.console.log" 2>&1
}

run_benchexec baseline 0-7 "$definitions/core-sc.xml" "$output/core/baseline" &
baseline_core=$!
run_benchexec candidate 8-15 "$definitions/core-sc.xml" "$output/core/candidate" &
candidate_core=$!
wait "$baseline_core"
wait "$candidate_core"

models=(sc tso pso)
baseline_ranges=(0-7 16-23 36-43)
candidate_ranges=(8-15 28-35 44-51)
pids=()
for index in 0 1 2; do
	model="${models[$index]}"
	for variant in baseline candidate; do
		cores="${baseline_ranges[$index]}"
		if [[ "$variant" == candidate ]]; then
			cores="${candidate_ranges[$index]}"
		fi
		destination="$output/hit-rich/$variant/$model"
		mkdir -p "$destination"
		run_benchexec "$variant" "$cores" "$definitions/hit-rich-$model.xml" \
			"$destination" &
		pids+=("$!")
	done
done
for pid in "${pids[@]}"; do
	wait "$pid"
done

date --iso-8601=seconds > "$output/complete.txt"
