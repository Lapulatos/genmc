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

#ifndef GENMC_CAT_REASONER_HPP
#define GENMC_CAT_REASONER_HPP

#include "genmc/CAT/Analysis.hpp"
#include "genmc/CAT/Evaluator.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cat {

/** One positive or negative ground base-predicate literal. */
struct BaseLiteral {
	PredicateId predicate{};
	std::string predicateName;
	ValueType type{ValueType::Relation};
	bool positive{true};
	std::size_t first{};
	std::optional<std::size_t> second;

	auto operator==(const BaseLiteral &) const -> bool = default;
};

/** Deterministic conjunction of base literals sufficient for one violation. */
using Explanation = std::vector<BaseLiteral>;

/** One structured violation paired with its replayable CAAT explanation. */
struct ExplainedViolation {
	Violation violation;
	Explanation explanation;
};

/** Result of projecting fixed-point witnesses to base predicates. */
struct ExplanationResult {
	std::vector<ExplainedViolation> violations;
	std::vector<std::string> errors;

	/** Return true exactly when every input violation received an explanation. */
	[[nodiscard]] auto ok() const -> bool { return errors.empty(); }
};

/**
 * Offline derivation reasoner for one completed CAAT fixed point.
 *
 * The reasoner reconstructs finite derivations stratum by stratum, including
 * positive recursion. It chooses deterministic short explanations, represents
 * semi-positive difference with negative base literals, and stores no state
 * after a call.
 */
class Reasoner {
public:
	/**
	 * Render one explanation as a stable single-line diagnostic.
	 *
	 * @param explained Valid explained violation returned by explain().
	 * @return Source position, check name, witness, and sufficient base literals.
	 */
	[[nodiscard]] static auto format(const ExplainedViolation &explained) -> std::string;

	/**
	 * Explain all @p violations using the supplied final predicate values.
	 *
	 * @param model Normalized equations that produced the values.
	 * @param analysis Matching admissibility/stratification analysis.
	 * @param eventCount Dense universe size.
	 * @param values Final fixed-point value for every predicate.
	 * @param violations Structured axiom witnesses to explain.
	 * @return Base-literal conjunctions or internal provenance errors.
	 */
	[[nodiscard]] auto explain(const NormalizedModel &model, const ModelAnalysis &analysis,
				   std::size_t eventCount,
				   const std::vector<std::optional<Value>> &values,
				   const std::vector<Violation> &violations) const
		-> ExplanationResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_REASONER_HPP */
