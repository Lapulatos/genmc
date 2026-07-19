/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/SCExecutionGraphAdapter.hpp"

#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <ranges>

namespace genmc::rvf {
namespace {

using StableIds = std::map<cat::StableEventKey, EventId>;

auto appendPredecessor(Event &event, EventId predecessor) -> bool
{
	if (predecessor == event.id)
		return false;
	if (std::ranges::find(event.extraPredecessors, predecessor) ==
	    event.extraPredecessors.end())
		event.extraPredecessors.push_back(predecessor);
	return true;
}

[[nodiscard]] auto stableId(const StableIds &ids, const cat::StableEventKey &key)
	-> std::optional<EventId>
{
	const auto found = ids.find(key);
	return found == ids.end() ? std::nullopt : std::optional<EventId>(found->second);
}

} /* namespace */

auto buildGraphProblem(const ExecutionGraph &graph, const StableGoodWrites &goodWrites)
	-> GraphProblem
{
	GraphProblem result;
	std::vector<const EventLabel *> labels;
	for (const auto &label : graph.labels()) {
		if (!genmc::isa<EmptyLabel>(&label) && !genmc::isa<InitLabel>(&label))
			labels.push_back(&label);
	}
	std::vector<SAddr> locations;
	for (auto location = graph.loc_begin(); location != graph.loc_end(); ++location)
		locations.push_back(location->first);
	std::ranges::sort(locations);
	locations.erase(std::ranges::unique(locations).begin(), locations.end());
	if (labels.size() + locations.size() > std::numeric_limits<EventId>::max()) {
		result.error = "SC RVF event universe exceeds 32-bit IDs";
		return result;
	}

	std::map<SAddr, VariableId> variables;
	for (const auto location : locations)
		variables.emplace(location, static_cast<VariableId>(variables.size()));
	for (const auto *label : labels) {
		if (const auto *memory = genmc::dyn_cast<MemAccessLabel>(label);
		    memory && !variables.contains(memory->getAddr()))
			variables.emplace(memory->getAddr(),
					  static_cast<VariableId>(variables.size()));
	}

	StableIds ids;
	result.problem.events.reserve(labels.size() + locations.size());
	result.denseToStable.reserve(labels.size() + locations.size());
	std::vector<std::uint32_t> nextThreadIndex(graph.getNumThreads(), 0U);
	for (const auto *label : labels) {
		if (label->getThread() < 0 ||
		    static_cast<std::size_t>(label->getThread()) >= graph.getNumThreads()) {
			result.error = "SC RVF label has an invalid thread";
			return result;
		}
		EventKind kind = EventKind::other;
		VariableId variable = noVariable;
		if (const auto *memory = genmc::dyn_cast<MemAccessLabel>(label)) {
			const auto supportedRead = label->getKind() == EventLabel::Read ||
						   genmc::isa<LockCasReadLabel>(label);
			const auto supportedWrite = label->getKind() == EventLabel::Write ||
						    genmc::isa<LockCasWriteLabel>(label) ||
						    genmc::isa<UnlockWriteLabel>(label);
			if (memory->isNotAtomic() || (!supportedRead && !supportedWrite)) {
				result.error = "SC RVF currently supports only plain atomic reads "
					       "and writes or lock-CAS pairs";
				return result;
			}
			kind = supportedRead ? EventKind::read : EventKind::write;
			variable = variables.at(memory->getAddr());
		}
		const auto id = static_cast<EventId>(result.problem.events.size());
		const auto thread = static_cast<ThreadId>(label->getThread());
		result.problem.events.push_back(
			{id, thread, nextThreadIndex[thread]++, variable, kind, {}});
		result.denseToStable.emplace_back(label->getPos());
		ids.emplace(result.denseToStable.back(), id);
	}
	for (std::size_t dense = 0; dense < labels.size(); ++dense) {
		const auto *read = genmc::dyn_cast<LockCasReadLabel>(labels[dense]);
		if (!read)
			continue;
		const auto *successor =
			genmc::dyn_cast_if_present<LockCasWriteLabel>(graph.po_imm_succ(read));
		if (!successor)
			continue;
		if (successor->getAddr() != read->getAddr()) {
			result.error = "SC RVF lock-CAS pair accesses different locations";
			return result;
		}
		const auto successorId = stableId(ids, successor->getPos());
		if (!successorId) {
			result.error = "SC RVF cannot map a lock-CAS write successor";
			return result;
		}
		result.problem.events[dense].atomicSuccessor = *successorId;
	}

	const auto initThread = static_cast<ThreadId>(graph.getNumThreads());
	for (const auto location : locations) {
		const auto id = static_cast<EventId>(result.problem.events.size());
		result.problem.events.push_back({id,
						 initThread,
						 static_cast<std::uint32_t>(id - labels.size()),
						 variables.at(location),
						 EventKind::write,
						 {}});
		result.denseToStable.emplace_back(location);
		ids.emplace(result.denseToStable.back(), id);
	}

	if (!locations.empty()) {
		const auto lastInitial = static_cast<EventId>(result.problem.events.size() - 1U);
		for (auto &event : result.problem.events) {
			if (event.thread != initThread && event.threadIndex == 0U)
				appendPredecessor(event, lastInitial);
		}
	}

	for (std::size_t dense = 0; dense < labels.size(); ++dense) {
		const auto *label = labels[dense];
		auto &event = result.problem.events[dense];
		if (const auto *start = genmc::dyn_cast<ThreadStartLabel>(label);
		    start && start->getCreate()) {
			const auto predecessor = stableId(ids, start->getCreate()->getPos());
			if (!predecessor || !appendPredecessor(event, *predecessor)) {
				result.error = "SC RVF cannot map a thread-create predecessor";
				return result;
			}
		}
		if (const auto *join = genmc::dyn_cast<ThreadJoinLabel>(label)) {
			const auto *finish = graph.getLastThreadLabel(join->getChildId());
			const auto predecessor = finish ? stableId(ids, finish->getPos())
							: std::nullopt;
			if (!predecessor || !appendPredecessor(event, *predecessor)) {
				result.error = "SC RVF cannot map a thread-join predecessor";
				return result;
			}
		}
	}

	result.problem.goodWrites.resize(result.problem.events.size());
	std::size_t mappedReads{};
	for (std::size_t dense = 0; dense < labels.size(); ++dense) {
		if (result.problem.events[dense].kind != EventKind::read)
			continue;
		const auto found = goodWrites.find(result.denseToStable[dense]);
		if (found == goodWrites.end() || found->second.empty()) {
			result.error = "SC RVF read has no stable good-write set";
			return result;
		}
		for (const auto &source : found->second) {
			const auto sourceId = stableId(ids, source);
			if (!sourceId) {
				result.error =
					"SC RVF good-write source is absent from the graph prefix";
				return result;
			}
			result.problem.goodWrites[dense].push_back(*sourceId);
		}
		std::ranges::sort(result.problem.goodWrites[dense]);
		const auto duplicate = std::ranges::unique(result.problem.goodWrites[dense]);
		result.problem.goodWrites[dense].erase(duplicate.begin(), duplicate.end());
		++mappedReads;
	}
	if (mappedReads != goodWrites.size()) {
		result.error = "SC RVF good-write map contains a read outside the graph prefix";
		return result;
	}
	return result;
}

auto applyGraphWitness(ExecutionGraph &graph, const GraphProblem &problem, const Result &witness)
	-> std::string
{
	if (!problem.error.empty())
		return "cannot apply a witness to an invalid graph problem";
	if (witness.status != Status::witness ||
	    witness.witness.size() != problem.problem.events.size() ||
	    witness.readsFrom.size() != problem.problem.events.size() ||
	    problem.denseToStable.size() != problem.problem.events.size())
		return "SC RVF witness has an incompatible event universe";

	std::vector<bool> seen(problem.problem.events.size(), false);
	std::map<SAddr, EventLabel *> lastWrite;
	for (const auto event : witness.witness) {
		if (event >= problem.problem.events.size() || seen[event])
			return "SC RVF witness is not a permutation of its event universe";
		seen[event] = true;
		if (problem.problem.events[event].kind != EventKind::write)
			continue;
		const auto &stable = problem.denseToStable[event];
		if (const auto *location = std::get_if<SAddr>(&stable)) {
			lastWrite[*location] = graph.getInitLabel();
			continue;
		}
		const auto *position = std::get_if<::Event>(&stable);
		auto *write = position ? genmc::dyn_cast<WriteLabel>(graph.getEventLabel(*position))
				       : nullptr;
		if (!write || !write->isInCo())
			return "SC RVF witness write is absent from graph coherence";
		const auto predecessor = lastWrite.find(write->getAddr());
		if (predecessor == lastWrite.end())
			return "SC RVF witness orders a write before its initial write";
		write->moveCo(predecessor->second);
		predecessor->second = write;
	}

	std::vector<std::pair<ReadLabel *, EventLabel *>> changes;
	for (const auto event : witness.witness) {
		if (event >= problem.problem.events.size() ||
		    problem.problem.events[event].kind != EventKind::read)
			continue;
		const auto *readPos = std::get_if<::Event>(&problem.denseToStable[event]);
		if (!readPos || !witness.readsFrom[event])
			return "SC RVF witness read lacks a concrete graph source";
		auto *read = genmc::dyn_cast<ReadLabel>(graph.getEventLabel(*readPos));
		if (!read)
			return "SC RVF witness read is absent from the graph clone";

		const auto sourceId = *witness.readsFrom[event];
		if (sourceId >= problem.denseToStable.size())
			return "SC RVF witness source is outside the graph universe";
		EventLabel *source{};
		if (const auto *sourcePos =
			    std::get_if<::Event>(&problem.denseToStable[sourceId])) {
			source = graph.getEventLabel(*sourcePos);
			if (!genmc::isa<WriteLabel>(source))
				return "SC RVF witness source is not a write";
		} else {
			const auto location = std::get<SAddr>(problem.denseToStable[sourceId]);
			if (location != read->getAddr())
				return "SC RVF initial source has a different location";
			source = graph.getInitLabel();
		}
		changes.emplace_back(read, source);
	}
	for (const auto &[read, source] : changes)
		read->setRf(source);
	return {};
}

} /* namespace genmc::rvf */
