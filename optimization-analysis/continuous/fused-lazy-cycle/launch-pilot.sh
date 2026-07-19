#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/fused-lazy-cycle
pipeline=/data3/sujie/svcomp2026-caat
output="$root/${1:-pilot}"
definition="$root/definitions/pilot.xml"

cd "$pipeline"
export PYTHONPATH="$pipeline"
mkdir -p "$output"
printf 'repetition\tbefore_status\tafter_status\n' > "$output/progress.tsv"

for repetition in 1 2; do
	printf -v tag 'r%02d' "$repetition"
	before_cores=0-2
	after_cores=3-5
	if ((repetition % 2 == 0)); then
		before_cores=3-5
		after_cores=0-2
	fi
	pids=()
	for variant in before after; do
		binary="$root/build-$variant/bin/genmc"
		cores="$before_cores"
		[[ "$variant" == after ]] && cores="$after_cores"
		run_output="$output/$variant/$tag"
		mkdir -p "$run_output"
		(
			export GENMC_BINARY="$binary"
			export GENMC_EXPERIMENT_ROOT="$pipeline"
			benchexec --no-container --numOfThreads 3 --allowedCores "$cores" \
				--outputpath "$run_output/" "$definition"
		) > "$run_output.console.log" 2>&1 &
		pids+=("$!")
	done
	before_status=0
	after_status=0
	wait "${pids[0]}" || before_status=$?
	wait "${pids[1]}" || after_status=$?
	printf '%s\t%s\t%s\n' "$repetition" "$before_status" "$after_status" \
		>> "$output/progress.tsv"
	((before_status == 0 && after_status == 0))
done

date --iso-8601=seconds > "$output/complete.txt"
