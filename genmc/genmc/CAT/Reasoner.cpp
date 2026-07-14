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

#include "genmc/CAT/Reasoner.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <sstream>
#include <tuple>
#include <utility>

namespace cat {
namespace {

using ReasonTable = std::vector<std::vector<std::optional<Explanation>>>;

static auto literalKey(const BaseLiteral &literal)
{
	return std::tuple{literal.predicate, literal.positive, literal.first,
			  literal.second.value_or(std::numeric_limits<std::size_t>::max())};
}

static void normalize(Explanation &explanation)
{
	std::ranges::sort(explanation, {}, literalKey);
	explanation.erase(std::unique(explanation.begin(), explanation.end()), explanation.end());
}

static auto combine(const Explanation &lhs, const Explanation &rhs) -> Explanation
{
	auto result = lhs;
	result.insert(result.end(), rhs.begin(), rhs.end());
	normalize(result);
	return result;
}

static auto better(const Explanation &candidate, const Explanation &current) -> bool
{
	if (candidate.size() != current.size())
		return candidate.size() < current.size();
	return std::lexicographical_compare(
		candidate.begin(), candidate.end(), current.begin(), current.end(),
		[](const auto &lhs, const auto &rhs) { return literalKey(lhs) < literalKey(rhs); });
}

/** Per-call full-fixed-point provenance reconstruction. */
class ExplanationBuilder {
public:
	ExplanationBuilder(const NormalizedModel &model, const ModelAnalysis &analysis,
			   std::size_t eventCount, const std::vector<std::optional<Value>> &values)
		: model_(model), analysis_(analysis), eventCount_(eventCount), values_(values),
		  reasons_(model.predicates().size())
	{
		for (const auto &predicate : model.predicates())
			reasons_[predicate.id].resize(predicate.type == ValueType::Set
							      ? eventCount
							      : eventCount * eventCount);
	}

	auto run(const std::vector<Violation> &violations) -> ExplanationResult
	{
		initializeBaseReasons();
		for (const auto &stratum : analysis_.strata())
			populateStratum(stratum);
		for (const auto &violation : violations)
			explainViolation(violation);
		return {std::move(explained_), std::move(errors_)};
	}

private:
	[[nodiscard]] auto index(const Predicate &predicate, std::size_t first,
				 std::optional<std::size_t> second) const -> std::size_t
	{
		return predicate.type == ValueType::Set ? first : first * eventCount_ + *second;
	}

	[[nodiscard]] auto contains(PredicateId id, std::size_t first,
				    std::optional<std::size_t> second) const -> bool
	{
		const auto &predicate = model_.predicates()[id];
		if (predicate.type == ValueType::Set)
			return std::get<EventSet>(*values_[id]).contains(first);
		return std::get<Relation>(*values_[id]).contains(first, *second);
	}

	[[nodiscard]] auto reason(PredicateId id, std::size_t first,
				  std::optional<std::size_t> second) const -> const Explanation *
	{
		const auto &predicate = model_.predicates()[id];
		const auto &entry = reasons_[id][index(predicate, first, second)];
		return entry ? &*entry : nullptr;
	}

	void initializeBaseReasons()
	{
		for (const auto &predicate : model_.predicates()) {
			if (predicate.kind != Predicate::Kind::Base)
				continue;
			const bool intrinsic = predicate.name == "0" || predicate.name == "_" ||
					       predicate.name == "id";
			if (predicate.type == ValueType::Set) {
				for (std::size_t event = 0; event < eventCount_; ++event) {
					if (!contains(predicate.id, event, std::nullopt))
						continue;
					reasons_[predicate.id][event] =
						intrinsic
							? Explanation{}
							: Explanation{{predicate.id, predicate.name,
								       ValueType::Set, true, event,
								       std::nullopt}};
				}
			} else {
				for (std::size_t first = 0; first < eventCount_; ++first) {
					for (std::size_t second = 0; second < eventCount_;
					     ++second) {
						if (!contains(predicate.id, first, second))
							continue;
						reasons_[predicate.id][first * eventCount_ +
								       second] =
							intrinsic ? Explanation{}
								  : Explanation{
									    {predicate.id,
									     predicate.name,
									     ValueType::Relation,
									     true, first, second}};
					}
				}
			}
		}
	}

	[[nodiscard]] auto negativeLiteral(const Predicate &predicate, std::size_t first,
					   std::optional<std::size_t> second) const -> Explanation
	{
		if (predicate.name == "0" || predicate.name == "_" || predicate.name == "id")
			return {};
		return {{predicate.id, predicate.name, predicate.type, false, first, second}};
	}

	[[nodiscard]] auto shortestPathReason(PredicateId operand, std::size_t source,
					      std::size_t target) const
		-> std::optional<Explanation>
	{
		if (source == target)
			return Explanation{};
		std::vector<std::size_t> parent(eventCount_, eventCount_);
		std::deque<std::size_t> queue;
		queue.push_back(source);
		parent[source] = source;
		while (!queue.empty() && parent[target] == eventCount_) {
			const auto from = queue.front();
			queue.pop_front();
			for (std::size_t to = 0; to < eventCount_; ++to) {
				if (parent[to] != eventCount_ || !contains(operand, from, to) ||
				    !reason(operand, from, to))
					continue;
				parent[to] = from;
				queue.push_back(to);
			}
		}
		if (parent[target] == eventCount_)
			return std::nullopt;
		Explanation result;
		for (auto to = target; to != source; to = parent[to])
			result = combine(result, *reason(operand, parent[to], to));
		return result;
	}

	[[nodiscard]] auto derive(const Predicate &predicate, std::size_t first,
				  std::optional<std::size_t> second) const
		-> std::optional<Explanation>
	{
		const auto unary =
			[&](PredicateId operand, std::size_t one,
			    std::optional<std::size_t> two) -> std::optional<Explanation> {
			if (const auto *found = reason(operand, one, two))
				return *found;
			return std::nullopt;
		};
		if (predicate.kind == Predicate::Kind::Alias)
			return unary(predicate.operands[0], first, second);
		if (predicate.kind == Predicate::Kind::Union) {
			std::optional<Explanation> result;
			for (const auto operand : predicate.operands) {
				if (!contains(operand, first, second))
					continue;
				if (auto candidate = unary(operand, first, second);
				    candidate && (!result || better(*candidate, *result)))
					result = std::move(candidate);
			}
			return result;
		}
		if (predicate.kind == Predicate::Kind::Intersection) {
			auto lhs = unary(predicate.operands[0], first, second);
			auto rhs = unary(predicate.operands[1], first, second);
			if (lhs && rhs)
				return combine(*lhs, *rhs);
			return std::nullopt;
		}
		if (predicate.kind == Predicate::Kind::Difference) {
			auto lhs = unary(predicate.operands[0], first, second);
			if (!lhs)
				return std::nullopt;
			return combine(*lhs,
				       negativeLiteral(model_.predicates()[predicate.operands[1]],
						       first, second));
		}
		if (predicate.kind == Predicate::Kind::Inverse)
			return unary(predicate.operands[0], *second, first);
		if (predicate.kind == Predicate::Kind::Identity) {
			if (first != *second)
				return std::nullopt;
			return unary(predicate.operands[0], first, std::nullopt);
		}
		if (predicate.kind == Predicate::Kind::Optional) {
			if (first == *second)
				return Explanation{};
			return unary(predicate.operands[0], first, second);
		}
		if (predicate.kind == Predicate::Kind::Product) {
			auto lhs = unary(predicate.operands[0], first, std::nullopt);
			auto rhs = unary(predicate.operands[1], *second, std::nullopt);
			if (lhs && rhs)
				return combine(*lhs, *rhs);
			return std::nullopt;
		}
		if (predicate.kind == Predicate::Kind::Composition) {
			std::optional<Explanation> result;
			for (std::size_t middle = 0; middle < eventCount_; ++middle) {
				auto lhs = unary(predicate.operands[0], first, middle);
				auto rhs = unary(predicate.operands[1], middle, second);
				if (!lhs || !rhs)
					continue;
				auto candidate = combine(*lhs, *rhs);
				if (!result || better(candidate, *result))
					result = std::move(candidate);
			}
			return result;
		}
		if (predicate.kind == Predicate::Kind::Domain) {
			for (std::size_t target = 0; target < eventCount_; ++target) {
				if (auto result = unary(predicate.operands[0], first, target))
					return result;
			}
			return std::nullopt;
		}
		if (predicate.kind == Predicate::Kind::Range) {
			for (std::size_t source = 0; source < eventCount_; ++source) {
				if (auto result = unary(predicate.operands[0], source, first))
					return result;
			}
			return std::nullopt;
		}
		if (predicate.kind == Predicate::Kind::TransitiveClosure ||
		    predicate.kind == Predicate::Kind::ReflexiveTransitiveClosure) {
			if (predicate.kind == Predicate::Kind::ReflexiveTransitiveClosure &&
			    first == *second)
				return Explanation{};
			return shortestPathReason(predicate.operands[0], first, *second);
		}
		return std::nullopt;
	}

	void populateStratum(const std::vector<PredicateId> &stratum)
	{
		bool changed = true;
		while (changed) {
			changed = false;
			for (const auto id : stratum) {
				const auto &predicate = model_.predicates()[id];
				if (predicate.kind == Predicate::Kind::Base)
					continue;
				const auto limit = predicate.type == ValueType::Set
							   ? eventCount_
							   : eventCount_ * eventCount_;
				for (std::size_t flat = 0; flat < limit; ++flat) {
					const auto first = predicate.type == ValueType::Set
								   ? flat
								   : flat / eventCount_;
					const auto second =
						predicate.type == ValueType::Set
							? std::nullopt
							: std::optional(flat % eventCount_);
					if (!contains(id, first, second))
						continue;
					auto candidate = derive(predicate, first, second);
					if (!candidate)
						continue;
					auto &current = reasons_[id][flat];
					if (!current || better(*candidate, *current)) {
						current = std::move(candidate);
						changed = true;
					}
				}
			}
		}
	}

	void explainViolation(const Violation &violation)
	{
		const auto check =
			std::ranges::find_if(model_.checks(), [&](const auto &candidate) {
				return candidate.name == violation.checkName;
			});
		if (check == model_.checks().end()) {
			errors_.push_back("unknown check '" + violation.checkName + "'");
			return;
		}
		const auto &predicate = model_.predicates()[check->predicate];
		Explanation explanation;
		if (violation.checkKind == Statement::CheckKind::Acyclic) {
			for (std::size_t i = 1; i < violation.witness.size(); ++i) {
				const auto *edge = reason(predicate.id, violation.witness[i - 1],
							  violation.witness[i]);
				if (!edge) {
					errors_.push_back("missing cycle-edge derivation for '" +
							  violation.checkName + "'");
					return;
				}
				explanation = combine(explanation, *edge);
			}
		} else {
			const auto second =
				predicate.type == ValueType::Relation
					? std::optional(
						  violation.checkKind ==
								  Statement::CheckKind::Irreflexive
							  ? violation.witness[0]
							  : violation.witness[1])
					: std::nullopt;
			const auto *membership = reason(predicate.id, violation.witness[0], second);
			if (!membership) {
				errors_.push_back("missing witness derivation for '" +
						  violation.checkName + "'");
				return;
			}
			explanation = *membership;
		}
		explained_.push_back({violation, std::move(explanation)});
	}

	const NormalizedModel &model_;
	const ModelAnalysis &analysis_;
	std::size_t eventCount_{};
	const std::vector<std::optional<Value>> &values_;
	ReasonTable reasons_;
	std::vector<ExplainedViolation> explained_;
	std::vector<std::string> errors_;
};

} /* namespace */

auto Reasoner::explain(const NormalizedModel &model, const ModelAnalysis &analysis,
		       std::size_t eventCount, const std::vector<std::optional<Value>> &values,
		       const std::vector<Violation> &violations) const -> ExplanationResult
{
	/* Provenance is meaningful only for the exact, fully evaluated snapshot.
	 * Reject stale/partial vectors here instead of dereferencing them in the
	 * derivation walk or printing a plausible but unrelated explanation. */
	if (values.size() != model.predicates().size() ||
	    analysis.componentOf().size() != model.predicates().size())
		return {{}, {"stale CAAT explanation snapshot"}};
	for (const auto &predicate : model.predicates()) {
		if (!values[predicate.id])
			return {{}, {"missing value for predicate '" + predicate.name + "'"}};
		const auto valid =
			predicate.type == ValueType::Set
				? std::holds_alternative<EventSet>(*values[predicate.id]) &&
					  std::get<EventSet>(*values[predicate.id]).size() ==
						  eventCount
				: std::holds_alternative<Relation>(*values[predicate.id]) &&
					  std::get<Relation>(*values[predicate.id]).size() ==
						  eventCount;
		if (!valid)
			return {{}, {"invalid value for predicate '" + predicate.name + "'"}};
	}
	return ExplanationBuilder(model, analysis, eventCount, values).run(violations);
}

auto Reasoner::format(const ExplainedViolation &explained) -> std::string
{
	std::ostringstream output;
	output << explained.violation.span.begin.line << ':'
	       << explained.violation.span.begin.column << ": CAT check '"
	       << explained.violation.checkName << "' failed; witness=";
	for (std::size_t i = 0; i < explained.violation.witness.size(); ++i) {
		if (i != 0)
			output << "->";
		output << explained.violation.witness[i];
	}
	output << "; because ";
	if (explained.explanation.empty()) {
		output << "the intrinsic event domain";
	} else {
		for (std::size_t i = 0; i < explained.explanation.size(); ++i) {
			if (i != 0)
				output << " & ";
			const auto &literal = explained.explanation[i];
			if (!literal.positive)
				output << '!';
			output << literal.predicateName << '(' << literal.first;
			if (literal.second)
				output << ',' << *literal.second;
			output << ')';
		}
	}
	return output.str();
}

} /* namespace cat */
