#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
program="$3"

run_case()
{
	local mode="$1"
	local extra=""
	[[ "${mode}" == rvf ]] && extra="--sc-rvf-exploration"
	set +e
	output="$(timeout 60s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads=1 ${extra} "${program}" 2>&1)"
	exit_code=$?
	set -e
	[[ ${exit_code} -ne 124 ]] || {
		echo "${mode}: timed out" >&2
		exit 1
	}
	semantic_summary="$(grep -E \
		'^(Error:|Warning:|No errors were detected\.|Number of complete executions explored:|Number of blocked executions:|Exploration statistics:)' \
		<<<"${output}")"
}

run_case baseline
baseline_exit_code=${exit_code}
baseline_summary=${semantic_summary}

run_case rvf
[[ ${exit_code} -eq ${baseline_exit_code} ]] || {
	echo "fallback exit status differs: baseline=${baseline_exit_code}, rvf=${exit_code}" >&2
	exit 1
}
grep -Fq 'SC RVF program gate: native-fallback reason=control-flow loop outside the current RVF performance gate' \
	<<<"${output}"
[[ "${semantic_summary}" == "${baseline_summary}" ]] || {
	echo "fallback changed the native one-worker result" >&2
	diff -u <(printf '%s\n' "${baseline_summary}") <(printf '%s\n' "${semantic_summary}") >&2 || true
	exit 1
}
if grep -E 'rvf-(loads-attempted|loads-reduced|verify-calls|representatives-queued)=[1-9][0-9]*' \
	<<<"${semantic_summary}"; then
	echo "fallback executed RVF-specific exploration work" >&2
	exit 1
fi
