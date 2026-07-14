#!/bin/bash

# GenMC -- Generic Model Checking.
#
# This project is dual-licensed under the Apache License 2.0 and the MIT License.
# You may choose to use, distribute, or modify this software under either license.
#
# Apache License 2.0:
#     http://www.apache.org/licenses/LICENSE-2.0
#
# MIT License:
#     https://opensource.org/licenses/MIT

set -euo pipefail

genmc="$1"
model="$2"
source_root="$3"

# Run one mode without allowing an expected verification failure to abort the script.
run_case()
{
	mode="$1"
	program="$2"
	set +e
	if [[ "${mode}" == cat ]]; then
		case_output="$("${genmc}" --model-file="${model}" --disable-estimation \
			"${program}" 2>&1)"
	else
		case_output="$("${genmc}" --sc --disable-estimation --disable-mm-detector \
			"${program}" 2>&1)"
	fi
	case_status=$?
	set -e
}

# Compare status, complete executions, verdict, and the selected error class.
compare_case()
{
	relative_program="$1"
	expected_count="$2"
	expected_marker="$3"
	program="${source_root}/${relative_program}"

	run_case builtin "${program}"
	builtin_output="${case_output}"
	builtin_status="${case_status}"
	run_case cat "${program}"
	cat_output="${case_output}"
	cat_status="${case_status}"

	builtin_count="$(sed -n 's/^Number of complete executions explored: //p' \
		<<<"${builtin_output}")"
	cat_count="$(sed -n 's/^Number of complete executions explored: //p' <<<"${cat_output}")"
	if [[ "${builtin_status}" != "${cat_status}" || "${builtin_count}" != "${cat_count}" ||
	      "${cat_count}" != "${expected_count}" ]]; then
		echo "SC mismatch for ${relative_program}: status ${builtin_status}/${cat_status}," \
			" executions ${builtin_count}/${cat_count}, expected ${expected_count}" >&2
		exit 1
	fi
	if ! grep -Fq -- "${expected_marker}" <<<"${builtin_output}" ||
	   ! grep -Fq -- "${expected_marker}" <<<"${cat_output}"; then
		echo "Missing shared verdict marker '${expected_marker}' for ${relative_program}" >&2
		exit 1
	fi
}

compare_case "correct/litmus/SB/variants/sb0.c" 3 "No errors were detected."
compare_case "correct/litmus/LB+ctrl/variants/lb+ctrl0.c" 3 "No errors were detected."
# These cases permanently cover two defects found by the broad corpus: a
# harmless control assumption must not acquire a spurious coherence warning,
# and dynamically allocated storage must not read from the static initializer.
compare_case "correct/litmus/assume-ctrl/variants/assume-ctrl0.c" 4 \
	"No errors were detected."
compare_case "correct/infr/atomic-min-max/variants/atomic-min-max0.c" 1 \
	"No errors were detected."
compare_case "correct/litmus/WWR+2WR/variants/wwr+2wr0.c" 0 "Unordered writes"
compare_case "wrong/infr/print-names-array/variants/names-2d-array0.c" 1 \
	"Error: Safety violation!"
