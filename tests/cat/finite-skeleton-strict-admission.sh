#!/usr/bin/env bash
set -euo pipefail

genmc=$1
switch_program=$2
gep_program=$3

switch_log=$($genmc --finite-skeleton-stats-only "$switch_program" 2>&1)
grep -q 'built=false' <<<"$switch_log"
grep -q 'unsupported-terminator:switch' <<<"$switch_log"

gep_log=$($genmc --finite-skeleton-stats-only "$gep_program" 2>&1)
grep -q 'encodable-subset=0' <<<"$gep_log"
grep -q 'dynamic-memory-address' <<<"$gep_log"
