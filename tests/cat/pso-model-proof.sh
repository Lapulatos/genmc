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
model_root="$2"
program="$3"
source_root="$4"

# Run one model without allowing PSO's expected safety violation to stop the script.
run_model()
{
	model="$1"
	set +e
	model_output="$("${genmc}" --model-file="${model_root}/${model}.cat" \
		--disable-estimation "${program}" 2>&1)"
	model_status=$?
	set -e
}

# SC and TSO preserve cross-location write order for this test, while PSO does not.
run_model sc
sc_output="${model_output}"
sc_status="${model_status}"
run_model tso
tso_output="${model_output}"
tso_status="${model_status}"
run_model pso
pso_output="${model_output}"
pso_status="${model_status}"

if [[ "${sc_status}" != 0 || "${tso_status}" != 0 || "${pso_status}" != 42 ]]; then
	echo "Unexpected SC/TSO/PSO statuses: ${sc_status}/${tso_status}/${pso_status}" >&2
	exit 1
fi
if ! grep -Fq "No errors were detected." <<<"${sc_output}" ||
   ! grep -Fq "No errors were detected." <<<"${tso_output}" ||
   ! grep -Fq "Error: Safety violation!" <<<"${pso_output}"; then
	echo "Changing only the model file did not produce the expected PSO distinction" >&2
	exit 1
fi

# Fixed counts catch accidental profile/pruning changes as well as verdict changes.
sc_count="$(sed -n 's/^Number of complete executions explored: //p' <<<"${sc_output}")"
tso_count="$(sed -n 's/^Number of complete executions explored: //p' <<<"${tso_output}")"
pso_count="$(sed -n 's/^Number of complete executions explored: //p' <<<"${pso_output}")"
if [[ "${sc_count}/${tso_count}/${pso_count}" != "3/3/2" ]]; then
	echo "Unexpected SC/TSO/PSO execution counts: ${sc_count}/${tso_count}/${pso_count}" >&2
	exit 1
fi

# PSO must retain the TSO result where its CAT ppo explicitly preserves order.
check_preserved_case()
{
	relative_program="$1"
	expected_count="$2"
	for preserved_model in tso pso; do
		preserved_output="$("${genmc}" \
			--model-file="${model_root}/${preserved_model}.cat" --disable-estimation \
			"${source_root}/${relative_program}" 2>&1)"
		preserved_count="$(sed -n 's/^Number of complete executions explored: //p' \
			<<<"${preserved_output}")"
		if [[ "${preserved_count}" != "${expected_count}" ]] ||
		   ! grep -Fq "No errors were detected." <<<"${preserved_output}"; then
			echo "Unexpected ${preserved_model} result for ${relative_program}" >&2
			exit 1
		fi
	done
}

check_preserved_case "correct/litmus/po-loc/variants/po-loc0.c" 3
check_preserved_case "correct/litmus/RMWFix/variants/rmwfix0.c" 4
check_preserved_case "correct/litmus/SB+scfs/variants/sb+scfs0.c" 3
