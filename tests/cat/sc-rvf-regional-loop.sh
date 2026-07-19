#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
program_root="$3"

run_case()
{
	local expected="$1" mode="$2" workers="$3"
	local source_file="${program_root}/rvf-regional-loop-outcome-${expected}.c"
	local -a extra=()
	if [[ "${mode}" == regional ]]; then
		extra=(--sc-rvf-exploration --sc-rvf-regional)
	fi
	set +e
	output="$(timeout 30s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" "${extra[@]}" "${source_file}" 2>&1)"
	exit_code=$?
	set -e
	[[ ${exit_code} -ne 124 ]]
	[[ ${exit_code} -eq 0 || ${exit_code} -eq 42 ]]
	semantic="$(grep -E '^(Error:|Warning:|No errors were detected\.)' <<<"${output}" | sort -u)"
}

run_safe_case()
{
	local mode="$1" workers="$2"
	local -a extra=()
	if [[ "${mode}" == regional ]]; then
		extra=(--sc-rvf-exploration --sc-rvf-regional)
	fi
	output="$(timeout 30s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" "${extra[@]}" \
		"${program_root}/rvf-regional-loop-safe.c" 2>&1)"
	semantic="$(grep -E '^(Error:|Warning:|No errors were detected\.)' <<<"${output}" | sort -u)"
	complete="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\).*/\1/p' \
		<<<"${output}")"
}

for expected in 0 1; do
	run_case "${expected}" native 1
	native_status=${exit_code}
	native_semantic=${semantic}
	for workers in 1 2; do
		run_case "${expected}" regional "${workers}"
		[[ ${exit_code} -eq ${native_status} && "${semantic}" == "${native_semantic}" ]] || {
			echo "loop outcome ${expected}: native/regional semantic mismatch" >&2
			echo "${output}" >&2
			exit 1
		}
		grep -Fq 'SC RVF program gate: regional' <<<"${output}"
		stats="$(grep -F 'Exploration statistics:' <<<"${output}" | tail -1)"
		grep -Fq 'rvf-fail-open=0' <<<"${stats}"
	done
done

run_safe_case native 1
native_semantic=${semantic}
native_complete=${complete}
regional_complete=""
for workers in 1 2; do
	run_safe_case regional "${workers}"
	[[ "${semantic}" == "${native_semantic}" ]]
	stats="$(grep -F 'Exploration statistics:' <<<"${output}" | tail -1)"
	grep -Eq 'rvf-loads-reduced=[1-9][0-9]*' <<<"${stats}"
	grep -Fq 'rvf-fail-open=0' <<<"${stats}"
	if [[ -z "${regional_complete}" ]]; then
		regional_complete=${complete}
	else
		[[ "${complete}" == "${regional_complete}" ]]
	fi
done
[[ ${regional_complete} -lt ${native_complete} ]]

echo "SC-RVF bounded-loop outcome oracle passed"
