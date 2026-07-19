#!/bin/bash

# Experiment-only wrapper: keep every command and exit code unchanged while enabling
# the P0.2 cache. The flag is inert for non-CAAT models and certified SC/TSO candidates.

set -euo pipefail

: "${GENMC_REAL:?set GENMC_REAL to the P0.2 GenMC binary}"
exec "${GENMC_REAL}" --cat-kernel-cache "$@"
