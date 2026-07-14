#!/bin/bash

# GenMC -- Generic Model Checking.
#
# This project is dual-licensed under the Apache License 2.0 and the MIT License.
# You may choose to use, distribute, or modify this software under either license.

set -euo pipefail

genmc="$1"
model_root="$2"
source_root="$3"

# Compare the stable semantic summary while allowing expected verification errors.
run_model()
{
	local selected_model="$1"
	local workers="$2"
	local program="$3"
	set +e
	output="$("${genmc}" --model-file="${model_root}/${selected_model}.cat" \
		--disable-estimation --nthreads="${workers}" "${program}" 2>&1)"
	status=$?
	set -e
	summary="$(grep -E '^(Error:|Warning:|No errors were detected\.|Number of complete executions explored:|Number of blocked executions:)' \
		<<<"${output}" | sort -u)"
}

# These fixtures exercise relaxation, RMW, fences, lifecycle, heap storage, and errors.
programs=(
	"correct/litmus/SB/variants/sb0.c"
	"correct/litmus/po-loc/variants/po-loc0.c"
	"correct/litmus/RMWFix/variants/rmwfix0.c"
	"correct/litmus/SB+scfs/variants/sb+scfs0.c"
	"correct/litmus/W+JW/variants/W+JW1.c"
	"correct/infr/atomic-min-max/variants/atomic-min-max0.c"
	"wrong/infr/print-names-array/variants/names-2d-array0.c"
	"cat/programs/WW+RR.c"
)

for model in sc tso pso; do
	for relative in "${programs[@]}"; do
		program="${source_root}/${relative}"
		run_model "${model}" 1 "${program}"
		baseline_status="${status}"
		baseline_summary="${summary}"
		for workers in 1 2; do
			run_model "recursive-${model}" "${workers}" "${program}"
			if [[ "${status}" != "${baseline_status}" ||
			      "${summary}" != "${baseline_summary}" ]]; then
				echo "Recursive ${model} mismatch for ${relative} with ${workers} workers" >&2
				echo "baseline status=${baseline_status}" >&2
				echo "${baseline_summary}" >&2
				echo "recursive status=${status}" >&2
				echo "${summary}" >&2
				exit 1
			fi
		done
	done
done

# The recursive backend feeds its own fixed-point values to the Phase 2 reasoner.
explanation="$("${genmc}" --model-file="${model_root}/recursive-sc.cat" --explain-cat \
	--disable-estimation --nthreads=1 \
	"${source_root}/correct/litmus/SB/variants/sb0.c" 2>&1)"
grep -Fq "CAT explanation:" <<<"${explanation}"
grep -Fq "CAT check 'sc' failed" <<<"${explanation}"
