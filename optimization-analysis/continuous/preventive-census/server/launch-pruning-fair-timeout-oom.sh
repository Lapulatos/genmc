#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/preventive-census
export PYTHONPATH="$root/pipeline"
export GENMC_BENCHMARK_ROOT=/data3/sujie/svcomp2026-caat/sv-benchmarks/c
export GENMC_COMPAT_HEADER=/data3/sujie/experiments/fair-coverage/pipeline/include/svcomp_genmc_compat.h
export GENMC_MODEL_ROOT=/data3/sujie/svcomp2026-caat/source/genmc-caat/models/cat

binary="$root/build-serverpath/bin/genmc"
wrapper="$root/preventive-genmc-wrapper.sh"
definition="$root/definitions/fair-preventive-timeout-oom-pso.xml"
out="$root/pruning-fair-timeout-oom-60"
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
		GENMC_REWRITE_ROOT="$root/fair-rewritten-pruning/r$repetition/baseline" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$baseline_cores" \
		--outputpath "$pair/baseline/" "$definition" \
		> "$pair/baseline.console.log" 2>&1 &
	baseline_pid=$!
	env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
		GENMC_REWRITE_ROOT="$root/fair-rewritten-pruning/r$repetition/pruning" \
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
