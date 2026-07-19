#!/bin/bash

set -euo pipefail

if [[ "$#" -ne 3 ]]; then
	echo "usage: $0 <stage> <genmc-binary> <result.tsv>" >&2
	exit 2
fi
stage="$1"
binary="$2"
result="$3"
root="$(cd "$(dirname "$0")/.." && pwd)"
programs=(
	"correct/data-structures/ms-queue-dynamic/variants/main0.c"
	"correct/data-structures/treiber-stack-dynamic/variants/main0.c"
	"correct/data-structures/fcombiner-async/variants/main0.c"
)
models=(sc tso pso)

if [[ ! -s "${result}" ]]; then
	printf 'stage\tmodel\tprogram\tstatus\tinitialize\tunchanged\tinsert\trollback\trollback_insert\trebuild\tevicted\toracle\teval_ops\tvalue_changes\tqueue_pushes\toffline_evals\tadapter_ns\tsync_ns\toffline_ns\tcopy_ns\tworklist_ns\tcheckpoint_ns\trollback_ns\tmaterialize_ns\tequality_ns\tinsert_attempt_ns\thistory_ns\trebuild_ns\tprofiled_queries\n' >"${result}"
fi

for model in "${models[@]}"; do
	for relative in "${programs[@]}"; do
		set +e
		output="$("${binary}" --model-file="${root}/models/cat/recursive-${model}.cat" \
			--cat-stats --disable-estimation --disable-mm-detector --nthreads=1 \
			"${root}/tests/${relative}" 2>&1)"
		status=$?
		set -e
		[[ "${status}" -eq 0 || "${status}" -eq 42 ]] || { echo "${output}" >&2; exit 1; }
		line="$(sed -n 's/^CAT incremental statistics: //p' <<<"${output}" | tail -1)"
		[[ -n "${line}" ]] || { echo "missing CAT stats: ${model}/${relative}" >&2; exit 1; }
		value() { sed -n "s/.* $1=\([0-9][0-9]*\).*/\1/p" <<<" ${line}"; }
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"${stage}" "${model}" "${relative}" "${status}" \
			"$(value initialize)" "$(value unchanged)" "$(value insert)" "$(value rollback)" \
			"$(value rollback-insert)" "$(value rebuild)" "$(value evicted)" "$(value oracle)" \
			"$(value eval-ops)" "$(value value-changes)" "$(value queue-pushes)" "$(value offline-evals)" \
			"$(value adapter-ns)" "$(value sync-ns)" "$(value offline-ns)" "$(value copy-ns)" \
			"$(value worklist-ns)" "$(value checkpoint-ns)" "$(value rollback-ns)" \
			"$(value materialize-ns)" "$(value equality-ns)" "$(value insert-attempt-ns)" \
			"$(value history-ns)" "$(value rebuild-ns)" \
			"$(value profiled-queries)" >>"${result}"
	done
done
