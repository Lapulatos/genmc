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

#include "genmc/CAT/IncrementalEvaluator.hpp"

#include "genmc/Support/Error.hpp"

namespace cat {

IncrementalCaatEvaluator::IncrementalCaatEvaluator(const NormalizedModel &model,
						   const ModelAnalysis &analysis)
	: model_(model), analysis_(analysis)
{}

auto IncrementalCaatEvaluator::initialize(std::size_t eventCount, const BaseValues &base)
	-> const CaatEvaluationResult &
{
	/* Compute into a temporary first. An evaluation error is a valid published
	 * result, but an exception or assertion cannot leave a mixed old/new state. */
	auto next = CaatEvaluator().evaluate(model_, analysis_, eventCount, base);
	eventCount_ = eventCount;
	base_ = base;
	result_ = std::move(next);
	++statistics_.initializations;
	++statistics_.offlineEvaluations;
	return *result_;
}

auto IncrementalCaatEvaluator::eventCount() const -> std::size_t
{
	VERIFY(initialized(), "incremental CAAT state has not been initialized");
	return eventCount_;
}

auto IncrementalCaatEvaluator::baseValues() const -> const BaseValues &
{
	VERIFY(initialized(), "incremental CAAT state has not been initialized");
	return base_;
}

auto IncrementalCaatEvaluator::result() const -> const CaatEvaluationResult &
{
	VERIFY(initialized(), "incremental CAAT state has not been initialized");
	return *result_;
}

} /* namespace cat */
