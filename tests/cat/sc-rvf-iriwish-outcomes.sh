#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
template="$3"
feature_define="${4:-}"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/genmc-rvf-iriwish.XXXXXX")"
trap 'rm -rf "${tmpdir}"' EXIT

run_case()
{
	local outcome="$1"
	local mode="$2"
	local workers="$3"
	local extra=""
	[[ "${mode}" == rvf ]] && extra="--sc-rvf-exploration"
	set +e
	output="$(timeout 15s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" ${extra} \
		"${tmpdir}/outcome-${outcome}.c" 2>&1)"
	exit_code=$?
	set -e
	[[ ${exit_code} -ne 124 ]] || {
		echo "outcome ${outcome}, ${mode}/n${workers}: timed out" >&2
		exit 1
	}
	[[ ${exit_code} -eq 0 || ${exit_code} -eq 42 ]] || {
		echo "outcome ${outcome}, ${mode}/n${workers}: unexpected status ${exit_code}" >&2
		echo "${output}" >&2
		exit 1
	}
	if [[ "${mode}" == rvf ]]; then
		grep -Fq 'SC RVF program gate: enabled' <<<"${output}" || {
			echo "outcome ${outcome}, ${mode}/n${workers}: gate not enabled" >&2
			echo "${output}" >&2
			exit 1
		}
		grep -Fq 'rvf-fail-open=0' <<<"${output}" || {
			echo "outcome ${outcome}, ${mode}/n${workers}: RVF fail-open" >&2
			echo "${output}" >&2
			exit 1
		}
	fi
}

reachable=0
reachable_outcomes=""
total_reduced=0
for outcome in $(seq 0 31); do
	{
		printf '#define RVF_IRIWISH_OUTCOME %s\n' "${outcome}"
		[[ -z "${feature_define}" ]] || printf '#define %s 1\n' "${feature_define}"
		printf '#include "%s"\n' "${template}"
	} >"${tmpdir}/outcome-${outcome}.c"
	run_case "${outcome}" baseline 1
	baseline_status=${exit_code}
	if [[ ${baseline_status} -eq 42 ]]; then
		reachable=$((reachable + 1))
		reachable_outcomes="${reachable_outcomes}${reachable_outcomes:+,}${outcome}"
	fi
	for workers in 1 2; do
		run_case "${outcome}" rvf "${workers}"
		[[ ${exit_code} -eq ${baseline_status} ]] || {
			echo "outcome ${outcome}: baseline=${baseline_status}, rvf/n${workers}=${exit_code}" >&2
			exit 1
		}
		if [[ -n "${feature_define}" ]]; then
			reduced="$(sed -n 's/.*rvf-loads-reduced=\([0-9][0-9]*\).*/\1/p' <<<"${output}")"
			[[ -n "${reduced}" ]]
			total_reduced=$((total_reduced + reduced))
		fi
	done
done

printf 'IRIWish reachable outcomes: %s/32 [%s]\n' "${reachable}" "${reachable_outcomes}"
if [[ -n "${feature_define}" ]]; then
	((total_reduced > 0)) || { echo 'feature fixture did not exercise RVF reduction' >&2; exit 1; }
	printf 'IRIWish feature=%s aggregate reduced-loads=%s\n' "${feature_define}" "${total_reduced}"
fi
