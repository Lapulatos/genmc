#!/usr/bin/env bash
set -euo pipefail

genmc=$1
model=$2
program=$3
scratch=$(mktemp -d "${TMPDIR:-/tmp}/genmc-finite-replay.XXXXXX")
trap 'rm -rf "$scratch"' EXIT

"$genmc" --model-file="$model" --finite-skeleton-stats-only \
    --finite-skeleton-solve-one --finite-skeleton-solve-max=16 \
    --finite-skeleton-replay-output="$scratch/replay.ll" "$program" \
    >"$scratch/solve.log" 2>&1

test -s "$scratch/replay.ll"
grep -q 'error-candidates=1' "$scratch/solve.log"
grep -q 'replay-artifact=written' "$scratch/solve.log"
grep -q 'finite.replay.value' "$scratch/replay.ll"

if "$genmc" --model-file="$model" --disable-estimation --disable-mm-detector \
    --nthreads=1 "$scratch/replay.ll" >"$scratch/replay.log" 2>&1; then
    echo "constrained replay did not report the expected safety violation" >&2
    exit 1
fi
grep -q 'Error: Safety violation!' "$scratch/replay.log"
