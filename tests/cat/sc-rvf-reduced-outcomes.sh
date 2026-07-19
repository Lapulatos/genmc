#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
program_root="$3"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/genmc-rvf-reduced.XXXXXX")"
trap 'rm -rf "${tmpdir}"' EXIT

run_case()
{
	local program="$1" mode="$2" workers="$3" extra=""
	[[ "${mode}" == rvf ]] && extra="--sc-rvf-exploration"
	set +e
	output="$(timeout 15s "${genmc}" --model-file="${model}" --disable-estimation \
		--cat-stats --nthreads="${workers}" ${extra} "${program}" 2>&1)"
	exit_code=$?
	set -e
	[[ ${exit_code} -ne 124 ]] || { echo "${mode}/n${workers}: timed out" >&2; exit 1; }
	[[ ${exit_code} -eq 0 || ${exit_code} -eq 42 ]] || {
		echo "${mode}/n${workers}: unexpected status ${exit_code}" >&2
		echo "${output}" >&2
		exit 1
	}
	if [[ "${mode}" == rvf ]]; then
		grep -Fq 'SC RVF program gate: enabled' <<<"${output}"
		grep -Fq 'rvf-fail-open=0' <<<"${output}"
	fi
}

for spec in wrc-dep:7 mp-rels-acq:5 s-rels-acq:5 cumul-release:7 rel-b-cumul-acq:15; do
	name="${spec%%:*}"
	maximum="${spec##*:}"
	template="${program_root}/rvf-${name}-outcome.inc"
	reachable=""
	for outcome in $(seq 0 "${maximum}"); do
		program="${tmpdir}/${name}-${outcome}.c"
		{
			printf '#define RVF_OUTCOME %s\n' "${outcome}"
			printf '#include "%s"\n' "${template}"
		} >"${program}"
		run_case "${program}" baseline 1
		baseline_status=${exit_code}
		[[ ${baseline_status} -eq 42 ]] && reachable="${reachable}${reachable:+,}${outcome}"
		for workers in 1 2; do
			run_case "${program}" rvf "${workers}"
			[[ ${exit_code} -eq ${baseline_status} ]] || {
				echo "${name}/${outcome}: baseline=${baseline_status}, rvf/n${workers}=${exit_code}" >&2
				exit 1
			}
		done
	done
	printf '%s reachable outcomes: [%s]\n' "${name}" "${reachable}"
done
