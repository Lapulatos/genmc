#!/usr/bin/env bash
set -euo pipefail

genmc=$1
models=$2
unsafe_program=$3
safe_program=$4
scratch=$(mktemp -d "${TMPDIR:-/tmp}/genmc-finite-oracle.XXXXXX")
trap 'rm -rf "$scratch"' EXIT

for architecture in sc tso pso; do
    model="$models/recursive-$architecture.cat"
    "$genmc" --model-file="$model" --finite-skeleton-stats-only \
        --finite-skeleton-solve-one --finite-skeleton-solve-max=32 \
        "$safe_program" >"$scratch/$architecture-safe-symbolic.log" 2>&1
    grep -q 'status=1' "$scratch/$architecture-safe-symbolic.log"
    grep -q 'error-candidates=0' "$scratch/$architecture-safe-symbolic.log"
    "$genmc" --model-file="$model" --disable-estimation --disable-mm-detector \
        --nthreads=1 "$safe_program" >"$scratch/$architecture-safe-native.log" 2>&1
    grep -q 'Verification complete' "$scratch/$architecture-safe-native.log"
    "$genmc" --model-file="$model" --disable-estimation --disable-mm-detector \
        --nthreads=1 --cat-stats --finite-symbolic-errors --finite-symbolic-max=32 \
        "$safe_program" >"$scratch/$architecture-safe-production.log" 2>&1
    grep -q 'terminal=exhausted-fallback' "$scratch/$architecture-safe-production.log"
    grep -q 'Verification complete' "$scratch/$architecture-safe-production.log"

    "$genmc" --model-file="$model" --finite-skeleton-stats-only \
        --finite-skeleton-solve-one --finite-skeleton-solve-max=32 \
        --finite-skeleton-replay-output="$scratch/$architecture-replay.ll" \
        "$unsafe_program" >"$scratch/$architecture-unsafe-symbolic.log" 2>&1
    grep -q 'error-candidates=1' "$scratch/$architecture-unsafe-symbolic.log"
    grep -q 'replay-artifact=written' "$scratch/$architecture-unsafe-symbolic.log"
    if "$genmc" --model-file="$model" --disable-estimation --disable-mm-detector \
        --nthreads=1 "$scratch/$architecture-replay.ll" \
        >"$scratch/$architecture-unsafe-native.log" 2>&1; then
        echo "$architecture constrained unsafe replay unexpectedly succeeded" >&2
        exit 1
    fi
    grep -q 'Error: Safety violation!' "$scratch/$architecture-unsafe-native.log"
    if "$genmc" --model-file="$model" --disable-estimation --disable-mm-detector \
        --nthreads=1 --cat-stats --finite-symbolic-errors --finite-symbolic-max=32 \
        "$unsafe_program" >"$scratch/$architecture-unsafe-production.log" 2>&1; then
        echo "$architecture production finite error search unexpectedly succeeded" >&2
        exit 1
    fi
    grep -q 'terminal=confirmed-error' "$scratch/$architecture-unsafe-production.log"
    grep -q 'finite-replay-confirmed=1' "$scratch/$architecture-unsafe-production.log"
    grep -q 'Error: Safety violation!' "$scratch/$architecture-unsafe-production.log"
done
