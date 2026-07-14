#!/bin/bash

# GenMC -- Generic Model Checking.
#
# This project is dual-licensed under the Apache License 2.0 and the MIT License.
# You may choose to use, distribute, or modify this software under either license.

set -euo pipefail

genmc="$1"
model_root="$2"
source_root="$3"

# These programs force alternative rf/co choices, RMW, lifecycle edges, dynamic
# allocation, graph cuts and repeated data-structure exploration branches.
programs=(
	"correct/litmus/RMWFix/variants/rmwfix0.c"
	"correct/litmus/W+JW/variants/W+JW1.c"
	"correct/data-structures/ms-queue-dynamic/variants/main0.c"
	"correct/data-structures/treiber-stack-dynamic/variants/main0.c"
	"correct/data-structures/fcombiner-async/variants/main0.c"
	"wrong/memory/malloc-not-hb/variants/malloc-not-hb0.c"
)

rows=0
oracle_checks=0
for model in sc tso pso; do
	for workers in 1 2; do
		for relative in "${programs[@]}"; do
			# GenMC's existing parallel replay scheduler asserts for the TSO-hosted
			# fcombiner graph before CAT checking. Single-worker coverage retains
			# this mutation-heavy case; other fixtures cover two-worker checking.
			if [[ "${workers}" -eq 2 &&
			      "${relative}" == "correct/data-structures/fcombiner-async/variants/main0.c" ]]; then
				continue
			fi
			set +e
			output="$("${genmc}" \
				--model-file="${model_root}/recursive-${model}.cat" \
				--cat-stats --cat-oracle --disable-estimation \
				--disable-mm-detector --nthreads="${workers}" \
				"${source_root}/${relative}" 2>&1)"
			status=$?
			set -e
			if [[ "${status}" -ne 0 && "${status}" -ne 42 ]]; then
				echo "Unexpected status ${status}: ${model}/${workers}/${relative}" >&2
				echo "${output}" >&2
				exit 1
			fi
			if grep -Fq "CAAT oracle mismatch:" <<<"${output}"; then
				echo "Oracle mismatch: ${model}/${workers}/${relative}" >&2
				echo "${output}" >&2
				exit 1
			fi
			checks="$(sed -n 's/.* oracle=\([0-9][0-9]*\).*/\1/p' \
				<<<"${output}" | awk '{sum += $1} END {print sum + 0}')"
			if [[ "${checks}" -eq 0 ]]; then
				echo "Oracle was not exercised: ${model}/${workers}/${relative}" >&2
				exit 1
			fi
			oracle_checks="$((oracle_checks + checks))"
			rows="$((rows + 1))"
		done
	done
done

# Fixed seeds make randomized branch order reproducible while exercising a
# different sequence of retained checkpoints than exhaustive left-to-right mode.
for model in sc tso pso; do
	for seed in 17 29; do
		output="$("${genmc}" --model-file="${model_root}/recursive-${model}.cat" \
			--cat-stats --cat-oracle --disable-estimation --disable-mm-detector \
			--mode=random --random-budget=100 --schedule-policy=wfr \
			--schedule-seed="${seed}" --nthreads=1 \
			"${source_root}/correct/data-structures/treiber-stack-dynamic/variants/main0.c" \
			2>&1)"
	if grep -Fq "CAAT oracle mismatch:" <<<"${output}"; then
		echo "Randomized oracle mismatch: ${model}/seed-${seed}" >&2
		echo "${output}" >&2
		exit 1
	fi
	checks="$(sed -n 's/.* oracle=\([0-9][0-9]*\).*/\1/p' <<<"${output}" | \
		awk '{sum += $1} END {print sum + 0}')"
	if [[ "${checks}" -eq 0 ]]; then
		echo "Randomized oracle was not exercised: ${model}/seed-${seed}" >&2
		exit 1
	fi
	oracle_checks="$((oracle_checks + checks))"
	rows="$((rows + 1))"
done
done

printf 'online-mutation-rows=%s oracle-checks=%s\n' "${rows}" "${oracle_checks}"
