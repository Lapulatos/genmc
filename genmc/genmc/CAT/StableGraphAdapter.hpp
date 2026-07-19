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

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace cat {

/** Persistent identity of a real label or one address-specific initial write. */
using StableEventKey = std::variant<Event, SAddr>;

/** One graph snapshot remapped into a worker-lifetime, never-reused ID space. */
struct StableGraphSnapshot {
	std::size_t eventCount{};
	/** Number of currently active IDs; eventCount also includes retained holes. */
	std::size_t activeEventCount{};
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
	explicit StableGraphAdapter(std::vector<std::string> requiredPrimitives = {},
				    bool fastPrimitiveBuild = false,
				    bool fastCoherenceBuild = false,
				    bool fastDescriptorBuild = false,
				    bool fastDescriptorReuse = false);
	/**
	 * Materialize directly from GenMC into the persistent stable-ID universe.
	 *
	 * This avoids constructing a dense GraphAdapter and then remapping every
	 * primitive. Stable IDs are still retained across cuts and revisits.
	 */
	[[nodiscard]] auto materialize(const ExecutionGraph &graph) -> StableGraphSnapshot;
	/**
	 * Reuse the previous exact primitive snapshot when a linear semantic scan
	 * proves that every primitive determinant is unchanged.
	 *
	 * @param verifyHit Independently rematerialize and compare every cache hit.
	 */
	[[nodiscard]] auto materializeCached(const ExecutionGraph &graph, bool verifyHit = false)
		-> StableGraphSnapshot;

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
	/** Return the persistent key assigned to @p id, if it exists. */
	[[nodiscard]] auto key(std::size_t id) const -> const StableEventKey *
	{
		return id < keys_.size() ? &keys_[id] : nullptr;
	}
	[[nodiscard]] auto cacheHits() const -> std::size_t { return cacheHits_; }
	[[nodiscard]] auto cacheMisses() const -> std::size_t { return cacheMisses_; }
	[[nodiscard]] auto cacheOracleChecks() const -> std::size_t { return cacheOracleChecks_; }
	[[nodiscard]] auto scanNanoseconds() const -> std::uint64_t { return scanNanoseconds_; }
	[[nodiscard]] auto labelNanoseconds() const -> std::uint64_t { return labelNanoseconds_; }
	[[nodiscard]] auto structuralNanoseconds() const -> std::uint64_t
	{
		return structuralNanoseconds_;
	}
	[[nodiscard]] auto coherenceNanoseconds() const -> std::uint64_t
	{
		return coherenceNanoseconds_;
	}
	[[nodiscard]] auto coherenceEdgeNanoseconds() const -> std::uint64_t
	{
		return coherenceEdgeNanoseconds_;
	}
	[[nodiscard]] auto fromReadNanoseconds() const -> std::uint64_t
	{
		return fromReadNanoseconds_;
	}
	[[nodiscard]] auto relationPackNanoseconds() const -> std::uint64_t
	{
		return relationPackNanoseconds_;
	}
	[[nodiscard]] auto assemblyNanoseconds() const -> std::uint64_t
	{
		return assemblyNanoseconds_;
	}

private:
	struct EventDescriptor {
		/* Transient pointer for same-call materialization; excluded from equality. */
		const EventLabel *label{};
		Event position;
		int thread{};
		int index{};
		std::uint8_t classes{};
		std::optional<SAddr> address;
		std::optional<StableEventKey> readsFrom;
		std::optional<Event> rmwTarget;
		std::optional<Event> createSource;
		std::optional<Event> joinTarget;

		auto operator==(const EventDescriptor &other) const -> bool
		{
			return position == other.position && thread == other.thread &&
			       index == other.index && classes == other.classes &&
			       address == other.address && readsFrom == other.readsFrom &&
			       rmwTarget == other.rmwTarget && createSource == other.createSource &&
			       joinTarget == other.joinTarget;
		}
	};
	struct GraphDescriptor {
		std::vector<EventDescriptor> events;
		std::vector<std::pair<SAddr, std::vector<Event>>> coherence;

		auto operator==(const GraphDescriptor &) const -> bool = default;
	};

	[[nodiscard]] auto describe(const ExecutionGraph &graph) const -> GraphDescriptor;
	void describeInto(const ExecutionGraph &graph, GraphDescriptor &result) const;
	[[nodiscard]] auto materializeImpl(const ExecutionGraph &graph,
					   const GraphDescriptor *prepared)
		-> StableGraphSnapshot;
	[[nodiscard]] auto required(std::string_view name) const -> bool;
	std::map<StableEventKey, std::size_t> ids_;
	std::vector<StableEventKey> keys_;
	std::set<std::string, std::less<>> requiredPrimitives_;
	bool buildAllPrimitives_{};
	bool fastPrimitiveBuild_{};
	bool fastCoherenceBuild_{};
	bool fastDescriptorBuild_{};
	bool fastDescriptorReuse_{};
	std::optional<GraphDescriptor> cachedDescriptor_;
	GraphDescriptor scratchDescriptor_;
	std::optional<StableGraphSnapshot> cachedSnapshot_;
	std::size_t cacheHits_{};
	std::size_t cacheMisses_{};
	std::size_t cacheOracleChecks_{};
	std::uint64_t scanNanoseconds_{};
	std::uint64_t labelNanoseconds_{};
	std::uint64_t structuralNanoseconds_{};
	std::uint64_t coherenceNanoseconds_{};
	std::uint64_t coherenceEdgeNanoseconds_{};
	std::uint64_t fromReadNanoseconds_{};
	std::uint64_t relationPackNanoseconds_{};
	std::uint64_t assemblyNanoseconds_{};
};

} /* namespace cat */

#endif /* GENMC_CAT_STABLE_GRAPH_ADAPTER_HPP */
