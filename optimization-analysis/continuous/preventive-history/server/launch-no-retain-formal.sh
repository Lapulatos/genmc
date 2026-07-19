#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat

root=/data3/sujie/experiments/caat-optimization/preventive-history
binary="$root/build/bin/genmc"
baseline_wrapper=/data3/sujie/experiments/caat-optimization/preventive-census/preventive-genmc-wrapper.sh
no_retain_wrapper="$root/no-retain-genmc-wrapper.sh"
definition=/data3/sujie/experiments/caat-optimization/preventive-census/definitions/pruning-caat-pso.xml
out="$root/no-retain-formal-96"
mkdir -p "$out"
printf 'repetition\tbaseline_cores\tno_retain_cores\tbaseline_status\tno_retain_status\tfinished\n' \
	> "$out/manifest.tsv"

for repetition in 01 02 03 04; do
	pair="$out/r$repetition"
	mkdir -p "$pair/baseline" "$pair/no-retain"
	if [[ "$repetition" == 02 || "$repetition" == 04 ]]; then
		baseline_cores=28-51
		no_retain_cores=0-23
	else
		baseline_cores=0-23
		no_retain_cores=28-51
	fi
	env GENMC_BINARY="$baseline_wrapper" GENMC_REAL_BINARY="$binary" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$baseline_cores" \
		--outputpath "$pair/baseline/" "$definition" \
		> "$pair/baseline.console.log" 2>&1 &
	baseline_pid=$!
	env GENMC_BINARY="$no_retain_wrapper" GENMC_REAL_BINARY="$binary" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$no_retain_cores" \
		--outputpath "$pair/no-retain/" "$definition" \
		> "$pair/no-retain.console.log" 2>&1 &
	no_retain_pid=$!
	baseline_status=0
	no_retain_status=0
	wait "$baseline_pid" || baseline_status=$?
	wait "$no_retain_pid" || no_retain_status=$?
	printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$repetition" "$baseline_cores" \
		"$no_retain_cores" "$baseline_status" "$no_retain_status" \
		"$(date -Iseconds)" >> "$out/manifest.tsv"
	if ((baseline_status != 0 || no_retain_status != 0)); then
		exit 1
	fi
done

printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
