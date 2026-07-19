#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
pipeline="/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline"
binary="${GENMC_CENSUS_BINARY:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests/bin/genmc}"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/core-direction-review-20260718/finite-representation-census-pthread-wmm.xml"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
expected=283
cores="${GENMC_CENSUS_CORES:-0-23,28-51}"
threads="${GENMC_CENSUS_THREADS:-48}"
container_memory="${GENMC_CENSUS_MEMORY:-220g}"

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/deagle-rvf-census-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
[[ "$threads" =~ ^[1-9][0-9]*$ ]] || exit 2
mkdir -p "$output_root/census"
sha256sum "$binary" "$wrapper" "$definition" >"$output_root/input-sha256.txt"

status=0
docker run --rm --privileged --cpuset-cpus="$cores" --memory="$container_memory" \
	--memory-swap="$container_memory" \
	-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
	-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
	genmc15noble:sujie bash -lc \
	"export PYTHONPATH='$pipeline'
	export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
	export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
	export GENMC_MODEL_ROOT='$source_root/models/cat'
	export GENMC_BINARY='$wrapper'
	export GENMC_REAL_BINARY='$binary'
	export GENMC_EXPERIMENT_MODE=skeleton-census
	export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-rewrite
	benchexec --no-container --numOfThreads '$threads' --allowedCores '$cores' \
		--outputpath '$output_root/census/' '$definition'" \
	>"$output_root/census/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\ndeagle-rvf-census\t%s\t%s\n' "$status" \
	"$(date -Iseconds)" >"$output_root/manifest.tsv"
((status == 0)) || exit "$status"

mapfile -t archives < <(find "$output_root/census" -maxdepth 1 -name '*.logfiles.zip')
mapfile -t result_xmls < <(find "$output_root/census" -maxdepth 1 -name '*.xml.bz2')
[[ ${#archives[@]} -eq 1 && ${#result_xmls[@]} -eq 1 ]] || exit 3
[[ "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq "$expected" ]] || exit 3
[[ "$(unzip -Z1 "${archives[0]}" | grep -c .)" -eq "$expected" ]] || exit 3
date -Iseconds >"$output_root/complete.txt"
