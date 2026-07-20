#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
build_root="${GENMC_DEV_BUILD_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests}"
pipeline="/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline"
definition="$source_root/optimization-analysis/core-direction-review-20260718/finite-rvf-direct-panel.xml"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
output_root="${1:?pass a new result root}"
expected_per_lane=15
expected_total=30

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/finite-rvf-direct-panel-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$build_root/bin/genmc" && -x "$wrapper" &&
   -f "$definition" ]] || exit 2

mkdir -p "$output_root/results"
sha256sum "$build_root/bin/genmc" "$wrapper" "$definition" >"$output_root/input-sha256.txt"
status=0
docker run --rm --privileged --cpus=30 --memory=160g --memory-swap=160g \
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
	benchexec --no-container --numOfThreads 30 \
		--outputpath '$output_root/results/' '$definition'" \
	>"$output_root/results/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\nfinite-rvf-direct\t%s\t%s\n' "$status" \
	"$(date -Iseconds)" >"$output_root/manifest.tsv"
((status == 0)) || exit "$status"

mapfile -t archives < <(find "$output_root/results" -maxdepth 1 -name '*.logfiles.zip')
mapfile -t xmls < <(find "$output_root/results" -maxdepth 1 -name '*.xml.bz2')
[[ ${#archives[@]} -eq 1 && ${#xmls[@]} -eq 2 ]] || exit 3
total=0
for xml in "${xmls[@]}"; do
	runs="$(bzcat "$xml" | grep -c '<run ')"
	[[ "$runs" -eq "$expected_per_lane" ]] || exit 3
	total=$((total + runs))
done
[[ "$total" -eq "$expected_total" ]] || exit 3
[[ "$(unzip -Z1 "${archives[0]}" | grep -c '\.log$')" -eq "$expected_total" ]] || exit 3
date -Iseconds >"$output_root/complete.txt"
