#!/bin/bash

set -euo pipefail

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat
export GENMC_BINARY=/data3/sujie/experiments/caat-optimization/rejection-kernel-cache/build/bin/genmc

root=/data3/sujie/experiments/caat-optimization/rejection-kernel-cache
out="$root/formal-96"
mkdir -p "$out"

for model in sc tso pso; do
	for rep in 01 02 03; do
		pair="$out/$model/r$rep"
		mkdir -p "$pair/baseline" "$pair/cache"
		if [[ "$rep" == 02 ]]; then
			baseline_cores=28-51
			cache_cores=0-23
		else
			baseline_cores=0-23
			cache_cores=28-51
		fi
		benchexec --no-container --numOfThreads 24 --allowedCores "$baseline_cores" \
			--outputpath "$pair/baseline/" "$root/definitions/baseline-caat-$model.xml" \
			> "$pair/baseline.console.log" 2>&1 &
		baseline_pid=$!
		benchexec --no-container --numOfThreads 24 --allowedCores "$cache_cores" \
			--outputpath "$pair/cache/" "$root/definitions/cache-caat-$model.xml" \
			> "$pair/cache.console.log" 2>&1 &
		cache_pid=$!
		baseline_status=0
		cache_status=0
		wait "$baseline_pid" || baseline_status=$?
		wait "$cache_pid" || cache_status=$?
		printf 'baseline_status=%s cache_status=%s finished=%s\n' \
			"$baseline_status" "$cache_status" "$(date -Iseconds)" > "$pair/status.txt"
		if ((baseline_status != 0 || cache_status != 0)); then
			exit 1
		fi
	done
done

printf 'complete=%s\n' "$(date -Iseconds)" > "$out/status.txt"
