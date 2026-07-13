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

#ifndef GENMC_CAT_CHECKER_HPP
#define GENMC_CAT_CHECKER_HPP

#include "genmc/Execution/Consistency/SCChecker.hpp"

/**
 * Correctness-first full-graph checker for a validated CAT model.
 *
 * Phase 1.6 reuses SCChecker's established view, prefix, race, and warning
 * machinery as the host exploration profile. CAT consistency itself is never
 * delegated: every candidate is rebuilt through the generic graph adapter and
 * evaluator. Model-specific candidate/revisit pruning is disabled in favor of
 * conservative enumeration. Like ConsistencyChecker, one instance belongs to
 * one verification worker and is not thread-safe.
 */
class CATChecker final : public SCChecker {
public:
	/**
	 * Construct a checker borrowing the worker's immutable configuration.
	 *
	 * @param conf Non-null Config that outlives this checker and owns catModel.
	 */
	explicit CATChecker(const Config *conf) : SCChecker(conf) {}

private:
	/**
	 * Evaluate the complete graph containing newly selected @p lab.
	 *
	 * @param lab Non-null graph-owned label whose rf/co choice is already installed.
	 * @return True exactly when every CAT check accepts the containing graph.
	 * @complexity One from-scratch adapter build and model evaluation.
	 */
	[[nodiscard]] auto isConsistent(const EventLabel *lab) const -> bool override;
	/**
	 * Evaluate all checks in Config::catModel against @p graph.
	 *
	 * @param graph Read-only candidate snapshot.
	 * @return True exactly when evaluation succeeds without a violation.
	 * @errors Internal invariant failure if validated IR/primitives cannot evaluate.
	 */
	[[nodiscard]] auto isConsistent(const ExecutionGraph &graph) const -> bool override;

	/**
	 * Return every currently available same-location rf source.
	 *
	 * @param read Graph-owned read whose address selects the co list.
	 * @return Init followed by every real write in current coherence order.
	 */
	auto getCoherentStores(ReadLabel *read) -> std::vector<EventLabel *> override;
	/**
	 * Keep every generic revisit candidate; CAT evaluation filters resulting graphs.
	 *
	 * @param write Newly added write that triggered revisits.
	 * @param reads Candidate list left unchanged by this conservative profile.
	 */
	void filterCoherentRevisits(WriteLabel *write, std::vector<ReadLabel *> &reads) override;
	/**
	 * Return every coherence predecessor, subject only to RMW adjacency.
	 *
	 * @param write Graph-owned write not yet assigned its final co position.
	 * @return All predecessor choices, or the unique rf source for an RMW write.
	 */
	auto getCoherentPlacings(WriteLabel *write) -> std::vector<EventLabel *> override;
};

#endif /* GENMC_CAT_CHECKER_HPP */
