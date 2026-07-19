#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
pipeline="$root/endpoint-pipeline"
binary=/data3/sujie/svcomp2026-caat/build/genmc-caat-llvm15noble/bin/genmc
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/core-direction-review-20260718/decisive-diagnostic-15.xml"
out="${1:?pass the dedicated paired result directory}"

[[ -f /.dockerenv ]] || { echo "must run inside the server Docker container" >&2; exit 2; }
[[ "$out" == "$root/formal-results/primitive-cache-paired-15-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
mkdir -p "$out/baseline/run" "$out/candidate/run"
sha256sum "$binary" "$wrapper" "$definition" >"$out/input-sha256.txt"
export PYTHONPATH="$pipeline"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT=/data3/sujie/svcomp2026-caat/source/genmc/models/cat
export GENMC_BINARY="$wrapper"
export GENMC_REAL_BINARY="$binary"

run_one()
{
	local mode="$1"
	local cores="$2"
	local target="$3"
	local rewrite="/workspace/experiments/caat-optimization/primitive-cache-rewrite/$(basename "$out")/$mode"
	mkdir -p "$rewrite"
	GENMC_EXPERIMENT_MODE="$mode" \
	GENMC_REWRITE_ROOT="$rewrite" \
	benchexec --no-container --numOfThreads 4 --allowedCores "$cores" \
		--outputpath "$target/run/" "$definition" >"$target/run/console.log" 2>&1
}

baseline_status=0
candidate_status=0
run_one baseline 0-3 "$out/baseline" &
baseline_pid=$!
run_one primitive-cache 4-7 "$out/candidate" &
candidate_pid=$!
wait "$baseline_pid" || baseline_status=$?
wait "$candidate_pid" || candidate_status=$?
printf 'configuration\tstatus\tfinished\nbaseline\t%s\t%s\nprimitive-cache\t%s\t%s\n' \
	"$baseline_status" "$(date -Iseconds)" "$candidate_status" "$(date -Iseconds)" \
	>"$out/manifest.tsv"
((baseline_status == 0 && candidate_status == 0)) || exit 1
date -Iseconds >"$out/complete.txt"
