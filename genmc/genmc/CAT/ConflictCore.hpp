/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_CAT_CONFLICT_CORE_HPP
#define GENMC_CAT_CONFLICT_CORE_HPP

#include "genmc/CAT/Evaluator.hpp"
#include "genmc/CAT/Normalized.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

namespace cat {

/** One positive, ground base fact used as a sufficient cycle proof literal. */
struct ConflictLiteral {
	PredicateId predicate{};
	std::uint32_t from{};
	std::uint32_t to{};
	/** Set literals use only `from`; relation literals use both endpoints. */
	bool set{};

	auto operator<=>(const ConflictLiteral &) const = default;
};

/** Bounded worker-local statistics for exact positive-core learning and matching. */
struct ConflictCoreStatistics {
	std::uint64_t learnAttempts{};
	std::uint64_t learned{};
	std::uint64_t unsupported{};
	std::uint64_t duplicateOrSubsumed{};
	std::uint64_t evicted{};
	std::uint64_t matchQueries{};
	std::uint64_t literalChecks{};
	std::uint64_t hits{};
	std::size_t maximumClauses{};
	std::size_t maximumLiterals{};
	std::size_t maximumBytes{};
};

/** Exact bounded database; hashes and partial matches never establish a hit. */
class ConflictCoreDatabase {
public:
	static constexpr std::size_t maximumClauses = 4096;
	static constexpr std::size_t maximumClauseLiterals = 64;

	/** Sort, deduplicate and admit one non-empty positive conjunction. */
	[[nodiscard]] auto learn(std::vector<ConflictLiteral> literals) -> bool;
	/** Match a clause against current base values plus one proposed positive delta. */
	[[nodiscard]] auto matches(const NormalizedModel &model,
				   const std::vector<std::optional<Value>> &values,
				   std::span<const ConflictLiteral> proposed) -> bool;

	[[nodiscard]] auto size() const -> std::size_t { return clauses_.size(); }
	[[nodiscard]] auto literalCount() const -> std::size_t { return literalCount_; }
	[[nodiscard]] auto statistics() const -> const ConflictCoreStatistics & { return stats_; }

private:
	[[nodiscard]] static auto subset(std::span<const ConflictLiteral> left,
					 std::span<const ConflictLiteral> right) -> bool;
	[[nodiscard]] static auto factPresent(const ConflictLiteral &literal,
					      const NormalizedModel &model,
					      const std::vector<std::optional<Value>> &values,
					      std::span<const ConflictLiteral> proposed) -> bool;
	void updateMaximums();

	std::deque<std::vector<ConflictLiteral>> clauses_;
	std::size_t literalCount_{};
	ConflictCoreStatistics stats_{};
};

} /* namespace cat */

#endif /* GENMC_CAT_CONFLICT_CORE_HPP */
