/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/SCGoodWritesSolver.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace genmc::rvf {
namespace {

constexpr auto noEvent = std::numeric_limits<EventId>::max();
constexpr auto noThread = std::numeric_limits<ThreadId>::max();

/** Validated input plus derived indexing. It borrows PROBLEM for one synchronous solver
 * call and is therefore neither owning nor shared. */
struct PreparedProblem {
	const Problem &problem;
	std::vector<EventId> threadPredecessor;
	std::vector<EventId> atomicPredecessor;
	std::size_t numVariables{};
};

/** Exact witness-state identity. The active write event is intentionally represented by
 * its thread: the executed event set determines that thread's latest write. */
struct StateKey {
	std::vector<std::uint64_t> executed;
	std::vector<ThreadId> activeWriterThread;

	auto operator==(const StateKey &) const -> bool = default;
};

/** Hash accelerator for StateKey; unordered_set still resolves collisions by structural
 * equality. */
struct StateKeyHash {
	auto operator()(const StateKey &key) const -> std::size_t
	{
		auto seed = std::size_t{0x9e3779b97f4a7c15ULL};
		auto mix = [&seed](auto value) {
			const auto hash = std::hash<std::remove_cvref_t<decltype(value)>>{}(value);
			seed ^= hash + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
		};
		for (const auto word : key.executed)
			mix(word);
		for (const auto thread : key.activeWriterThread)
			mix(thread);
		return seed;
	}
};

/** Search node retaining the concrete active writes and prefix needed to construct a
 * witness. These fields are worker-local and never shared between solver calls. */
struct Node {
	StateKey key;
	std::vector<EventId> activeWrite;
	std::vector<std::optional<EventId>> readsFrom;
	std::vector<EventId> sequence;
};

/** Query the dense executed-event bitset. */
[[nodiscard]] auto contains(const std::vector<std::uint64_t> &bits, EventId id) -> bool
{
	return (bits[id / 64U] & (std::uint64_t{1} << (id % 64U))) != 0;
}

/** Mark one dense event ID as executed. */
void insert(std::vector<std::uint64_t> &bits, EventId id)
{
	bits[id / 64U] |= std::uint64_t{1} << (id % 64U);
}

/** Validate the proper-event-set representation and precompute immediate thread
 * predecessors. Returns an empty string on success. */
[[nodiscard]] auto validate(const Problem &problem, PreparedProblem &prepared) -> std::string
{
	if (problem.goodWrites.size() != problem.events.size())
		return "goodWrites must have one entry per event";

	std::size_t numThreads{};
	std::size_t numVariables{};
	for (std::size_t i = 0; i < problem.events.size(); ++i) {
		const auto &event = problem.events[i];
		if (event.id != i)
			return "event IDs must be dense and equal to their vector index";
		numThreads = std::max(numThreads, static_cast<std::size_t>(event.thread) + 1U);
		if (event.kind == EventKind::other) {
			if (event.variable != noVariable)
				return "non-memory events must use noVariable";
		} else {
			if (event.variable == noVariable)
				return "read and write events must name a variable";
			numVariables = std::max(numVariables,
						static_cast<std::size_t>(event.variable) + 1U);
		}
		for (const auto predecessor : event.extraPredecessors) {
			if (predecessor >= problem.events.size() || predecessor == event.id)
				return "extra predecessor is invalid";
		}
	}

	std::vector<std::vector<EventId>> perThread(numThreads);
	for (const auto &event : problem.events) {
		auto &events = perThread[event.thread];
		if (event.threadIndex >= problem.events.size())
			return "thread index is out of range";
		if (events.size() <= event.threadIndex)
			events.resize(event.threadIndex + 1U, noEvent);
		if (events[event.threadIndex] != noEvent)
			return "thread indices must be unique";
		events[event.threadIndex] = event.id;
	}
	prepared.threadPredecessor.assign(problem.events.size(), noEvent);
	for (const auto &events : perThread) {
		for (std::size_t i = 0; i < events.size(); ++i) {
			if (events[i] == noEvent)
				return "each thread's event set must be a dense prefix";
			if (i != 0)
				prepared.threadPredecessor[events[i]] = events[i - 1U];
		}
	}
	prepared.atomicPredecessor.assign(problem.events.size(), noEvent);
	for (const auto &read : problem.events) {
		if (!read.atomicSuccessor)
			continue;
		if (read.kind != EventKind::read || *read.atomicSuccessor >= problem.events.size())
			return "only a read may name an in-range atomic successor";
		const auto &write = problem.events[*read.atomicSuccessor];
		if (write.kind != EventKind::write || write.thread != read.thread ||
		    write.threadIndex != read.threadIndex + 1U || write.variable != read.variable ||
		    prepared.threadPredecessor[write.id] != read.id)
			return "an atomic successor must be the read's next same-variable write";
		if (prepared.atomicPredecessor[write.id] != noEvent || write.atomicSuccessor)
			return "atomic pairs must be disjoint read/write pairs";
		prepared.atomicPredecessor[write.id] = read.id;
	}

	for (const auto &event : problem.events) {
		const auto &good = problem.goodWrites[event.id];
		if (event.kind != EventKind::read) {
			if (!good.empty())
				return "only reads may have good writes";
			continue;
		}
		if (good.empty())
			return "each read must have at least one good write";
		for (const auto writeId : good) {
			if (writeId >= problem.events.size())
				return "good-write ID is out of range";
			const auto &write = problem.events[writeId];
			if (write.kind != EventKind::write || write.variable != event.variable)
				return "a good write must write the read's variable";
		}
	}

	prepared.numVariables = numVariables;
	return {};
}

/** Return whether thread order and explicit cross-thread prerequisites permit EVENT. */
[[nodiscard]] auto predecessorsExecuted(const PreparedProblem &prepared, const Node &node,
					const Event &event) -> bool
{
	const auto threadPred = prepared.threadPredecessor[event.id];
	if (threadPred != noEvent && !contains(node.key.executed, threadPred))
		return false;
	return std::ranges::all_of(event.extraPredecessors, [&](const auto predecessor) {
		return contains(node.key.executed, predecessor);
	});
}

/** Calculate variables whose remaining read has no not-yet-executed good write. Such a
 * variable cannot be overwritten without destroying every witness extension. */
[[nodiscard]] auto calculateHeld(const PreparedProblem &prepared, const Node &node)
	-> std::vector<bool>
{
	std::vector<bool> held(prepared.numVariables, false);
	for (const auto &read : prepared.problem.events) {
		if (read.kind != EventKind::read || contains(node.key.executed, read.id))
			continue;
		const auto &good = prepared.problem.goodWrites[read.id];
		if (std::ranges::all_of(good, [&](const auto write) {
			    return contains(node.key.executed, write);
		    }))
			held[read.variable] = true;
	}
	return held;
}

/** Test the published VerifySC executability rules for one event. */
[[nodiscard]] auto executable(const PreparedProblem &prepared, const Node &node,
			      const std::vector<bool> &held, const Event &event) -> bool
{
	if (contains(node.key.executed, event.id) || !predecessorsExecuted(prepared, node, event))
		return false;
	if (event.kind == EventKind::other)
		return true;
	if (event.kind == EventKind::write)
		return !held[event.variable];
	const auto active = node.activeWrite[event.variable];
	if (active == noEvent)
		return false;
	const auto &good = prepared.problem.goodWrites[event.id];
	return std::ranges::find(good, active) != good.end();
}

} /* namespace */

auto verifySC(const Problem &problem) -> Result
{
	Result result;
	PreparedProblem prepared{problem, {}, {}, 0U};
	if (auto error = validate(problem, prepared); !error.empty()) {
		result.error = std::move(error);
		return result;
	}

	Node initial;
	initial.key.executed.resize((problem.events.size() + 63U) / 64U, 0U);
	initial.key.activeWriterThread.resize(prepared.numVariables, noThread);
	initial.activeWrite.resize(prepared.numVariables, noEvent);
	initial.readsFrom.resize(problem.events.size());

	std::vector<Node> worklist;
	worklist.push_back(initial);
	std::unordered_set<StateKey, StateKeyHash> done;
	done.insert(initial.key);
	result.metrics.statesDiscovered = 1U;
	result.metrics.maximumWorklist = 1U;

	while (!worklist.empty()) {
		auto node = std::move(worklist.back());
		worklist.pop_back();
		++result.metrics.statesExpanded;
		if (node.sequence.size() == problem.events.size()) {
			result.status = Status::witness;
			result.witness = std::move(node.sequence);
			result.readsFrom = std::move(node.readsFrom);
			return result;
		}

		const auto held = calculateHeld(prepared, node);
		for (auto i = problem.events.size(); i-- > 0U;) {
			const auto &event = problem.events[i];
			if (prepared.atomicPredecessor[event.id] != noEvent)
				continue;
			if (!executable(prepared, node, held, event))
				continue;
			++result.metrics.executableTransitions;
			auto successor = node;
			insert(successor.key.executed, event.id);
			successor.sequence.push_back(event.id);
			if (event.kind == EventKind::read)
				successor.readsFrom[event.id] = node.activeWrite[event.variable];
			if (event.kind == EventKind::write) {
				successor.activeWrite[event.variable] = event.id;
				successor.key.activeWriterThread[event.variable] = event.thread;
			}
			if (event.atomicSuccessor) {
				const auto &write = problem.events[*event.atomicSuccessor];
				const auto afterReadHeld = calculateHeld(prepared, successor);
				if (!predecessorsExecuted(prepared, successor, write) ||
				    afterReadHeld[write.variable])
					continue;
				insert(successor.key.executed, write.id);
				successor.sequence.push_back(write.id);
				successor.activeWrite[write.variable] = write.id;
				successor.key.activeWriterThread[write.variable] = write.thread;
			}
			if (!done.insert(successor.key).second) {
				++result.metrics.duplicateStates;
				continue;
			}
			++result.metrics.statesDiscovered;
			worklist.push_back(std::move(successor));
		}
		result.metrics.maximumWorklist =
			std::max<std::uint64_t>(result.metrics.maximumWorklist, worklist.size());
	}

	result.status = Status::noWitness;
	return result;
}

} /* namespace genmc::rvf */
