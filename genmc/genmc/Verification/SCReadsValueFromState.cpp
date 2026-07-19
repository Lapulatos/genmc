/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/SCReadsValueFromState.hpp"

#include <algorithm>
#include <cstddef>

namespace genmc::rvf {
namespace {

[[nodiscard]] auto validateEvents(const std::vector<Event> &events) -> std::string
{
	std::size_t numThreads{};
	for (std::size_t i = 0; i < events.size(); ++i) {
		const auto &event = events[i];
		if (event.id != i)
			return "event IDs must be dense and equal to their vector index";
		numThreads = std::max(numThreads, static_cast<std::size_t>(event.thread) + 1U);
		if (event.kind == EventKind::other) {
			if (event.variable != noVariable)
				return "non-memory events must use noVariable";
		} else if (event.variable == noVariable) {
			return "read and write events must name a variable";
		}
		for (const auto predecessor : event.extraPredecessors) {
			if (predecessor >= events.size() || predecessor == event.id)
				return "extra predecessor is invalid";
		}
	}

	std::vector<std::vector<EventId>> perThread(numThreads);
	for (const auto &event : events) {
		auto &thread = perThread[event.thread];
		if (thread.size() <= event.threadIndex)
			thread.resize(event.threadIndex + 1U, static_cast<EventId>(events.size()));
		if (thread[event.threadIndex] != events.size())
			return "thread indices must be unique";
		thread[event.threadIndex] = event.id;
	}
	for (const auto &thread : perThread) {
		if (std::ranges::any_of(thread,
					[&](const auto event) { return event == events.size(); }))
			return "each thread's event set must be a dense prefix";
	}
	return {};
}

[[nodiscard]] auto programOrder(const std::vector<Event> &events) -> std::vector<std::vector<bool>>
{
	std::vector order(events.size(), std::vector(events.size(), false));
	for (const auto &before : events) {
		for (const auto &after : events) {
			if (before.thread == after.thread && before.threadIndex < after.threadIndex)
				order[before.id][after.id] = true;
		}
		for (const auto predecessor : before.extraPredecessors)
			order[predecessor][before.id] = true;
	}
	for (std::size_t via = 0; via < events.size(); ++via) {
		for (std::size_t from = 0; from < events.size(); ++from) {
			if (!order[from][via])
				continue;
			for (std::size_t to = 0; to < events.size(); ++to)
				order[from][to] = order[from][to] || order[via][to];
		}
	}
	return order;
}

} /* namespace */

auto visibleWrites(const std::vector<Event> &events, EventId read) -> VisibleWritesResult
{
	VisibleWritesResult result;
	if (auto error = validateEvents(events); !error.empty()) {
		result.error = std::move(error);
		return result;
	}
	if (read >= events.size() || events[read].kind != EventKind::read) {
		result.error = "visible-writes query must name a read";
		return result;
	}
	const auto order = programOrder(events);
	if (std::ranges::any_of(events,
				[&](const auto &event) { return order[event.id][event.id]; })) {
		result.error = "program order must be acyclic";
		return result;
	}

	const auto variable = events[read].variable;
	for (const auto &write : events) {
		if (write.kind != EventKind::write || write.variable != variable ||
		    order[read][write.id])
			continue;
		const auto hidden = std::ranges::any_of(events, [&](const auto &later) {
			return later.kind == EventKind::write && later.variable == variable &&
			       order[write.id][later.id] && order[later.id][read];
		});
		if (!hidden)
			result.writes.push_back(write.id);
	}
	return result;
}

CausalMap::CausalMap(std::size_t numEvents, std::size_t numThreads)
	: numThreads_(numThreads), entries_(numEvents)
{}

auto CausalMap::grow(std::size_t numEvents, std::size_t numThreads) -> bool
{
	if (numEvents < entries_.size() || numThreads < numThreads_)
		return false;
	entries_.resize(numEvents);
	for (auto &entry : entries_) {
		if (entry)
			entry->resize(numThreads, 0U);
	}
	numThreads_ = numThreads;
	return true;
}

auto CausalMap::isDefined(EventId read) const -> bool
{
	return read < entries_.size() && entries_[read].has_value();
}

auto CausalMap::isForbidden(EventId read, const Event &write) const -> bool
{
	if (!isDefined(read) || write.thread >= numThreads_)
		return false;
	return write.threadIndex < (*entries_[read])[write.thread];
}

auto CausalMap::record(EventId read, std::span<const std::uint32_t> threadEventCounts) -> bool
{
	if (read >= entries_.size() || threadEventCounts.size() != numThreads_)
		return false;
	entries_[read] = std::vector(threadEventCounts.begin(), threadEventCounts.end());
	return true;
}

auto CausalMap::cutoffs(EventId read) const -> const std::optional<std::vector<std::uint32_t>> &
{
	static const std::optional<std::vector<std::uint32_t>> undefined;
	return read < entries_.size() ? entries_[read] : undefined;
}

auto AncestorSignals::push(EventId read, bool initial) -> Token
{
	const auto token = Token{entries_.size(), read};
	entries_.push_back({read, initial});
	return token;
}

auto AncestorSignals::observeWrite(const std::vector<Event> &events, EventId write) -> bool
{
	if (write >= events.size() || events[write].kind != EventKind::write)
		return false;
	for (auto &entry : entries_) {
		if (entry.read >= events.size() || events[entry.read].kind != EventKind::read)
			return false;
		const auto &read = events[entry.read];
		if (read.variable == events[write].variable && read.thread != events[write].thread)
			entry.signal = true;
	}
	return true;
}

auto AncestorSignals::signal(const Token &token) const -> std::optional<bool>
{
	if (token.depth >= entries_.size() || entries_[token.depth].read != token.read)
		return std::nullopt;
	return entries_[token.depth].signal;
}

auto AncestorSignals::pop(const Token &token) -> std::optional<bool>
{
	if (token.depth + 1U != entries_.size() || entries_.back().read != token.read)
		return std::nullopt;
	const auto result = entries_.back().signal;
	entries_.pop_back();
	return result;
}

} /* namespace genmc::rvf */
