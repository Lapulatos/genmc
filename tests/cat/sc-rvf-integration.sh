#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
program="$3"

run_case()
{
	local workers="$1"
	shift
	set +e
	output="$(timeout 15s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" "$@" "${program}" 2>&1)"
	status=$?
	set -e
	[[ ${status} -eq 0 ]] || {
		echo "SC RVF integration run failed with status ${status}" >&2
		echo "${output}" >&2
		exit 1
	}
	verdict="$(grep -E '^(Error:|Warning:|No errors were detected\.)' \
		<<<"${output}" | sort -u)"
	complete="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\).*/\1/p' \
		<<<"${output}")"
}

run_case 1
baseline_verdict="${verdict}"
baseline_complete="${complete}"
rvf_complete=""
for workers in 1 2; do
	run_case "${workers}" --sc-rvf-exploration
	[[ "${verdict}" == "${baseline_verdict}" ]] || {
		echo "SC RVF semantic mismatch with ${workers} worker(s)" >&2
		exit 1
	}
	if [[ -z "${rvf_complete}" ]]; then
		rvf_complete="${complete}"
		[[ ${rvf_complete} -lt ${baseline_complete} ]] || {
			echo "SC RVF fixture did not reduce complete executions" >&2
			exit 1
		}
	else
		[[ "${complete}" == "${rvf_complete}" ]] || {
			echo "SC RVF worker-count mismatch" >&2
			exit 1
		}
	fi
	stats="$(grep -F 'Exploration statistics:' <<<"${output}")"
	grep -Eq 'rvf-loads-reduced=[1-9][0-9]*' <<<"${stats}"
	grep -Fq 'rvf-fail-open=0' <<<"${stats}"
done
