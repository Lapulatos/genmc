#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/incremental-topological-certificate
pipeline=/data3/sujie/svcomp2026-caat
output="$root/${1:-pilot}"
definition="$root/definitions/pilot.xml"

cd "$pipeline"
export PYTHONPATH="$pipeline"
mkdir -p "$output"
printf 'repetition\tbefore_cores\tafter_cores\tbefore_status\tafter_status\n' \
	> "$output/progress.tsv"

for repetition in 1 2 3 4 5 6; do
	printf -v tag 'r%02d' "$repetition"
	before_cores=0-5
	after_cores=6-11
	variants=(before after)
	if ((repetition % 2 == 0)); then
		before_cores=6-11
		after_cores=0-5
		variants=(after before)
	fi
	declare -A pids=()
	for variant in "${variants[@]}"; do
		if [[ "$variant" == before ]]; then
			binary=/data3/sujie/experiments/caat-optimization/fused-lazy-cycle/build-before/bin/genmc
			cores="$before_cores"
		else
			binary="$root/build-after/bin/genmc"
			cores="$after_cores"
		fi
		run_output="$output/$variant/$tag"
		mkdir -p "$run_output"
		(
			export GENMC_BINARY="$binary"
			export GENMC_EXPERIMENT_ROOT="$pipeline"
			benchexec --no-container --numOfThreads 6 --allowedCores "$cores" \
				--outputpath "$run_output/" "$definition"
		) > "$run_output.console.log" 2>&1 &
		pids[$variant]="$!"
	done
	before_status=0
	after_status=0
	wait "${pids[before]}" || before_status=$?
	wait "${pids[after]}" || after_status=$?
	printf '%s\t%s\t%s\t%s\t%s\n' "$repetition" "$before_cores" "$after_cores" \
		"$before_status" "$after_status" >> "$output/progress.tsv"
	((before_status == 0 && after_status == 0))
done

date --iso-8601=seconds > "$output/complete.txt"
