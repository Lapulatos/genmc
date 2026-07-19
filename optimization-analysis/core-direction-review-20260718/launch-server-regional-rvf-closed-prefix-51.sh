#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
build_root="${GENMC_DEV_BUILD_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests}"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
definition="$source_root/optimization-analysis/core-direction-review-20260718/regional-rvf-paired-51.xml"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
threads=36
task_memory_gb=12
container_memory_gib=500

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/p0-regional-rvf-closed-prefix-51-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$build_root/bin/genmc" && -x "$wrapper" && -f "$definition" ]] || exit 2

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
grep -Evq '^/workspace/' "$set_host" && {
	echo "task set contains a non-/workspace path: $set_host" >&2
	exit 2
}
((threads * task_memory_gb <= container_memory_gib)) || {
	echo "aggregate task memory exceeds the container allowance" >&2
	exit 2
}

mkdir -p "$output_root"
docker run --rm --privileged --cpus="$threads" --memory="${container_memory_gib}g" \
	--memory-swap="${container_memory_gib}g" \
	-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
	-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
	genmc15noble:sujie bash -lc \
	"export PYTHONPATH=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline
	export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
	export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
	export GENMC_MODEL_ROOT='$source_root/models/cat'
	export GENMC_BINARY='$wrapper'
	export GENMC_REAL_BINARY='$build_root/bin/genmc'
	export GENMC_EXPERIMENT_MODE=regional-rvf
	export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-rewrite
	benchexec --no-container --numOfThreads '$threads' --outputpath '$output_root/' '$definition'" \
	>"$output_root/console.log" 2>&1

mapfile -t archives < <(find "$output_root" -name '*.logfiles.zip')
mapfile -t result_xmls < <(find "$output_root" -name '*.xml.bz2')
[[ ${#archives[@]} -eq 1 && ${#result_xmls[@]} -eq 1 ]] || {
	echo "expected exactly one log archive and result XML" >&2
	exit 3
}
[[ "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq 51 ]] || {
	echo "closed-prefix census did not execute exactly 51 tasks" >&2
	exit 3
}
[[ "$(zipinfo -1 "${archives[0]}" | grep -c '\.log$')" -eq 51 ]] || {
	echo "closed-prefix census did not archive exactly 51 task logs" >&2
	exit 3
}
date -Iseconds >"$output_root/complete.txt"
