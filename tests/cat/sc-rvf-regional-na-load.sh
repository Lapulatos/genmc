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
	output="$(timeout 30s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" "$@" "${program}" 2>&1)"
	status=$?
	set -e
	[[ ${status} -ne 124 ]]
	[[ ${status} -eq 0 || ${status} -eq 42 ]]
	semantic="$(grep -E '^(Error:|Warning:|No errors were detected\.)' <<<"${output}" | sort -u)"
	complete="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\).*/\1/p' \
		<<<"${output}")"
}

run_case 1
baseline_status=${status}
baseline_semantic=${semantic}
baseline_complete=${complete}

run_case 1 --sc-rvf-exploration
[[ ${status} -eq ${baseline_status} && "${semantic}" == "${baseline_semantic}" &&
   ${complete} -eq ${baseline_complete} ]]
grep -Fq 'SC RVF program gate: native-fallback' <<<"${output}"

regional_complete=""
for workers in 1 2; do
	run_case "${workers}" --sc-rvf-exploration --sc-rvf-regional
	[[ ${status} -eq ${baseline_status} && "${semantic}" == "${baseline_semantic}" ]]
	grep -Fq 'SC RVF program gate: regional' <<<"${output}"
	stats="$(grep -F 'Exploration statistics:' <<<"${output}" | tail -1)"
	grep -Fq 'rvf-loads-reduced=0' <<<"${stats}"
	grep -Fq 'rvf-fail-open=0' <<<"${stats}"
	if [[ -z "${regional_complete}" ]]; then
		regional_complete=${complete}
	else
		[[ ${complete} -eq ${regional_complete} ]]
	fi
done
[[ ${regional_complete} -eq ${baseline_complete} ]]

echo "SC-RVF regional non-atomic frontier fallback passed"
