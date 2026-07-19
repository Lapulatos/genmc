#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat

root=/data3/sujie/experiments/caat-optimization/preventive-census
binary="$root/build-serverpath/bin/genmc"
wrapper="$root/preventive-genmc-wrapper.sh"
definition="$root/definitions/pruning-timeout-oom-pso.xml"
out="$root/pruning-timeout-oom-60"
mkdir -p "$out"
printf 'repetition\tbaseline_cores\tpruning_cores\tbaseline_status\tpruning_status\tfinished\n' \
	> "$out/manifest.tsv"

for repetition in 01 02; do
	pair="$out/r$repetition"
	mkdir -p "$pair/baseline" "$pair/pruning"
	if [[ "$repetition" == 02 ]]; then
		baseline_cores=28-51
		pruning_cores=0-23
	else
		baseline_cores=0-23
		pruning_cores=28-51
	fi
	env GENMC_BINARY="$binary" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$baseline_cores" \
		--outputpath "$pair/baseline/" "$definition" \
		> "$pair/baseline.console.log" 2>&1 &
	baseline_pid=$!
	env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$pruning_cores" \
		--outputpath "$pair/pruning/" "$definition" \
		> "$pair/pruning.console.log" 2>&1 &
	pruning_pid=$!
	baseline_status=0
	pruning_status=0
	wait "$baseline_pid" || baseline_status=$?
	wait "$pruning_pid" || pruning_status=$?
	printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$repetition" "$baseline_cores" \
		"$pruning_cores" "$baseline_status" "$pruning_status" "$(date -Iseconds)" \
		>> "$out/manifest.tsv"
	if ((baseline_status != 0 || pruning_status != 0)); then
		exit 1
	fi
done

printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
