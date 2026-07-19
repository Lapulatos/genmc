#!/bin/bash
set -euo pipefail

# Run this entry point on the server host. BenchExec needs the host cgroup mount;
# keeping docker invocation here prevents an ad-hoc launch from silently running zero tasks.
source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
build_root="${GENMC_DEV_BUILD_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests}"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
definition="$source_root/optimization-analysis/core-direction-review-20260718/regional-rvf-gate-census-725.xml"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/p0-regional-rvf-gate-census-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$build_root/bin/genmc" && -x "$wrapper" && -f "$definition" ]] || exit 2
mkdir -p "$output_root"

docker run --rm --privileged --cpus=8 --memory=100g --memory-swap=100g \
	-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
	-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
	genmc15noble:sujie bash -lc \
	"export PYTHONPATH=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline
	export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
	export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
	export GENMC_MODEL_ROOT='$source_root/models/cat'
	export GENMC_BINARY='$wrapper'
	export GENMC_REAL_BINARY='$build_root/bin/genmc'
	export GENMC_EXPERIMENT_MODE=regional-rvf-gate-census
	export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-rewrite
	benchexec --no-container --numOfThreads 8 --allowedCores 0-7 --outputpath '$output_root/' '$definition'"

test "$(find "$output_root" -name '*.logfiles.zip' | wc -l)" -eq 1
mapfile -t result_xmls < <(find "$output_root" -name '*.xml.bz2')
[[ ${#result_xmls[@]} -eq 1 && "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq 725 ]] || {
	echo "gate census did not execute exactly 725 tasks" >&2
	exit 3
}
date -Iseconds >"$output_root/complete.txt"
