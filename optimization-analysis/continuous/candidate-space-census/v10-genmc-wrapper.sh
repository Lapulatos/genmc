#!/bin/bash

# Add the V10 experiment flags only to recursive-PSO invocations. SC and TSO do not
# carry V9's structural preventive certificate and must remain unchanged.

set -euo pipefail

if [[ -n "${V10_REAL_BIN:-}" ]]; then
	real_genmc="${V10_REAL_BIN}"
else
	real_genmc="${GENMC_REAL_BINARY:?set V10_REAL_BIN or GENMC_REAL_BINARY}"
fi
extra=()
for argument in "$@"; do
	if [[ "${argument}" == *"/recursive-pso.cat" ]]; then
		extra=(--cat-preventive-pruning --cat-conflict-cores)
		break
	fi
done

exec "${real_genmc}" "${extra[@]}" "$@"
