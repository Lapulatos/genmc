#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
pipeline="/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline"
binary="${GENMC_FIRST_MODEL_BINARY:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/build-dev-tests/bin/genmc}"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="${GENMC_FIRST_MODEL_DEFINITION:-$source_root/optimization-analysis/core-direction-review-20260718/finite-rvf-first-model-panel.xml}"
task_set="${GENMC_FIRST_MODEL_TASK_SET:-$source_root/optimization-analysis/core-direction-review-20260718/finite-rvf-first-model-panel.set}"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
expected="${GENMC_FIRST_MODEL_EXPECTED:-45}"
expected_per_mode="${GENMC_FIRST_MODEL_EXPECTED_PER_MODE:-15}"
cores="0-23,28-51"

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$binary" && -x "$wrapper" && -f "$definition" ]] || exit 2
[[ "$expected" =~ ^[1-9][0-9]*$ && "$expected_per_mode" =~ ^[1-9][0-9]*$ ]] || exit 2
if [[ "$task_set" != none ]]; then
	[[ -f "$task_set" && "$(grep -cve '^[[:space:]]*$' "$task_set")" -eq "$expected_per_mode" ]] || exit 2
fi
mkdir -p "$output_root/results"
sha256sum "$binary" "$wrapper" "$definition" >"$output_root/input-sha256.txt"
[[ "$task_set" == none ]] || sha256sum "$task_set" >>"$output_root/input-sha256.txt"

status=0
docker run --rm --privileged --cpuset-cpus="$cores" --memory=220g --memory-swap=220g \
	-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
	-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
	genmc15noble:sujie bash -lc \
	"export PYTHONPATH='$pipeline'
	export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
	export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
	export GENMC_MODEL_ROOT='$source_root/models/cat'
	export GENMC_BINARY='$wrapper'
	export GENMC_REAL_BINARY='$binary'
	export GENMC_EXPERIMENT_MODE=baseline
	export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-rewrite
	benchexec --no-container --numOfThreads 48 --allowedCores '$cores' \
		--outputpath '$output_root/results/' '$definition'" \
	>"$output_root/results/console.log" 2>&1 || status=$?
printf 'configuration\tstatus\tfinished\nfinite-rvf-first-model\t%s\t%s\n' "$status" \
	"$(date -Iseconds)" >"$output_root/manifest.tsv"
((status == 0)) || exit "$status"

mapfile -t archives < <(find "$output_root/results" -maxdepth 1 -name '*.logfiles.zip')
mapfile -t result_xmls < <(find "$output_root/results" -maxdepth 1 -name '*.xml.bz2')
[[ ${#archives[@]} -eq 1 && ${#result_xmls[@]} -eq 3 ]] || exit 3
total_runs=0
for xml in "${result_xmls[@]}"; do
	runs="$(bzcat "$xml" | grep -c '<run ')"
	[[ "$runs" -eq "$expected_per_mode" ]] || exit 3
	total_runs=$((total_runs + runs))
done
[[ "$total_runs" -eq "$expected" ]] || exit 3
[[ "$(unzip -Z1 "${archives[0]}" | grep -c .)" -eq "$expected" ]] || exit 3
date -Iseconds >"$output_root/complete.txt"
