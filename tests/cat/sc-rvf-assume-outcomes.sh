#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
program_root="$3"

check_outcome_pair()
{
	local prefix="$1" require_merge="$2" require_rejected="$3"
	local program_name
	for outcome in 0 1; do
		program_name="${prefix}-${outcome}.c"
		run_named_case "${program_name}" baseline 1
		baseline_status=${exit_code}
		baseline_semantic=${semantic}
		baseline_complete=${complete}
		rvf_complete=""
		for workers in 1 2; do
			run_named_case "${program_name}" rvf "${workers}"
			[[ ${exit_code} -eq ${baseline_status} && "${semantic}" == "${baseline_semantic}" ]] || {
				echo "${program_name}: baseline/RVF semantic mismatch" >&2
				exit 1
			}
			grep -Fq 'SC RVF program gate: enabled' <<<"${output}"
			stats="$(grep -F 'Exploration statistics:' <<<"${output}" | tail -1)"
			grep -Eq 'rvf-loads-reduced=[1-9][0-9]*' <<<"${stats}"
			grep -Fq 'rvf-fail-open=0' <<<"${stats}"
			if [[ "${require_rejected}" == yes ]]; then
				grep -Eq 'rvf-annotated-groups-rejected=[1-9][0-9]*' <<<"${stats}"
			fi
			if [[ -z "${rvf_complete}" ]]; then
				rvf_complete=${complete}
			else
				[[ "${complete}" == "${rvf_complete}" ]] || {
					echo "outcome ${outcome}: worker complete-count mismatch" >&2
					exit 1
				}
			fi
		done
		if [[ ${outcome} -eq 0 && "${require_merge}" == yes ]]; then
			[[ ${rvf_complete} -lt ${baseline_complete} ]] || {
				echo "${program_name}: fixture did not merge same-value executions" >&2
				exit 1
			}
		fi
	done
}

run_named_case()
{
	local program_name="$1" mode="$2" workers="$3"
	local extra=""
	if [[ "${mode}" == rvf ]]; then
		extra="--sc-rvf-exploration --sc-rvf-annotated-reads --nthreads=${workers}"
	fi
	set +e
	output="$(timeout 30s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats ${extra} "${program_root}/${program_name}" 2>&1)"
	exit_code=$?
	set -e
	[[ ${exit_code} -ne 124 ]] || { echo "${mode}/n${workers}/${program_name}: timeout" >&2; exit 1; }
	[[ ${exit_code} -eq 0 || ${exit_code} -eq 42 ]] || {
		echo "${mode}/n${workers}/${program_name}: unexpected status ${exit_code}" >&2
		echo "${output}" >&2
		exit 1
	}
	semantic="$(grep -E '^(Error:|Warning:|No errors were detected\.)' <<<"${output}" | sort -u)"
	complete="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\).*/\1/p' \
		<<<"${output}")"
}

check_single()
{
	local program_name="$1" require_rejected="$2"
	run_named_case "${program_name}" baseline 1
	baseline_status=${exit_code}
	baseline_semantic=${semantic}
	for workers in 1 2; do
		run_named_case "${program_name}" rvf "${workers}"
		[[ ${exit_code} -eq ${baseline_status} && "${semantic}" == "${baseline_semantic}" ]] || {
			echo "${program_name}: baseline/RVF semantic mismatch" >&2
			exit 1
		}
		grep -Fq 'SC RVF program gate: enabled' <<<"${output}"
		stats="$(grep -F 'Exploration statistics:' <<<"${output}" | tail -1)"
		grep -Fq 'rvf-fail-open=0' <<<"${stats}"
		if [[ "${require_rejected}" == yes ]]; then
			grep -Eq 'rvf-annotated-groups-rejected=[1-9][0-9]*' <<<"${stats}"
		fi
	done
}

check_native_fallback()
{
	local program_name="$1"
	run_named_case "${program_name}" baseline 1
	baseline_status=${exit_code}
	baseline_semantic=${semantic}
	baseline_complete=${complete}
	run_named_case "${program_name}" rvf 1
	[[ ${exit_code} -eq ${baseline_status} && "${semantic}" == "${baseline_semantic}" &&
	   "${complete}" == "${baseline_complete}" ]] || {
		echo "${program_name}: native fallback mismatch" >&2
		exit 1
	}
	grep -Fq 'SC RVF program gate: native-fallback reason=assume is not covered by exactly one supported annotated plain load' \
		<<<"${output}"
	stats="$(grep -F 'Exploration statistics:' <<<"${output}" | tail -1)"
	grep -Fq 'rvf-loads-attempted=0' <<<"${stats}"
	grep -Fq 'rvf-loads-reduced=0' <<<"${stats}"
}

check_outcome_pair rvf-assume-outcome yes no
check_outcome_pair rvf-assume-future-outcome no yes
check_outcome_pair rvf-assume-double-outcome no no
check_outcome_pair rvf-assume-mutex-outcome no no
check_single rvf-assume-error-failing.c yes
check_single rvf-assume-error-succeeding.c no
check_single rvf-assume-wwrace.c no
check_native_fallback rvf-assume-unsupported-multisource.c

echo "SC-RVF annotated-read outcome/error matrix passed"
