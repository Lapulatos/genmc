#!/bin/bash

# GenMC -- Generic Model Checking.
#
# This project is dual-licensed under the Apache License 2.0 and the MIT License.
# You may choose to use, distribute, or modify this software under either license.

set -euo pipefail

if [[ "$#" -ne 5 ]]; then
	echo "usage: $0 <phase2-genmc> <phase3-genmc> <models-dir> <tests-dir> <result.tsv>" >&2
	exit 2
fi

phase2="$1"
phase3="$2"
models="$3"
tests="$4"
result="$5"

programs=(
	"correct/litmus/SB/variants/sb0.c"
	"correct/data-structures/treiber-stack-dynamic/variants/main0.c"
	"correct/data-structures/fcombiner-async/variants/main0.c"
)

mkdir -p "$(dirname "${result}")"
work="$(mktemp -d "${TMPDIR:-/tmp}/genmc-caat-performance.XXXXXX")"
trap 'rm -rf "${work}"' EXIT
printf 'backend\tmodel\tprogram\trepetition\tstatus\treal_seconds\tpeak_rss_bytes\tcomplete_executions\n' \
	>"${result}"

# Measure the complete command, including compilation/transformation, because
# this is the latency users observe. Three repetitions expose run-to-run noise.
for backend in phase2-offline phase3-online; do
	if [[ "${backend}" == "phase2-offline" ]]; then
		binary="${phase2}"
	else
		binary="${phase3}"
	fi
	for model in sc tso pso; do
		for relative in "${programs[@]}"; do
			for repetition in 1 2 3; do
				stdout="${work}/stdout"
				measurement="${work}/measurement"
				set +e
				/usr/bin/time -lp "${binary}" \
					--model-file="${models}/recursive-${model}.cat" \
					--disable-estimation --disable-mm-detector --nthreads=1 \
					"${tests}/${relative}" >"${stdout}" 2>"${measurement}"
				status=$?
				set -e
				if [[ "${status}" -ne 0 && "${status}" -ne 42 ]]; then
					echo "benchmark failed: ${backend}/${model}/${relative}" >&2
					cat "${measurement}" >&2
					exit 1
				fi
				real="$(awk '$1 == "real" {print $2}' "${measurement}")"
				rss="$(awk '$2 == "maximum" && $3 == "resident" {print $1}' \
					"${measurement}")"
				executions="$(sed -n \
					's/^Number of complete executions explored: \([0-9][0-9]*\)$/\1/p' \
					"${stdout}")"
				[[ -n "${real}" && -n "${rss}" ]] || {
					echo "time output was incomplete" >&2
					exit 1
				}
				[[ -n "${executions}" ]] || executions=0
				printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
					"${backend}" "${model}" "${relative}" "${repetition}" \
					"${status}" "${real}" "${rss}" "${executions}" >>"${result}"
			done
		done
	done
done

awk -F '\t' 'NR > 1 {
	count[$1]++; time[$1] += $6; if ($7 > rss[$1]) rss[$1] = $7
} END {
	for (backend in count)
		printf "%s runs=%d mean-real=%.4f peak-rss=%d\n", backend, count[backend],
		       time[backend] / count[backend], rss[backend]
}' "${result}" | LC_ALL=C sort
