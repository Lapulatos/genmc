#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
build_root="${GENMC_DEV_BUILD_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests}"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
definition="$source_root/optimization-analysis/core-direction-review-20260718/backjump-census-15.xml"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
case "$output_root" in
	/data3/sujie/experiments/caat-optimization/p2-backjump-census-15-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$build_root/bin/genmc" && -x "$wrapper" && -f "$definition" ]] || exit 2
[[ "$(grep -c '<include>/workspace/' "$definition")" -eq 15 ]] || {
	echo "definition must contain exactly 15 container-visible tasks" >&2; exit 2;
}
mkdir -p "$output_root/baseline" "$output_root/candidate"
sha256sum "$build_root/bin/genmc" "$wrapper" "$definition" >"$output_root/input-sha256.txt"

run_lane()
{
	local lane="$1" mode="$2" cores="$3"
	docker run --rm --privileged --cpuset-cpus="$cores" --memory=40g --memory-swap=40g \
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
		benchexec --no-container --numOfThreads 4 --allowedCores '$cores' --outputpath '$output_root/$lane/' '$definition'" \
		>"$output_root/$lane/console.log" 2>&1
}

run_lane baseline pso-v9 0-3 & baseline_pid=$!
run_lane candidate pso-backjump-census 4-7 & candidate_pid=$!
baseline_status=0; candidate_status=0
wait "$baseline_pid" || baseline_status=$?
wait "$candidate_pid" || candidate_status=$?
printf 'lane\tmode\tstatus\n%s\t%s\t%s\n%s\t%s\t%s\n' \
	baseline pso-v9 "$baseline_status" candidate pso-backjump-census "$candidate_status" \
	>"$output_root/manifest.tsv"
((baseline_status == 0 && candidate_status == 0)) || exit 1
for lane in baseline candidate; do
	mapfile -t xmls < <(find "$output_root/$lane" -name '*.xml.bz2')
	[[ ${#xmls[@]} -eq 1 && "$(bzcat "${xmls[0]}" | grep -c '<run ')" -eq 15 ]] || {
		echo "$lane did not execute exactly 15 tasks" >&2; exit 3;
	}
	[[ "$(find "$output_root/$lane" -name '*.logfiles.zip' | wc -l)" -eq 1 ]] || exit 3
done
date -Iseconds >"$output_root/complete.txt"
