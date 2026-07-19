#!/bin/bash

# Enable V11 only for recursive PSO. Other models remain byte-for-byte on their
# existing command line so the full table cannot silently broaden the experiment.

set -euo pipefail

real_genmc="${V11_REAL_BIN:-${GENMC_REAL_BINARY:?set V11_REAL_BIN or GENMC_REAL_BINARY}}"
extra=()
for argument in "$@"; do
	if [[ "${argument}" == *"/recursive-pso.cat" ]]; then
		extra=(--cat-preventive-pruning --cat-focus-reach)
		break
	fi
done

exec "${real_genmc}" "${extra[@]}" "$@"
