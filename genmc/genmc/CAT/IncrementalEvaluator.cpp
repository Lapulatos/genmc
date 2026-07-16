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

#include "genmc/CAT/LazyCycle.hpp"
#include "genmc/Support/Error.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
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
	return rhs && lhs->isSubsetOf(*rhs);
}

/** Return the facts present in @p value but absent from @p previous. */
auto valueDifference(const Value &value, const Value &previous) -> Value
{
	if (const auto *set = std::get_if<EventSet>(&value))
		return setDifference(*set, std::get<EventSet>(previous));
	return relationDifference(std::get<Relation>(value), std::get<Relation>(previous));
}

/** Accumulate insertion facts of the same type and universe. */
void mergeFacts(Value &target, const Value &added)
{
	if (const auto *set = std::get_if<EventSet>(&target))
		target = setUnion(*set, std::get<EventSet>(added));
	else
		target = relationUnion(std::get<Relation>(target), std::get<Relation>(added));
}

/** Remove one insertion delta and restore the preceding event universe. */
void removeFactsAndShrink(Value &target, const Value &added, std::size_t size)
{
	if (auto *set = std::get_if<EventSet>(&target)) {
		const auto &delta = std::get<EventSet>(added);
		for (std::size_t event = 0; event < delta.size(); ++event) {
			if (delta.contains(event))
				set->erase(event);
		}
		set->shrink(size);
		return;
	}
	auto &relation = std::get<Relation>(target);
	const auto &delta = std::get<Relation>(added);
	for (std::size_t from = 0; from < delta.size(); ++from) {
		for (std::size_t to = 0; to < delta.size(); ++to) {
			if (delta.contains(from, to))
				relation.erase(from, to);
		}
	}
	relation.shrink(size);
}

/** Shrink a value whose insertion delta was empty. */
void shrinkValue(Value &value, std::size_t size)
{
	if (auto *set = std::get_if<EventSet>(&value))
		set->shrink(size);
	else
		std::get<Relation>(value).shrink(size);
}

auto valueStorageBytes(const Value &value) -> std::size_t
{
	return std::visit([](const auto &typed) { return typed.storageBytes(); }, value);
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
	const auto found = base.find(predicate.name);
	if (found == base.end()) {
		if (predicate.name == "0")
			return Relation(eventCount);
		if (predicate.name == "_")
			return universe(eventCount);
		if (predicate.name == "id")
			return identity(universe(eventCount));
		return std::nullopt;
	}
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
auto violations(const NormalizedModel &model, const ModelAnalysis &analysis,
		const std::vector<std::optional<Value>> &values, std::size_t eventCount,
		bool enableLazyCycles, FixedPointStatistics *statistics) -> std::vector<Violation>
{
	std::vector<Violation> result;
	for (std::size_t checkIndex = 0; checkIndex < model.checks().size(); ++checkIndex) {
		const auto &check = model.checks()[checkIndex];
		if (enableLazyCycles && analysis.lazyCycleRoots()[checkIndex]) {
			LazyCycleStatistics lazyStatistics;
			auto witness = findLazyCycle(model, *analysis.lazyCycleRoots()[checkIndex],
						 values, eventCount, &lazyStatistics);
			statistics->lazyCycleChecks += lazyStatistics.checks;
			statistics->lazyEdgeCandidates += lazyStatistics.emittedCandidates;
			statistics->lazyDepthFallbacks += lazyStatistics.depthFallbacks;
			if (!witness.empty())
				result.push_back({check.name, check.kind, check.span,
						  std::move(witness)});
			continue;
		}
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
						   const ModelAnalysis &analysis, bool profiling,
						   bool enableLazyCycles)
	: model_(model), analysis_(analysis), dependents_(model.predicates().size()),
	  profiling_(profiling), enableLazyCycles_(enableLazyCycles)
{
	VERIFY(analysis_.componentOf().size() == model_.predicates().size(),
	       "incremental CAAT analysis/model predicate count mismatch");
	for (const auto &dependency : analysis_.dependencies())
		dependents_[dependency.source].push_back(dependency.target);
	supportsInsertions_ = std::ranges::none_of(model_.predicates(), [](const auto &predicate) {
		return predicate.kind == Predicate::Kind::Difference;
	});
	for (const auto &predicate : model_.predicates()) {
		if (predicate.kind != Predicate::Kind::Base &&
		    predicate.kind != Predicate::Kind::Alias &&
		    predicate.kind != Predicate::Kind::Union)
			supportsReplacements_ = false;
	}
	for (const auto &stratum : analysis_.strata()) {
		if (stratum.size() > 1)
			supportsReplacements_ = false;
		if (stratum.size() == 1) {
			const auto id = stratum.front();
			if (std::ranges::any_of(model_.predicates()[id].operands,
						[id](auto operand) { return operand == id; }))
				supportsReplacements_ = false;
		}
	}
}

auto IncrementalCaatEvaluator::initialize(std::size_t eventCount, const BaseValues &base)
	-> const CaatEvaluationResult &
{
	const auto started = profiling_ ? std::chrono::steady_clock::now()
					: std::chrono::steady_clock::time_point{};
	/* Compute into a temporary first. An evaluation error is a valid published
	 * result, but an exception or assertion cannot leave a mixed old/new state. */
	auto next = CaatEvaluator().evaluate(model_, analysis_, eventCount, base,
					     enableLazyCycles_);
	eventCount_ = eventCount;
	base_ = base;
	result_ = std::move(next);
	statistics_.lazyCycleChecks += result_->statistics.lazyCycleChecks;
	statistics_.lazyEdgeCandidates += result_->statistics.lazyEdgeCandidates;
	statistics_.lazyDepthFallbacks += result_->statistics.lazyDepthFallbacks;
	checkpoints_.clear();
	undoTrail_.clear();
	++statistics_.initializations;
	++statistics_.offlineEvaluations;
	if (profiling_)
		statistics_.offlineNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started)
				.count());
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
	const auto copyStarted = profiling_ ? std::chrono::steady_clock::now()
					    : std::chrono::steady_clock::time_point{};
	auto values = result_->values;
	auto counts = result_->evaluationCounts;
	UndoDelta undo{eventCount_,
		       {},
		       {},
		       std::vector<std::optional<Value>>(model_.predicates().size()),
		       std::vector<std::size_t>(model_.predicates().size()),
		       result_->violations,
		       result_->errors,
		       result_->statistics};
	for (auto &value : values) {
		if (value)
			growValue(*value, eventCount);
	}
	if (profiling_)
		statistics_.transactionalCopyNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - copyStarted)
				.count());
	/* Graph synchronization must classify mutations across every primitive,
	 * including relations not referenced by the current model. Otherwise an rf,
	 * co, or label-category replacement could be mistaken for insertion merely
	 * because dead-code elimination omitted that base from normalized IR. */
	for (const auto &[name, oldValue] : base_) {
		const auto found = base.find(name);
		if (found == base.end())
			return reject("primitive '" + name + "' was removed");
		auto grown = oldValue;
		growValue(grown, eventCount);
		if (!valueSubset(grown, found->second))
			return reject("primitive '" + name + "' removed a fact");
	}
	/* Retain only newly inserted primitive facts. Extra primitive names are
	 * recorded so rollback can erase them rather than snapshotting the map. */
	for (const auto &[name, nextValue] : base) {
		auto previous = nextValue;
		if (const auto found = base_.find(name); found != base_.end()) {
			previous = found->second;
			growValue(previous, eventCount);
		} else {
			undo.newBaseNames.push_back(name);
			previous = std::holds_alternative<EventSet>(nextValue)
					   ? Value{EventSet(eventCount)}
					   : Value{Relation(eventCount)};
		}
		auto added = valueDifference(nextValue, previous);
		const auto empty =
			std::visit([](const auto &facts) { return facts.empty(); }, added);
		if (!empty)
			undo.addedBase.emplace(name, std::move(added));
	}

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
			    predicate.kind == Predicate::Kind::ReflexiveTransitiveClosure) {
				if (enableLazyCycles_ && analysis_.lazyCycleElided()[predicate.id])
					continue;
				enqueue(predicate.id);
			}
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
		undo.addedValues[predicate.id] = valueDifference(*next, *values[predicate.id]);
		values[predicate.id] = std::move(*next);
		++updateStatistics.valueChanges;
		for (const auto dependent : dependents_[predicate.id]) {
			if (!(enableLazyCycles_ && analysis_.lazyCycleElided()[dependent]))
				enqueue(dependent);
		}
	}

	/* Re-evaluating an affected operator is deliberately correctness-first.
	 * Only strict growth is published to its users, which is the CAAT delta
	 * worklist contract; later profiling may specialize individual operators. */
	const auto worklistStarted = profiling_ ? std::chrono::steady_clock::now()
						: std::chrono::steady_clock::time_point{};
	while (!worklist.empty()) {
		const auto id = worklist.front();
		worklist.pop_front();
		queued[id] = false;
		const auto next = operation(model_.predicates()[id], values);
		++counts[id];
		++undo.evaluationCountDeltas[id];
		++updateStatistics.operationEvaluations;
		if (!valueSubset(*values[id], next))
			return reject("derived predicate '" + model_.predicates()[id].name +
				      "' was non-monotone");
		if (*values[id] == next)
			continue;
		auto added = valueDifference(next, *values[id]);
		if (undo.addedValues[id])
			mergeFacts(*undo.addedValues[id], added);
		else
			undo.addedValues[id] = std::move(added);
		values[id] = next;
		++updateStatistics.valueChanges;
		for (const auto dependent : dependents_[id]) {
			if (!(enableLazyCycles_ && analysis_.lazyCycleElided()[dependent]))
				enqueue(dependent);
		}
	}
	if (profiling_)
		statistics_.worklistNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - worklistStarted)
				.count());

	CaatEvaluationResult nextResult{violations(model_, analysis_, values, eventCount,
						enableLazyCycles_, &updateStatistics),
					{},
					std::move(values),
					std::move(counts),
					updateStatistics};
	eventCount_ = eventCount;
	base_ = base;
	result_ = std::move(nextResult);
	undoTrail_.push_back(std::move(undo));
	++statistics_.insertionUpdates;
	statistics_.operationEvaluations += updateStatistics.operationEvaluations;
	statistics_.valueChanges += updateStatistics.valueChanges;
	statistics_.worklistPushes += updateStatistics.worklistPushes;
	statistics_.lazyCycleChecks += updateStatistics.lazyCycleChecks;
	statistics_.lazyEdgeCandidates += updateStatistics.lazyEdgeCandidates;
	statistics_.lazyDepthFallbacks += updateStatistics.lazyDepthFallbacks;
	return {IncrementalUpdateStatus::Applied, {}};
}

auto IncrementalCaatEvaluator::tryReplace(std::size_t eventCount, const BaseValues &base)
	-> IncrementalUpdateResult
{
	const auto reject = [&](std::string reason) {
		++statistics_.rejectedUpdates;
		return IncrementalUpdateResult{IncrementalUpdateStatus::RequiresRebuild,
					       std::move(reason)};
	};
	if (!initialized())
		return reject("incremental CAAT state has not been initialized");
	if (!supportsReplacements_)
		return reject("replacement closure needs operators beyond acyclic alias/union");
	if (eventCount != eventCount_)
		return reject("replacement changed the event universe");

	auto values = result_->values;
	auto counts = result_->evaluationCounts;
	std::deque<PredicateId> worklist;
	std::vector<bool> queued(model_.predicates().size());
	const auto enqueue = [&](PredicateId id) {
		if (!queued[id]) {
			queued[id] = true;
			worklist.push_back(id);
		}
	};
	for (const auto &predicate : model_.predicates()) {
		if (predicate.kind != Predicate::Kind::Base)
			continue;
		auto next = baseValue(predicate, eventCount, base);
		if (!next)
			return reject("replacement primitive is missing or ill-typed");
		if (*values[predicate.id] == *next)
			continue;
		values[predicate.id] = std::move(*next);
		for (const auto dependent : dependents_[predicate.id]) {
			if (!(enableLazyCycles_ && analysis_.lazyCycleElided()[dependent]))
				enqueue(dependent);
		}
	}
	FixedPointStatistics updateStatistics;
	while (!worklist.empty()) {
		const auto id = worklist.front();
		worklist.pop_front();
		queued[id] = false;
		const auto next = operation(model_.predicates()[id], values);
		++counts[id];
		++updateStatistics.operationEvaluations;
		if (*values[id] == next)
			continue;
		values[id] = next;
		++updateStatistics.valueChanges;
		for (const auto dependent : dependents_[id]) {
			if (!(enableLazyCycles_ && analysis_.lazyCycleElided()[dependent]))
				enqueue(dependent);
		}
	}
	result_ = CaatEvaluationResult{violations(model_, analysis_, values, eventCount,
					       enableLazyCycles_, &updateStatistics),
				       {},
				       std::move(values),
				       std::move(counts),
				       updateStatistics};
	base_ = base;
	checkpoints_.clear();
	undoTrail_.clear();
	++statistics_.replacementUpdates;
	statistics_.operationEvaluations += updateStatistics.operationEvaluations;
	statistics_.valueChanges += updateStatistics.valueChanges;
	statistics_.lazyCycleChecks += updateStatistics.lazyCycleChecks;
	statistics_.lazyEdgeCandidates += updateStatistics.lazyEdgeCandidates;
	statistics_.lazyDepthFallbacks += updateStatistics.lazyDepthFallbacks;
	return {IncrementalUpdateStatus::Applied, {}};
}

auto IncrementalCaatEvaluator::checkpoint() -> IncrementalCheckpoint
{
	const auto started = profiling_ ? std::chrono::steady_clock::now()
					: std::chrono::steady_clock::time_point{};
	VERIFY(initialized(), "incremental CAAT state has not been initialized");
	compactUndoTrail();
	const auto handle = allocateCheckpoint();
	std::size_t snapshotEquivalentBytes{};
	if (profiling_) {
		for (const auto &[name, value] : base_)
			snapshotEquivalentBytes += valueStorageBytes(value);
		for (const auto &value : result_->values) {
			if (value)
				snapshotEquivalentBytes += valueStorageBytes(*value);
		}
		snapshotEquivalentBytes += result_->evaluationCounts.size() * sizeof(std::size_t);
	}
	checkpoints_.push_back({handle, undoTrail_.size(), snapshotEquivalentBytes});
	updateCheckpointMemoryStatistics();
	++statistics_.checkpoints;
	if (profiling_)
		statistics_.checkpointNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started)
				.count());
	return handle;
}

auto IncrementalCaatEvaluator::rollback(IncrementalCheckpoint checkpoint)
	-> IncrementalRollbackResult
{
	const auto started = profiling_ ? std::chrono::steady_clock::now()
					: std::chrono::steady_clock::time_point{};
	const auto found = std::ranges::find(checkpoints_, checkpoint, &Snapshot::checkpoint);
	if (found == checkpoints_.end()) {
		++statistics_.rejectedRollbacks;
		return {false, "incremental CAAT checkpoint is stale or belongs to another state"};
	}

	const auto target = found->trailIndex;
	while (undoTrail_.size() > target) {
		auto &undo = undoTrail_.back();
		for (const auto &[name, added] : undo.addedBase) {
			auto current = base_.find(name);
			VERIFY(current != base_.end(), "CAAT undo base predicate is missing");
			removeFactsAndShrink(current->second, added, undo.previousEventCount);
		}
		for (auto &[name, value] : base_) {
			if (!undo.addedBase.contains(name))
				shrinkValue(value, undo.previousEventCount);
		}
		for (const auto &name : undo.newBaseNames)
			base_.erase(name);
		for (std::size_t id = 0; id < result_->values.size(); ++id) {
			if (!result_->values[id])
				continue;
			auto &value = *result_->values[id];
			if (undo.addedValues[id])
				removeFactsAndShrink(value, *undo.addedValues[id],
						     undo.previousEventCount);
			else
				shrinkValue(value, undo.previousEventCount);
			VERIFY(result_->evaluationCounts[id] >= undo.evaluationCountDeltas[id],
			       "CAAT undo evaluation count underflow");
			result_->evaluationCounts[id] -= undo.evaluationCountDeltas[id];
		}
		result_->violations = std::move(undo.previousViolations);
		result_->errors = std::move(undo.previousErrors);
		result_->statistics = undo.previousFixedPointStatistics;
		eventCount_ = undo.previousEventCount;
		undoTrail_.pop_back();
	}
	checkpoints_.erase(std::next(found), checkpoints_.end());
	++statistics_.rollbacks;
	if (profiling_)
		statistics_.rollbackNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started)
				.count());
	compactUndoTrail();
	updateCheckpointMemoryStatistics();
	return {true, {}};
}

auto IncrementalCaatEvaluator::forget(IncrementalCheckpoint checkpoint) -> bool
{
	const auto found = std::ranges::find(checkpoints_, checkpoint, &Snapshot::checkpoint);
	if (found == checkpoints_.end())
		return false;
	checkpoints_.erase(found);
	compactUndoTrail();
	updateCheckpointMemoryStatistics();
	return true;
}

void IncrementalCaatEvaluator::compactUndoTrail()
{
	if (checkpoints_.empty()) {
		undoTrail_.clear();
		return;
	}
	const auto firstNeeded =
		std::ranges::min(checkpoints_, {}, &Snapshot::trailIndex).trailIndex;
	if (firstNeeded == 0)
		return;
	undoTrail_.erase(undoTrail_.begin(),
			 std::next(undoTrail_.begin(), static_cast<std::ptrdiff_t>(firstNeeded)));
	for (auto &snapshot : checkpoints_)
		snapshot.trailIndex -= firstNeeded;
}

void IncrementalCaatEvaluator::updateCheckpointMemoryStatistics()
{
	if (!profiling_)
		return;
	std::size_t undoBytes{};
	for (const auto &undo : undoTrail_) {
		for (const auto &[name, value] : undo.addedBase)
			undoBytes += valueStorageBytes(value);
		for (const auto &value : undo.addedValues) {
			if (value)
				undoBytes += valueStorageBytes(*value);
		}
		undoBytes += undo.evaluationCountDeltas.size() * sizeof(std::size_t);
	}
	std::size_t snapshotBytes{};
	for (const auto &checkpoint : checkpoints_)
		snapshotBytes += checkpoint.snapshotEquivalentBytes;
	statistics_.retainedUndoBytes = undoBytes;
	statistics_.retainedSnapshotEquivalentBytes = snapshotBytes;
	statistics_.peakRetainedUndoBytes = std::max(statistics_.peakRetainedUndoBytes, undoBytes);
	statistics_.peakRetainedSnapshotEquivalentBytes =
		std::max(statistics_.peakRetainedSnapshotEquivalentBytes, snapshotBytes);
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

auto IncrementalCaatEvaluator::offlineOracleMismatch() const -> std::optional<std::string>
{
	VERIFY(initialized(), "incremental CAAT oracle requested before initialization");
	const auto offline = CaatEvaluator().evaluate(model_, analysis_, eventCount_, base_,
						      enableLazyCycles_);
	if (offline.errors.size() != result_->errors.size())
		return "error-count incremental=" + std::to_string(result_->errors.size()) +
		       " offline=" + std::to_string(offline.errors.size());
	for (std::size_t index = 0; index < offline.errors.size(); ++index) {
		const auto &expected = offline.errors[index];
		const auto &actual = result_->errors[index];
		if (expected.node != actual.node || expected.span != actual.span ||
		    expected.message != actual.message)
			return "error=" + std::to_string(index) + " diagnostic differs";
	}
	if (offline.values.size() != result_->values.size())
		return "value-count incremental=" + std::to_string(result_->values.size()) +
		       " offline=" + std::to_string(offline.values.size());
	for (std::size_t predicate = 0; predicate < offline.values.size(); ++predicate) {
		if (offline.values[predicate] != result_->values[predicate])
			return "predicate=" + std::to_string(predicate) + " value differs";
	}
	if (offline.violations.size() != result_->violations.size())
		return "violation-count incremental=" + std::to_string(result_->violations.size()) +
		       " offline=" + std::to_string(offline.violations.size());
	for (std::size_t index = 0; index < offline.violations.size(); ++index) {
		const auto &expected = offline.violations[index];
		const auto &actual = result_->violations[index];
		if (expected.checkName != actual.checkName ||
		    expected.checkKind != actual.checkKind || expected.span != actual.span ||
		    expected.witness != actual.witness)
			return "violation=" + std::to_string(index) + " witness differs";
	}
	return std::nullopt;
}

} /* namespace cat */
