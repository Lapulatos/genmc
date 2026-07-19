#include "genmc/Verification/CATDecisionState.hpp"

#include "genmc/ADT/View.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Verification/Config.hpp"

#include <gtest/gtest.h>

namespace {

class TestExecutionGraph : public ExecutionGraph {
public:
	using ExecutionGraph::addLabelToGraph;
	using ExecutionGraph::ExecutionGraph;
};

template <typename Label, typename... Args>
auto addLabel(TestExecutionGraph &graph, Args &&...args) -> Label *
{
	auto label = std::make_unique<Label>(std::forward<Args>(args)...);
	return static_cast<Label *>(graph.addLabelToGraph(std::move(label)));
}

TEST(CATDecisionState, ReplacesChoiceWithFreshBranchOrdinal)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	auto *first = addLabel<WriteLabel>(graph, Event{0, 1}, MemOrdering::Relaxed,
					     SAddr{0x1000}, ASize{4}, SVal{1});
	first->addCo(graph.getInitLabel());
	auto *second = addLabel<WriteLabel>(graph, Event{0, 2}, MemOrdering::Relaxed,
					      SAddr{0x1000}, ASize{4}, SVal{1});
	second->addCo(first);
	graph.addNewThread();
	auto *read = addLabel<ReadLabel>(graph, Event{1, 0}, MemOrdering::Relaxed,
					   SAddr{0x1000}, ASize{4});
	read->setRf(first);
	genmc::catcensus::DecisionState state;
	state.recordRf(*read);
	ASSERT_NE(state.find(read->getPos()), nullptr);
	EXPECT_EQ(state.find(read->getPos())->ordinal, 1U);
	read->setRf(second);
	state.recordRf(*read);
	EXPECT_EQ(state.size(), 1U);
	EXPECT_EQ(state.find(read->getPos())->ordinal, 2U);
	EXPECT_EQ(state.find(read->getPos())->alternative,
		  cat::StableEventKey{second->getPos()});
	state.recordRf(*read);
	EXPECT_EQ(state.nextOrdinal(), 3U);
}

TEST(CATDecisionState, CutDropsSubjectAndRealAlternativeButKeepsInitialAddress)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	graph.addNewThread();
	graph.addNewThread();
	graph.addNewThread();
	(void)addLabel<FenceLabel>(graph, Event{1, 0}, MemOrdering::Acquire);
	(void)addLabel<FenceLabel>(graph, Event{2, 0}, MemOrdering::Acquire);
	(void)addLabel<FenceLabel>(graph, Event{3, 0}, MemOrdering::Acquire);
	auto *write = addLabel<WriteLabel>(graph, Event{2, 1}, MemOrdering::Relaxed,
					     SAddr{0x2000}, ASize{4}, SVal{1});
	write->addCo(graph.getInitLabel());
	auto *realRead = addLabel<ReadLabel>(graph, Event{1, 1}, MemOrdering::Relaxed,
					       SAddr{0x2000}, ASize{4});
	realRead->setRf(write);
	auto *initRead = addLabel<ReadLabel>(graph, Event{3, 1}, MemOrdering::Relaxed,
					       SAddr{0x3000}, ASize{4});
	initRead->setRf(graph.getInitLabel());
	genmc::catcensus::DecisionState state;
	state.recordRf(*realRead);
	state.recordRf(*initRead);

	View prefix;
	prefix.setMax(Event{1, 1});
	prefix.setMax(Event{2, 0});
	prefix.setMax(Event{3, 1});
	state.cut(prefix);
	EXPECT_EQ(state.find(realRead->getPos()), nullptr);
	ASSERT_NE(state.find(initRead->getPos()), nullptr);
	EXPECT_EQ(state.find(initRead->getPos())->alternative,
		  cat::StableEventKey{initRead->getAddr()});
}

TEST(CATDecisionState, CopiesRemainBranchLocal)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	graph.addNewThread();
	(void)addLabel<FenceLabel>(graph, Event{1, 0}, MemOrdering::Acquire);
	auto *read = addLabel<ReadLabel>(graph, Event{1, 1}, MemOrdering::Relaxed,
					   SAddr{0x4000}, ASize{4});
	read->setRf(graph.getInitLabel());
	genmc::catcensus::DecisionState parent;
	parent.recordRf(*read);
	auto child = parent;
	View empty;
	child.cut(empty);
	EXPECT_EQ(child.size(), 0U);
	EXPECT_EQ(parent.size(), 1U);
}

TEST(CATDecisionState, TracksImmediateCoPredecessorReplacement)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x5000};
	auto *first = addLabel<WriteLabel>(graph, Event{0, 1}, MemOrdering::Relaxed, x,
					     ASize{4}, SVal{1});
	first->addCo(graph.getInitLabel());
	auto *second = addLabel<WriteLabel>(graph, Event{0, 2}, MemOrdering::Relaxed, x,
					      ASize{4}, SVal{2});
	second->addCo(first);
	genmc::catcensus::DecisionState state;
	state.recordCo(*second);
	ASSERT_NE(state.find(second->getPos()), nullptr);
	EXPECT_EQ(state.find(second->getPos())->alternative,
		  cat::StableEventKey{first->getPos()});
	second->moveCo(graph.getInitLabel());
	state.recordCo(*second);
	EXPECT_EQ(state.find(second->getPos())->ordinal, 2U);
	EXPECT_EQ(state.find(second->getPos())->alternative, cat::StableEventKey{x});
}

} /* namespace */
