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

#ifndef GENMC_CAT_CAAT_EVALUATOR_HPP
#define GENMC_CAT_CAAT_EVALUATOR_HPP

#include "genmc/CAT/Analysis.hpp"
#include "genmc/CAT/Evaluator.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace cat {

/** Fixed-point population statistics for one complete graph snapshot. */
struct FixedPointStatistics {
	std::size_t operationEvaluations{};
	std::size_t valueChanges{};
	std::size_t worklistPushes{};
};

/** Complete offline CAAT evaluation result, including predicate fixed points. */
struct CaatEvaluationResult {
	std::vector<Violation> violations;
	std::vector<EvaluationError> errors;
	std::vector<std::optional<Value>> values;
	std::vector<std::size_t> evaluationCounts;
	FixedPointStatistics statistics;

	/** Return true only when evaluation succeeded and every axiom held. */
	[[nodiscard]] auto consistent() const -> bool
	{
		return errors.empty() && violations.empty();
	}
};

/**
 * From-scratch stratified least-fixed-point evaluator for offline CAAT.
 *
 * Each call owns its worklist and values. Dependencies outside a stratum are
 * fixed before the stratum starts; positive recursive SCCs begin at bottom and
 * are rescheduled only when an operand changes. The evaluator is therefore
 * deterministic, finite-domain terminating, and thread-safe across calls.
 */
class CaatEvaluator {
public:
	/**
	 * Evaluate one analyzed normalized model over a fixed event universe.
	 *
	 * @param model Immutable one-operation predicate equations.
	 * @param analysis Successful analysis produced for @p model.
	 * @param eventCount Dense event universe shared by all predicate values.
	 * @param base Primitive event sets/relations from the graph adapter.
	 * @return Fixed-point values, violations, errors, and convergence counters.
	 * @errors Missing, ill-typed, or wrong-universe base predicates.
	 */
	[[nodiscard]] auto evaluate(const NormalizedModel &model, const ModelAnalysis &analysis,
				    std::size_t eventCount, const BaseValues &base) const
		-> CaatEvaluationResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_CAAT_EVALUATOR_HPP */
