#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
build_root="${GENMC_DEV_BUILD_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests}"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
definition="$source_root/optimization-analysis/core-direction-review-20260718/regional-rvf-paired-51.xml"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/p0-regional-rvf-paired-51-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$build_root/bin/genmc" && -x "$wrapper" && -f "$definition" ]] || exit 2

# Preflight the exact container-visible task set before allocating a result directory.
# BenchExec may return success for an empty selection, so its exit status is insufficient.
set_file="$(sed -n 's|.*<includesfile>\([^<]*\)</includesfile>.*|\1|p' "$definition")"
[[ -n "$set_file" ]] || { echo "cannot resolve includesfile from $definition" >&2; exit 2; }
case "$set_file" in
	/data3/sujie/*) set_host="$set_file" ;;
	/workspace/*) set_host="/data3/sujie${set_file#/workspace}" ;;
	*) echo "task set is outside the mounted container roots: $set_file" >&2; exit 2 ;;
esac
[[ -f "$set_host" ]] || { echo "host task set is missing: $set_host" >&2; exit 2; }
[[ "$(grep -cve '^[[:space:]]*$' "$set_host")" -eq 51 ]] || {
	echo "task set must contain exactly 51 nonempty rows: $set_host" >&2
	exit 2
}
if grep -Evq '^/workspace/' "$set_host"; then
	echo "task set contains a non-/workspace path: $set_host" >&2
	exit 2
fi
[[ "${GENMC_PREFLIGHT_ONLY:-0}" != 1 ]] || {
	echo "paired-51 preflight passed: 51 container-visible tasks"
	exit 0
}
mkdir -p "$output_root/baseline" "$output_root/candidate"

run_lane()
{
	local lane="$1" mode="$2" cores="$3"
	docker run --rm --privileged --cpuset-cpus="$cores" --memory=100g --memory-swap=100g \
		-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
		-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
		genmc15noble:sujie bash -lc \
		"export PYTHONPATH=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline
		export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
		export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
		export GENMC_MODEL_ROOT='$source_root/models/cat'
		export GENMC_BINARY='$wrapper'
		export GENMC_REAL_BINARY='$build_root/bin/genmc'
		export GENMC_EXPERIMENT_MODE='$mode'
		export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-$lane-rewrite
		benchexec --no-container --numOfThreads 8 --allowedCores '$cores' --outputpath '$output_root/$lane/' '$definition'" \
		>"$output_root/$lane/console.log" 2>&1
}

run_lane baseline baseline 0-7 & baseline_pid=$!
run_lane candidate regional-rvf 8-15 & candidate_pid=$!
status_baseline=0
status_candidate=0
wait "$baseline_pid" || status_baseline=$?
wait "$candidate_pid" || status_candidate=$?
printf 'lane\tstatus\n%s\t%s\n%s\t%s\n' baseline "$status_baseline" candidate "$status_candidate" \
	>"$output_root/manifest.tsv"
((status_baseline == 0 && status_candidate == 0)) || exit 1
test "$(find "$output_root/baseline" -name '*.logfiles.zip' | wc -l)" -eq 1
test "$(find "$output_root/candidate" -name '*.logfiles.zip' | wc -l)" -eq 1
for lane in baseline candidate; do
	mapfile -t result_xmls < <(find "$output_root/$lane" -name '*.xml.bz2')
	[[ ${#result_xmls[@]} -eq 1 && "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq 51 ]] || {
		echo "$lane did not execute exactly 51 tasks" >&2
		exit 3
	}
done
date -Iseconds >"$output_root/complete.txt"
