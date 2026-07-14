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

#ifndef GENMC_CAT_ANALYSIS_HPP
#define GENMC_CAT_ANALYSIS_HPP

#include "genmc/CAT/Normalized.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace cat {

/** One signed dependency from an operand predicate to its defining user. */
struct Dependency {
	PredicateId source{};
	PredicateId target{};
	bool negative{};
};

/**
 * Immutable CAAT admissibility and stratification result.
 *
 * Strata are deterministic SCCs in dependency-first topological order. Every
 * predicate occurs exactly once; `componentOf()[p]` indexes its stratum. The
 * object owns only IDs/flags and can be shared with the immutable normalized
 * model across worker threads.
 */
class ModelAnalysis {
public:
	/**
	 * Construct a fully validated analysis.
	 *
	 * @param dependencies Signed normalized dependency edges.
	 * @param strata Dependency-first strongly connected components.
	 * @param componentOf Predicate-to-stratum dense mapping.
	 * @param domainIndependent Per-predicate syntactic DI result.
	 */
	ModelAnalysis(std::vector<Dependency> dependencies,
		      std::vector<std::vector<PredicateId>> strata,
		      std::vector<std::uint32_t> componentOf, std::vector<bool> domainIndependent)
		: dependencies_(std::move(dependencies)), strata_(std::move(strata)),
		  componentOf_(std::move(componentOf)),
		  domainIndependent_(std::move(domainIndependent))
	{}

	/** Return signed edges in stable target/operand order. */
	[[nodiscard]] auto dependencies() const -> const std::vector<Dependency> &
	{
		return dependencies_;
	}
	/** Return SCC strata in dependency-first order. */
	[[nodiscard]] auto strata() const -> const std::vector<std::vector<PredicateId>> &
	{
		return strata_;
	}
	/** Return the stratum index of each predicate. */
	[[nodiscard]] auto componentOf() const -> const std::vector<std::uint32_t> &
	{
		return componentOf_;
	}
	/** Return whether each predicate satisfies CAAT's DI grammar. */
	[[nodiscard]] auto domainIndependent() const -> const std::vector<bool> &
	{
		return domainIndependent_;
	}

private:
	std::vector<Dependency> dependencies_;
	std::vector<std::vector<PredicateId>> strata_;
	std::vector<std::uint32_t> componentOf_;
	std::vector<bool> domainIndependent_;
};

/** Result of signed dependency and CAAT admissibility analysis. */
struct AnalysisResult {
	std::shared_ptr<const ModelAnalysis> analysis;
	std::vector<Diagnostic> diagnostics;

	/** Return true exactly when the normalized model is admissible offline CAAT. */
	[[nodiscard]] auto ok() const -> bool { return analysis && diagnostics.empty(); }
};

/**
 * Validate one normalized model and compute its canonical SCC stratification.
 *
 * The analysis is pure and deterministic. It rejects undeclared recursion,
 * negative dependencies within an SCC, non-semi-positive difference, and
 * axioms outside CAAT's syntactic domain-independent fragment.
 */
class Analyzer {
public:
	/** Analyze @p model without changing its predicate equations. */
	[[nodiscard]] auto analyze(const NormalizedModel &model) const -> AnalysisResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_ANALYSIS_HPP */
