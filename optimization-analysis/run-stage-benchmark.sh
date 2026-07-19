#!/bin/bash

set -euo pipefail

if [[ "$#" -lt 3 || "$#" -gt 5 ]]; then
	echo "usage: $0 <stage> <genmc-binary> <result.tsv> [repetitions=5] [warmups=1]" >&2
	exit 2
fi

stage="$1"
binary="$2"
result="$3"
repetitions="${4:-5}"
warmups="${5:-1}"
root="$(cd "$(dirname "$0")/.." && pwd)"
programs=(
	"correct/litmus/SB/variants/sb0.c"
	"correct/litmus/RMWFix/variants/rmwfix0.c"
	"correct/data-structures/ms-queue-dynamic/variants/main0.c"
	"correct/data-structures/treiber-stack-dynamic/variants/main0.c"
	"correct/data-structures/fcombiner-async/variants/main0.c"
)
models=(sc tso pso)
backends=(cat caat)

[[ -x "${binary}" ]] || { echo "missing executable: ${binary}" >&2; exit 2; }
mkdir -p "$(dirname "${result}")"
work="$(mktemp -d "${TMPDIR:-/tmp}/genmc-opt-stage.XXXXXX")"
trap 'rm -rf "${work}"' EXIT

if [[ ! -s "${result}" ]]; then
	printf 'stage\tbackend\tmodel\tprogram\trepetition\tstatus\treal_seconds\tuser_seconds\tsys_seconds\tpeak_rss_bytes\tcomplete_executions\tblocked_executions\n' >"${result}"
fi

run_one() {
	local backend="$1" model="$2" relative="$3" repetition="$4" record="$5"
	local model_file="${root}/models/cat/${model}.cat"
	[[ "${backend}" == caat ]] && model_file="${root}/models/cat/recursive-${model}.cat"
	local stdout="${work}/stdout" measurement="${work}/measurement"
	set +e
	/usr/bin/time -lp "${binary}" --model-file="${model_file}" \
		--disable-estimation --disable-mm-detector --nthreads=1 \
		"${root}/tests/${relative}" >"${stdout}" 2>"${measurement}"
	local status=$?
	set -e
	if [[ "${status}" -ne 0 && "${status}" -ne 42 ]]; then
		echo "failed: ${stage}/${backend}/${model}/${relative}, status=${status}" >&2
		sed -n '1,120p' "${measurement}" >&2
		return 1
	fi
	[[ "${record}" == yes ]] || return 0
	local real user sys rss executions blocked
	real="$(awk '$1 == "real" {print $2}' "${measurement}")"
	user="$(awk '$1 == "user" {print $2}' "${measurement}")"
	sys="$(awk '$1 == "sys" {print $2}' "${measurement}")"
	rss="$(awk '$2 == "maximum" && $3 == "resident" {print $1}' "${measurement}")"
	executions="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\)$/\1/p' "${stdout}")"
	blocked="$(sed -n 's/^Number of blocked executions seen: \([0-9][0-9]*\)$/\1/p' "${stdout}")"
	[[ -n "${real}" && -n "${rss}" ]] || { echo "incomplete time output" >&2; return 1; }
	[[ -n "${executions}" ]] || executions=0
	[[ -n "${blocked}" ]] || blocked=0
	printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
		"${stage}" "${backend}" "${model}" "${relative}" "${repetition}" \
		"${status}" "${real}" "${user}" "${sys}" "${rss}" "${executions}" "${blocked}" >>"${result}"
}

for ((warmup = 1; warmup <= warmups; warmup++)); do
	for model in "${models[@]}"; do
		for relative in "${programs[@]}"; do
			for backend in "${backends[@]}"; do
				run_one "${backend}" "${model}" "${relative}" 0 no
			done
		done
	done
done

for ((repetition = 1; repetition <= repetitions; repetition++)); do
	for model in "${models[@]}"; do
		for relative in "${programs[@]}"; do
			# Alternate CAT/CAAT order between repetitions.
			if (( repetition % 2 == 1 )); then order=(cat caat); else order=(caat cat); fi
			for backend in "${order[@]}"; do
				run_one "${backend}" "${model}" "${relative}" "${repetition}" yes
			done
		done
	done
	echo "${stage}: completed repetition ${repetition}/${repetitions}" >&2
done

