#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
before_binary="$root/build-gcc13-v12-before/bin/genmc"
after_binary="$root/build-gcc13-release/bin/genmc"
wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/generic-preventive-wrapper.sh"
definition=/workspace/experiments/caat-optimization/rvf-opportunity-census/definitions/full-caat-pso.xml
out="${1:?pass a new full-725 result directory}"

[[ "$out" == "$root/formal-results/v12-full-725-"* && ! -e "$out" ]] || exit 2
[[ -x "$before_binary" && -x "$after_binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
pgrep -af '[b]enchexec|build-gcc13-(v12-before|release)/bin/[g]enmc' && exit 2
available_kib=$(df -Pk /data3/sujie | awk 'NR==2 {print $4}')
((available_kib >= 100 * 1024 * 1024)) || exit 2

export PYTHONPATH=/workspace/experiments/fair-coverage/pipeline
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"

mkdir -p "$out/before" "$out/after"
run_id=$(basename "$out")
before_rewrite="/workspace/experiments/caat-optimization/v12-full-rewrite/$run_id/before"
after_rewrite="/workspace/experiments/caat-optimization/v12-full-rewrite/$run_id/after"
[[ ! -e "$before_rewrite" && ! -e "$after_rewrite" ]] || exit 2
mkdir -p "$before_rewrite" "$after_rewrite"
printf 'variant\tcores\tstatus\tfinished\n' >"$out/manifest.tsv"
sha256sum "$before_binary" "$after_binary" "$wrapper" "$definition" >"$out/input-sha256.txt"
env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$before_binary" \
	GENMC_REWRITE_ROOT="$before_rewrite" \
	benchexec --no-container --numOfThreads 24 --allowedCores 0-23 \
	--outputpath "$out/before/" "$definition" >"$out/before.console.log" 2>&1 &
before_pid=$!
env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$after_binary" \
	GENMC_REWRITE_ROOT="$after_rewrite" \
	benchexec --no-container --numOfThreads 24 --allowedCores 28-51 \
	--outputpath "$out/after/" "$definition" >"$out/after.console.log" 2>&1 &
after_pid=$!
before_status=0
after_status=0
wait "$before_pid" || before_status=$?
printf 'before\t%s\t%s\t%s\n' 0-23 "$before_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
wait "$after_pid" || after_status=$?
printf 'after\t%s\t%s\t%s\n' 28-51 "$after_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
((before_status == 0 && after_status == 0)) || exit 1
date -Iseconds >"$out/complete.txt"
