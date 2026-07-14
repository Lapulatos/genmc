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

#ifndef GENMC_CAT_STABLE_GRAPH_ADAPTER_HPP
#define GENMC_CAT_STABLE_GRAPH_ADAPTER_HPP

#include "genmc/CAT/GraphAdapter.hpp"

#include <map>
#include <variant>
#include <vector>

namespace cat {

/** Persistent identity of a real label or one address-specific initial write. */
using StableEventKey = std::variant<Event, SAddr>;

/** One graph snapshot remapped into a worker-lifetime, never-reused ID space. */
struct StableGraphSnapshot {
	std::size_t eventCount{};
	BaseValues base;
	std::vector<GraphAdapter::EventId> denseToStable;
};

/**
 * Worker-local remapper from ephemeral GraphAdapter IDs to stable CAAT IDs.
 *
 * Real labels are keyed by EventPos and virtual initial writes by address. IDs
 * are never reused while this object lives. Consequently a cut may leave holes;
 * the `_` base set identifies active IDs and `id` is its identity relation.
 * The class owns no graph or label and is not thread-safe.
 */
class StableGraphAdapter {
public:
	/**
	 * Remap a complete immutable dense snapshot into the persistent universe.
	 *
	 * @param snapshot Current GraphAdapter whose labels outlive this call.
	 * @return Owned base values and dense-to-stable witness mapping.
	 * @complexity O(predicates * dense-universe^2) in the current packed format.
	 */
	[[nodiscard]] auto materialize(const GraphAdapter &snapshot) -> StableGraphSnapshot;

	/** Return the number of IDs ever assigned by this worker. */
	[[nodiscard]] auto universeSize() const -> std::size_t { return keys_.size(); }
	/** Return the stable ID already assigned to @p key, if any. */
	[[nodiscard]] auto id(const StableEventKey &key) const -> std::optional<std::size_t>;

private:
	std::map<StableEventKey, std::size_t> ids_;
	std::vector<StableEventKey> keys_;
};

} /* namespace cat */

#endif /* GENMC_CAT_STABLE_GRAPH_ADAPTER_HPP */
