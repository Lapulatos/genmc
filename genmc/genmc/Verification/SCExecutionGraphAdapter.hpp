/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#ifndef GENMC_SC_EXECUTION_GRAPH_ADAPTER_HPP
#define GENMC_SC_EXECUTION_GRAPH_ADAPTER_HPP

#include "genmc/CAT/StableGraphAdapter.hpp"
#include "genmc/Verification/SCGoodWritesSolver.hpp"

#include <map>
#include <string>
#include <vector>

class ExecutionGraph;

namespace genmc::rvf {

/** Allowed source sets indexed by stable GenMC event identities. */
using StableGoodWrites = std::map<cat::StableEventKey, std::vector<cat::StableEventKey>>;

/** One graph prefix translated to the independent VerifySC representation. */
struct GraphProblem {
	Problem problem{};
	std::vector<cat::StableEventKey> denseToStable{};
	std::string error{};
};

/** Build a proper SC event set without invoking a built-in consistency checker. */
[[nodiscard]] auto buildGraphProblem(const ExecutionGraph &graph,
				     const StableGoodWrites &goodWrites) -> GraphProblem;

/** Apply one successful VerifySC witness's RF and per-location write order to a graph clone. */
[[nodiscard]] auto applyGraphWitness(ExecutionGraph &graph, const GraphProblem &problem,
				     const Result &witness) -> std::string;

} /* namespace genmc::rvf */

#endif /* GENMC_SC_EXECUTION_GRAPH_ADAPTER_HPP */
