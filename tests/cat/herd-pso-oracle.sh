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
fixture="$2"

# Official x86tso forbids MP, whereas herd's official mips.cat explicitly uses
# a PSO ppo. With plain R/W events and architecture checking disabled, it permits
# the same existential outcome without relying on architecture-specific fences.
tso_output="$("${herd7}" -model x86tso.cat "${fixture}")"
pso_output="$("${herd7}" -model mips.cat -archcheck false "${fixture}")"

if ! grep -Eq '^Observation MP Never' <<<"${tso_output}"; then
	echo "Official herd x86tso.cat unexpectedly permits the MP anomaly" >&2
	exit 1
fi
if ! grep -Eq '^Observation MP Sometimes' <<<"${pso_output}"; then
	echo "Official herd PSO ppo unexpectedly forbids the MP anomaly" >&2
	exit 1
fi
