#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
build_root="${GENMC_DEV_BUILD_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests}"
pipeline="/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline"
definition="$source_root/optimization-analysis/core-direction-review-20260718/finite-rvf-sc-order-full.xml"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
threads=40
task_memory_gb=4
container_memory_gib=500
expected=283

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-full-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$build_root/bin/genmc" && -x "$wrapper" &&
   -f "$definition" ]] || exit 2
((threads * task_memory_gb <= container_memory_gib)) || {
	echo "aggregate task memory exceeds the container allowance" >&2
	exit 2
}

mkdir -p "$output_root/results"
sha256sum "$build_root/bin/genmc" "$wrapper" "$definition" >"$output_root/input-sha256.txt"
status=0
docker run --rm --privileged --cpus=48 --memory="${container_memory_gib}g" \
	--memory-swap="${container_memory_gib}g" \
	-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
	-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
	genmc15noble:sujie bash -lc \
	"export PYTHONPATH='$pipeline'
	export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
	export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
	export GENMC_MODEL_ROOT='$source_root/models/cat'
	export GENMC_BINARY='$wrapper'
	export GENMC_REAL_BINARY='$build_root/bin/genmc'
	export GENMC_EXPERIMENT_MODE=baseline
	export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-rewrite
	benchexec --no-container --numOfThreads '$threads' \
		--outputpath '$output_root/results/' '$definition'" \
	>"$output_root/results/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\nfinite-rvf-sc-order\t%s\t%s\n' "$status" \
	"$(date -Iseconds)" >"$output_root/manifest.tsv"
((status == 0)) || exit "$status"

mapfile -t archives < <(find "$output_root/results" -maxdepth 1 -name '*.logfiles.zip')
mapfile -t result_xmls < <(find "$output_root/results" -maxdepth 1 -name '*.xml.bz2')
[[ ${#archives[@]} -eq 1 && ${#result_xmls[@]} -eq 1 ]] || exit 3
[[ "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq "$expected" ]] || exit 3
[[ "$(unzip -Z1 "${archives[0]}" | grep -c '\.log$')" -eq "$expected" ]] || exit 3
date -Iseconds >"$output_root/complete.txt"
