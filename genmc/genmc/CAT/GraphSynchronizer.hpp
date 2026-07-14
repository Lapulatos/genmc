/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 *
 * Apache License 2.0:
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * MIT License:
 *     https://opensource.org/licenses/MIT
 */

#ifndef GENMC_CAT_GRAPH_SYNCHRONIZER_HPP
#define GENMC_CAT_GRAPH_SYNCHRONIZER_HPP

#include "genmc/CAT/IncrementalEvaluator.hpp"
#include "genmc/CAT/StableGraphAdapter.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cat {

/** Observable transition selected for one immutable execution-graph snapshot. */
enum class GraphTransition : std::uint8_t {
	Initialize,
	Unchanged,
	Insert,
	Rollback,
	RollbackInsert,
	Rebuild
};

/** Result and fallback reason from one graph synchronization. */
struct GraphSynchronizationResult {
	GraphTransition transition{GraphTransition::Initialize};
	std::string reason;
};

/** Lifetime transition counters for diagnostics and integration tests. */
struct GraphSynchronizationStatistics {
	std::size_t initializations{};
	std::size_t unchanged{};
	std::size_t insertions{};
	std::size_t rollbacks{};
	std::size_t rollbackInsertions{};
	std::size_t rebuilds{};
	std::size_t evictedCheckpoints{};
	std::size_t oracleChecks{};
};

/**
 * Classify stable graph snapshots and drive an IncrementalCaatEvaluator.
 *
 * The synchronizer owns stable event identities and a bounded list of semantic
 * predecessor checkpoints. It first tries unchanged/insertion, then exact or
 * subset checkpoint restoration, and finally the Phase 2 rebuild oracle. One
 * instance is worker-local and not thread-safe.
 */
class GraphSynchronizer {
public:
	/**
	 * Bind one evaluator and cap retained full-state checkpoints.
	 *
	 * @param evaluator Mutable worker-local evaluator, borrowed for this lifetime.
	 * @param checkpointLimit Maximum retained semantic predecessors; at least one.
	 * @param oracleInterval Check every N queries against Phase 2, or zero to disable.
	 */
	explicit GraphSynchronizer(IncrementalCaatEvaluator &evaluator,
				   std::size_t checkpointLimit = 32,
				   std::size_t oracleInterval = 0);

	/** Materialize and synchronize one current graph snapshot. */
	[[nodiscard]] auto synchronize(const GraphAdapter &snapshot) -> GraphSynchronizationResult;

	/** Return the persistent event universe used by the evaluator. */
	[[nodiscard]] auto stableAdapter() const -> const StableGraphAdapter & { return stable_; }
	/** Return the number of retained semantic predecessor states. */
	[[nodiscard]] auto retainedCheckpoints() const -> std::size_t { return history_.size(); }
	/** Return cumulative observable transition and eviction counters. */
	[[nodiscard]] auto statistics() const -> const GraphSynchronizationStatistics &
	{
		return statistics_;
	}

private:
	struct HistoryEntry {
		std::size_t eventCount{};
		BaseValues base;
		IncrementalCheckpoint checkpoint;
	};

	void retainCurrent();
	void clearHistory();
	void verifyCurrent(GraphTransition transition);

	IncrementalCaatEvaluator *evaluator_{};
	StableGraphAdapter stable_;
	std::vector<HistoryEntry> history_;
	std::size_t checkpointLimit_{};
	std::size_t oracleInterval_{};
	std::size_t queryCount_{};
	GraphSynchronizationStatistics statistics_;
};

} /* namespace cat */

#endif /* GENMC_CAT_GRAPH_SYNCHRONIZER_HPP */
