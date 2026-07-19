#!/bin/bash
set -euo pipefail

root=/workspace/experiments/caat-optimization/rvf-opportunity-census
source_root=/data3/sujie/experiments/caat-optimization/generic-preventive
fair_root=/workspace/experiments/fair-coverage
binary="$source_root/build-release/bin/genmc"
wrapper="$source_root/generic-preventive-wrapper.sh"
out="${1:-$root/full-725}"

export PYTHONPATH="$fair_root/pipeline"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_REWRITE_ROOT="$fair_root/rewritten-census"
export GENMC_COMPAT_HEADER="$fair_root/include/svcomp_genmc_compat_v2.h"
export GENMC_MODEL_ROOT="$source_root/source/models/cat"

mkdir -p "$out"
printf 'model\tcores\tstatus\tfinished\n' > "$out/manifest.tsv"
for model in pso sc tso; do
	mkdir -p "$out/$model"
	status=0
	if [[ "$model" == pso ]]; then
		env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
			benchexec --no-container --numOfThreads 48 \
			--allowedCores 0-23,28-51 \
			--outputpath "$out/$model/" "$root/definitions/full-caat-$model.xml" \
			> "$out/$model/console.log" 2>&1 || status=$?
	else
		env GENMC_BINARY="$binary" \
			benchexec --no-container --numOfThreads 48 \
			--allowedCores 0-23,28-51 \
			--outputpath "$out/$model/" "$root/definitions/full-caat-$model.xml" \
			> "$out/$model/console.log" 2>&1 || status=$?
	fi
	printf '%s\t%s\t%s\t%s\n' "$model" 0-23,28-51 "$status" "$(date -Iseconds)" \
		>> "$out/manifest.tsv"
	((status == 0)) || exit "$status"
done
date -Iseconds > "$out/complete.txt"
