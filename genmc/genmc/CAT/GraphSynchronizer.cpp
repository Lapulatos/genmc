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
#include <iostream>

namespace cat {
namespace {

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
			for (std::size_t from = 0; from < oldSize; ++from) {
				for (std::size_t to = 0; to < oldSize; ++to) {
					if (oldRelation.contains(from, to) &&
					    !newRelation.contains(from, to))
						return false;
				}
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
	case GraphTransition::Rebuild:
		return "rebuild";
	}
	UNREACHABLE("unknown CAAT graph transition");
}

} /* namespace */

GraphSynchronizer::GraphSynchronizer(IncrementalCaatEvaluator &evaluator,
				     std::size_t checkpointLimit, std::size_t oracleInterval)
	: evaluator_(&evaluator), checkpointLimit_(checkpointLimit), oracleInterval_(oracleInterval)
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

void GraphSynchronizer::retainCurrent()
{
	history_.push_back(
		{evaluator_->eventCount(), evaluator_->baseValues(), evaluator_->checkpoint()});
	while (history_.size() > checkpointLimit_) {
		(void)evaluator_->forget(history_.front().checkpoint);
		history_.erase(history_.begin());
		++statistics_.evictedCheckpoints;
	}
}

auto GraphSynchronizer::synchronize(const GraphAdapter &snapshot) -> GraphSynchronizationResult
{
	auto stable = stable_.materialize(snapshot);
	if (!evaluator_->initialized()) {
		evaluator_->initialize(stable.eventCount, stable.base);
		retainCurrent();
		++statistics_.initializations;
		verifyCurrent(GraphTransition::Initialize);
		return {GraphTransition::Initialize, {}};
	}
	if (evaluator_->eventCount() == stable.eventCount &&
	    evaluator_->baseValues() == stable.base) {
		++statistics_.unchanged;
		verifyCurrent(GraphTransition::Unchanged);
		return {GraphTransition::Unchanged, {}};
	}

	auto inserted = evaluator_->tryInsert(stable.eventCount, stable.base);
	if (inserted.applied()) {
		retainCurrent();
		++statistics_.insertions;
		verifyCurrent(GraphTransition::Insert);
		return {GraphTransition::Insert, {}};
	}

	/* Search newest-first: it minimizes undo distance and maximizes reusable facts. */
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
		if (candidateEventCount == stable.eventCount && candidateBase == stable.base) {
			++statistics_.rollbacks;
			verifyCurrent(GraphTransition::Rollback);
			return {GraphTransition::Rollback, {}};
		}
		auto advanced = evaluator_->tryInsert(stable.eventCount, stable.base);
		if (advanced.applied()) {
			retainCurrent();
			++statistics_.rollbackInsertions;
			verifyCurrent(GraphTransition::RollbackInsert);
			return {GraphTransition::RollbackInsert, {}};
		}
		break;
	}

	/* An rf/co replacement or unknown mixed mutation is never approximated as
	 * insertion: rebuild through the offline oracle and start a fresh epoch. */
	evaluator_->initialize(stable.eventCount, stable.base);
	clearHistory();
	retainCurrent();
	++statistics_.rebuilds;
	verifyCurrent(GraphTransition::Rebuild);
	return {GraphTransition::Rebuild, std::move(inserted.reason)};
}

} /* namespace cat */
