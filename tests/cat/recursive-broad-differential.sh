#!/bin/bash

# Broad Phase 1 versus recursive-CAAT differential validation.
#
# This program is dual-licensed under the Apache License 2.0 and the MIT License.
# You may choose to use, distribute, or modify this software under either license.

set -euo pipefail

usage()
{
	echo "usage: $0 <genmc> <models-dir> <tests-dir> <result.tsv> [jobs]" >&2
	exit 2
}

# Normalize only externally observable verification semantics. Timing, paths,
# banners, and graph dumps are deliberately excluded from the oracle.
normalize_output()
{
	sed -n \
		-e '/^No errors were detected\.$/p' \
		-e '/^Error: /p' \
		-e '/^[Ww][Aa][Rr][Nn][Ii][Nn][Gg]: /p' \
		-e '/^Number of complete executions explored: /p' \
		-e '/^Number of blocked executions/ p' \
		-e '/^Number of executions exceeding bound: /p' "$1" |
		paste -sd ';' -
}

if [[ "${1:-}" == "--worker" ]]; then
	shift
	[[ "$#" -eq 7 ]] || usage
	genmc="$1"
	models_dir="$2"
	tests_dir="$3"
	parts_dir="$4"
	index="$5"
	kind="$6"
	relative_source="$7"
	source_path="${tests_dir}/${relative_source}"
	case_dir="$(dirname "$(dirname "${source_path}")")"
	args_file="${case_dir}/args.sc.mo.in"
	genmc_args=""
	clang_args=""
	if [[ -f "${args_file}" ]]; then
		argument_line="$(sed -n '/[^[:space:]]/ {p; q;}' "${args_file}")"
		genmc_args="${argument_line%%|*}"
		if [[ "${argument_line}" == *"|"* ]]; then
			clang_args="${argument_line#*|}"
		fi
	fi
	source_hash="$(shasum -a 256 "${source_path}" | awk '{print $1}')"

	for model in sc tso pso; do
		baseline_output="${parts_dir}/${index}-${model}-baseline.log"
		recursive_output="${parts_dir}/${index}-${model}-recursive.log"
		baseline_command=("${genmc}" "--model-file=${models_dir}/${model}.cat"
			--disable-estimation --disable-mm-detector)
		recursive_command=("${genmc}"
			"--model-file=${models_dir}/recursive-${model}.cat"
			--disable-estimation --disable-mm-detector)
		if [[ "${genmc_args}" =~ [^[:space:]] ]]; then
			read -r -a parsed_genmc_args <<<"${genmc_args}"
			baseline_command+=("${parsed_genmc_args[@]}")
			recursive_command+=("${parsed_genmc_args[@]}")
		fi
		baseline_command+=(--)
		recursive_command+=(--)
		if [[ "${clang_args}" =~ [^[:space:]] ]]; then
			read -r -a parsed_clang_args <<<"${clang_args}"
			baseline_command+=("${parsed_clang_args[@]}")
			recursive_command+=("${parsed_clang_args[@]}")
		fi
		baseline_command+=("${source_path}")
		recursive_command+=("${source_path}")

		set +e
		"${baseline_command[@]}" >"${baseline_output}" 2>&1
		baseline_status="$?"
		"${recursive_command[@]}" >"${recursive_output}" 2>&1
		recursive_status="$?"
		set -e
		baseline_signature="$(normalize_output "${baseline_output}")"
		recursive_signature="$(normalize_output "${recursive_output}")"
		classification="match"
		if [[ "${baseline_status}" -ne "${recursive_status}" ]]; then
			classification="status-mismatch"
		elif [[ "${baseline_signature}" != "${recursive_signature}" ]]; then
			classification="signature-mismatch"
		elif [[ "${baseline_status}" -ne 0 && "${baseline_status}" -ne 42 &&
			-z "${baseline_signature}" ]]; then
			classification="unsupported-baseline"
		fi
		[[ -n "${baseline_signature}" ]] || baseline_signature="-"
		[[ -n "${recursive_signature}" ]] || recursive_signature="-"
		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"${index}" "${kind}" "${model}" "${relative_source}" \
			"${source_hash}" "${genmc_args} | ${clang_args}" \
			"${baseline_status}" "${recursive_status}" "${classification}" \
			"${baseline_signature}" "${recursive_signature}" \
			>"${parts_dir}/${index}-${model}.tsv"
		if [[ "${classification}" == "match" ]]; then
			rm -f "${baseline_output}" "${recursive_output}"
		fi
	done
	exit 0
fi

[[ "$#" -ge 4 && "$#" -le 5 ]] || usage
genmc="$1"
models_dir="$2"
tests_dir="$3"
result_file="$4"
jobs="${5:-4}"
[[ -x "${genmc}" ]] || { echo "GenMC executable is not runnable" >&2; exit 2; }
for model in sc tso pso recursive-sc recursive-tso recursive-pso; do
	[[ -f "${models_dir}/${model}.cat" ]] || {
		echo "missing CAT model: ${models_dir}/${model}.cat" >&2
		exit 2
	}
done
[[ "${jobs}" =~ ^[1-9][0-9]*$ ]] || { echo "jobs must be positive" >&2; exit 2; }

mkdir -p "$(dirname "${result_file}")"
result_file="$(cd "$(dirname "${result_file}")" && pwd)/$(basename "${result_file}")"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/genmc-caat-broad.XXXXXX")"
trap 'rm -rf "${work_dir}"' EXIT
manifest="${work_dir}/manifest.tsv"
parts_dir="${work_dir}/parts"
mkdir -p "${parts_dir}"

# Keep the exact Phase 1 corpus roots so cross-phase results are comparable.
index=0
for specification in \
	"correct:correct/litmus" \
	"correct:correct/infr" \
	"correct:correct/data-structures" \
	"wrong:wrong/safety" \
	"wrong:wrong/racy" \
	"wrong:wrong/memory"; do
	kind="${specification%%:*}"
	root="${specification#*:}"
	while IFS= read -r source_path; do
		index="$((index + 1))"
		relative_source="${source_path#${tests_dir}/}"
		printf '%s\t%s\t%s\n' "${index}" "${kind}" "${relative_source}" \
			>>"${manifest}"
	done < <(find "${tests_dir}/${root}" -path '*/variants/*' -type f \
		\( -name '*.c' -o -name '*.cpp' -o -name '*.ll' \) | LC_ALL=C sort)
done

if [[ "${index}" -lt 200 ]]; then
	echo "broad corpus contains only ${index} distinct programs" >&2
	exit 1
fi

export -f usage normalize_output
script_path="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
while IFS=$'\t' read -r case_index kind relative_source; do
	printf '%s\0%s\0%s\0' "${case_index}" "${kind}" "${relative_source}"
done <"${manifest}" |
	xargs -0 -P "${jobs}" -n 3 bash "${script_path}" --worker "${genmc}" \
		"${models_dir}" "${tests_dir}" "${parts_dir}"

{
	printf 'index\tkind\tmodel\tprogram\tsha256\targuments\tbaseline_status\trecursive_status\tclassification\tbaseline_signature\trecursive_signature\n'
	for case_index in $(seq 1 "${index}"); do
		cat "${parts_dir}/${case_index}-sc.tsv" \
			"${parts_dir}/${case_index}-tso.tsv" \
			"${parts_dir}/${case_index}-pso.tsv"
	done
} >"${result_file}"

mismatch_count="$(awk -F '\t' \
	'NR > 1 && $9 != "match" && $9 != "unsupported-baseline" {count++}
	 END {print count + 0}' "${result_file}")"
unsupported_count="$(awk -F '\t' \
	'NR > 1 && $9 == "unsupported-baseline" {count++} END {print count + 0}' \
	"${result_file}")"
matched_pairs="$(awk -F '\t' 'NR > 1 && $9 == "match" {count++} END {print count + 0}' \
	"${result_file}")"
program_count="$(wc -l <"${manifest}" | tr -d ' ')"
printf 'discovered=%s recursive-pairs=%s matches=%s mismatches=%s unsupported=%s result=%s\n' \
	"${program_count}" "$((program_count * 3))" "${matched_pairs}" \
	"${mismatch_count}" "${unsupported_count}" "${result_file}"

if [[ "${matched_pairs}" -lt 200 || "${mismatch_count}" -ne 0 ]]; then
	mismatch_dir="${result_file%.tsv}-mismatches"
	mkdir -p "${mismatch_dir}"
	find "${parts_dir}" -name '*.log' -type f -exec cp {} "${mismatch_dir}/" \;
	exit 1
fi
