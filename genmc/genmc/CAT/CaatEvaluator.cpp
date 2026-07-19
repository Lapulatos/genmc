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

#include "genmc/CAT/CaatEvaluator.hpp"

#include "genmc/CAT/LazyCycle.hpp"

#include "genmc/Support/Error.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <utility>

namespace cat {
namespace {

/** Mutable state for one complete offline fixed-point evaluation. */
class Evaluation {
public:
	Evaluation(const NormalizedModel &model, const ModelAnalysis &analysis,
		   std::size_t eventCount, const BaseValues &base, bool enableLazyCycles,
		   std::optional<PredicateId> retainedLazyRoot, bool profiling, bool fastChecks,
		   bool fastComposition, bool fastCycleChecks)
		: model_(model), analysis_(analysis), eventCount_(eventCount), base_(base),
		  values_(model.predicates().size()), counts_(model.predicates().size()),
		  dependents_(model.predicates().size()), enableLazyCycles_(enableLazyCycles),
		  retainedLazyRoot_(retainedLazyRoot), profiling_(profiling), fastChecks_(fastChecks),
		  fastComposition_(fastComposition), fastCycleChecks_(fastCycleChecks)
	{
		VERIFY(analysis.componentOf().size() == model.predicates().size(),
		       "CAAT analysis/model predicate count mismatch");
		VERIFY(!retainedLazyRoot_ ||
			       (*retainedLazyRoot_ < analysis_.lazyCycleElided().size() &&
				analysis_.lazyCycleElided()[*retainedLazyRoot_]),
		       "retained CAT root is not analyzer-certified for lazy evaluation");
		for (const auto &dependency : analysis.dependencies())
			dependents_[dependency.source].push_back(dependency.target);
	}

	auto run() -> CaatEvaluationResult
	{
		const auto initializeStarted = profiling_ ? std::chrono::steady_clock::now()
							  : std::chrono::steady_clock::time_point{};
		initializeValues();
		if (profiling_)
			statistics_.initializationNanoseconds += elapsed(initializeStarted);
		if (errors_.empty()) {
			for (std::uint32_t stratum = 0; stratum < analysis_.strata().size();
			     ++stratum)
				evaluateStratum(stratum);
			if (errors_.empty()) {
				const auto checksStarted = profiling_ ? std::chrono::steady_clock::now()
								     : std::chrono::steady_clock::time_point{};
				evaluateChecks();
				if (profiling_)
					statistics_.checkNanoseconds += elapsed(checksStarted);
			}
		}
		return {std::move(violations_), std::move(errors_), std::move(values_),
			std::move(counts_), statistics_};
	}

private:
	[[nodiscard]] static auto elapsed(std::chrono::steady_clock::time_point started)
		-> std::uint64_t
	{
		return static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started)
				.count());
	}

	[[nodiscard]] auto universe() const -> EventSet
	{
		EventSet result(eventCount_);
		for (std::size_t event = 0; event < eventCount_; ++event)
			result.insert(event);
		return result;
	}

	[[nodiscard]] auto emptyValue(ValueType type) const -> Value
	{
		if (type == ValueType::Set)
			return EventSet(eventCount_);
		return Relation(eventCount_);
	}

	[[nodiscard]] auto validBase(const Predicate &predicate, const Value &value) const -> bool
	{
		if (predicate.type == ValueType::Set) {
			const auto *set = std::get_if<EventSet>(&value);
			return set && set->size() == eventCount_;
		}
		const auto *relation = std::get_if<Relation>(&value);
		return relation && relation->size() == eventCount_;
	}

	void initializeValues()
	{
		for (const auto &predicate : model_.predicates()) {
			if (enableLazyCycles_ && analysis_.lazyCycleElided()[predicate.id])
				continue;
			if (predicate.kind != Predicate::Kind::Base) {
				values_[predicate.id] = emptyValue(predicate.type);
				continue;
			}
			/* Stable graph universes may contain inactive holes. Adapters can
			 * therefore override built-ins with the exact active `_` and `id`;
			 * standalone callers retain the dense-universe defaults. */
			if (const auto found = base_.find(predicate.name); found != base_.end()) {
				if (validBase(predicate, found->second))
					values_[predicate.id] = found->second;
				else
					errors_.push_back(
						{predicate.id, predicate.span,
						 "primitive '" + predicate.name +
							 "' has wrong type or event universe"});
			} else if (predicate.name == "0") {
				values_[predicate.id] = Relation(eventCount_);
			} else if (predicate.name == "_") {
				values_[predicate.id] = universe();
			} else if (predicate.name == "id") {
				values_[predicate.id] = identity(universe());
			} else {
				errors_.push_back(
					{predicate.id, predicate.span,
					 "missing CAT primitive '" + predicate.name + "'"});
			}
		}
	}

	[[nodiscard]] auto operation(const Predicate &predicate) const -> Value
	{
		const auto &operand = [&](std::size_t index) -> const Value & {
			return *values_[predicate.operands[index]];
		};
		switch (predicate.kind) {
		case Predicate::Kind::Alias:
			return operand(0);
		case Predicate::Kind::Identity:
			return identity(std::get<EventSet>(operand(0)));
		case Predicate::Kind::Domain:
			return domain(std::get<Relation>(operand(0)));
		case Predicate::Kind::Range:
			return range(std::get<Relation>(operand(0)));
		case Predicate::Kind::Inverse:
			return inverse(std::get<Relation>(operand(0)));
		case Predicate::Kind::Optional:
			return optional(std::get<Relation>(operand(0)));
		case Predicate::Kind::TransitiveClosure:
			return transitiveClosure(std::get<Relation>(operand(0)));
		case Predicate::Kind::ReflexiveTransitiveClosure:
			return reflexiveTransitiveClosure(std::get<Relation>(operand(0)));
		case Predicate::Kind::Product:
			return product(std::get<EventSet>(operand(0)),
				       std::get<EventSet>(operand(1)));
		case Predicate::Kind::Composition:
			return fastComposition_
				       ? composeFast(std::get<Relation>(operand(0)),
						     std::get<Relation>(operand(1)))
				       : compose(std::get<Relation>(operand(0)),
						 std::get<Relation>(operand(1)));
		case Predicate::Kind::Union:
		case Predicate::Kind::Intersection:
		case Predicate::Kind::Difference:
			break;
		case Predicate::Kind::Base:
			UNREACHABLE("base predicate reached CAAT operation evaluator");
		}
		if (predicate.type == ValueType::Set) {
			const auto &lhs = std::get<EventSet>(operand(0));
			const auto &rhs = std::get<EventSet>(operand(1));
			if (predicate.kind == Predicate::Kind::Union)
				return setUnion(lhs, rhs);
			if (predicate.kind == Predicate::Kind::Intersection)
				return setIntersection(lhs, rhs);
			return setDifference(lhs, rhs);
		}
		const auto &lhs = std::get<Relation>(operand(0));
		const auto &rhs = std::get<Relation>(operand(1));
		if (predicate.kind == Predicate::Kind::Union)
			return relationUnion(lhs, rhs);
		if (predicate.kind == Predicate::Kind::Intersection)
			return relationIntersection(lhs, rhs);
		return relationDifference(lhs, rhs);
	}

	void evaluateStratum(std::uint32_t stratum)
	{
		std::deque<PredicateId> worklist;
		std::vector<bool> queued(model_.predicates().size());
		const auto enqueue = [&](PredicateId predicate, auto &queue, auto &present,
					 auto &statistics) {
			if (!present[predicate]) {
				queue.push_back(predicate);
				present[predicate] = true;
				++statistics.worklistPushes;
			}
		};
		for (const auto predicate : analysis_.strata()[stratum]) {
			if (model_.predicates()[predicate].kind != Predicate::Kind::Base &&
			    !(enableLazyCycles_ && analysis_.lazyCycleElided()[predicate]))
				enqueue(predicate, worklist, queued, statistics_);
		}
		while (!worklist.empty()) {
			const auto id = worklist.front();
			worklist.pop_front();
			queued[id] = false;
			const auto &predicate = model_.predicates()[id];
			const auto operationStarted = profiling_ ? std::chrono::steady_clock::now()
								 : std::chrono::steady_clock::time_point{};
			const auto next = operation(predicate);
			const auto kind = static_cast<std::size_t>(predicate.kind);
			if (profiling_) {
				statistics_.operationNanosecondsByKind[kind] += elapsed(operationStarted);
				++statistics_.operationEvaluationsByKind[kind];
			}
			++counts_[id];
			++statistics_.operationEvaluations;
			const auto comparisonStarted = profiling_ ? std::chrono::steady_clock::now()
								  : std::chrono::steady_clock::time_point{};
			const bool unchanged = *values_[id] == next;
			if (profiling_)
				statistics_.valueComparisonNanoseconds += elapsed(comparisonStarted);
			if (unchanged)
				continue;
			values_[id] = next;
			++statistics_.valueChanges;
			for (const auto dependent : dependents_[id]) {
				if (analysis_.componentOf()[dependent] == stratum)
					enqueue(dependent, worklist, queued, statistics_);
			}
		}
	}

	[[nodiscard]] auto findCycle(const Relation &relation) const -> std::vector<std::size_t>
	{
		std::vector<std::uint8_t> color(eventCount_);
		std::vector<std::size_t> parent(eventCount_, eventCount_);
		std::vector<std::size_t> cycle;
		auto visit = [&](auto &self, std::size_t event) -> bool {
			color[event] = 1;
			for (auto target = fastCycleChecks_ ? relation.nextSuccessor(event, 0) : 0;
			     target < eventCount_;
			     target = fastCycleChecks_ ? relation.nextSuccessor(event, target + 1)
						       : target + 1) {
				if (!fastCycleChecks_ && !relation.contains(event, target))
					continue;
				if (color[target] == 0) {
					parent[target] = event;
					if (self(self, target))
						return true;
				} else if (color[target] == 1) {
					cycle.push_back(event);
					while (cycle.back() != target)
						cycle.push_back(parent[cycle.back()]);
					std::ranges::reverse(cycle);
					cycle.push_back(target);
					return true;
				}
			}
			color[event] = 2;
			return false;
		};
		for (std::size_t event = 0; event < eventCount_; ++event) {
			if (color[event] == 0 && visit(visit, event))
				break;
		}
		return cycle;
	}

	void evaluateChecks()
	{
		for (std::size_t checkIndex = 0; checkIndex < model_.checks().size(); ++checkIndex) {
			const auto &check = model_.checks()[checkIndex];
			if (enableLazyCycles_ && analysis_.lazyCycleRoots()[checkIndex]) {
				LazyCycleStatistics lazyStatistics;
				const auto root = *analysis_.lazyCycleRoots()[checkIndex];
				Relation retained(eventCount_);
				auto *materialized = retainedLazyRoot_ == root ? &retained : nullptr;
				auto witness = findLazyCycle(model_,
							 root, values_, eventCount_, &lazyStatistics,
							 materialized);
				statistics_.lazyCycleChecks += lazyStatistics.checks;
				statistics_.lazyEdgeCandidates += lazyStatistics.emittedCandidates;
				statistics_.lazyBaseCandidates += lazyStatistics.baseCandidates;
				statistics_.lazyDepthFallbacks += lazyStatistics.depthFallbacks;
				if (!witness.empty())
					violations_.push_back({check.name, check.kind, check.span,
							   std::move(witness)});
				else if (materialized)
					values_[root] = std::move(retained);
				continue;
			}
			const auto &value = *values_[check.predicate];
			std::vector<std::size_t> witness;
			if (check.kind == Statement::CheckKind::Empty) {
				if (const auto *set = std::get_if<EventSet>(&value)) {
					if (!set->empty())
						witness.push_back(set->first());
				} else {
					const auto &relation = std::get<Relation>(value);
					if (fastChecks_) {
						if (const auto pair = relation.firstPair())
							witness = {pair->first, pair->second};
					} else
						for (std::size_t from = 0;
						     from < eventCount_ && witness.empty(); ++from) {
							const auto target = relation.successors(from).first();
							if (target != eventCount_)
								witness = {from, target};
						}
				}
			} else if (check.kind == Statement::CheckKind::Irreflexive) {
				const auto &relation = std::get<Relation>(value);
				if (fastChecks_) {
					const auto event = relation.firstReflexive();
					if (event < eventCount_)
						witness.push_back(event);
				} else
					for (std::size_t event = 0; event < eventCount_; ++event) {
						if (relation.contains(event, event)) {
							witness.push_back(event);
							break;
						}
					}
			} else {
				witness = findCycle(std::get<Relation>(value));
			}
			if (!witness.empty())
				violations_.push_back(
					{check.name, check.kind, check.span, std::move(witness)});
		}
	}

	const NormalizedModel &model_;
	const ModelAnalysis &analysis_;
	std::size_t eventCount_{};
	const BaseValues &base_;
	std::vector<std::optional<Value>> values_;
	std::vector<std::size_t> counts_;
	std::vector<std::vector<PredicateId>> dependents_;
	std::vector<Violation> violations_;
	std::vector<EvaluationError> errors_;
	FixedPointStatistics statistics_;
	bool enableLazyCycles_{};
	std::optional<PredicateId> retainedLazyRoot_;
	bool profiling_{};
	bool fastChecks_{};
	bool fastComposition_{};
	bool fastCycleChecks_{};
};

} /* namespace */

auto CaatEvaluator::evaluate(const NormalizedModel &model, const ModelAnalysis &analysis,
			     std::size_t eventCount, const BaseValues &base,
			     bool enableLazyCycles,
			     std::optional<PredicateId> retainedLazyRoot, bool profiling,
			     bool fastChecks, bool fastComposition, bool fastCycleChecks) const
	-> CaatEvaluationResult
{
	return Evaluation(model, analysis, eventCount, base, enableLazyCycles,
			  retainedLazyRoot, profiling, fastChecks, fastComposition,
			  fastCycleChecks)
		.run();
}

} /* namespace cat */
