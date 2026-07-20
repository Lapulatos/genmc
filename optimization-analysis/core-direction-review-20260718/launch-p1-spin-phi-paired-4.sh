#!/bin/bash
set -euo pipefail

experiment_root=/data3/sujie/experiments/caat-optimization/core-p1-phase-census-20260719a
source_root="$experiment_root/source-dev"
baseline_binary="$experiment_root/build-stable-probe/bin/genmc"
candidate_root=/data3/sujie/experiments/caat-optimization/p1-spin-phi-clean-20260720
candidate_binary="$candidate_root/build/bin/genmc"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/core-direction-review-20260718/p1-spin-phi-paired-4.xml"
output_root="${1:?pass a new p1-spin-phi-paired-4 result directory}"

case "$output_root" in
	/data3/sujie/experiments/caat-optimization/p1-spin-phi-paired-4-*) ;;
	*) echo "refusing unsupported output root: $output_root" >&2; exit 2 ;;
esac
[[ ! -e "$output_root" && -x "$baseline_binary" && -x "$candidate_binary" &&
   -x "$wrapper" && -f "$definition" ]] || exit 2
! pgrep -af '/bin/[g]enmc|[b]enchexec' || exit 2

mkdir -p "$output_root/baseline" "$output_root/candidate"
sha256sum "$baseline_binary" "$candidate_binary" "$wrapper" "$definition" \
	>"$output_root/input-sha256.txt"
diff -qr "$experiment_root/source-stable-probe" "$candidate_root/source" \
	>"$output_root/source-diff.txt" || true
[[ "$(wc -l <"$output_root/source-diff.txt")" -eq 1 ]] || exit 2
grep -Fq 'SpinAssumePass.cpp' "$output_root/source-diff.txt" || exit 2

run_lane()
{
	local lane="$1" binary="$2" cores="$3"
	docker run --rm --privileged --cpuset-cpus="$cores" --memory=64g --memory-swap=64g \
		-v /sys/fs/cgroup:/sys/fs/cgroup:rw \
		-v /data3/sujie:/data3/sujie -v /data3/sujie:/workspace -w /workspace \
		genmc15noble:sujie bash -lc \
		"export PYTHONPATH=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration/endpoint-pipeline
		export GENMC_BENCHMARK_ROOT=/workspace/svcomp2026-caat/sv-benchmarks
		export GENMC_COMPAT_HEADER=/workspace/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
		export GENMC_MODEL_ROOT='$source_root/models/cat'
		export GENMC_BINARY='$wrapper'
		export GENMC_REAL_BINARY='$binary'
		export GENMC_EXPERIMENT_MODE=primitive-cache-fast-compose-cycle
		export GENMC_REWRITE_ROOT=/workspace/experiments/caat-optimization/$(basename "$output_root")-$lane-rewrite
		benchexec --no-container --numOfThreads 4 --allowedCores '$cores' \
		--outputpath '$output_root/$lane/' '$definition'" \
		>"$output_root/$lane/console.log" 2>&1
}

run_lane baseline "$baseline_binary" 0-3 & baseline_pid=$!
run_lane candidate "$candidate_binary" 28-31 & candidate_pid=$!
baseline_status=0
candidate_status=0
wait "$baseline_pid" || baseline_status=$?
wait "$candidate_pid" || candidate_status=$?
printf 'lane\tstatus\nbaseline\t%s\ncandidate\t%s\n' "$baseline_status" "$candidate_status" \
	>"$output_root/manifest.tsv"
((baseline_status == 0 && candidate_status == 0)) || exit 1

for lane in baseline candidate; do
	mapfile -t archives < <(find "$output_root/$lane" -maxdepth 1 -name '*.logfiles.zip')
	mapfile -t result_xmls < <(find "$output_root/$lane" -maxdepth 1 -name '*.xml.bz2')
	[[ ${#archives[@]} -eq 1 && ${#result_xmls[@]} -eq 1 ]] || exit 3
	[[ "$(bzcat "${result_xmls[0]}" | grep -c '<run ')" -eq 4 ]] || exit 3
	[[ "$(unzip -Z1 "${archives[0]}" | grep -c .)" -eq 4 ]] || exit 3
done
date -Iseconds >"$output_root/complete.txt"
