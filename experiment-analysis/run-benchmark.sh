#!/bin/bash

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
binary="${BINARY:-${root}/RelWithDebInfo/bin/genmc}"
result="${RESULT:-${root}/experiment-analysis/raw-results.tsv}"
repetitions="${REPETITIONS:-10}"
warmups="${WARMUPS:-1}"

programs=(
	"correct/litmus/SB/variants/sb0.c"
	"correct/litmus/RMWFix/variants/rmwfix0.c"
	"correct/data-structures/ms-queue-dynamic/variants/main0.c"
	"correct/data-structures/treiber-stack-dynamic/variants/main0.c"
	"correct/data-structures/fcombiner-async/variants/main0.c"
)
read -r -a models <<<"${MODELS:-sc tso}"
read -r -a backends <<<"${BACKENDS:-genmc cat caat}"

[[ -x "${binary}" ]] || { echo "missing executable: ${binary}" >&2; exit 2; }
mkdir -p "$(dirname "${result}")"
work="$(mktemp -d "${TMPDIR:-/tmp}/genmc-three-way.XXXXXX")"
trap 'rm -rf "${work}"' EXIT

args_for() {
	local backend="$1" model="$2"
	case "${backend}" in
		genmc) printf '%s\n' "--${model}" ;;
		cat) printf '%s\n' "--model-file=${root}/models/cat/${model}.cat" ;;
		caat) printf '%s\n' "--model-file=${root}/models/cat/recursive-${model}.cat" ;;
		*) echo "unknown backend: ${backend}" >&2; return 2 ;;
	esac
}

run_one() {
	local backend="$1" model="$2" relative="$3" repetition="$4" record="$5"
	local stdout="${work}/stdout" measurement="${work}/measurement"
	local backend_arg
	backend_arg="$(args_for "${backend}" "${model}")"
	set +e
	/usr/bin/time -lp "${binary}" "${backend_arg}" \
		--disable-estimation --disable-mm-detector --nthreads=1 \
		"${root}/tests/${relative}" >"${stdout}" 2>"${measurement}"
	local status=$?
	set -e
	if [[ "${status}" -ne 0 && "${status}" -ne 42 ]]; then
		echo "failed: ${backend}/${model}/${relative}, status=${status}" >&2
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
	printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
		"${backend}" "${model}" "${relative}" "${repetition}" "${status}" \
		"${real}" "${user}" "${sys}" "${rss}" "${executions}" "${blocked}" >>"${result}"
}

printf 'backend\tmodel\tprogram\trepetition\tstatus\treal_seconds\tuser_seconds\tsys_seconds\tpeak_rss_bytes\tcomplete_executions\tblocked_executions\n' >"${result}"

for ((warmup = 1; warmup <= warmups; warmup++)); do
	for model in "${models[@]}"; do
		for relative in "${programs[@]}"; do
			for backend in "${backends[@]}"; do
				run_one "${backend}" "${model}" "${relative}" 0 no
			done
		done
	done
done

# Rotate the backend order by repetition to reduce systematic thermal/order bias.
for ((repetition = 1; repetition <= repetitions; repetition++)); do
	shift_by=$(( (repetition - 1) % ${#backends[@]} ))
	for model in "${models[@]}"; do
		for relative in "${programs[@]}"; do
			for ((index = 0; index < ${#backends[@]}; index++)); do
				backend="${backends[$(( (index + shift_by) % ${#backends[@]} ))]}"
				run_one "${backend}" "${model}" "${relative}" "${repetition}" yes
			done
		done
	done
	echo "completed repetition ${repetition}/${repetitions}" >&2
done

echo "wrote $((repetitions * ${#models[@]} * ${#programs[@]} * ${#backends[@]})) rows to ${result}" >&2
