#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
pipeline="$root/endpoint-pipeline"
binary="$root/build-gcc13-release/bin/genmc"
definition="$source_root/optimization-analysis/core-direction-review-20260718/finite-symbolic-full-725.xml"
out="${1:?pass a new finite-symbolic-full-725 result directory}"

[[ "$out" == "$root/formal-results/finite-symbolic-full-725-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -f "$definition" ]] || exit 2
! pgrep -af '/bin/[g]enmc|[b]enchexec' || exit 2
mkdir -p "$out"
printf 'configuration\tcores\tstatus\tfinished\n' >"$out/manifest.tsv"
sha256sum "$binary" "$definition" "$pipeline/tools/genmc_svcomp_fair.py" \
  "$pipeline/tools/rewrite_sources.py" >"$out/input-sha256.txt"

export PYTHONPATH="$pipeline"
export GENMC_BINARY="$binary"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"

run_one() {
  local configuration=$1 cores=$2
  local result="$out/$configuration"
  local rewrite="/workspace/experiments/caat-optimization/finite-symbolic-full-rewrite/$(basename "$out")/$configuration"
  mkdir -p "$result" "$rewrite"
  local status=0
  GENMC_REWRITE_ROOT="$rewrite" benchexec --no-container --numOfThreads 24 \
    --allowedCores "$cores" --rundefinition "$configuration" \
    --outputpath "$result/" "$definition" >"$result/console.log" 2>&1 || status=$?
  printf '%s\t%s\t%s\t%s\n' "$configuration" "$cores" "$status" \
    "$(date -Iseconds)" >>"$out/manifest.tsv"
  return "$status"
}

run_one baseline 0-23 & baseline_pid=$!
run_one finite-symbolic 28-51 & candidate_pid=$!
status=0
wait "$baseline_pid" || status=$?
wait "$candidate_pid" || status=$?
((status == 0)) || exit "$status"
date -Iseconds >"$out/complete.txt"
