#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat
export GENMC_BINARY=/data3/sujie/experiments/caat-optimization/rejection-kernel-census/build-final/bin/genmc

root=/data3/sujie/experiments/caat-optimization/rejection-kernel-census
out="$root/formal-96"
mkdir -p "$out"

for model in sc tso pso; do
	for rep in 01 02 03; do
		pair="$out/$model/r$rep"
		mkdir -p "$pair/baseline" "$pair/census"
		if [[ "$rep" == 02 ]]; then
			baseline_cores=28-51
			census_cores=0-23
		else
			baseline_cores=0-23
			census_cores=28-51
		fi

		benchexec --no-container --numOfThreads 24 --allowedCores "$baseline_cores" \
			--outputpath "$pair/baseline/" \
			"$root/definitions/baseline-caat-$model.xml" \
			> "$pair/baseline.console.log" 2>&1 &
		baseline_pid=$!
		benchexec --no-container --numOfThreads 24 --allowedCores "$census_cores" \
			--outputpath "$pair/census/" \
			"$root/definitions/census-caat-$model.xml" \
			> "$pair/census.console.log" 2>&1 &
		census_pid=$!

		baseline_status=0
		census_status=0
		wait "$baseline_pid" || baseline_status=$?
		wait "$census_pid" || census_status=$?
		printf 'baseline_status=%s census_status=%s finished=%s\n' \
			"$baseline_status" "$census_status" "$(date -Iseconds)" \
			> "$pair/status.txt"
		if ((baseline_status != 0 || census_status != 0)); then
			exit 1
		fi
	done
done

printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
