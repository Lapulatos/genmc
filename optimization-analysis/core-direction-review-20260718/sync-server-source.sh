#!/bin/bash
set -euo pipefail

# Single deployment entry point for the current CAAT diagnostic patch.  Paths are
# anchored below the repository root so rsync --relative preserves every parent
# directory instead of flattening multiple source files into one remote folder.
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
remote_host=server@frp-arm.com
remote_port=36722
remote_root="${GENMC_SYNC_REMOTE_ROOT:-/data3/sujie/svcomp2026-caat/source/genmc}"
case "$remote_root" in
	/data3/sujie/svcomp2026-caat/source/genmc|/data3/sujie/experiments/caat-optimization/*/source-*) ;;
	*) echo "refusing unsupported remote source root: $remote_root" >&2; exit 2 ;;
esac
files=(
	genmc/CMakeLists.txt
	genmc/genmc/Execution/EventLabel.hpp
	genmc/genmc/Execution/ExecutionGraph.hpp
	genmc/genmc/Execution/ExecutionGraph.cpp
	genmc/genmc/Execution/DepExecutionGraph.hpp
	genmc/genmc/Execution/DepExecutionGraph.cpp
	genmc/genmc/Execution/Consistency/SCChecker.cpp
	genmc/genmc/Execution/Consistency/ConsistencyChecker.hpp
	genmc/genmc/Execution/Consistency/TSOChecker.cpp
	genmc/genmc/Execution/Consistency/RAChecker.cpp
	genmc/genmc/Execution/Consistency/RC11Checker.cpp
	genmc/genmc/Execution/Consistency/IMMChecker.cpp
	genmc/genmc/Support/ThreadPool.hpp
	genmc/genmc/Support/ThreadPool.cpp
	genmc/genmc/CAT/StableGraphAdapter.hpp
	genmc/genmc/CAT/StableGraphAdapter.cpp
	genmc/genmc/CAT/CaatEvaluator.hpp
	genmc/genmc/CAT/CaatEvaluator.cpp
	genmc/genmc/CAT/Value.hpp
	genmc/genmc/CAT/Value.cpp
	genmc/genmc/CAT/IncrementalEvaluator.hpp
	genmc/genmc/CAT/IncrementalEvaluator.cpp
	genmc/genmc/CAT/LazyCycle.hpp
	genmc/genmc/CAT/LazyCycle.cpp
	genmc/genmc/CAT/GraphSynchronizer.hpp
	genmc/genmc/CAT/GraphSynchronizer.cpp
	genmc/genmc/Execution/Consistency/CATChecker.cpp
	genmc/genmc/Execution/Consistency/CATChecker.hpp
	genmc/genmc/Verification/Config.hpp
	genmc/genmc/Verification/Config.cpp
	genmc/genmc/Verification/GenMCDriver.hpp
	genmc/genmc/Verification/GenMCDriver.cpp
	genmc/genmc/Verification/CATDecisionState.hpp
	genmc/genmc/Verification/CATDecisionState.cpp
	genmc/genmc/Verification/Revisit.hpp
	genmc/genmc/Verification/WorkList.hpp
	genmc/genmc/Verification/RegionalRvfTransaction.hpp
	genmc/genmc/Verification/RegionalRvfTransaction.cpp
	genmc/genmc/Verification/SCReadsValueFromFrame.hpp
	genmc/genmc/Verification/SCExecutionGraphAdapter.hpp
	genmc/genmc/Verification/SCExecutionGraphAdapter.cpp
	genmc/genmc/Verification/VerificationResult.hpp
	lli/main.cpp
	passes/passes/LLIConfig.hpp
	optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh
	optimization-analysis/continuous/candidate-equivalence-quotient/run-generated-state-oracle.py
	optimization-analysis/core-direction-review-20260718/decisive-diagnostic-15.xml
	optimization-analysis/core-direction-review-20260718/decisive-diagnostic-283.xml
	optimization-analysis/core-direction-review-20260718/launch-primitive-fast-paired-15.sh
	optimization-analysis/core-direction-review-20260718/launch-primitive-fast-paired-283.sh
	optimization-analysis/core-direction-review-20260718/finite-symbolic-full-725.xml
	optimization-analysis/core-direction-review-20260718/regional-na-load-16.xml
	optimization-analysis/core-direction-review-20260718/regional-rvf-activation-16.xml
	optimization-analysis/core-direction-review-20260718/regional-rvf-gate-census-725.xml
	optimization-analysis/core-direction-review-20260718/launch-server-regional-rvf-gate-census.sh
	optimization-analysis/core-direction-review-20260718/analyze-regional-rvf-gate-census.py
	optimization-analysis/core-direction-review-20260718/regional-rvf-paired-51.xml
	optimization-analysis/core-direction-review-20260718/launch-server-regional-rvf-paired-51.sh
	optimization-analysis/core-direction-review-20260718/launch-server-regional-rvf-opportunity-51.sh
	optimization-analysis/core-direction-review-20260718/analyze-regional-rvf-paired.py
	optimization-analysis/core-direction-review-20260718/regional-rvf-paired-725.xml
	optimization-analysis/core-direction-review-20260718/launch-server-regional-rvf-paired-725.sh
	optimization-analysis/core-direction-review-20260718/p1-cumulative-paired-725.xml
	optimization-analysis/core-direction-review-20260718/launch-server-p1-cumulative-paired-725.sh
	optimization-analysis/core-direction-review-20260718/analyze-p1-cumulative-paired.py
	optimization-analysis/core-direction-review-20260718/backjump-census-15.xml
	optimization-analysis/core-direction-review-20260718/launch-server-backjump-census-15.sh
	optimization-analysis/core-direction-review-20260718/analyze-backjump-census.py
	optimization-analysis/core-direction-review-20260718/p2-backjump-gate-a1-report-20260720.md
	optimization-analysis/core-direction-review-20260718/launch-primitive-fast-paired-725.sh
	tests/unit/CatGraphAdapterTest.cpp
	tests/unit/CATDecisionStateTest.cpp
	tests/unit/CatEvaluatorTest.cpp
	tests/unit/ConfigTest.cpp
	tests/unit/CMakeLists.txt
	tests/unit/EventLabelTest.cpp
	tests/unit/RevisitTest.cpp
	tests/unit/RegionalRvfTransactionTest.cpp
	tests/unit/SCExecutionGraphAdapterTest.cpp
	tests/unit/IntervalMapTest.cpp
	tests/unit/ViewTest.cpp
	tests/cat/online-mutation-stress.sh
	tests/cat/sc-rvf-assume-outcomes.sh
	tests/cat/sc-rvf-regional-na-load.sh
	tests/cat/programs/rvf-regional-na-load.c
	tests/cat/sc-rvf-regional-loop.sh
	tests/cat/programs/rvf-regional-loop.inc
	tests/cat/programs/rvf-regional-loop-outcome-0.c
	tests/cat/programs/rvf-regional-loop-outcome-1.c
	tests/cat/programs/rvf-regional-loop-safe.c
	tests/cat/programs/rvf-repeated-future-write-loop.c
	tests/cat/programs/rvf-repeated-future-write-unrolled.c
	tests/CMakeLists.txt
)

cd "$repo_root"
declare -A manifest_paths=()
for file in "${files[@]}"; do
	[[ -f "$file" ]] || { echo "missing local source: $file" >&2; exit 2; }
	manifest_paths["$file"]=1
done

# A hash comparison cannot detect a modified dependency that was never listed. Refuse
# deployment whenever any existing source/test path in the worktree falls outside this
# explicit manifest; update the manifest first and let the same run hash-check it.
mapfile -t modified_code_paths < <(
	git status --porcelain=v1 --untracked-files=all | sed 's/^...//' |
		grep -E '^(genmc|lli|passes|tests)/' || true
)
for file in "${modified_code_paths[@]}"; do
	[[ ! -e "$file" || -n "${manifest_paths[$file]:-}" ]] || {
		echo "modified code path missing from sync manifest: $file" >&2
		exit 2
	}
done

rsync_args=(-avR -e "ssh -p $remote_port")
printf 'Dry-run deployment paths:\n'
rsync -anR -e "ssh -p $remote_port" "${files[@]/#/./}" "$remote_host:$remote_root/"
rsync "${rsync_args[@]}" "${files[@]/#/./}" "$remote_host:$remote_root/"

local_hashes="$(mktemp)"
remote_hashes="$(mktemp)"
trap 'rm -f "$local_hashes" "$remote_hashes"' EXIT
sha256sum "${files[@]}" >"$local_hashes"
ssh -p "$remote_port" "$remote_host" \
	"cd '$remote_root' && sha256sum ${files[*]}" >"$remote_hashes"
diff -u "$local_hashes" "$remote_hashes"

# Fail before any build if this exact manifest was accidentally flattened at
# genmc/genmc/.  The script never deletes remote files.
flat_command=true
for file in "${files[@]}"; do
	flat_command+=" && test ! -e 'genmc/genmc/${file##*/}'"
done
if ssh -p "$remote_port" "$remote_host" "cd '$remote_root' && $flat_command"; then
	printf 'Verified %s files and no flattened duplicates.\n' "${#files[@]}"
else
	echo "flattened duplicate detected; refusing deployment handoff" >&2
	exit 3
fi
