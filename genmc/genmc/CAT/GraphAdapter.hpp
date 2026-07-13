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

#ifndef GENMC_CAT_GRAPH_ADAPTER_HPP
#define GENMC_CAT_GRAPH_ADAPTER_HPP

#include "genmc/CAT/Evaluator.hpp"
#include "genmc/Execution/Event.hpp"
#include "genmc/Support/SAddr.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class EventLabel;
class ExecutionGraph;

namespace cat {

/**
 * Immutable CAT view of one GenMC execution-graph snapshot.
 *
 * The adapter does not own or modify the graph. Real-label IDs follow GenMC's
 * insertion order after EmptyLabel/InitLabel are removed, then per-location
 * virtual initial writes follow in sorted-address order. Virtual events retain
 * InitLabel plus their address for witness mapping. The referenced graph and
 * labels must outlive this object.
 *
 * GenMC exposes no mutation generation counter. Consequently every graph
 * mutation, including an rf/co edge-only change, invalidates the snapshot and
 * requires rebuilding the adapter. structurallyCurrent() is only a debugging
 * aid for detecting label insertion/removal/replacement; it cannot prove that
 * relation edges remained unchanged.
 */
class GraphAdapter {
public:
	using EventId = std::size_t;

	/**
	 * Materialize all Phase 1 CAT primitives from @p graph.
	 *
	 * @param graph Complete or prefix ExecutionGraph to expose read-only.
	 * @complexity O(events^2 + coherence-pairs) time and O(events^2) packed storage.
	 */
	explicit GraphAdapter(const ExecutionGraph &graph);

	/** Return the fixed number of exposed events. */
	[[nodiscard]] auto eventCount() const -> std::size_t { return labels_.size(); }
	/** Return the graph label represented by one dense ID; virtual IW IDs return InitLabel. */
	[[nodiscard]] auto label(EventId id) const -> const EventLabel * { return labels_.at(id); }
	/** Return the dense ID for a real exposed graph position; Init has one ID per location. */
	[[nodiscard]] auto id(Event event) const -> std::optional<EventId>;
	/** Return the virtual initial-write ID for @p location, if the graph tracks it. */
	[[nodiscard]] auto initialId(SAddr location) const -> std::optional<EventId>;
	/** Return the address attached to a virtual initial-write ID, if any. */
	[[nodiscard]] auto initialLocation(EventId id) const -> std::optional<SAddr>
	{
		return initialLocations_.at(id);
	}
	/** Return every materialized primitive value for Evaluator. */
	[[nodiscard]] auto baseValues() const -> const BaseValues & { return values_; }

	/**
	 * Best-effort check that the graph still has the same exposed labels in the
	 * same order. Edge-only changes are deliberately not detectable here.
	 */
	[[nodiscard]] auto structurallyCurrent() const -> bool;

	/**
	 * Validate adapter endpoints and GenMC/CAT relation invariants.
	 *
	 * @return Human-readable errors; an empty vector means validation passed.
	 */
	[[nodiscard]] auto validate() const -> std::vector<std::string>;

private:
	const ExecutionGraph *graph_{};
	std::vector<const EventLabel *> labels_;
	std::vector<std::optional<SAddr>> initialLocations_;
	std::unordered_map<Event, EventId> ids_;
	std::unordered_map<SAddr, EventId> initialIds_;
	BaseValues values_;
};

} /* namespace cat */

#endif /* GENMC_CAT_GRAPH_ADAPTER_HPP */
