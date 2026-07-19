#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
template="$3"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/genmc-rvf-mutex.XXXXXX")"
trap 'rm -rf "${tmpdir}"' EXIT

run_case()
{
	local program="$1" mode="$2" workers="$3" extra=""
	[[ "${mode}" == rvf ]] && extra="--sc-rvf-exploration"
	[[ "${mode}" == control ]] && extra="--sc-rvf-exploration --sc-rvf-disable-quotient"
	set +e
	output="$(timeout 30s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" ${extra} "${program}" 2>&1)"
	exit_code=$?
	set -e
	[[ ${exit_code} -ne 124 ]] || { echo "${mode}/n${workers}: timed out" >&2; exit 1; }
	[[ ${exit_code} -eq 0 || ${exit_code} -eq 42 ]] || {
		echo "${mode}/n${workers}: unexpected status ${exit_code}" >&2
		echo "${output}" >&2
		exit 1
	}
	if [[ "${mode}" == rvf || "${mode}" == control ]]; then
		grep -Fq 'SC RVF program gate: enabled' <<<"${output}" || {
			echo "${mode}/n${workers}: gate not enabled" >&2; echo "${output}" >&2; exit 1;
		}
		grep -Fq 'rvf-fail-open=0' <<<"${output}" || {
			echo "${mode}/n${workers}: RVF fail-open" >&2; echo "${output}" >&2; exit 1;
		}
	fi
}

for outcome in 0 1 2; do
	program="${tmpdir}/mutex-${outcome}.c"
	{
		printf '#define RVF_MUTEX_OUTCOME %s\n' "${outcome}"
		printf '#include "%s"\n' "${template}"
	} >"${program}"
	run_case "${program}" baseline 1
	baseline_status=${exit_code}
	run_case "${program}" control 1
	control_status=${exit_code}
	[[ ${control_status} -eq ${baseline_status} ]] || {
		echo "mutex/${outcome}: baseline=${baseline_status}, control=${control_status}" >&2; exit 1;
	}
	control_counts="$(grep -E '^Number of (complete|blocked) executions' <<<"${output}")"
	for workers in 1 2; do
		run_case "${program}" rvf "${workers}"
		[[ ${exit_code} -eq ${baseline_status} ]] || {
			echo "mutex/${outcome}: baseline=${baseline_status}, rvf/n${workers}=${exit_code}" >&2
			exit 1
		}
		reduced="$(sed -n 's/.*rvf-loads-reduced=\([0-9][0-9]*\).*/\1/p' <<<"${output}")"
		[[ -n "${reduced}" ]]
		counts="$(grep -E '^Number of (complete|blocked) executions' <<<"${output}")"
		[[ ${baseline_status} -ne 0 || "${counts}" == "${control_counts}" ]] || {
			echo "mutex/${outcome}/n${workers}: native lock execution counts changed" >&2
			diff -u <(printf '%s\n' "${control_counts}") <(printf '%s\n' "${counts}") >&2 || true
			exit 1
		}
		printf 'mutex outcome=%s workers=%s status=%s reduced-loads=%s\n' \
			"${outcome}" "${workers}" "${exit_code}" "${reduced}"
	done
done
