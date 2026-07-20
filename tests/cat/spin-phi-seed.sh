#!/bin/bash
set -euo pipefail

genmc="$1"
model="$2"
programs="$3"

positive_output="$(timeout 20s "${genmc}" --model-file="${model}" \
	--disable-estimation --cat-stats --nthreads=1 \
	"${programs}/spin-phi-preheader-seed.c" 2>&1)"
grep -Fq 'No errors were detected.' <<<"${positive_output}"
grep -Fq 'Number of complete executions explored: 0' <<<"${positive_output}"
grep -Eq 'Number of blocked executions seen: [1-9][0-9]*' <<<"${positive_output}"

set +e
negative_output="$(timeout 20s "${genmc}" --model-file="${model}" \
	--disable-estimation --cat-stats --nthreads=1 \
	"${programs}/spin-phi-backedge-constant.c" 2>&1)"
negative_status=$?
set -e
[[ ${negative_status} -eq 42 ]] || {
	echo "backedge-constant oracle exited ${negative_status}, expected assertion status 42" >&2
	echo "${negative_output}" >&2
	exit 1
}
grep -Fq 'Error: Safety violation!' <<<"${negative_output}"

set +e
control_output="$(timeout 20s "${genmc}" --model-file="${model}" \
	--disable-estimation --cat-stats --nthreads=1 \
	"${programs}/spin-phi-dynamic-seed-controls-exit.c" 2>&1)"
control_status=$?
set -e
[[ ${control_status} -eq 42 ]] || {
	echo "preheader-control oracle exited ${control_status}, expected assertion status 42" >&2
	echo "${control_output}" >&2
	exit 1
}
grep -Fq 'Error: Safety violation!' <<<"${control_output}"
