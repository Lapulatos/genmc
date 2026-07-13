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

#ifndef GENMC_CAT_EVALUATOR_HPP
#define GENMC_CAT_EVALUATOR_HPP

#include "genmc/CAT/Model.hpp"
#include "genmc/CAT/Value.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cat {

/** Runtime value of one typed ModelIR node. */
using Value = std::variant<EventSet, Relation>;

/** Read-only primitive values keyed by their canonical CAT names. */
using BaseValues = std::unordered_map<std::string, Value>;

/** A missing or ill-shaped primitive that prevents model evaluation. */
struct EvaluationError {
	NodeId node{};
	SourceSpan span;
	std::string message;
};

/**
 * Structured failed consistency check.
 *
 * `witness` is one event for non-empty sets/diagonals, two events for a
 * non-empty pair, or a closed event sequence for an acyclicity cycle.
 */
struct Violation {
	std::string checkName;
	Statement::CheckKind checkKind{Statement::CheckKind::Acyclic};
	SourceSpan span;
	std::vector<std::size_t> witness;
};

/** Complete from-scratch evaluation outcome for one fixed graph snapshot. */
struct EvaluationResult {
	std::vector<Violation> violations;
	std::vector<EvaluationError> errors;
	std::vector<std::size_t> evaluationCounts;

	/** Return true only when evaluation succeeded and every check held. */
	[[nodiscard]] auto consistent() const -> bool
	{
		return errors.empty() && violations.empty();
	}
};

/**
 * Pure full-graph interpreter for typed CAT relation DAGs.
 *
 * Evaluator stores no state between calls. Each call memoizes every requested
 * node once, reads but never mutates ModelIR/BaseValues, and owns all computed
 * values locally. Separate calls are therefore thread-safe.
 */
class Evaluator {
public:
	/**
	 * Evaluate all bindings and checks for one event universe.
	 *
	 * @param model Immutable typed model.
	 * @param eventCount Number of dense events in every supplied value.
	 * @param base Primitive values such as `R`, `po`, `rf`, and `co`.
	 * @return Structured errors/violations plus per-node memoization counts.
	 * @errors Missing primitives, type mismatches, or universe-size mismatches.
	 * @complexity Dominated by composition/closure, O(events^3 / word size) in dense cases.
	 */
	[[nodiscard]] auto evaluate(const ModelIR &model, std::size_t eventCount,
				    const BaseValues &base) const -> EvaluationResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_EVALUATOR_HPP */
