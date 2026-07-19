#!/usr/bin/env bash
set -euo pipefail

root="${GENMC_EXPERIMENT_ROOT:-${HOME}/svcomp2026-caat}"
definition="${1:?usage: run_remote_benchexec.sh <definition.xml> [extra benchexec args...]}"
shift

export GENMC_EXPERIMENT_ROOT="${root}"
export GENMC_BINARY="${GENMC_BINARY:-${root}/build/genmc-caat/bin/genmc}"
export PYTHONPATH="${root}${PYTHONPATH:+:${PYTHONPATH}}"
tool_directory="${BENCHEXEC_TOOL_DIRECTORY:-${root}/build/genmc-caat/bin}"
benchexec_binary="${BENCHEXEC_BIN:-${root}/conda/bin/benchexec}"
benchexec=(
  "${benchexec_binary}" --no-container
  --tool-directory "${tool_directory}"
  --outputpath "${root}/results/" "$definition" "$@"
)

if [[ "${BENCHEXEC_DIRECT:-0}" == "1" ]]; then
  exec "${benchexec[@]}"
fi

exec systemd-run --user --scope --slice=benchexec -p Delegate=yes \
  "${benchexec[@]}"
