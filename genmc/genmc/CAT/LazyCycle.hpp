/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#ifndef GENMC_CAT_LAZY_CYCLE_HPP
#define GENMC_CAT_LAZY_CYCLE_HPP

#include "genmc/CAT/Evaluator.hpp"
#include "genmc/CAT/ConflictCore.hpp"
#include "genmc/CAT/Normalized.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace cat {

/** Work counters for one or more exact lazy cycle checks. */
struct LazyCycleStatistics {
	std::size_t checks{};
	std::size_t emittedCandidates{};
	/** Base-relation cursor entries visited before derived filters/deduplication. */
	std::size_t baseCandidates{};
	std::size_t depthFallbacks{};
};

/**
 * Find one deterministic cycle in an analyzer-certified positive relation DAG.
 *
 * The root relation is enumerated extensionally from base values without
 * publishing any derived relation. The returned witness repeats its start;
 * an empty result means that the exact root relation is acyclic.
 */
[[nodiscard]] auto findLazyCycle(const NormalizedModel &model, PredicateId root,
				 const std::vector<std::optional<Value>> &values,
				 std::size_t eventCount, LazyCycleStatistics *statistics = nullptr,
				 Relation *materialized = nullptr)
	-> std::vector<std::size_t>;

/** Enumerate the exact strict root-reachable set from or to one focus event. */
[[nodiscard]] auto findLazyReach(const NormalizedModel &model, PredicateId root,
				 const std::vector<std::optional<Value>> &values,
				 std::size_t eventCount, std::size_t focus, bool reverse,
				 LazyCycleStatistics *statistics = nullptr) -> EventSet;

/** Return one deterministic positive base derivation of an exact root edge. */
[[nodiscard]] auto deriveLazyEdge(const NormalizedModel &model, PredicateId root,
				  const std::vector<std::optional<Value>> &values,
				  std::size_t eventCount, std::size_t from,
				  std::size_t to)
	-> std::optional<std::vector<ConflictLiteral>>;


} /* namespace cat */

#endif /* GENMC_CAT_LAZY_CYCLE_HPP */
