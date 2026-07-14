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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace cat {
namespace {

/** Allocate a process-wide ID so one worker's handle cannot match another. */
auto allocateCheckpoint() -> IncrementalCheckpoint
{
	static std::atomic<std::uint64_t> next{1};
	return {next.fetch_add(1, std::memory_order_relaxed)};
}

/** Grow one typed value copy to a common stable event universe. */
void growValue(Value &value, std::size_t size)
{
	if (auto *set = std::get_if<EventSet>(&value))
		set->grow(size);
	else
		std::get<Relation>(value).grow(size);
}

/** Return whether every membership in @p subset also occurs in @p superset. */
auto valueSubset(const Value &subset, const Value &superset) -> bool
{
	if (const auto *lhs = std::get_if<EventSet>(&subset)) {
		const auto *rhs = std::get_if<EventSet>(&superset);
		if (!rhs || lhs->size() != rhs->size())
			return false;
		for (std::size_t event = 0; event < lhs->size(); ++event) {
			if (lhs->contains(event) && !rhs->contains(event))
				return false;
		}
		return true;
	}
	const auto *lhs = std::get_if<Relation>(&subset);
	const auto *rhs = std::get_if<Relation>(&superset);
	if (!rhs || lhs->size() != rhs->size())
		return false;
	for (std::size_t from = 0; from < lhs->size(); ++from) {
		for (std::size_t to = 0; to < lhs->size(); ++to) {
			if (lhs->contains(from, to) && !rhs->contains(from, to))
				return false;
		}
	}
	return true;
}

/** Construct the complete active event set used by `_` and `id`. */
auto universe(std::size_t size) -> EventSet
{
	EventSet result(size);
	for (std::size_t event = 0; event < size; ++event)
		result.insert(event);
	return result;
}

/** Resolve one base predicate from built-ins or the supplied graph snapshot. */
auto baseValue(const Predicate &predicate, std::size_t eventCount, const BaseValues &base)
	-> std::optional<Value>
{
	if (predicate.name == "0")
		return Relation(eventCount);
	if (predicate.name == "_")
		return universe(eventCount);
	if (predicate.name == "id")
		return identity(universe(eventCount));
	const auto found = base.find(predicate.name);
	if (found == base.end())
		return std::nullopt;
	if (predicate.type == ValueType::Set) {
		const auto *set = std::get_if<EventSet>(&found->second);
		if (!set || set->size() != eventCount)
			return std::nullopt;
	} else {
		const auto *relation = std::get_if<Relation>(&found->second);
		if (!relation || relation->size() != eventCount)
			return std::nullopt;
	}
	return found->second;
}

/** Evaluate one positive normalized equation over the current approximation. */
auto operation(const Predicate &predicate, const std::vector<std::optional<Value>> &values) -> Value
{
	const auto &operand = [&](std::size_t index) -> const Value & {
		return *values[predicate.operands[index]];
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
		return product(std::get<EventSet>(operand(0)), std::get<EventSet>(operand(1)));
	case Predicate::Kind::Composition:
		return compose(std::get<Relation>(operand(0)), std::get<Relation>(operand(1)));
	case Predicate::Kind::Union:
	case Predicate::Kind::Intersection:
	case Predicate::Kind::Difference:
		break;
	case Predicate::Kind::Base:
		UNREACHABLE("base predicate reached incremental CAAT operation evaluator");
	}
	VERIFY(predicate.kind != Predicate::Kind::Difference,
	       "non-monotone difference reached insertion propagation");
	if (predicate.type == ValueType::Set) {
		const auto &lhs = std::get<EventSet>(operand(0));
		const auto &rhs = std::get<EventSet>(operand(1));
		return predicate.kind == Predicate::Kind::Union ? setUnion(lhs, rhs)
								: setIntersection(lhs, rhs);
	}
	const auto &lhs = std::get<Relation>(operand(0));
	const auto &rhs = std::get<Relation>(operand(1));
	return predicate.kind == Predicate::Kind::Union ? relationUnion(lhs, rhs)
							: relationIntersection(lhs, rhs);
}

/** Find one deterministic directed cycle, encoded with the start repeated. */
auto findCycle(const Relation &relation) -> std::vector<std::size_t>
{
	std::vector<std::uint8_t> color(relation.size());
	std::vector<std::size_t> parent(relation.size(), relation.size());
	std::vector<std::size_t> cycle;
	auto visit = [&](auto &self, std::size_t event) -> bool {
		color[event] = 1;
		for (std::size_t target = 0; target < relation.size(); ++target) {
			if (!relation.contains(event, target))
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
	for (std::size_t event = 0; event < relation.size(); ++event) {
		if (color[event] == 0 && visit(visit, event))
			break;
	}
	return cycle;
}

/** Recompute small axiom witnesses from the final incrementally maintained values. */
auto violations(const NormalizedModel &model, const std::vector<std::optional<Value>> &values,
		std::size_t eventCount) -> std::vector<Violation>
{
	std::vector<Violation> result;
	for (const auto &check : model.checks()) {
		const auto &value = *values[check.predicate];
		std::vector<std::size_t> witness;
		if (check.kind == Statement::CheckKind::Empty) {
			if (const auto *set = std::get_if<EventSet>(&value)) {
				if (!set->empty())
					witness.push_back(set->first());
			} else {
				const auto &relation = std::get<Relation>(value);
				for (std::size_t from = 0; from < eventCount && witness.empty();
				     ++from) {
					const auto target = relation.successors(from).first();
					if (target != eventCount)
						witness = {from, target};
				}
			}
		} else if (check.kind == Statement::CheckKind::Irreflexive) {
			const auto &relation = std::get<Relation>(value);
			for (std::size_t event = 0; event < eventCount; ++event) {
				if (relation.contains(event, event)) {
					witness.push_back(event);
					break;
				}
			}
		} else {
			witness = findCycle(std::get<Relation>(value));
		}
		if (!witness.empty())
			result.push_back({check.name, check.kind, check.span, std::move(witness)});
	}
	return result;
}

} /* namespace */

IncrementalCaatEvaluator::IncrementalCaatEvaluator(const NormalizedModel &model,
						   const ModelAnalysis &analysis)
	: model_(model), analysis_(analysis)
{
	VERIFY(analysis_.componentOf().size() == model_.predicates().size(),
	       "incremental CAAT analysis/model predicate count mismatch");
	supportsInsertions_ = std::ranges::none_of(model_.predicates(), [](const auto &predicate) {
		return predicate.kind == Predicate::Kind::Difference;
	});
}

auto IncrementalCaatEvaluator::initialize(std::size_t eventCount, const BaseValues &base)
	-> const CaatEvaluationResult &
{
	/* Compute into a temporary first. An evaluation error is a valid published
	 * result, but an exception or assertion cannot leave a mixed old/new state. */
	auto next = CaatEvaluator().evaluate(model_, analysis_, eventCount, base);
	eventCount_ = eventCount;
	base_ = base;
	result_ = std::move(next);
	checkpoints_.clear();
	++statistics_.initializations;
	++statistics_.offlineEvaluations;
	return *result_;
}

auto IncrementalCaatEvaluator::tryInsert(std::size_t eventCount, const BaseValues &base)
	-> IncrementalUpdateResult
{
	const auto reject = [&](std::string reason) {
		++statistics_.rejectedUpdates;
		return IncrementalUpdateResult{IncrementalUpdateStatus::RequiresRebuild,
					       std::move(reason)};
	};
	if (!initialized())
		return reject("incremental CAAT state has not been initialized");
	if (!supportsInsertions_)
		return reject("normalized model contains non-monotone difference");
	if (!result_->errors.empty())
		return reject("current CAAT state contains evaluation errors");
	if (eventCount < eventCount_)
		return reject("event universe shrank");

	/* All work happens on copies. This transactional boundary is essential:
	 * discovering a deletion halfway through base validation cannot corrupt the
	 * last exact checkpoint that the synchronizer may still need. */
	auto values = result_->values;
	auto counts = result_->evaluationCounts;
	for (auto &value : values) {
		VERIFY(value.has_value(), "initialized CAAT predicate has no value");
		growValue(*value, eventCount);
	}

	std::vector<std::vector<PredicateId>> dependents(model_.predicates().size());
	for (const auto &dependency : analysis_.dependencies())
		dependents[dependency.source].push_back(dependency.target);
	std::deque<PredicateId> worklist;
	std::vector<bool> queued(model_.predicates().size());
	FixedPointStatistics updateStatistics;
	const auto enqueue = [&](PredicateId id) {
		if (!queued[id]) {
			worklist.push_back(id);
			queued[id] = true;
			++updateStatistics.worklistPushes;
		}
	};
	if (eventCount > eventCount_) {
		/* `r?` and `r*` add identity for every event even when `r` itself did
		 * not change. Their dependency edge alone cannot expose domain growth. */
		for (const auto &predicate : model_.predicates()) {
			if (predicate.kind == Predicate::Kind::Optional ||
			    predicate.kind == Predicate::Kind::ReflexiveTransitiveClosure)
				enqueue(predicate.id);
		}
	}

	/* Base values are assigned only after proving that the old grown value is
	 * a subset. Any missing fact is a deletion and must use rollback/rebuild. */
	for (const auto &predicate : model_.predicates()) {
		if (predicate.kind != Predicate::Kind::Base)
			continue;
		auto next = baseValue(predicate, eventCount, base);
		if (!next)
			return reject("primitive '" + predicate.name +
				      "' is missing or has the wrong type/universe");
		if (!valueSubset(*values[predicate.id], *next))
			return reject("primitive '" + predicate.name + "' removed a fact");
		if (*values[predicate.id] == *next)
			continue;
		values[predicate.id] = std::move(*next);
		++updateStatistics.valueChanges;
		for (const auto dependent : dependents[predicate.id])
			enqueue(dependent);
	}

	/* Re-evaluating an affected operator is deliberately correctness-first.
	 * Only strict growth is published to its users, which is the CAAT delta
	 * worklist contract; later profiling may specialize individual operators. */
	while (!worklist.empty()) {
		const auto id = worklist.front();
		worklist.pop_front();
		queued[id] = false;
		const auto next = operation(model_.predicates()[id], values);
		++counts[id];
		++updateStatistics.operationEvaluations;
		if (!valueSubset(*values[id], next))
			return reject("derived predicate '" + model_.predicates()[id].name +
				      "' was non-monotone");
		if (*values[id] == next)
			continue;
		values[id] = next;
		++updateStatistics.valueChanges;
		for (const auto dependent : dependents[id])
			enqueue(dependent);
	}

	CaatEvaluationResult nextResult{violations(model_, values, eventCount),
					{},
					std::move(values),
					std::move(counts),
					updateStatistics};
	eventCount_ = eventCount;
	base_ = base;
	result_ = std::move(nextResult);
	++statistics_.insertionUpdates;
	statistics_.operationEvaluations += updateStatistics.operationEvaluations;
	statistics_.valueChanges += updateStatistics.valueChanges;
	statistics_.worklistPushes += updateStatistics.worklistPushes;
	return {IncrementalUpdateStatus::Applied, {}};
}

auto IncrementalCaatEvaluator::checkpoint() -> IncrementalCheckpoint
{
	VERIFY(initialized(), "incremental CAAT state has not been initialized");
	const auto handle = allocateCheckpoint();
	checkpoints_.push_back({handle, eventCount_, base_, *result_});
	++statistics_.checkpoints;
	return handle;
}

auto IncrementalCaatEvaluator::rollback(IncrementalCheckpoint checkpoint)
	-> IncrementalRollbackResult
{
	const auto found = std::ranges::find(checkpoints_, checkpoint, &Snapshot::checkpoint);
	if (found == checkpoints_.end()) {
		++statistics_.rejectedRollbacks;
		return {false, "incremental CAAT checkpoint is stale or belongs to another state"};
	}

	/* Publish the complete snapshot before discarding descendants. Exact value
	 * restoration also replaces violation witnesses, evaluation counters, and
	 * every primitive fact, so no explanation can observe a mixed epoch. */
	eventCount_ = found->eventCount;
	base_ = found->base;
	result_ = found->result;
	checkpoints_.erase(std::next(found), checkpoints_.end());
	++statistics_.rollbacks;
	return {true, {}};
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
