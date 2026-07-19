/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/SCReadsValueFromState.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using genmc::rvf::AncestorSignals;
using genmc::rvf::CausalMap;
using genmc::rvf::Event;
using genmc::rvf::EventKind;

TEST(SCReadsValueFromState, ComputesExactProgramOrderVisibleWrites)
{
	const std::vector<Event> events = {
		{0, 0, 0, 0, EventKind::write, {}}, {1, 0, 1, 0, EventKind::write, {}},
		{2, 0, 2, 0, EventKind::read, {}},  {3, 0, 3, 0, EventKind::write, {}},
		{4, 1, 0, 0, EventKind::write, {}},
	};
	const auto result = genmc::rvf::visibleWrites(events, 2);
	EXPECT_TRUE(result.error.empty());
	EXPECT_EQ(result.writes, (std::vector<genmc::rvf::EventId>{1, 4}));
}

TEST(SCReadsValueFromState, IncludesCrossThreadPredecessorsInProgramOrder)
{
	const std::vector<Event> events = {
		{0, 0, 0, 0, EventKind::write, {}},
		{1, 1, 0, 0, EventKind::write, {0}},
		{2, 2, 0, 0, EventKind::read, {1}},
	};
	const auto result = genmc::rvf::visibleWrites(events, 2);
	EXPECT_TRUE(result.error.empty());
	EXPECT_EQ(result.writes, (std::vector<genmc::rvf::EventId>{1}));
}

TEST(SCReadsValueFromState, CausalMapForbidsExactlyRecordedThreadPrefixes)
{
	CausalMap map(6, 2);
	const std::vector<std::uint32_t> counts = {2, 1};
	ASSERT_TRUE(map.record(5, counts));
	EXPECT_TRUE(map.isDefined(5));
	EXPECT_TRUE(map.isForbidden(5, Event{0, 0, 0, 0, EventKind::write, {}}));
	EXPECT_TRUE(map.isForbidden(5, Event{1, 0, 1, 0, EventKind::write, {}}));
	EXPECT_FALSE(map.isForbidden(5, Event{2, 0, 2, 0, EventKind::write, {}}));
	EXPECT_TRUE(map.isForbidden(5, Event{3, 1, 0, 0, EventKind::write, {}}));
	EXPECT_FALSE(map.isForbidden(5, Event{4, 1, 1, 0, EventKind::write, {}}));
	ASSERT_TRUE(map.grow(8, 3));
	EXPECT_TRUE(map.isForbidden(5, Event{0, 0, 0, 0, EventKind::write, {}}));
	EXPECT_FALSE(map.isForbidden(5, Event{6, 2, 0, 0, EventKind::write, {}}));
	EXPECT_FALSE(map.grow(7, 3));
}

TEST(SCReadsValueFromState, AncestorSignalsRollbackInStrictStackOrder)
{
	const std::vector<Event> events = {
		{0, 0, 0, 0, EventKind::read, {}},
		{1, 1, 0, 0, EventKind::read, {}},
		{2, 2, 0, 0, EventKind::write, {}},
		{3, 1, 1, 1, EventKind::write, {}},
	};
	AncestorSignals signals;
	const auto outer = signals.push(0, false);
	const auto inner = signals.push(1, false);
	ASSERT_TRUE(signals.observeWrite(events, 2));
	EXPECT_EQ(signals.signal(outer), true);
	EXPECT_EQ(signals.signal(inner), true);
	ASSERT_TRUE(signals.observeWrite(events, 3));
	EXPECT_FALSE(signals.pop(outer).has_value());
	EXPECT_EQ(signals.pop(inner), true);
	EXPECT_EQ(signals.pop(outer), true);
	EXPECT_EQ(signals.size(), 0U);
}

} /* namespace */
