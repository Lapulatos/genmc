#!/bin/bash
set -euo pipefail

source_root="${GENMC_DEV_SOURCE_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a/source-dev}"
experiment_root="${GENMC_P1_ROOT:-/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a}"
baseline_binary="${GENMC_P1_BASELINE_BINARY:-$experiment_root/build-stable-probe/bin/genmc}"
candidate_binary="${GENMC_P1_CANDIDATE_BINARY:-$experiment_root/build-dev-p1-cumulative/bin/genmc}"
output_root="${1:?pass a new /data3/sujie/experiments/caat-optimization result directory}"
definition="${GENMC_P1_DEFINITION:-$source_root/optimization-analysis/core-direction-review-20260718/p1-cumulative-paired-725.xml}"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
expected="${GENMC_P1_EXPECTED_TASKS:-725}"
output_prefix="${GENMC_P1_OUTPUT_PREFIX:-p1-cumulative-paired-725-}"
threads_per_lane="${GENMC_THREADS_PER_LANE:-24}"
baseline_cores="${GENMC_BASELINE_CORES:-0-23}"
candidate_cores="${GENMC_CANDIDATE_CORES:-28-51}"
container_memory="${GENMC_LANE_MEMORY:-300g}"

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/"$output_prefix"*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$baseline_binary" && -x "$candidate_binary" &&
   -x "$wrapper" && -f "$definition" ]] || exit 2
[[ "$threads_per_lane" =~ ^[1-9][0-9]*$ ]] || exit 2
[[ "$expected" =~ ^[1-9][0-9]*$ ]] || exit 2
(( $(nproc) >= 2 * threads_per_lane )) || exit 2
python3 - "$threads_per_lane" "$baseline_cores" "$candidate_cores" <<'PY'
import subprocess
import sys

threads = int(sys.argv[1])

def expand(spec):
    cpus = set()
    for part in spec.split(","):
        bounds = [int(value) for value in part.split("-", 1)]
        cpus.update(range(bounds[0], bounds[-1] + 1))
    return cpus

cpu_nodes = {}
for line in subprocess.check_output(["lscpu", "-p=CPU,NODE"], text=True).splitlines():
    if line and not line.startswith("#"):
        cpu, node = map(int, line.split(","))
        cpu_nodes[cpu] = node
lanes = [expand(sys.argv[2]), expand(sys.argv[3])]
if any(len(cpus) != threads for cpus in lanes) or lanes[0] & lanes[1]:
    raise SystemExit("lane CPU sets must be disjoint and match the worker count")
if any(len({cpu_nodes.get(cpu) for cpu in cpus}) != 1 for cpus in lanes):
    raise SystemExit("each lane CPU set must remain inside one NUMA node")
PY

set_file="$(sed -n 's|.*<includesfile>\([^<]*\)</includesfile>.*|\1|p' "$definition")"
[[ -f "$set_file" && "$(grep -cve '^[[:space:]]*$' "$set_file")" -eq "$expected" ]] || exit 2
grep -Evq '^/workspace/' "$set_file" && {
	echo "full task set contains a non-/workspace path: $set_file" >&2
	exit 2
}
mkdir -p "$output_root/baseline" "$output_root/candidate"
sha256sum "$baseline_binary" "$candidate_binary" >"$output_root/input-sha256.txt"

run_lane()
{
	local lane="$1" binary="$2" cores="$3"
	docker run --rm --privileged --cpuset-cpus="$cores" --memory="$container_memory" --memory-swap="$container_memory" \
		-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
		-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
		genmc15noble:sujie bash -lc \
		"export PYTHONPATH=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline
		export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
		export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
		export GENMC_MODEL_ROOT='$source_root/models/cat'
		export GENMC_BINARY='$wrapper'
		export GENMC_REAL_BINARY='$binary'
		export GENMC_EXPERIMENT_MODE=baseline
		export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-$lane-rewrite
		benchexec --no-container --numOfThreads '$threads_per_lane' --allowedCores '$cores' --outputpath '$output_root/$lane/' '$definition'" \
		>"$output_root/$lane/console.log" 2>&1
}

run_lane baseline "$baseline_binary" "$baseline_cores" & baseline_pid=$!
run_lane candidate "$candidate_binary" "$candidate_cores" & candidate_pid=$!
baseline_status=0
candidate_status=0
wait "$baseline_pid" || baseline_status=$?
wait "$candidate_pid" || candidate_status=$?
printf 'lane\tstatus\n%s\t%s\n%s\t%s\n' baseline "$baseline_status" candidate "$candidate_status" \
	>"$output_root/manifest.tsv"
((baseline_status == 0 && candidate_status == 0)) || exit 1

for lane in baseline candidate; do
	mapfile -t archives < <(find "$output_root/$lane" -maxdepth 1 -name '*.logfiles.zip')
	mapfile -t result_xmls < <(find "$output_root/$lane" -maxdepth 1 -name '*.xml.bz2')
	[[ ${#archives[@]} -eq 1 && ${#result_xmls[@]} -eq 1 ]] || exit 3
	[[ "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq "$expected" ]] || exit 3
	[[ "$(unzip -Z1 "${archives[0]}" | grep -c .)" -eq "$expected" ]] || exit 3
done
date -Iseconds >"$output_root/complete.txt"
