#!/bin/bash

# Broad SC/TSO differential validation for the generic CAT checker.
#
# This program is dual-licensed under the Apache License 2.0 and the MIT License.
# You may choose to use, distribute, or modify this software under either license.
#
# Apache License 2.0:
#     http://www.apache.org/licenses/LICENSE-2.0
#
# MIT License:
#     https://opensource.org/licenses/MIT

set -euo pipefail

usage() {
	echo "usage: $0 <genmc> <models-dir> <tests-dir> <result.tsv> [jobs]" >&2
	exit 2
}

if [[ "${1:-}" == "--worker" ]]; then
	shift
	[[ "$#" -eq 8 ]] || usage
	genmc="$1"
	models_dir="$2"
	tests_dir="$3"
	parts_dir="$4"
	index="$5"
	kind="$6"
	relative_source="$7"
	expected_sc="$8"
	[[ "${expected_sc}" == "-" ]] && expected_sc=""

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

	# Normalize only observable verification semantics. Version banners, graph
	# traces, absolute paths, and wall-clock time are intentionally excluded.
	normalize_output() {
		sed -n \
			-e '/^No errors were detected\.$/p' \
			-e '/^Error: /p' \
			-e '/^[Ww][Aa][Rr][Nn][Ii][Nn][Gg]: /p' \
			-e '/^Number of complete executions explored: /p' \
			-e '/^Number of blocked executions/ p' \
			-e '/^Number of executions exceeding bound: /p' "$1" |
			paste -sd ';' -
	}

	for model in sc tso; do
		builtin_output="${parts_dir}/${index}-${model}-builtin.log"
		cat_output="${parts_dir}/${index}-${model}-cat.log"
		builtin_command=("${genmc}" "--${model}" --disable-estimation \
			--disable-mm-detector)
		cat_command=("${genmc}" "--model-file=${models_dir}/${model}.cat" \
			--disable-estimation --disable-mm-detector)
		if [[ "${genmc_args}" =~ [^[:space:]] ]]; then
			read -r -a parsed_genmc_args <<<"${genmc_args}"
			builtin_command+=("${parsed_genmc_args[@]}")
			cat_command+=("${parsed_genmc_args[@]}")
		fi
		builtin_command+=(--)
		cat_command+=(--)
		if [[ "${clang_args}" =~ [^[:space:]] ]]; then
			read -r -a parsed_clang_args <<<"${clang_args}"
			builtin_command+=("${parsed_clang_args[@]}")
			cat_command+=("${parsed_clang_args[@]}")
		fi
		builtin_command+=("${source_path}")
		cat_command+=("${source_path}")
		set +e
		"${builtin_command[@]}" >"${builtin_output}" 2>&1
		builtin_status="$?"
		"${cat_command[@]}" >"${cat_output}" 2>&1
		cat_status="$?"
		set -e

		builtin_signature="$(normalize_output "${builtin_output}")"
		cat_signature="$(normalize_output "${cat_output}")"
		classification="match"
		if [[ "${builtin_status}" -ne "${cat_status}" ]]; then
			classification="status-mismatch"
		elif [[ "${builtin_signature}" != "${cat_signature}" ]]; then
			classification="signature-mismatch"
		fi

		# Repository correct/wrong categories are not model-independent: a race
		# under RC11 may be absent under SC, while TSO can expose a race in a test
		# whose stored expectation is SC-only. Apply only the exact SC expectation.
		# A shared pre-verification failure has no memory-model evidence and is
		# reported separately rather than counted as a pass or mismatch.
		if [[ "${classification}" == "match" && "${builtin_status}" -ne 0 &&
		      "${builtin_status}" -ne 42 && -z "${builtin_signature}" ]]; then
			classification="unsupported-baseline"
		elif [[ "${classification}" == "match" && "${model}" == "sc" &&
		        "${kind}" == "correct" && -n "${expected_sc}" ]]; then
			builtin_count="$(sed -n 's/^Number of complete executions explored: //p' \
				"${builtin_output}")"
			if [[ "${builtin_status}" -ne 0 || "${builtin_count}" != "${expected_sc}" ]]; then
				classification="oracle-invalid"
			fi
		fi

		# Keep the TSV rectangular without trailing empty fields, so the evidence
		# also passes the repository's whitespace checks.
		[[ -n "${builtin_signature}" ]] || builtin_signature="-"
		[[ -n "${cat_signature}" ]] || cat_signature="-"

		printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
			"${index}" "${kind}" "${model}" "${relative_source}" \
			"${source_hash}" "${genmc_args} | ${clang_args}" "${expected_sc}" \
			"${builtin_status}" "${cat_status}" "${classification}" \
			"${builtin_signature}" "${cat_signature}" \
			>"${parts_dir}/${index}-${model}.tsv"

		if [[ "${classification}" == "match" ]]; then
			rm -f "${builtin_output}" "${cat_output}"
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

[[ -x "${genmc}" ]] || { echo "GenMC executable is not runnable: ${genmc}" >&2; exit 2; }
for model in sc tso; do
	[[ -f "${models_dir}/${model}.cat" ]] || {
		echo "missing CAT model: ${models_dir}/${model}.cat" >&2
		exit 2
	}
done
[[ "${jobs}" =~ ^[1-9][0-9]*$ ]] || { echo "jobs must be a positive integer" >&2; exit 2; }

mkdir -p "$(dirname "${result_file}")"
result_file="$(cd "$(dirname "${result_file}")" && pwd)/$(basename "${result_file}")"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/genmc-cat-broad.XXXXXX")"
trap 'rm -rf "${work_dir}"' EXIT
manifest="${work_dir}/manifest.tsv"
parts_dir="${work_dir}/parts"
mkdir -p "${parts_dir}"

# These roots are deliberately frozen in the validation plan. Sorting makes
# indexes and result review stable across machines at the same Git commit.
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
		case_dir="$(dirname "$(dirname "${source_path}")")"
		expected_file="${case_dir}/expected.sc.mo.in"
		expected_sc=""
		if [[ "${kind}" == "correct" && -f "${expected_file}" ]]; then
			expected_sc="$(sed -n '/[^[:space:]]/ {p; q;}' "${expected_file}")"
		fi
		[[ -n "${expected_sc}" ]] || expected_sc="-"
		printf '%s\t%s\t%s\t%s\n' "${index}" "${kind}" \
			"${relative_source}" "${expected_sc}" >>"${manifest}"
	done < <(find "${tests_dir}/${root}" -path '*/variants/*' -type f \
		\( -name '*.c' -o -name '*.cpp' -o -name '*.ll' \) | LC_ALL=C sort)
done

if [[ "${index}" -lt 200 ]]; then
	echo "broad corpus contains only ${index} distinct programs" >&2
	exit 1
fi

export -f usage
script_path="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
while IFS=$'\t' read -r case_index kind relative_source expected_sc; do
	printf '%s\0%s\0%s\0%s\0' "${case_index}" "${kind}" "${relative_source}" \
		"${expected_sc}"
done <"${manifest}" |
	xargs -0 -P "${jobs}" -n 4 bash "${script_path}" --worker "${genmc}" \
		"${models_dir}" "${tests_dir}" "${parts_dir}"

{
	printf 'index\tkind\tmodel\tprogram\tsha256\targuments\texpected_sc\tbuiltin_status\tcat_status\tclassification\tbuiltin_signature\tcat_signature\n'
	for case_index in $(seq 1 "${index}"); do
		cat "${parts_dir}/${case_index}-sc.tsv" "${parts_dir}/${case_index}-tso.tsv"
	done
} >"${result_file}"

mismatch_count="$(awk -F '\t' \
	'NR > 1 && $10 != "match" && $10 != "unsupported-baseline" {count++} \
	 END {print count + 0}' "${result_file}")"
unsupported_count="$(awk -F '\t' \
	'NR > 1 && $10 == "unsupported-baseline" {count++} END {print count + 0}' \
	"${result_file}")"
validated_programs="$(awk -F '\t' \
	'NR > 1 && $10 == "match" {seen[$4] = 1} END {for (program in seen) count++; print count + 0}' \
	"${result_file}")"
program_count="$(wc -l <"${manifest}" | tr -d ' ')"
printf 'discovered=%s validated=%s model-pairs=%s mismatches=%s unsupported=%s result=%s\n' \
	"${program_count}" "${validated_programs}" "$((program_count * 2))" \
	"${mismatch_count}" "${unsupported_count}" "${result_file}"

if [[ "${validated_programs}" -lt 200 ]]; then
	echo "only ${validated_programs} programs produced valid differential evidence" >&2
	exit 1
fi

if [[ "${mismatch_count}" -ne 0 ]]; then
	mismatch_dir="${result_file%.tsv}-mismatches"
	mkdir -p "${mismatch_dir}"
	find "${parts_dir}" -name '*.log' -type f -exec cp {} "${mismatch_dir}/" \;
	exit 1
fi
