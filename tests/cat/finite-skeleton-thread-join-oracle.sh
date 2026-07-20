#!/usr/bin/env bash
set -euo pipefail

genmc=$1
model=$2
program_dir=$3
scratch=$(mktemp -d "${TMPDIR:-/tmp}/genmc-finite-join.XXXXXX")
trap 'rm -rf "$scratch"' EXIT

for outcome in 00 01 10 11; do
	program="$program_dir/rvf-sb-outcome-$outcome.c"
	finite_log="$scratch/$outcome-finite.log"
	native_log="$scratch/$outcome-native.log"
	"$genmc" --model-file="$model" --finite-skeleton-stats-only \
		--finite-skeleton-solve-one \
		--finite-skeleton-first-model=abstract-cardinality-rvf-value-sc-order \
		"$program" >"$finite_log" 2>&1
	grep -q 'creates=2 joins=2' "$finite_log"
	grep -q 'Finite skeleton IR: built=true' "$finite_log"
	if [[ "$outcome" == 00 ]]; then
		grep -q 'supported=true status=1 assignment=false' "$finite_log"
		"$genmc" --model-file="$model" --disable-estimation \
			--disable-mm-detector --nthreads=1 "$program" >"$native_log" 2>&1
		grep -q 'Verification complete' "$native_log"
	else
		grep -q 'supported=true status=0 assignment=true' "$finite_log"
		grep -q 'abstract=false materialization-errors=0 evaluation-errors=0 violations=0' \
			"$finite_log"
		if "$genmc" --model-file="$model" --disable-estimation \
			--disable-mm-detector --nthreads=1 "$program" >"$native_log" 2>&1; then
			echo "$outcome native oracle unexpectedly reported safe" >&2
			exit 1
		fi
		grep -q 'Error: Safety violation!' "$native_log"
	fi
done
