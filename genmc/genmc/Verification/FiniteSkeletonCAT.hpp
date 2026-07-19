/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_SKELETON_CAT_HPP
#define GENMC_FINITE_SKELETON_CAT_HPP

#include "genmc/CAT/Evaluator.hpp"
#include "genmc/CAT/CaatEvaluator.hpp"
#include "genmc/CAT/Reasoner.hpp"
#include "genmc/Verification/FiniteSkeletonEncoder.hpp"

#include <string>
#include <vector>

namespace genmc::symbolic {

enum class FiniteDensePart : std::uint8_t { ordinary, lockRead, lockWrite, initial };

struct FiniteDenseEvent {
	skeleton::NodeID site{skeleton::invalidNode};
	FiniteDensePart part{FiniteDensePart::ordinary};
	std::string address{};
};

struct FiniteCATResult {
	cat::EvaluationResult evaluation{};
	std::vector<std::string> errors{};
	std::size_t eventCount{};
	std::vector<FiniteDenseEvent> denseEvents{};
	std::vector<cat::Explanation> explanations{};
	[[nodiscard]] auto consistent() const -> bool
	{
		return errors.empty() && evaluation.consistent();
	}
};

/** Materialize the exact CAT primitive snapshot without evaluating a model. Exposed for
 * differential verification against the production ExecutionGraph adapter. */
[[nodiscard]] auto materializeFiniteAssignment(const skeleton::Program &program,
					       const FiniteAssignment &assignment)
	-> std::pair<FiniteCATResult, cat::BaseValues>;

/** Materialize CAT primitives directly from one complete finite assignment and evaluate
 * the generic ModelIR. No built-in SC/TSO checker is called. */
[[nodiscard]] auto evaluateFiniteAssignment(const skeleton::Program &program,
					    const FiniteAssignment &assignment,
					    const cat::ModelIR &model) -> FiniteCATResult;

[[nodiscard]] auto evaluateFiniteAssignment(const skeleton::Program &program,
					    const FiniteAssignment &assignment,
					    const cat::NormalizedModel &model,
					    const cat::ModelAnalysis &analysis) -> FiniteCATResult;

} /* namespace genmc::symbolic */

#endif
