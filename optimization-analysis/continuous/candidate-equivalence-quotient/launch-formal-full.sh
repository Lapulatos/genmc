#!/bin/bash
set -euo pipefail

root=/data3/sujie/experiments/caat-optimization/sc-rvf-exploration
source_root="$root/source"
binary="$root/build-gcc13-release/bin/genmc"
wrapper="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh"
definition="$source_root/optimization-analysis/continuous/candidate-equivalence-quotient/definitions/full-caat.xml"
out="${1:?pass a new, unique formal-result directory}"

[[ "$out" == "$root/formal-results/"* ]] || {
	echo "output must be below $root/formal-results" >&2
	exit 2
}
[[ ! -e "$out" ]] || { echo "refusing to reuse existing output: $out" >&2; exit 2; }
[[ -x "$binary" && -x "$wrapper" && -f "$definition" ]] || {
	echo "binary, wrapper, or definition is missing" >&2
	exit 2
}
"$binary" --help-hidden 2>&1 | grep -q -- '--sc-rvf-disable-quotient' || {
	echo "binary does not contain the quotient-disabled control" >&2
	exit 2
}
pgrep -af '[b]enchexec|[g]enmc' && {
	echo "another GenMC/BenchExec process is already running" >&2
	exit 2
}
available_kib=$(df -Pk /data3/sujie | awk 'NR==2 {print $4}')
((available_kib >= 100 * 1024 * 1024)) || {
	echo "less than 100 GiB is available on /data3/sujie" >&2
	exit 2
}

export PYTHONPATH=/data3/sujie/experiments/fair-coverage/pipeline
export GENMC_BENCHMARK_ROOT=/data3/sujie/svcomp2026-caat/sv-benchmarks
export GENMC_REWRITE_ROOT=/data3/sujie/experiments/fair-coverage/rewritten-census
export GENMC_COMPAT_HEADER=/data3/sujie/experiments/fair-coverage/include/svcomp_genmc_compat_v2.h
export GENMC_MODEL_ROOT="$source_root/models/cat"

mkdir -p "$out"
printf 'configuration\tcores\tstatus\tfinished\n' > "$out/manifest.tsv"
sha256sum "$binary" "$wrapper" "$definition" > "$out/input-sha256.txt"
for configuration in baseline control rvf tso pso-v9; do
	result="$out/$configuration"
	mkdir -p "$result"
	status=0
	env GENMC_BINARY="$wrapper" GENMC_REAL_BINARY="$binary" \
		GENMC_EXPERIMENT_MODE="$configuration" \
		benchexec --no-container --numOfThreads 48 --allowedCores 0-23,28-51 \
		--outputpath "$result/" "$definition" > "$result/console.log" 2>&1 || status=$?
	printf '%s\t%s\t%s\t%s\n' "$configuration" 0-23,28-51 "$status" \
		"$(date -Iseconds)" >> "$out/manifest.tsv"
	((status == 0)) || exit "$status"
done
date -Iseconds > "$out/complete.txt"
