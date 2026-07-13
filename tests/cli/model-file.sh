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

# All invalid combinations fail during command-line/config processing.
expect_failure "CAT model file does not exist" \
	"${genmc}" --model-file=/definitely/missing/genmc-model.cat "${program}"
expect_failure "CAT model file is not a regular file" \
	"${genmc}" --model-file="$(dirname "${model}")" "${program}"
expect_failure "cannot be combined with an explicit built-in" \
	"${genmc}" --model-file="${model}" --sc "${program}"
expect_failure "--model-file may only be specified once" \
	"${genmc}" --model-file="${model}" --model-file="${model}" "${program}"

# A valid file reaches the deliberate boundary without compiling the program.
expect_failure "CAT model parsed" \
	"${genmc}" --model-file="${model}" --nthreads=2 "${program}"

# The legacy built-in path remains executable and retains its prior meaning.
"${genmc}" --sc --disable-estimation --disable-mm-detector --v0 "${program}"
