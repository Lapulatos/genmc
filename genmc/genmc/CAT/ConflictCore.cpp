/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/CAT/ConflictCore.hpp"

#include <algorithm>
#include <ranges>

namespace cat {

auto ConflictCoreDatabase::subset(std::span<const ConflictLiteral> left,
				  std::span<const ConflictLiteral> right) -> bool
{
	return std::ranges::includes(right, left);
}

void ConflictCoreDatabase::updateMaximums()
{
	stats_.maximumClauses = std::max(stats_.maximumClauses, clauses_.size());
	stats_.maximumLiterals = std::max(stats_.maximumLiterals, literalCount_);
	stats_.maximumBytes =
		std::max(stats_.maximumBytes, literalCount_ * sizeof(ConflictLiteral));
}

auto ConflictCoreDatabase::learn(std::vector<ConflictLiteral> literals) -> bool
{
	++stats_.learnAttempts;
	std::ranges::sort(literals);
	literals.erase(std::ranges::unique(literals).begin(), literals.end());
	if (literals.empty() || literals.size() > maximumClauseLiterals) {
		++stats_.unsupported;
		return false;
	}
	if (std::ranges::any_of(clauses_,
				[&](const auto &clause) { return subset(clause, literals); })) {
		++stats_.duplicateOrSubsumed;
		return false;
	}
	for (auto it = clauses_.begin(); it != clauses_.end();) {
		if (subset(literals, *it)) {
			literalCount_ -= it->size();
			it = clauses_.erase(it);
			++stats_.duplicateOrSubsumed;
		} else {
			++it;
		}
	}
	if (clauses_.size() == maximumClauses) {
		literalCount_ -= clauses_.front().size();
		clauses_.pop_front();
		++stats_.evicted;
	}
	literalCount_ += literals.size();
	clauses_.push_back(std::move(literals));
	++stats_.learned;
	updateMaximums();
	return true;
}

auto ConflictCoreDatabase::factPresent(const ConflictLiteral &literal, const NormalizedModel &model,
				       const std::vector<std::optional<Value>> &values,
				       std::span<const ConflictLiteral> proposed) -> bool
{
	if (std::ranges::binary_search(proposed, literal))
		return true;
	if (literal.predicate >= model.predicates().size() || literal.predicate >= values.size() ||
	    !values[literal.predicate])
		return false;
	const auto &predicate = model.predicates()[literal.predicate];
	if (predicate.kind != Predicate::Kind::Base)
		return false;
	if (literal.set) {
		const auto *set = std::get_if<EventSet>(&*values[literal.predicate]);
		return set && set->contains(literal.from);
	}
	const auto *relation = std::get_if<Relation>(&*values[literal.predicate]);
	return relation && relation->contains(literal.from, literal.to);
}

auto ConflictCoreDatabase::matches(const NormalizedModel &model,
				   const std::vector<std::optional<Value>> &values,
				   std::span<const ConflictLiteral> proposed) -> bool
{
	++stats_.matchQueries;
	for (const auto &clause : clauses_) {
		bool matched = true;
		for (const auto &literal : clause) {
			++stats_.literalChecks;
			if (!factPresent(literal, model, values, proposed)) {
				matched = false;
				break;
			}
		}
		if (matched) {
			++stats_.hits;
			return true;
		}
	}
	return false;
}

} /* namespace cat */
