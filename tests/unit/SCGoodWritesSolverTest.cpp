/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/SCGoodWritesSolver.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace {

using genmc::rvf::Event;
using genmc::rvf::EventId;
using genmc::rvf::EventKind;
using genmc::rvf::Problem;
using genmc::rvf::Status;
using genmc::rvf::noVariable;

/** Exhaustively enumerate SC interleavings without using witness-state merging. */
auto bruteForceHasWitness(const Problem &problem) -> bool
{
	std::vector<bool> used(problem.events.size(), false);
	std::vector<EventId> atomicPredecessor(problem.events.size(), static_cast<EventId>(-1));
	for (const auto &event : problem.events)
		if (event.atomicSuccessor)
			atomicPredecessor[*event.atomicSuccessor] = event.id;
	std::vector<EventId> active(1U, static_cast<EventId>(-1));
	std::vector<std::uint32_t> next(problem.events.size(), 0U);
	std::function<bool(std::size_t)> search = [&](const std::size_t depth) {
		if (depth == problem.events.size())
			return true;
		for (const auto &event : problem.events) {
			if (used[event.id] || atomicPredecessor[event.id] != static_cast<EventId>(-1) ||
			    event.threadIndex != next[event.thread])
				continue;
			if (!std::ranges::all_of(event.extraPredecessors,
						 [&](const auto pred) { return used[pred]; }))
				continue;
			if (event.kind == EventKind::read &&
			    std::ranges::find(problem.goodWrites[event.id],
					      active[event.variable]) ==
				    problem.goodWrites[event.id].end())
				continue;
			const auto oldActive = event.kind == EventKind::other
						       ? static_cast<EventId>(-1)
						       : active[event.variable];
			used[event.id] = true;
			++next[event.thread];
			if (event.kind == EventKind::write)
				active[event.variable] = event.id;
			bool found = false;
			if (event.atomicSuccessor) {
				const auto &write = problem.events[*event.atomicSuccessor];
				const auto ready = write.threadIndex == next[write.thread] &&
					std::ranges::all_of(write.extraPredecessors,
							    [&](const auto pred) { return used[pred]; });
				if (ready) {
					used[write.id] = true;
					++next[write.thread];
					active[write.variable] = write.id;
					found = search(depth + 2U);
					active[write.variable] = oldActive;
					--next[write.thread];
					used[write.id] = false;
				}
			} else {
				found = search(depth + 1U);
			}
			if (found)
				return true;
			if (event.kind != EventKind::other)
				active[event.variable] = oldActive;
			--next[event.thread];
			used[event.id] = false;
		}
		return false;
	};
	return search(0U);
}

/** Independently replay a returned witness against direct SC latest-write semantics. */
auto isValidWitness(const Problem &problem, const std::vector<EventId> &witness,
		    const std::vector<std::optional<EventId>> &readsFrom) -> bool
{
	if (witness.size() != problem.events.size() || readsFrom.size() != problem.events.size())
		return false;
	std::vector<bool> used(problem.events.size(), false);
	std::vector<EventId> active(1U, static_cast<EventId>(-1));
	std::vector<std::uint32_t> next(problem.events.size(), 0U);
	for (const auto id : witness) {
		if (id >= problem.events.size())
			return false;
		const auto &event = problem.events[id];
		if (used[id] || event.threadIndex != next[event.thread])
			return false;
		if (!std::ranges::all_of(event.extraPredecessors,
					 [&](const auto pred) { return used[pred]; }))
			return false;
		if (event.kind == EventKind::read &&
		    std::ranges::find(problem.goodWrites[id], active[event.variable]) ==
			    problem.goodWrites[id].end())
			return false;
		if (event.kind == EventKind::read && readsFrom[id] != active[event.variable])
			return false;
		if (event.kind != EventKind::read && readsFrom[id].has_value())
			return false;
		used[id] = true;
		++next[event.thread];
		if (event.kind == EventKind::write)
			active[event.variable] = id;
	}
	return true;
}

TEST(SCGoodWritesSolver, FindsWitnessAcrossSameValueWriteSet)
{
	Problem problem{
		.events = {{0, 0, 0, 0, EventKind::write, {}},
			   {1, 0, 1, 0, EventKind::read, {}},
			   {2, 1, 0, 0, EventKind::write, {}}},
		.goodWrites = {{}, {0, 2}, {}},
	};
	const auto result = genmc::rvf::verifySC(problem);
	EXPECT_EQ(result.status, Status::witness);
	EXPECT_TRUE(isValidWitness(problem, result.witness, result.readsFrom));
}

TEST(SCGoodWritesSolver, MatchesAllFourEventReadWriteShapes)
{
	for (std::uint32_t kindMask = 1; kindMask < 16; ++kindMask) {
		Problem problem{
			.events = {{0, 0, 0, 0, EventKind::read, {}},
				   {1, 0, 1, 0, EventKind::read, {}},
				   {2, 1, 0, 0, EventKind::read, {}},
				   {3, 1, 1, 0, EventKind::read, {}}},
			.goodWrites = std::vector(4, std::vector<EventId>{}),
		};
		std::vector<EventId> writes;
		std::vector<EventId> reads;
		for (auto id = EventId{0}; id < problem.events.size(); ++id) {
			if (kindMask & (1U << id)) {
				problem.events[id].kind = EventKind::write;
				writes.push_back(id);
			} else {
				reads.push_back(id);
			}
		}
		std::function<void(std::size_t)> enumerateGoodWrites = [&](const auto index) {
			if (index == reads.size()) {
				const auto result = genmc::rvf::verifySC(problem);
				EXPECT_EQ(result.status == Status::witness,
					  bruteForceHasWitness(problem))
					<< "kindMask=" << kindMask;
				if (result.status == Status::witness)
					EXPECT_TRUE(isValidWitness(problem, result.witness,
								   result.readsFrom));
				return;
			}
			const auto read = reads[index];
			for (std::uint32_t subset = 1; subset < (1U << writes.size()); ++subset) {
				auto &good = problem.goodWrites[read];
				good.clear();
				for (std::size_t writeIndex = 0; writeIndex < writes.size();
				     ++writeIndex) {
					if (subset & (1U << writeIndex))
						good.push_back(writes[writeIndex]);
				}
				enumerateGoodWrites(index + 1U);
			}
		};
		enumerateGoodWrites(0U);
	}
}

TEST(SCGoodWritesSolver, RejectsForcedBadOverwrite)
{
	Problem problem{
		.events = {{0, 0, 0, 0, EventKind::write, {}},
			   {1, 0, 1, 0, EventKind::write, {}},
			   {2, 0, 2, 0, EventKind::read, {}}},
		.goodWrites = {{}, {}, {0}},
	};
	EXPECT_EQ(genmc::rvf::verifySC(problem).status, Status::noWitness);
}

TEST(SCGoodWritesSolver, MatchesExhaustiveSCLinearizations)
{
	const std::vector<Event> events = {
		{0, 0, 0, 0, EventKind::write, {}},
		{1, 0, 1, 0, EventKind::read, {}},
		{2, 1, 0, 0, EventKind::write, {}},
		{3, 1, 1, 0, EventKind::read, {}},
	};
	for (std::uint32_t firstMask = 1; firstMask < 4; ++firstMask) {
		for (std::uint32_t secondMask = 1; secondMask < 4; ++secondMask) {
			Problem problem{.events = events,
					.goodWrites = std::vector(4, std::vector<EventId>{})};
			for (const auto write : {EventId{0}, EventId{2}}) {
				if (firstMask & (1U << (write / 2U)))
					problem.goodWrites[1].push_back(write);
				if (secondMask & (1U << (write / 2U)))
					problem.goodWrites[3].push_back(write);
			}
			const auto result = genmc::rvf::verifySC(problem);
			EXPECT_EQ(result.status == Status::witness, bruteForceHasWitness(problem))
				<< "firstMask=" << firstMask << " secondMask=" << secondMask;
		}
	}
}

TEST(SCGoodWritesSolver, RejectsMalformedGoodWrite)
{
	Problem problem{
		.events = {{0, 0, 0, 0, EventKind::read, {}}},
		.goodWrites = {{0}},
	};
	const auto result = genmc::rvf::verifySC(problem);
	EXPECT_EQ(result.status, Status::invalidInput);
	EXPECT_FALSE(result.error.empty());
}

TEST(SCGoodWritesSolver, KeepsSuccessfulRmwReadWriteAdjacent)
{
	Problem problem;
	problem.events = {
		{0, 2, 0, 0, EventKind::write},
		{1, 0, 0, 0, EventKind::read, {}, 2},
		{2, 0, 1, 0, EventKind::write},
		{3, 1, 0, 0, EventKind::write},
	};
	problem.goodWrites.resize(problem.events.size());
	problem.goodWrites[1] = {0, 3};

	const auto result = genmc::rvf::verifySC(problem);
	ASSERT_EQ(result.status, Status::witness);
	const auto read = std::ranges::find(result.witness, 1);
	ASSERT_NE(read, result.witness.end());
	ASSERT_NE(std::next(read), result.witness.end());
	EXPECT_EQ(*std::next(read), 2);
	EXPECT_TRUE(result.readsFrom[1] == 0 || result.readsFrom[1] == 3);
}

TEST(SCGoodWritesSolver, RejectsMalformedRmwPair)
{
	Problem problem;
	problem.events = {
		{0, 0, 0, 0, EventKind::read, {}, 2},
		{1, 0, 1, noVariable, EventKind::other},
		{2, 0, 2, 0, EventKind::write},
	};
	problem.goodWrites.resize(problem.events.size());
	problem.goodWrites[0] = {2};

	const auto result = genmc::rvf::verifySC(problem);
	EXPECT_EQ(result.status, Status::invalidInput);
	EXPECT_NE(result.error.find("atomic successor"), std::string::npos);
}

TEST(SCGoodWritesSolver, RejectsRequiredInterpositionInsideRmwPair)
{
	Problem problem;
	problem.events = {
		{0, 2, 0, 0, EventKind::write},
		{1, 0, 0, 0, EventKind::read, {}, 2},
		{2, 0, 1, 0, EventKind::write, {3}},
		{3, 1, 0, 0, EventKind::write, {1}},
	};
	problem.goodWrites.resize(problem.events.size());
	problem.goodWrites[1] = {0};

	EXPECT_EQ(genmc::rvf::verifySC(problem).status, Status::noWitness);
}

TEST(SCGoodWritesSolver, MatchesExhaustiveRmwLinearizations)
{
	const std::vector<Event> events = {
		{0, 2, 0, 0, EventKind::write},
		{1, 0, 0, 0, EventKind::read, {}, 2},
		{2, 0, 1, 0, EventKind::write},
		{3, 1, 0, 0, EventKind::write},
		{4, 1, 1, 0, EventKind::read},
	};
	for (std::uint32_t rmwMask = 1; rmwMask < 4; ++rmwMask) {
		for (std::uint32_t readMask = 1; readMask < 8; ++readMask) {
			Problem problem{.events = events,
					.goodWrites = std::vector(5, std::vector<EventId>{})};
			for (const auto write : {EventId{0}, EventId{3}})
				if (rmwMask & (1U << (write / 3U)))
					problem.goodWrites[1].push_back(write);
			for (const auto write : {EventId{0}, EventId{2}, EventId{3}})
				if (readMask & (1U << (write == 0 ? 0 : write - 1U)))
					problem.goodWrites[4].push_back(write);

			const auto result = genmc::rvf::verifySC(problem);
			EXPECT_EQ(result.status == Status::witness, bruteForceHasWitness(problem))
				<< "rmwMask=" << rmwMask << " readMask=" << readMask;
			if (result.status == Status::witness)
				EXPECT_TRUE(isValidWitness(problem, result.witness, result.readsFrom));
		}
	}
}

} /* namespace */
