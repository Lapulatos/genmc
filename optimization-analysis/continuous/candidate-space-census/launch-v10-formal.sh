#!/bin/bash

set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
binary="$root/build-gcc13-release/bin/genmc"
v9_wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/generic-preventive-wrapper.sh"
v10_wrapper="$source_root/optimization-analysis/continuous/candidate-space-census/v10-genmc-wrapper.sh"
definition=/data3/sujie/experiments/caat-optimization/generic-preventive/definitions/census-caat-pso.xml
out="${1:?pass a new V10 formal-result directory}"

[[ "$out" == "$root/formal-results/v10-"* ]] || {
	echo "output must be a v10-* directory below $root/formal-results" >&2
	exit 2
}
[[ ! -e "$out" ]] || { echo "refusing to reuse existing output: $out" >&2; exit 2; }
[[ -x "$binary" && -x "$v9_wrapper" && -x "$v10_wrapper" && -f "$definition" ]] || {
	echo "binary, wrappers, or definition is missing" >&2
	exit 2
}
"$binary" --help 2>&1 | grep -q -- '--cat-conflict-cores' || {
	echo "binary does not contain V10 conflict cores" >&2
	exit 2
}
pgrep -af '[b]enchexec|build-gcc13-release/bin/[g]enmc' && {
	echo "another GenMC/BenchExec process is already running" >&2
	exit 2
}
available_kib=$(df -Pk /data3/sujie | awk 'NR==2 {print $4}')
((available_kib >= 100 * 1024 * 1024)) || {
	echo "less than 100 GiB is available on /data3/sujie" >&2
	exit 2
}

export PYTHONPATH=/data3/sujie/svcomp2026-caat
export GENMC_EXPERIMENT_ROOT=/data3/sujie/svcomp2026-caat

mkdir -p "$out"
printf 'repetition\tv9_cores\tv10_cores\tv9_status\tv10_status\tfinished\n' >"$out/manifest.tsv"
sha256sum "$binary" "$v9_wrapper" "$v10_wrapper" "$definition" >"$out/input-sha256.txt"
for repetition in 01 02 03 04; do
	pair="$out/r$repetition"
	mkdir -p "$pair/v9" "$pair/v10"
	if [[ "$repetition" == 02 || "$repetition" == 04 ]]; then
		v9_cores=28-51
		v10_cores=0-23
	else
		v9_cores=0-23
		v10_cores=28-51
	fi
	env GENMC_BINARY="$v9_wrapper" GENMC_REAL_BINARY="$binary" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$v9_cores" \
		--outputpath "$pair/v9/" "$definition" >"$pair/v9.console.log" 2>&1 &
	v9_pid=$!
	env GENMC_BINARY="$v10_wrapper" GENMC_REAL_BINARY="$binary" \
		benchexec --no-container --numOfThreads 24 --allowedCores "$v10_cores" \
		--outputpath "$pair/v10/" "$definition" >"$pair/v10.console.log" 2>&1 &
	v10_pid=$!
	v9_status=0
	v10_status=0
	wait "$v9_pid" || v9_status=$?
	wait "$v10_pid" || v10_status=$?
	printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$repetition" "$v9_cores" "$v10_cores" \
		"$v9_status" "$v10_status" "$(date -Iseconds)" >>"$out/manifest.tsv"
	((v9_status == 0 && v10_status == 0)) || exit 1
done
date -Iseconds >"$out/complete.txt"
