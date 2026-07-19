#!/bin/bash

set -euo pipefail

: "${GENMC_REAL_BINARY:?GENMC_REAL_BINARY must name the experiment binary}"

arguments=("$@")
for argument in "$@"; do
	if [[ "${argument}" == --model-file=*/recursive-pso.cat ]]; then
		arguments=(--cat-preventive-pruning --cat-preventive-no-retain "${arguments[@]}")
		break
	fi
done

exec "${GENMC_REAL_BINARY}" "${arguments[@]}"
