#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/learned-cat-nogoods
definitions=/data3/sujie/experiments/caat-optimization/product-eog-cycle/definitions
pipeline=/data3/sujie/svcomp2026-caat
output="$root/formal-v3"
models=(sc tso pso)
# Every range stays within one 28-core NUMA node. Even repetitions swap variants.
baseline_ranges=(0-7 16-23 36-43)
candidate_ranges=(8-15 28-35 44-51)

cd "$pipeline"
export PYTHONPATH="$pipeline"
mkdir -p "$output"
printf 'repetition\tmodel\tbaseline_cores\tcandidate_cores\tbaseline_status\tcandidate_status\tstarted\tfinished\n' \
	> "$output/progress.tsv"

for repetition in 1 2 3 4; do
	printf -v tag 'r%02d' "$repetition"
	pids=()
	variants=()
	model_indices=()
	started=$(date --iso-8601=seconds)
	for index in 0 1 2; do
		baseline_cores="${baseline_ranges[$index]}"
		candidate_cores="${candidate_ranges[$index]}"
		if ((repetition % 2 == 0)); then
			temporary="$baseline_cores"
			baseline_cores="$candidate_cores"
			candidate_cores="$temporary"
		fi
		for variant in baseline candidate; do
			cores="$baseline_cores"
			binary="$root/build-baseline/bin/genmc"
			real_binary=""
			if [[ "$variant" == candidate ]]; then
				cores="$candidate_cores"
				binary="$root/run-with-nogoods.sh"
				real_binary="$root/build-gcc/bin/genmc"
			fi
			model="${models[$index]}"
			run_output="$output/$variant/$model/$tag"
			mkdir -p "$run_output"
			(
				export GENMC_BINARY="$binary"
				export GENMC_EXPERIMENT_ROOT="$pipeline"
				if [[ -n "$real_binary" ]]; then
					export GENMC_REAL="$real_binary"
				else
					unset GENMC_REAL || true
				fi
				benchexec --no-container --numOfThreads 8 --allowedCores "$cores" \
					--outputpath "$run_output/" "$definitions/perf-caat-$model.xml"
			) > "$run_output.console.log" 2>&1 &
			pids+=("$!")
			variants+=("$variant")
			model_indices+=("$index")
		done
	done

	declare -A statuses=()
	failed=0
	for job in "${!pids[@]}"; do
		exit_code=0
		wait "${pids[$job]}" || exit_code=$?
		index="${model_indices[$job]}"
		variant="${variants[$job]}"
		statuses["$index-$variant"]="$exit_code"
		((exit_code == 0)) || failed=1
	done
	finished=$(date --iso-8601=seconds)
	for index in 0 1 2; do
		baseline_cores="${baseline_ranges[$index]}"
		candidate_cores="${candidate_ranges[$index]}"
		if ((repetition % 2 == 0)); then
			temporary="$baseline_cores"
			baseline_cores="$candidate_cores"
			candidate_cores="$temporary"
		fi
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$repetition" \
			"${models[$index]}" "$baseline_cores" "$candidate_cores" \
			"${statuses[$index-baseline]}" "${statuses[$index-candidate]}" \
			"$started" "$finished" >> "$output/progress.tsv"
	done
	((failed == 0))
done

date --iso-8601=seconds > "$output/complete.txt"
