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
program="$3"

# Require a failing invocation to emit one stable diagnostic fragment.
expect_failure()
{
	expected="$1"
	shift

	set +e
	output="$("$@" 2>&1)"
	status=$?
	set -e

	if [[ ${status} -eq 0 ]]; then
		echo "Expected command to fail: $*" >&2
		exit 1
	fi
	if ! grep -Fq -- "${expected}" <<<"${output}"; then
		echo "Missing diagnostic '${expected}' in output:" >&2
		echo "${output}" >&2
		exit 1
	fi
}

# The public help must expose the new spelling used by the project contract.
help_output="$("${genmc}" --help 2>&1)"
grep -Fq -- "--model-file=<model.cat>" <<<"${help_output}"
grep -Fq -- "--explain-cat" <<<"${help_output}"

# All invalid combinations fail during command-line/config processing.
expect_failure "CAT model file does not exist" \
	"${genmc}" --model-file=/definitely/missing/genmc-model.cat "${program}"
expect_failure "CAT model file is not a regular file" \
	"${genmc}" --model-file="$(dirname "${model}")" "${program}"
expect_failure "cannot be combined with an explicit built-in" \
	"${genmc}" --model-file="${model}" --sc "${program}"
expect_failure "--model-file may only be specified once" \
	"${genmc}" --model-file="${model}" --model-file="${model}" "${program}"
expect_failure "--explain-cat requires --model-file" \
	"${genmc}" --explain-cat "${program}"

# A valid SC file executes through the generic checker and matches built-in SC.
cat_output="$("${genmc}" --model-file="${model}" --disable-estimation --nthreads=2 \
	"${program}" 2>&1)"
builtin_output="$("${genmc}" --sc --disable-estimation --disable-mm-detector "${program}" 2>&1)"
grep -Fq -- "No errors were detected." <<<"${cat_output}"
grep -Fq -- "No errors were detected." <<<"${builtin_output}"
cat_count="$(sed -n 's/^Number of complete executions explored: //p' <<<"${cat_output}")"
builtin_count="$(sed -n 's/^Number of complete executions explored: //p' <<<"${builtin_output}")"
if [[ -z "${cat_count}" || "${cat_count}" != "${builtin_count}" || "${cat_count}" != 3 ]]; then
	echo "SC differential mismatch: CAT=${cat_count}, built-in=${builtin_count}" >&2
	exit 1
fi

# Explanations are opt-in and identify rejected candidates using base literals.
if grep -Fq -- "CAT explanation:" <<<"${cat_output}"; then
	echo "Default CAT output unexpectedly contains explanations" >&2
	exit 1
fi
explained_output="$("${genmc}" --model-file="${model}" --explain-cat --disable-estimation \
	--nthreads=1 "${program}" 2>&1)"
grep -Fq -- "CAT explanation:" <<<"${explained_output}"
grep -Fq -- "CAT check 'sc' failed" <<<"${explained_output}"
