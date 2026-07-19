#!/usr/bin/env bash
set -euo pipefail

before="${1:?before binary}"
after="${2:?after binary}"
source_root="${3:?source root}"
output="${4:?output tsv}"
repetitions="${5:-7}"
script_dir="$(cd "$(dirname "$0")" && pwd)"

programs=(
  correct/litmus/SB/variants/sb0.c
  correct/litmus/RMWFix/variants/rmwfix0.c
  correct/data-structures/ms-queue-dynamic/variants/main0.c
  correct/data-structures/treiber-stack-dynamic/variants/main0.c
  correct/data-structures/fcombiner-async/variants/main0.c
)
models=(sc tso pso)
mkdir -p "$(dirname "${output}")"
printf 'variant\tmodel\tprogram\trepetition\tstatus\twall_seconds\tuser_seconds\tsys_seconds\tpeak_rss_kib\texecutions\tblocked\n' >"${output}"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

run_one()
{
  local variant="$1" binary="$2" model="$3" program="$4" repetition="$5"
  local stdout="${work}/stdout" timing="${work}/timing"
  python3 "${script_dir}/measure-command.py" "${timing}" "${stdout}" "${work}/stderr" -- \
    "${binary}" "--model-file=${source_root}/models/cat/recursive-${model}.cat" \
    --disable-estimation --disable-mm-detector --nthreads=1 \
    "${source_root}/tests/${program}"
  local wall user sys rss status executions blocked
  IFS=$'\t' read -r wall user sys rss status <"${timing}"
  if [[ "${status}" -ne 0 && "${status}" -ne 42 ]]; then
    echo "unexpected status ${status}: ${variant}/${model}/${program}" >&2
    sed -n '1,80p' "${work}/stderr" >&2
    exit 1
  fi
  executions="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\)$/\1/p' "${stdout}")"
  blocked="$(sed -n 's/^Number of blocked executions seen: \([0-9][0-9]*\)$/\1/p' "${stdout}")"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "${variant}" "${model}" "${program}" "${repetition}" "${status}" \
    "${wall}" "${user}" "${sys}" "${rss}" "${executions:-0}" "${blocked:-0}" \
    >>"${output}"
}

for ((rep=1; rep<=repetitions; ++rep)); do
  for model in "${models[@]}"; do
    for program in "${programs[@]}"; do
      if ((rep % 2)); then
        run_one before "${before}" "${model}" "${program}" "${rep}"
        run_one after "${after}" "${model}" "${program}" "${rep}"
      else
        run_one after "${after}" "${model}" "${program}" "${rep}"
        run_one before "${before}" "${model}" "${program}" "${rep}"
      fi
    done
  done
  echo "completed repetition ${rep}/${repetitions}" >&2
done
