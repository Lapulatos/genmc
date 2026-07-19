#!/bin/bash
set -euo pipefail

: "${GENMC_REAL_BINARY:?GENMC_REAL_BINARY must name the experiment binary}"
: "${GENMC_EXPERIMENT_MODE:?GENMC_EXPERIMENT_MODE must select a supported experiment}"

arguments=("$@")
case "$GENMC_EXPERIMENT_MODE" in
	baseline) ;;
	primitive-cache) arguments=(--cat-primitive-cache "${arguments[@]}") ;;
	primitive-cache-fast) arguments=(--cat-primitive-cache --cat-fast-primitive-build --cat-fast-checks "${arguments[@]}") ;;
	primitive-cache-fast-compose) arguments=(--cat-primitive-cache --cat-fast-primitive-build --cat-fast-checks --cat-fast-composition "${arguments[@]}") ;;
	primitive-cache-fast-compose-cycle) arguments=(--cat-primitive-cache --cat-fast-primitive-build --cat-fast-checks --cat-fast-composition --cat-fast-cycle-checks "${arguments[@]}") ;;
	primitive-cache-fast-compose-cycle-coherence) arguments=(--cat-primitive-cache --cat-fast-primitive-build --cat-fast-coherence-build --cat-fast-checks --cat-fast-composition --cat-fast-cycle-checks "${arguments[@]}") ;;
	primitive-cache-fast-compose-cycle-descriptor) arguments=(--cat-primitive-cache --cat-fast-primitive-build --cat-fast-descriptor-build --cat-fast-checks --cat-fast-composition --cat-fast-cycle-checks "${arguments[@]}") ;;
	primitive-cache-fast-compose-cycle-descriptor-reuse) arguments=(--cat-primitive-cache --cat-fast-primitive-build --cat-fast-descriptor-build --cat-fast-descriptor-reuse --cat-fast-checks --cat-fast-composition --cat-fast-cycle-checks "${arguments[@]}") ;;
	rvf) arguments=(--sc-rvf-exploration "${arguments[@]}") ;;
	annotated-rvf) arguments=(--sc-rvf-exploration --sc-rvf-annotated-reads "${arguments[@]}") ;;
	control) arguments=(--sc-rvf-exploration --sc-rvf-disable-quotient "${arguments[@]}") ;;
	skeleton-census) arguments=(--finite-skeleton-stats-only "${arguments[@]}") ;;
	tso)
		for index in "${!arguments[@]}"; do
			if [[ "${arguments[$index]}" == --model-file=*/recursive-sc.cat ]]; then
				arguments[$index]="${arguments[$index]%/recursive-sc.cat}/recursive-tso.cat"
			fi
		done
		;;
	pso-v9)
		for index in "${!arguments[@]}"; do
			if [[ "${arguments[$index]}" == --model-file=*/recursive-sc.cat ]]; then
				arguments[$index]="${arguments[$index]%/recursive-sc.cat}/recursive-pso.cat"
			fi
		done
		arguments=(--cat-preventive-pruning "${arguments[@]}")
		;;
	*) echo "unknown formal experiment mode: $GENMC_EXPERIMENT_MODE" >&2; exit 2 ;;
esac

exec "$GENMC_REAL_BINARY" "${arguments[@]}"
