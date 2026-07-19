#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
pipeline="$root/endpoint-pipeline"
binary="$root/build-gcc13-release/bin/genmc"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/core-direction-review-20260718/finite-sc-completion-panel.xml"
out="${1:?pass a new SC-completion result directory}"

[[ -f /.dockerenv ]] || {
  echo "finite SC completion experiments must run inside the server Docker container" >&2
  exit 2
}
[[ "$out" == "$root/formal-results/finite-sc-completion-panel-"* && ! -e "$out" ]] || exit 2
[[ -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
! pgrep -af '/bin/[g]enmc|[b]enchexec' || exit 2
mkdir -p "$out/results"
sha256sum "$binary" "$wrapper" "$definition" "$pipeline/tools/rewrite_sources.py" \
  >"$out/input-sha256.txt"

export PYTHONPATH="$pipeline"
export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"
rewrite="/workspace/experiments/caat-optimization/finite-skeleton-rewrite/$(basename "$out")"
mkdir -p "$rewrite"
status=0
env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
  GENMC_EXPERIMENT_MODE=skeleton-census GENMC_REWRITE_ROOT="$rewrite" \
  benchexec --no-container --numOfThreads 15 --allowedCores 0-14 \
  --outputpath "$out/results/" "$definition" >"$out/results/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\nfinite-sc-completion-panel\t%s\t%s\n' "$status" \
  "$(date -Iseconds)" >"$out/manifest.tsv"
((status == 0)) || exit "$status"
date -Iseconds >"$out/complete.txt"
