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

#ifndef GENMC_CAT_INCREMENTAL_EVALUATOR_HPP
#define GENMC_CAT_INCREMENTAL_EVALUATOR_HPP

#include "genmc/CAT/CaatEvaluator.hpp"

#include <cstddef>
#include <optional>

namespace cat {

/** Lifetime counters for one worker-local incremental CAAT state. */
struct IncrementalStatistics {
	std::size_t initializations{};
	std::size_t offlineEvaluations{};
};

/**
 * Mutable worker-local state for incremental CAAT evaluation.
 *
 * The model and analysis are borrowed immutable objects and must outlive this
 * instance. Phase 3.1 establishes the initialization boundary: it owns a copy
 * of the current base predicates and bootstraps the exact fixed point through
 * the Phase 2 evaluator. Later substages add insertion deltas and rollback
 * without changing this initialization oracle.
 *
 * An instance is intentionally not thread-safe. GenMC owns one checker, and
 * therefore one incremental state, per exploration worker. Immutable model
 * metadata remains safe to share between workers.
 */
class IncrementalCaatEvaluator {
public:
	/**
	 * Bind the state to one normalized model and its successful analysis.
	 *
	 * @param model Immutable predicate equations, borrowed for this lifetime.
	 * @param analysis Analysis produced for exactly @p model, also borrowed.
	 */
	IncrementalCaatEvaluator(const NormalizedModel &model, const ModelAnalysis &analysis);

	/**
	 * Replace all mutable state with one exact Phase 2 fixed point.
	 *
	 * This operation is the correctness fallback used for the first graph and
	 * for future graph mutations that cannot be classified as insertion or
	 * rollback. It provides the only initialization path, so partially
	 * initialized predicate vectors are never externally visible.
	 *
	 * @param eventCount Dense universe shared by every base value.
	 * @param base Complete primitive snapshot; copied into worker-local state.
	 * @return The immutable current result after successful initialization.
	 */
	auto initialize(std::size_t eventCount, const BaseValues &base)
		-> const CaatEvaluationResult &;

	/** Return whether `initialize()` has published a complete state. */
	[[nodiscard]] auto initialized() const -> bool { return result_.has_value(); }
	/** Return the current universe size; valid after initialization. */
	[[nodiscard]] auto eventCount() const -> std::size_t;
	/** Return the owned primitive snapshot; valid after initialization. */
	[[nodiscard]] auto baseValues() const -> const BaseValues &;
	/** Return the exact current fixed point; valid after initialization. */
	[[nodiscard]] auto result() const -> const CaatEvaluationResult &;
	/** Return cumulative initialization/fallback counters. */
	[[nodiscard]] auto statistics() const -> const IncrementalStatistics &
	{
		return statistics_;
	}

private:
	const NormalizedModel &model_;
	const ModelAnalysis &analysis_;
	std::size_t eventCount_{};
	BaseValues base_;
	std::optional<CaatEvaluationResult> result_;
	IncrementalStatistics statistics_;
};

} /* namespace cat */

#endif /* GENMC_CAT_INCREMENTAL_EVALUATOR_HPP */
