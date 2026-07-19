#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/grouped-primitive-materialization
definitions="$root/definitions"
output="$root/formal"
pipeline=/data3/sujie/svcomp2026-caat
before_binary="$root/build-before/bin/genmc"
after_binary="$root/build-after/bin/genmc"
models=(sc tso pso)
before_ranges=(0-7 16-23 32-39)
after_ranges=(8-15 24-31 40-47)

cd "$pipeline"
export PYTHONPATH="$pipeline"
mkdir -p "$output"
printf 'repetition\tmodel\tbefore_cores\tafter_cores\tbefore_status\tafter_status\tstarted\tfinished\n' \
	> "$output/progress.tsv"

for repetition in 1 2 3 4; do
	printf -v tag 'r%02d' "$repetition"
	pids=()
	variants=()
	model_indices=()
	started=$(date --iso-8601=seconds)
	for index in 0 1 2; do
		before_cores="${before_ranges[$index]}"
		after_cores="${after_ranges[$index]}"
		if ((repetition % 2 == 0)); then
			temporary="$before_cores"
			before_cores="$after_cores"
			after_cores="$temporary"
		fi
		for variant in before after; do
			binary="$before_binary"
			cores="$before_cores"
			if [[ "$variant" == after ]]; then
				binary="$after_binary"
				cores="$after_cores"
			fi
			model="${models[$index]}"
			run_output="$output/$variant/$model/$tag"
			mkdir -p "$run_output"
			(
				export GENMC_BINARY="$binary"
				export GENMC_EXPERIMENT_ROOT="$pipeline"
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
		if ((exit_code != 0)); then
			failed=1
		fi
	done
	finished=$(date --iso-8601=seconds)
	for index in 0 1 2; do
		before_cores="${before_ranges[$index]}"
		after_cores="${after_ranges[$index]}"
		if ((repetition % 2 == 0)); then
			temporary="$before_cores"
			before_cores="$after_cores"
			after_cores="$temporary"
		fi
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"$repetition" "${models[$index]}" "$before_cores" "$after_cores" \
			"${statuses[$index-before]}" "${statuses[$index-after]}" \
			"$started" "$finished" >> "$output/progress.tsv"
	done
	((failed == 0))
done

printf 'complete=%s\n' "$(date --iso-8601=seconds)" > "$output/status.txt"
