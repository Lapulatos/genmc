#!/bin/bash
set -euo pipefail

: "${RVF_ROOT:?set RVF_ROOT to the experiment or repository root}"
: "${RVF_OUTPUT:?set RVF_OUTPUT to the output directory}"

source_root="${RVF_SOURCE_ROOT:-${RVF_ROOT}/source}"
bin="${RVF_BIN:-${RVF_ROOT}/build-gcc13-release/bin/genmc}"
model="${source_root}/models/cat/recursive-sc.cat"
tests="${source_root}/tests"
[[ -x "${bin}" ]] || { echo "missing GenMC binary: ${bin}" >&2; exit 2; }
mkdir -p "${RVF_OUTPUT}/logs"

cases=(
	"correct/litmus/SB/variants/sb0.c"
	"correct/litmus/MP/variants/mp0.c"
	"correct/litmus/LB/variants/lb0.c"
	"correct/litmus/po-loc/variants/po-loc0.c"
	"correct/litmus/CoRR1/variants/corr10.c"
	"correct/litmus/CoWR/variants/cowr0.c"
	"correct/litmus/W+JW/variants/W+JW0.c"
	"correct/litmus/SB+assert/variants/SB+assert0.c"
	"wrong/safety/dekker_rlx/variants/dekker_rlx0.c"
	"correct/litmus/RMWFix/variants/rmwfix0.c"
	"correct/infr/atomic-min-max/variants/atomic-min-max0.c"
	"correct/infr/cpp-sb-static/variants/cpp-mp-static0.cpp"
	"correct/litmus/WWmerge1/variants/WWmerge10.c"
	"correct/litmus/LB2/variants/lb0.c"
	"correct/litmus/Z6.U/variants/Z6.U0.c"
	"correct/litmus/TC1/variants/tc10.c"
	"correct/litmus/cii/variants/cii0.c"
	"correct/litmus/wcii/variants/wcii0.c"
	"correct/litmus/rii/variants/rii0.c"
	"correct/litmus/ori/variants/ori0.c"
	"correct/litmus/rfi-preserved/variants/rfi-preserved0.c"
	"correct/litmus/SB+rfis/variants/SB+rfis0.c"
	"correct/litmus/W+RWC/variants/w+rwc0.c"
	"correct/litmus/MCP-rrw/variants/MCP-rrw0.c"
	"correct/litmus/2+2W+4sc/variants/2+2w+4sc0.c"
	"correct/litmus/MPU+rel+acq/variants/mpu+rel+acq0.c"
	"correct/litmus/LB+addr-fun/variants/lb+addr-fun0.c"
	"wrong/racy/MP+rlx+rlx/variants/mp+rlx+rlx0.c"
	"wrong/racy/S+rlx+rlx/variants/s+rlx+rlx0.c"
	"wrong/safety/fib_bench/variants/fib_bench0.c"
)

if [[ -n "${RVF_CASE_FILE:-}" ]]; then
	cases=()
	while IFS= read -r relative || [[ -n "${relative}" ]]; do
		[[ -z "${relative}" || "${relative}" == \#* ]] && continue
		cases+=("${relative}")
	done <"${RVF_CASE_FILE}"
	[[ ${#cases[@]} -gt 0 ]] || {
		echo "case manifest is empty: ${RVF_CASE_FILE}" >&2
		exit 2
	}
fi

if [[ "${RVF_AUTO_LITMUS:-0}" == 1 ]]; then
	[[ -z "${RVF_CASE_FILE:-}" ]] || {
		echo "RVF_AUTO_LITMUS and RVF_CASE_FILE are mutually exclusive" >&2
		exit 2
	}
	cases=()
	while IFS= read -r program; do
		cases+=("${program#"${tests}/"}")
	done < <(find "${tests}/correct/litmus" "${tests}/wrong/racy" \
		-path '*/variants/*.c' -type f | sort)
	[[ ${#cases[@]} -gt 0 ]] || {
		echo "automatic litmus discovery found no programs" >&2
		exit 2
	}
fi

printf 'case\tmode\tworkers\tstatus\telapsed_ns\tgate\tcomplete\tblocked\treduced\tsingleton_bypass\tnative_reads_synthesized\tfail_open\tverdict_sha256\tsummary_sha256\n' \
	>"${RVF_OUTPUT}/results.tsv"

for relative in "${cases[@]}"; do
	program="${tests}/${relative}"
	[[ -f "${program}" ]] || {
		echo "missing fixture: ${relative}" >&2
		exit 2
	}
	case_id="$(tr '/+' '__' <<<"${relative%.*}")"
	for spec in baseline:1 baseline:2 rvf:1 rvf:2; do
		mode="${spec%%:*}"
		workers="${spec##*:}"
		extra=""
		[[ "${mode}" == rvf ]] && extra="--sc-rvf-exploration"
		log="${RVF_OUTPUT}/logs/${case_id}-${mode}-n${workers}"
		start="$(date +%s%N)"
		set +e
		timeout --signal=TERM --kill-after=2s 20s "${bin}" \
			--model-file="${model}" --disable-estimation --cat-stats \
			--nthreads="${workers}" ${extra} "${program}" \
			>"${log}.out" 2>"${log}.err"
		status=$?
		set -e
		end="$(date +%s%N)"
		{ grep -E '^(Error:|Warning:|No errors were detected\.|Number of complete executions explored:|Number of blocked executions seen:)' \
			"${log}.out" || true; } | sort -u >"${log}.summary"
		{ grep -E '^(Error:|Warning:|No errors were detected\.)' "${log}.out" || true; } \
			| sort -u >"${log}.verdict"
		gate="$(sed -n 's/^SC RVF program gate: //p' "${log}.out")"
		complete="$(sed -n 's/^Number of complete executions explored: \([0-9][0-9]*\).*/\1/p' "${log}.out")"
		blocked="$(sed -n 's/^Number of blocked executions seen: \([0-9][0-9]*\).*/\1/p' "${log}.out")"
		stats="$(grep -F 'Exploration statistics:' "${log}.out" || true)"
		reduced="$(sed -n 's/.*rvf-loads-reduced=\([0-9][0-9]*\).*/\1/p' <<<"${stats}")"
		singleton_bypass="$(sed -n 's/.*rvf-singleton-bypass=\([0-9][0-9]*\).*/\1/p' <<<"${stats}")"
		native_reads_synthesized="$(sed -n 's/.*rvf-native-reads-synthesized=\([0-9][0-9]*\).*/\1/p' <<<"${stats}")"
		fail_open="$(sed -n 's/.*rvf-fail-open=\([0-9][0-9]*\).*/\1/p' <<<"${stats}")"
		summary_sha="$(sha256sum "${log}.summary" | cut -d' ' -f1)"
		verdict_sha="$(sha256sum "${log}.verdict" | cut -d' ' -f1)"
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"${relative}" "${mode}" "${workers}" "${status}" "$((end-start))" \
			"${gate}" "${complete}" "${blocked:-0}" "${reduced}" \
			"${singleton_bypass}" "${native_reads_synthesized}" "${fail_open}" \
			"${verdict_sha}" "${summary_sha}" \
			>>"${RVF_OUTPUT}/results.tsv"
	done
done
