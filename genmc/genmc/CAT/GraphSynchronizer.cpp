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

#include "genmc/CAT/GraphSynchronizer.hpp"

#include "genmc/Support/Error.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>

namespace cat {
namespace {

auto valueStorageBytes(const Value &value) -> std::size_t
{
	return std::visit([](const auto &typed) { return typed.storageBytes(); }, value);
}

auto baseStorageBytes(const BaseValues &base) -> std::size_t
{
	return std::accumulate(base.begin(), base.end(), std::size_t{},
			       [](std::size_t bytes, const auto &entry) {
				       return bytes + valueStorageBytes(entry.second);
			       });
}

/** Compare complete primitive maps after allowing only universe growth. */
auto baseSubset(std::size_t oldSize, const BaseValues &oldBase, std::size_t newSize,
		const BaseValues &newBase) -> bool
{
	if (oldSize > newSize || oldBase.size() != newBase.size())
		return false;
	for (const auto &[name, oldValue] : oldBase) {
		const auto found = newBase.find(name);
		if (found == newBase.end() || oldValue.index() != found->second.index())
			return false;
		if (const auto *oldSet = std::get_if<EventSet>(&oldValue)) {
			const auto &newSet = std::get<EventSet>(found->second);
			for (std::size_t event = 0; event < oldSize; ++event) {
				if (oldSet->contains(event) && !newSet.contains(event))
					return false;
			}
		} else {
			const auto &oldRelation = std::get<Relation>(oldValue);
			const auto &newRelation = std::get<Relation>(found->second);
			if (!oldRelation.isStructural() && !newRelation.isStructural()) {
				for (std::size_t from = 0; from < oldSize; ++from) {
					for (std::size_t to = 0; to < oldSize; ++to) {
						if (oldRelation.contains(from, to) &&
						    !newRelation.contains(from, to))
							return false;
					}
				}
			} else {
				auto grown = oldRelation;
				grown.grow(newSize);
				if (!grown.isSubsetOf(newRelation))
					return false;
			}
		}
	}
	return true;
}

/** Return a stable diagnostic spelling for a synchronization transition. */
auto transitionName(GraphTransition transition) -> const char *
{
	switch (transition) {
	case GraphTransition::Initialize:
		return "initialize";
	case GraphTransition::Unchanged:
		return "unchanged";
	case GraphTransition::Insert:
		return "insert";
	case GraphTransition::Rollback:
		return "rollback";
	case GraphTransition::RollbackInsert:
		return "rollback-insert";
	case GraphTransition::Replace:
		return "replace";
	case GraphTransition::Rebuild:
		return "rebuild";
	}
	UNREACHABLE("unknown CAAT graph transition");
}

} /* namespace */

GraphSynchronizer::GraphSynchronizer(IncrementalCaatEvaluator &evaluator,
				     std::size_t checkpointLimit, std::size_t oracleInterval,
				     bool profiling, std::vector<std::string> requiredPrimitives,
				     std::size_t adaptiveOfflineEventLimit)
	: evaluator_(&evaluator), stable_(std::move(requiredPrimitives)),
	  checkpointLimit_(checkpointLimit), oracleInterval_(oracleInterval),
	  adaptiveOfflineEventLimit_(adaptiveOfflineEventLimit), profiling_(profiling)
{
	VERIFY(checkpointLimit_ > 0, "CAAT graph synchronizer needs one checkpoint");
}

void GraphSynchronizer::verifyCurrent(GraphTransition transition)
{
	++queryCount_;
	if (oracleInterval_ == 0 || queryCount_ % oracleInterval_ != 0)
		return;
	++statistics_.oracleChecks;
	const auto mismatch = evaluator_->offlineOracleMismatch();
	if (!mismatch)
		return;
	/* Keep the dump independent of hash iteration and source addresses so the
	 * failing query can be compared across workers and reruns. */
	std::cerr << "CAAT oracle mismatch: query=" << queryCount_
		  << " transition=" << transitionName(transition)
		  << " events=" << evaluator_->eventCount() << " detail=" << *mismatch << '\n';
	VERIFY(false, "incremental CAAT state diverged from Phase 2 oracle");
}

void GraphSynchronizer::clearHistory() { history_.clear(); }

void GraphSynchronizer::recordSpaceStatistics(const StableGraphSnapshot &stable)
{
	statistics_.maximumActiveEvents =
		std::max(statistics_.maximumActiveEvents, stable.activeEventCount);
	statistics_.maximumStableEvents =
		std::max(statistics_.maximumStableEvents, stable.eventCount);
	statistics_.maximumInactiveEvents = std::max(statistics_.maximumInactiveEvents,
						     stable.eventCount - stable.activeEventCount);
	statistics_.maximumCurrentBaseBytes =
		std::max(statistics_.maximumCurrentBaseBytes, baseStorageBytes(stable.base));
	for (const auto &[name, value] : stable.base) {
		(void)name;
		const auto *relation = std::get_if<Relation>(&value);
		if (!relation)
			continue;
		const auto pairs = relation->count();
		statistics_.maximumBaseRelationPairs =
			std::max(statistics_.maximumBaseRelationPairs, pairs);
		if (stable.activeEventCount == 0)
			continue;
		const auto denominator = stable.activeEventCount * stable.activeEventCount;
		statistics_.maximumBaseRelationDensityPpm =
			std::max(statistics_.maximumBaseRelationDensityPpm,
				 static_cast<std::uint64_t>(pairs) * 1'000'000 / denominator);
	}
}

void GraphSynchronizer::refreshHistoryBytes()
{
	const auto bytes = std::accumulate(history_.begin(), history_.end(), std::size_t{},
					   [](std::size_t total, const HistoryEntry &entry) {
						   return total + baseStorageBytes(entry.base);
					   });
	statistics_.maximumHistoryBaseBytes = std::max(statistics_.maximumHistoryBaseBytes, bytes);
}

void GraphSynchronizer::retainCurrent()
{
	history_.push_back(
		{evaluator_->eventCount(), evaluator_->baseValues(), evaluator_->checkpoint()});
	while (history_.size() > checkpointLimit_) {
		(void)evaluator_->forget(history_.front().checkpoint);
		history_.erase(history_.begin());
		++statistics_.evictedCheckpoints;
	}
	if (profiling_)
		refreshHistoryBytes();
}

auto GraphSynchronizer::synchronize(const GraphAdapter &snapshot) -> GraphSynchronizationResult
{
	const auto materializeStarted = profiling_ ? std::chrono::steady_clock::now()
						   : std::chrono::steady_clock::time_point{};
	auto stable = stable_.materialize(snapshot);
	if (profiling_)
		statistics_.materializeNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - materializeStarted)
				.count());
	return synchronize(std::move(stable));
}

auto GraphSynchronizer::synchronize(const ExecutionGraph &graph) -> GraphSynchronizationResult
{
	const auto materializeStarted = profiling_ ? std::chrono::steady_clock::now()
						   : std::chrono::steady_clock::time_point{};
	auto stable = stable_.materialize(graph);
	if (profiling_)
		statistics_.materializeNanoseconds += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - materializeStarted)
				.count());
	return synchronize(std::move(stable));
}

auto GraphSynchronizer::synchronize(StableGraphSnapshot stable) -> GraphSynchronizationResult
{
	if (profiling_)
		recordSpaceStatistics(stable);
	const auto timed = [&](auto started) -> std::uint64_t {
		return profiling_ ? static_cast<std::uint64_t>(
					    std::chrono::duration_cast<std::chrono::nanoseconds>(
						    std::chrono::steady_clock::now() - started)
						    .count())
				  : 0;
	};
	if (!evaluator_->initialized()) {
		const auto rebuildStarted = profiling_ ? std::chrono::steady_clock::now()
						       : std::chrono::steady_clock::time_point{};
		evaluator_->initialize(stable.eventCount, stable.base);
		statistics_.rebuildNanoseconds += timed(rebuildStarted);
		retainCurrent();
		++statistics_.initializations;
		verifyCurrent(GraphTransition::Initialize);
		return {GraphTransition::Initialize, {}};
	}
	const auto equalityStarted = profiling_ ? std::chrono::steady_clock::now()
						: std::chrono::steady_clock::time_point{};
	const auto unchanged = evaluator_->eventCount() == stable.eventCount &&
			       evaluator_->baseValues() == stable.base;
	statistics_.equalityNanoseconds += timed(equalityStarted);
	if (unchanged) {
		++statistics_.unchanged;
		verifyCurrent(GraphTransition::Unchanged);
		return {GraphTransition::Unchanged, {}};
	}
	if (adaptiveOfflineEventLimit_ != 0 && stable.eventCount <= adaptiveOfflineEventLimit_) {
		const auto rebuildStarted = profiling_ ? std::chrono::steady_clock::now()
						       : std::chrono::steady_clock::time_point{};
		evaluator_->initialize(stable.eventCount, stable.base);
		statistics_.rebuildNanoseconds += timed(rebuildStarted);
		clearHistory();
		retainCurrent();
		++statistics_.adaptiveOfflineSelections;
		verifyCurrent(GraphTransition::Rebuild);
		return {GraphTransition::Rebuild, "adaptive offline small-graph epoch"};
	}

	const auto insertionStarted = profiling_ ? std::chrono::steady_clock::now()
						 : std::chrono::steady_clock::time_point{};
	auto inserted = evaluator_->tryInsert(stable.eventCount, stable.base);
	statistics_.insertionAttemptNanoseconds += timed(insertionStarted);
	if (inserted.applied()) {
		retainCurrent();
		++statistics_.insertions;
		verifyCurrent(GraphTransition::Insert);
		return {GraphTransition::Insert, {}};
	}
	/* Search newest-first: it minimizes undo distance and maximizes reusable facts. */
	const auto historyStarted = profiling_ ? std::chrono::steady_clock::now()
					       : std::chrono::steady_clock::time_point{};
	for (auto candidate = history_.rbegin(); candidate != history_.rend(); ++candidate) {
		if (!baseSubset(candidate->eventCount, candidate->base, stable.eventCount,
				stable.base))
			continue;
		const auto position =
			static_cast<std::size_t>(std::distance(candidate, history_.rend()) - 1);
		const auto candidateEventCount = candidate->eventCount;
		const auto candidateBase = candidate->base;
		const auto candidateCheckpoint = candidate->checkpoint;
		auto restored = evaluator_->rollback(candidateCheckpoint);
		if (!restored.restored)
			continue;
		history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(position + 1),
			       history_.end());
		if (profiling_)
			refreshHistoryBytes();
		if (candidateEventCount == stable.eventCount && candidateBase == stable.base) {
			statistics_.historySearchNanoseconds += timed(historyStarted);
			++statistics_.rollbacks;
			verifyCurrent(GraphTransition::Rollback);
			return {GraphTransition::Rollback, {}};
		}
		auto advanced = evaluator_->tryInsert(stable.eventCount, stable.base);
		if (advanced.applied()) {
			statistics_.historySearchNanoseconds += timed(historyStarted);
			retainCurrent();
			++statistics_.rollbackInsertions;
			verifyCurrent(GraphTransition::RollbackInsert);
			return {GraphTransition::RollbackInsert, {}};
		}
		break;
	}
	statistics_.historySearchNanoseconds += timed(historyStarted);

	/* Acyclic alias/union models can retract a fact locally after history reuse
	 * has failed. Richer derivations fail closed and continue to rebuild. */
	auto replaced = evaluator_->tryReplace(stable.eventCount, stable.base);
	if (replaced.applied()) {
		clearHistory();
		retainCurrent();
		++statistics_.replacements;
		verifyCurrent(GraphTransition::Replace);
		return {GraphTransition::Replace, {}};
	}

	/* An rf/co replacement or unknown mixed mutation is never approximated as
	 * insertion: rebuild through the offline oracle and start a fresh epoch. */
	const auto rebuildStarted = profiling_ ? std::chrono::steady_clock::now()
					       : std::chrono::steady_clock::time_point{};
	evaluator_->initialize(stable.eventCount, stable.base);
	statistics_.rebuildNanoseconds += timed(rebuildStarted);
	clearHistory();
	retainCurrent();
	++statistics_.rebuilds;
	verifyCurrent(GraphTransition::Rebuild);
	return {GraphTransition::Rebuild, std::move(inserted.reason)};
}

} /* namespace cat */
