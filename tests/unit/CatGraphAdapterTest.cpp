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

#include "genmc/CAT/GraphAdapter.hpp"
#include "genmc/CAT/StableGraphAdapter.hpp"
#include "genmc/CAT/Value.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <utility>

namespace {

/** Test-only graph exposing GenMC's protected label-construction primitive. */
class TestExecutionGraph : public ExecutionGraph {
public:
	using ExecutionGraph::addLabelToGraph;
	using ExecutionGraph::ExecutionGraph;
};

/** Add a concrete label to a synthetic graph and retain its typed pointer. */
template <typename Label, typename... Args>
static auto addLabel(TestExecutionGraph &graph, Args &&...args) -> Label *
{
	auto label = std::make_unique<Label>(std::forward<Args>(args)...);
	return static_cast<Label *>(graph.addLabelToGraph(std::move(label)));
}

/** Retrieve one named primitive relation from an adapter. */
static auto relation(const cat::GraphAdapter &adapter, const char *name) -> const cat::Relation &
{
	return std::get<cat::Relation>(adapter.baseValues().at(name));
}

/** Retrieve one named primitive event set from an adapter. */
static auto eventSet(const cat::GraphAdapter &adapter, const char *name) -> const cat::EventSet &
{
	return std::get<cat::EventSet>(adapter.baseValues().at(name));
}

/** Synthetic graph containing every Phase 1 primitive-category boundary. */
struct GraphFixture {
	GraphFixture()
	{
		const auto childInfo = ThreadInfo(1, 0, 7, SVal(0), "child");
		writeX = addLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::Release, x, ASize(4),
					      SVal(1));
		writeX->addCo(graph.getInitLabel());
		readY = addLabel<ReadLabel>(graph, Event(0, 2), MemOrdering::NotAtomic, y,
					    ASize(4));
		readY->setRf(graph.getInitLabel());
		writeY = addLabel<WriteLabel>(graph, Event(0, 3), MemOrdering::Relaxed, y, ASize(4),
					      SVal(1));
		writeY->addCo(graph.getInitLabel());
		fence = addLabel<FenceLabel>(graph, Event(0, 4),
					     MemOrdering::SequentiallyConsistent);
		create = addLabel<ThreadCreateLabel>(graph, Event(0, 5), childInfo);

		graph.addNewThread();
		start = addLabel<ThreadStartLabel>(graph, Event(1, 0), create->getPos(), create,
						   childInfo);
		readX = addLabel<ReadLabel>(graph, Event(1, 1), MemOrdering::Acquire, x, ASize(4));
		readX->setRf(writeX);
		rmwRead = addLabel<FaiReadLabel>(graph, Event(1, 2), MemOrdering::AcquireRelease, x,
						 ASize(4), RMWBinOp::Add, SVal(1));
		rmwWrite = addLabel<FaiWriteLabel>(graph, Event(1, 3), MemOrdering::AcquireRelease,
						   x, ASize(4), SVal(2));
		rmwWrite->addCo(writeX);
		rmwRead->setRf(writeX);
		finish = addLabel<ThreadFinishLabel>(graph, Event(1, 4), SVal(0));

		join = addLabel<ThreadJoinLabel>(graph, Event(0, 6), 1U);
		finish->setParentJoin(join);
	}

	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	const SAddr y{0x2000};
	WriteLabel *writeX{};
	ReadLabel *readY{};
	WriteLabel *writeY{};
	FenceLabel *fence{};
	ThreadCreateLabel *create{};
	ThreadStartLabel *start{};
	ReadLabel *readX{};
	FaiReadLabel *rmwRead{};
	FaiWriteLabel *rmwWrite{};
	ThreadFinishLabel *finish{};
	ThreadJoinLabel *join{};
};

} /* namespace */

/* Dense IDs preserve insertion order and round-trip to every exposed graph label. */
TEST(CatGraphAdapterTest, BuildsStableDenseSnapshot)
{
	GraphFixture fixture;
	cat::GraphAdapter adapter(fixture.graph);

	EXPECT_EQ(adapter.eventCount(), 13U);
	for (std::size_t event = 0; event < adapter.eventCount(); ++event) {
		if (const auto location = adapter.initialLocation(event)) {
			ASSERT_TRUE(adapter.initialId(*location));
			EXPECT_EQ(*adapter.initialId(*location), event);
		} else {
			ASSERT_TRUE(adapter.id(adapter.label(event)->getPos()));
			EXPECT_EQ(*adapter.id(adapter.label(event)->getPos()), event);
		}
	}
	EXPECT_FALSE(adapter.id(Event::getInit()));
	EXPECT_TRUE(adapter.structurallyCurrent());
	EXPECT_TRUE(adapter.validate().empty());

	addLabel<FenceLabel>(fixture.graph, Event(0, 7), MemOrdering::AcquireRelease);
	EXPECT_FALSE(adapter.structurallyCurrent());
}

/* Event predicates include Init as IW/W and preserve NA and split RMW classifications. */
TEST(CatGraphAdapterTest, MapsEventPredicates)
{
	GraphFixture fixture;
	cat::GraphAdapter adapter(fixture.graph);
	const auto initX = *adapter.initialId(fixture.x);
	const auto initY = *adapter.initialId(fixture.y);
	const auto write = *adapter.id(fixture.writeX->getPos());
	const auto nonatomicRead = *adapter.id(fixture.readY->getPos());
	const auto fence = *adapter.id(fixture.fence->getPos());
	const auto rmwRead = *adapter.id(fixture.rmwRead->getPos());
	const auto rmwWrite = *adapter.id(fixture.rmwWrite->getPos());

	EXPECT_EQ(eventSet(adapter, "_").count(), adapter.eventCount());
	EXPECT_EQ(eventSet(adapter, "R").count(), 3U);
	EXPECT_TRUE(eventSet(adapter, "R").contains(nonatomicRead));
	EXPECT_TRUE(eventSet(adapter, "R").contains(rmwRead));
	EXPECT_EQ(eventSet(adapter, "W").count(), 5U);
	EXPECT_TRUE(eventSet(adapter, "W").contains(initX));
	EXPECT_TRUE(eventSet(adapter, "W").contains(initY));
	EXPECT_TRUE(eventSet(adapter, "W").contains(write));
	EXPECT_TRUE(eventSet(adapter, "W").contains(rmwWrite));
	EXPECT_EQ(eventSet(adapter, "IW").count(), 2U);
	EXPECT_TRUE(eventSet(adapter, "IW").contains(initX));
	EXPECT_TRUE(eventSet(adapter, "IW").contains(initY));
	EXPECT_EQ(eventSet(adapter, "F").count(), 1U);
	EXPECT_TRUE(eventSet(adapter, "SC").contains(fence));
}

/* po/rf/co/fr/rmw/loc match the corresponding direct graph structure. */
TEST(CatGraphAdapterTest, MapsMemoryAndOrderRelations)
{
	GraphFixture fixture;
	cat::GraphAdapter adapter(fixture.graph);
	const auto initX = *adapter.initialId(fixture.x);
	const auto initY = *adapter.initialId(fixture.y);
	const auto write = *adapter.id(fixture.writeX->getPos());
	const auto writeY = *adapter.id(fixture.writeY->getPos());
	const auto readY = *adapter.id(fixture.readY->getPos());
	const auto readX = *adapter.id(fixture.readX->getPos());
	const auto rmwRead = *adapter.id(fixture.rmwRead->getPos());
	const auto rmwWrite = *adapter.id(fixture.rmwWrite->getPos());

	EXPECT_FALSE(relation(adapter, "po").contains(initX, write));
	EXPECT_TRUE(relation(adapter, "po").contains(readX, rmwWrite));
	EXPECT_FALSE(relation(adapter, "po").contains(write, readX));
	EXPECT_TRUE(relation(adapter, "rf").contains(initY, readY));
	EXPECT_TRUE(relation(adapter, "rf").contains(write, readX));
	EXPECT_TRUE(relation(adapter, "rf").contains(write, rmwRead));
	EXPECT_TRUE(relation(adapter, "co").contains(initX, write));
	EXPECT_TRUE(relation(adapter, "co").contains(initY, writeY));
	EXPECT_TRUE(relation(adapter, "co").contains(write, rmwWrite));
	EXPECT_TRUE(relation(adapter, "co").contains(initX, rmwWrite));
	EXPECT_TRUE(relation(adapter, "fr").contains(readY, writeY));
	EXPECT_FALSE(relation(adapter, "fr").contains(readY, write));
	EXPECT_TRUE(relation(adapter, "fr").contains(readX, rmwWrite));
	EXPECT_TRUE(relation(adapter, "rmw").contains(rmwRead, rmwWrite));
	EXPECT_TRUE(relation(adapter, "loc").contains(initY, readY));
	EXPECT_FALSE(relation(adapter, "loc").contains(initX, readY));
	EXPECT_TRUE(relation(adapter, "loc").contains(readX, rmwWrite));
	EXPECT_FALSE(relation(adapter, "loc").contains(readY, write));
}

/* Thread partitions and lifecycle edges use the links stored by ExecutionGraph labels. */
TEST(CatGraphAdapterTest, MapsThreadAndLifecycleRelations)
{
	GraphFixture fixture;
	cat::GraphAdapter adapter(fixture.graph);
	const auto write = *adapter.id(fixture.writeX->getPos());
	const auto create = *adapter.id(fixture.create->getPos());
	const auto start = *adapter.id(fixture.start->getPos());
	const auto finish = *adapter.id(fixture.finish->getPos());
	const auto join = *adapter.id(fixture.join->getPos());
	const auto initX = *adapter.initialId(fixture.x);
	const auto initY = *adapter.initialId(fixture.y);

	EXPECT_TRUE(relation(adapter, "int").contains(write, create));
	EXPECT_TRUE(relation(adapter, "int").contains(write, write));
	EXPECT_TRUE(relation(adapter, "ext").contains(write, start));
	EXPECT_FALSE(relation(adapter, "ext").contains(write, join));
	EXPECT_TRUE(relation(adapter, "int").contains(initX, initY));
	EXPECT_TRUE(relation(adapter, "ext").contains(initX, write));
	EXPECT_FALSE(relation(adapter, "po").contains(initX, initY));
	EXPECT_TRUE(relation(adapter, "tc").contains(create, start));
	EXPECT_TRUE(relation(adapter, "tj").contains(finish, join));
	EXPECT_EQ(relation(adapter, "id").count(), adapter.eventCount());
	EXPECT_TRUE(relation(adapter, "0").empty());
}

/* Persistent keys survive virtual-IW reordering and leave inactive IDs after a cut. */
TEST(CatStableGraphAdapterTest, PreservesIdsAcrossAddressInsertionAndRemoval)
{
	GraphFixture fixture;
	cat::StableGraphAdapter stable;
	cat::GraphAdapter firstDense(fixture.graph);
	auto first = stable.materialize(firstDense);
	const auto stableWrite = first.denseToStable[*firstDense.id(fixture.writeX->getPos())];
	const auto stableInitX = first.denseToStable[*firstDense.initialId(fixture.x)];
	EXPECT_EQ(first.eventCount, 13U);
	EXPECT_EQ(first.activeEventCount, 13U);

	const SAddr z{0x0800};
	auto *writeZ = addLabel<WriteLabel>(fixture.graph, Event(0, 7), MemOrdering::Relaxed, z,
					    ASize(4), SVal(1));
	writeZ->addCo(fixture.graph.getInitLabel());
	cat::GraphAdapter secondDense(fixture.graph);
	auto second = stable.materialize(secondDense);
	EXPECT_EQ(second.denseToStable[*secondDense.id(fixture.writeX->getPos())], stableWrite);
	EXPECT_EQ(second.denseToStable[*secondDense.initialId(fixture.x)], stableInitX);
	EXPECT_EQ(std::get<cat::EventSet>(second.base.at("_")).count(), secondDense.eventCount());
	EXPECT_EQ(second.eventCount, 15U);
	EXPECT_EQ(second.activeEventCount, 15U);

	/* Removing the real event leaves GenMC's discovered location and virtual IW
	 * active. Only the removed real event becomes an inactive stable ID. */
	auto removed = fixture.graph.removeLast(0);
	ASSERT_NE(removed, nullptr);
	cat::GraphAdapter thirdDense(fixture.graph);
	auto third = stable.materialize(thirdDense);
	EXPECT_EQ(third.eventCount, 15U);
	EXPECT_EQ(third.activeEventCount, 14U);
	EXPECT_EQ(std::get<cat::EventSet>(third.base.at("_")).count(), 14U);
	EXPECT_EQ(std::get<cat::Relation>(third.base.at("id")).count(), 14U);
	EXPECT_EQ(third.denseToStable[*thirdDense.id(fixture.writeX->getPos())], stableWrite);
	EXPECT_EQ(third.denseToStable[*thirdDense.initialId(fixture.x)], stableInitX);
}

/* Construction measurements cover increasing snapshots and report packed primitive storage. */
TEST(CatGraphAdapterTest, BenchmarkIncreasingGraphSizes)
{
	std::size_t packedBytes{};
	const auto start = std::chrono::steady_clock::now();
	for (const auto size : {64U, 128U, 256U, 512U}) {
		TestExecutionGraph graph{{nullptr, nullptr, true}};
		WriteLabel *previous{};
		for (std::size_t index = 1; index < size; ++index) {
			auto *write = addLabel<WriteLabel>(graph, Event(0, static_cast<int>(index)),
							   MemOrdering::Relaxed, SAddr(0x1000),
							   ASize(4), SVal(index));
			write->addCo(previous ? static_cast<EventLabel *>(previous)
					      : static_cast<EventLabel *>(graph.getInitLabel()));
			previous = write;
		}
		cat::GraphAdapter adapter(graph);
		ASSERT_TRUE(adapter.validate().empty());
		for (const auto &[name, value] : adapter.baseValues()) {
			(void)name;
			packedBytes += std::visit(
				[](const auto &primitive) { return primitive.storageBytes(); },
				value);
		}
	}
	const auto elapsed = std::chrono::steady_clock::now() - start;
	RecordProperty("elapsed_microseconds",
		       std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
	RecordProperty("packed_bytes", packedBytes);
}
