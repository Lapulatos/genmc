#!/bin/bash

set -euo pipefail

: "${GENMC_REAL:?set GENMC_REAL to the candidate GenMC binary}"
exec "${GENMC_REAL}" --cat-learned-nogoods "$@"
