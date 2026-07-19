#!/bin/bash

# Experiment-only adapter used by existing differential harnesses. The production
# command line remains explicit in BenchExec definitions; this wrapper only injects the
# measurement flag while preserving every argument and exit status.

set -euo pipefail

: "${GENMC_REAL:?set GENMC_REAL to the instrumented GenMC binary}"
exec "${GENMC_REAL}" --cat-kernel-stats "$@"
