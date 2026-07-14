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
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cat {

/** Lifetime counters for one worker-local incremental CAAT state. */
struct IncrementalStatistics {
	std::size_t initializations{};
	std::size_t offlineEvaluations{};
	std::size_t insertionUpdates{};
	std::size_t rejectedUpdates{};
	std::size_t operationEvaluations{};
	std::size_t valueChanges{};
	std::size_t worklistPushes{};
	std::size_t checkpoints{};
	std::size_t rollbacks{};
	std::size_t rejectedRollbacks{};
};

/** Opaque, evaluator-local identity for one retained exact state. */
struct IncrementalCheckpoint {
	std::uint64_t id{};

	auto operator==(const IncrementalCheckpoint &) const -> bool = default;
};

/** Outcome of an exact checkpoint restoration attempt. */
struct IncrementalRollbackResult {
	bool restored{};
	std::string reason;
};

/** Classification returned without mutating state for unsupported updates. */
enum class IncrementalUpdateStatus : std::uint8_t { Applied, RequiresRebuild };

/** Outcome of one attempted insertion-only transition. */
struct IncrementalUpdateResult {
	IncrementalUpdateStatus status{IncrementalUpdateStatus::RequiresRebuild};
	std::string reason;

	/** Return true exactly when the new state was committed. */
	[[nodiscard]] auto applied() const -> bool
	{
		return status == IncrementalUpdateStatus::Applied;
	}
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

	/**
	 * Apply a complete primitive snapshot that only inserts semantic facts.
	 *
	 * All validation and fixed-point propagation occur on temporary values.
	 * Deletion, universe shrinkage, type changes, missing bases, evaluation
	 * errors, or a non-monotone model return `RequiresRebuild` and preserve the
	 * previous state byte-for-byte.
	 *
	 * @param eventCount New universe, greater than or equal to the current one.
	 * @param base Complete new primitive snapshot in the same stable ID space.
	 * @return Applied status or a deterministic offline-rebuild reason.
	 */
	[[nodiscard]] auto tryInsert(std::size_t eventCount, const BaseValues &base)
		-> IncrementalUpdateResult;

	/**
	 * Retain an exact copy of the current online state.
	 *
	 * Handles belong to this evaluator and remain valid until initialization or
	 * rollback invalidates them. Snapshot storage is correctness-first and costs
	 * O(total predicate memberships) per retained checkpoint.
	 *
	 * @return Opaque handle accepted by rollback().
	 */
	[[nodiscard]] auto checkpoint() -> IncrementalCheckpoint;

	/**
	 * Restore a retained state and invalidate every later checkpoint.
	 *
	 * Rejected foreign, stale, or future handles do not mutate current state.
	 * The restored handle remains live so exploration may revisit the same
	 * branch point more than once.
	 *
	 * @param checkpoint Handle previously returned by this evaluator.
	 * @return Restoration status and a deterministic diagnostic on rejection.
	 */
	[[nodiscard]] auto rollback(IncrementalCheckpoint checkpoint) -> IncrementalRollbackResult;

	/**
	 * Release one retained snapshot without changing the current state.
	 *
	 * @param checkpoint Live handle returned by checkpoint().
	 * @return True when a retained snapshot was removed.
	 */
	[[nodiscard]] auto forget(IncrementalCheckpoint checkpoint) -> bool;

	/** Return whether `initialize()` has published a complete state. */
	[[nodiscard]] auto initialized() const -> bool { return result_.has_value(); }
	/** Return the current universe size; valid after initialization. */
	[[nodiscard]] auto eventCount() const -> std::size_t;
	/** Return the owned primitive snapshot; valid after initialization. */
	[[nodiscard]] auto baseValues() const -> const BaseValues &;
	/** Return the exact current fixed point; valid after initialization. */
	[[nodiscard]] auto result() const -> const CaatEvaluationResult &;
	/** Return whether the normalized equations are monotone under insertion. */
	[[nodiscard]] auto supportsInsertions() const -> bool { return supportsInsertions_; }
	/** Return cumulative initialization/fallback counters. */
	[[nodiscard]] auto statistics() const -> const IncrementalStatistics &
	{
		return statistics_;
	}

private:
	/** Complete immutable-by-convention state retained for exact restoration. */
	struct Snapshot {
		IncrementalCheckpoint checkpoint;
		std::size_t eventCount{};
		BaseValues base;
		CaatEvaluationResult result;
	};

	const NormalizedModel &model_;
	const ModelAnalysis &analysis_;
	std::size_t eventCount_{};
	BaseValues base_;
	std::optional<CaatEvaluationResult> result_;
	IncrementalStatistics statistics_;
	std::vector<Snapshot> checkpoints_;
	bool supportsInsertions_{true};
};

} /* namespace cat */

#endif /* GENMC_CAT_INCREMENTAL_EVALUATOR_HPP */
