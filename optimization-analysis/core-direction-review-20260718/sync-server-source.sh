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
	genmc/genmc/Execution/EventLabel.hpp
	genmc/genmc/Execution/ExecutionGraph.hpp
	genmc/genmc/Execution/ExecutionGraph.cpp
	genmc/genmc/Execution/DepExecutionGraph.hpp
	genmc/genmc/Execution/DepExecutionGraph.cpp
	genmc/genmc/Execution/Consistency/SCChecker.cpp
	genmc/genmc/Execution/Consistency/TSOChecker.cpp
	genmc/genmc/Execution/Consistency/RAChecker.cpp
	genmc/genmc/Execution/Consistency/RC11Checker.cpp
	genmc/genmc/Execution/Consistency/IMMChecker.cpp
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
	genmc/genmc/Verification/Config.hpp
	genmc/genmc/Verification/Config.cpp
	genmc/genmc/Verification/GenMCDriver.hpp
	genmc/genmc/Verification/GenMCDriver.cpp
	genmc/genmc/Verification/Revisit.hpp
	genmc/genmc/Verification/VerificationResult.hpp
	lli/main.cpp
	optimization-analysis/continuous/candidate-equivalence-quotient/formal-wrapper.sh
	optimization-analysis/core-direction-review-20260718/decisive-diagnostic-15.xml
	optimization-analysis/core-direction-review-20260718/decisive-diagnostic-283.xml
	optimization-analysis/core-direction-review-20260718/launch-primitive-fast-paired-15.sh
	optimization-analysis/core-direction-review-20260718/launch-primitive-fast-paired-283.sh
	optimization-analysis/core-direction-review-20260718/finite-symbolic-full-725.xml
	optimization-analysis/core-direction-review-20260718/launch-primitive-fast-paired-725.sh
	tests/unit/CatGraphAdapterTest.cpp
	tests/unit/CatEvaluatorTest.cpp
	tests/unit/ConfigTest.cpp
	tests/unit/CMakeLists.txt
	tests/unit/EventLabelTest.cpp
	tests/unit/RevisitTest.cpp
	tests/unit/IntervalMapTest.cpp
	tests/unit/ViewTest.cpp
	tests/cat/online-mutation-stress.sh
)

cd "$repo_root"
for file in "${files[@]}"; do
	[[ -f "$file" ]] || { echo "missing local source: $file" >&2; exit 2; }
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
