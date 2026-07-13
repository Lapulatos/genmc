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

herd7="$1"
fixture_root="$2"

# Run official herd x86tso.cat on an assembly-aligned oracle and require the
# expected existential observation classification, independent of GenMC output.
check_observation()
{
	fixture="$1"
	expected="$2"
	output="$("${herd7}" -model x86tso.cat "${fixture_root}/${fixture}.litmus")"
	if ! grep -Eq "^Observation ${fixture} ${expected}" <<<"${output}"; then
		echo "Unexpected herd x86tso observation for ${fixture}:" >&2
		echo "${output}" >&2
		exit 1
	fi
}

# TSO permits store buffering but forbids the write/write + read/read MP anomaly.
check_observation SB Sometimes
check_observation MP Never
