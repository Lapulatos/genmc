#!/usr/bin/env bash
set -euo pipefail

root="${GENMC_EXPERIMENT_ROOT:-${HOME}/svcomp2026-caat}"
workers="${CENSUS_WORKERS:-8}"
queue_unit="svcomp-caat-queue.service"

if systemctl --user is-active --quiet "${queue_unit}"; then
	echo "refusing to overlap compatibility censuses with active ${queue_unit}" >&2
	exit 2
fi
[[ "${workers}" =~ ^[1-9][0-9]*$ ]] || {
	echo "CENSUS_WORKERS must be a positive integer" >&2
	exit 2
}

run_definition() {
	local definition="$1"
	[[ -f "${root}/definitions/${definition}.xml" ]] || {
		echo "missing definition ${root}/definitions/${definition}.xml" >&2
		exit 2
	}
	GENMC_EXPERIMENT_ROOT="${root}" \
		"${root}/pipeline/run_remote_benchexec.sh" \
		"${root}/definitions/${definition}.xml" -N "${workers}"
}

# These are compatibility censuses. Their wall/CPU measurements are not used
# in performance tables; each individual task still has one core and 4 GB.
run_definition census-no-data-race-sc
run_definition trust-family-rc11-census
run_definition trust-family-sc-census

echo "post-core GenMC and TruSt-family censuses complete"
echo "Deagle and CBMC remain gated on archive installation and executable smoke tests"
