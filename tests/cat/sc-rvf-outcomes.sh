#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
program_root="$3"

for outcome in 00 01 10 11; do
	expected=42
	[[ "${outcome}" == 00 ]] && expected=0
	for spec in baseline:1 rvf:1 rvf:2; do
		mode="${spec%%:*}"
		workers="${spec##*:}"
		extra=""
		[[ "${mode}" == rvf ]] && extra="--sc-rvf-exploration"
		set +e
		output="$(timeout 15s "${genmc}" --model-file="${model}" --disable-estimation \
			--cat-stats --nthreads="${workers}" ${extra} \
			"${program_root}/rvf-sb-outcome-${outcome}.c" 2>&1)"
		status=$?
		set -e
		[[ ${status} -eq ${expected} ]] || {
			echo "outcome ${outcome}, ${mode}/n${workers}: expected ${expected}, got ${status}" >&2
			echo "${output}" >&2
			exit 1
		}
		if [[ "${mode}" == rvf ]]; then
			grep -Fq 'SC RVF program gate: enabled' <<<"${output}"
			grep -Fq 'rvf-fail-open=0' <<<"${output}"
		fi
	done
done
