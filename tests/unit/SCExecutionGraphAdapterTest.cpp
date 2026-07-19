/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/SCExecutionGraphAdapter.hpp"

#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

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

TEST(SCExecutionGraphAdapter, BuildsWitnessProblemWithVirtualInitialWrite)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	auto *write = addLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::Relaxed, x, ASize(4),
					   SVal(1));
	write->addCo(graph.getInitLabel());
	auto *read = addLabel<ReadLabel>(graph, Event(0, 2), MemOrdering::Relaxed, x, ASize(4));
	read->setRf(write);
	genmc::rvf::StableGoodWrites goodWrites;
	goodWrites[cat::StableEventKey{read->getPos()}] = {cat::StableEventKey{x},
							   cat::StableEventKey{write->getPos()}};

	const auto adapted = genmc::rvf::buildGraphProblem(graph, goodWrites);
	ASSERT_TRUE(adapted.error.empty()) << adapted.error;
	ASSERT_EQ(adapted.problem.events.size(), 3U);
	EXPECT_EQ(adapted.problem.events[0].kind, genmc::rvf::EventKind::write);
	EXPECT_EQ(adapted.problem.events[1].kind, genmc::rvf::EventKind::read);
	EXPECT_EQ(adapted.problem.events[2].kind, genmc::rvf::EventKind::write);
	EXPECT_EQ(adapted.problem.goodWrites[1], (std::vector<genmc::rvf::EventId>{0, 2}));
	const auto result = genmc::rvf::verifySC(adapted.problem);
	EXPECT_EQ(result.status, genmc::rvf::Status::witness);
	ASSERT_EQ(result.readsFrom.size(), adapted.problem.events.size());
	EXPECT_EQ(result.readsFrom[1], 0U);
	read->setRf(graph.getInitLabel());
	EXPECT_TRUE(genmc::rvf::applyGraphWitness(graph, adapted, result).empty());
	EXPECT_EQ(read->getRf(), write);
}

TEST(SCExecutionGraphAdapter, RejectsRmwWithoutChangingExploration)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	auto *read = addLabel<FaiReadLabel>(graph, Event(0, 1), MemOrdering::Relaxed, x, ASize(4),
					    RMWBinOp::Add, SVal(1));
	read->setRf(graph.getInitLabel());
	genmc::rvf::StableGoodWrites goodWrites;
	goodWrites[cat::StableEventKey{read->getPos()}] = {cat::StableEventKey{x}};
	const auto adapted = genmc::rvf::buildGraphProblem(graph, goodWrites);
	EXPECT_FALSE(adapted.error.empty());
}

TEST(SCExecutionGraphAdapter, MapsSuccessfulLockCasAsAtomicPair)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr lock{0x1000};
	auto *read = addLabel<LockCasReadLabel>(graph, Event(0, 1), lock, ASize(4));
	read->setRf(graph.getInitLabel());
	auto *write = addLabel<LockCasWriteLabel>(graph, Event(0, 2), lock, ASize(4));
	write->addCo(graph.getInitLabel());
	genmc::rvf::StableGoodWrites goodWrites;
	goodWrites[cat::StableEventKey{read->getPos()}] = {cat::StableEventKey{lock}};

	const auto adapted = genmc::rvf::buildGraphProblem(graph, goodWrites);
	ASSERT_TRUE(adapted.error.empty()) << adapted.error;
	ASSERT_EQ(adapted.problem.events.size(), 3U);
	EXPECT_EQ(adapted.problem.events[0].kind, genmc::rvf::EventKind::read);
	EXPECT_EQ(adapted.problem.events[0].atomicSuccessor, 1U);
	EXPECT_EQ(adapted.problem.events[1].kind, genmc::rvf::EventKind::write);
	const auto witness = genmc::rvf::verifySC(adapted.problem);
	ASSERT_EQ(witness.status, genmc::rvf::Status::witness);
	ASSERT_EQ(witness.witness.size(), 3U);
	const auto readIt = std::ranges::find(witness.witness, 0U);
	ASSERT_NE(readIt, witness.witness.end());
	ASSERT_NE(std::next(readIt), witness.witness.end());
	EXPECT_EQ(*std::next(readIt), 1U);
	EXPECT_TRUE(genmc::rvf::applyGraphWitness(graph, adapted, witness).empty());
	EXPECT_EQ(read->getRf(), graph.getInitLabel());
}

TEST(SCExecutionGraphAdapter, MapsFailedLockCasAsReadOnly)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr lock{0x1000};
	auto *locked = addLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::Relaxed, lock,
					    ASize(4), SVal(1));
	locked->addCo(graph.getInitLabel());
	auto *read = addLabel<LockCasReadLabel>(graph, Event(0, 2), lock, ASize(4));
	read->setRf(locked);
	genmc::rvf::StableGoodWrites goodWrites;
	goodWrites[cat::StableEventKey{read->getPos()}] = {cat::StableEventKey{locked->getPos()}};

	const auto adapted = genmc::rvf::buildGraphProblem(graph, goodWrites);
	ASSERT_TRUE(adapted.error.empty()) << adapted.error;
	const auto readId = static_cast<genmc::rvf::EventId>(
		std::ranges::find(adapted.denseToStable, cat::StableEventKey{read->getPos()}) -
		adapted.denseToStable.begin());
	EXPECT_FALSE(adapted.problem.events[readId].atomicSuccessor.has_value());
	EXPECT_EQ(genmc::rvf::verifySC(adapted.problem).status, genmc::rvf::Status::witness);
}

TEST(SCExecutionGraphAdapter, MapsUnlockAsPlainWrite)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr lock{0x1000};
	auto *unlock = addLabel<UnlockWriteLabel>(graph, Event(0, 1), MemOrdering::Release, lock,
						  ASize(4), SVal(0));
	unlock->addCo(graph.getInitLabel());

	const auto adapted = genmc::rvf::buildGraphProblem(graph, {});
	ASSERT_TRUE(adapted.error.empty()) << adapted.error;
	ASSERT_EQ(adapted.problem.events.size(), 2U);
	EXPECT_EQ(adapted.problem.events[0].kind, genmc::rvf::EventKind::write);
}

TEST(SCExecutionGraphAdapter, RejectsNonAtomicMemoryEvents)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	auto *write = addLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::NotAtomic, x, ASize(4),
					   SVal(1));
	write->addCo(graph.getInitLabel());
	const auto adapted = genmc::rvf::buildGraphProblem(graph, {});
	EXPECT_FALSE(adapted.error.empty());
}

TEST(SCExecutionGraphAdapter, AppliesWitnessDerivedCoherenceOrder)
{
	TestExecutionGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	auto *first = addLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::SequentiallyConsistent,
					   x, ASize(4), SVal(1));
	first->addCo(graph.getInitLabel());
	auto *second = addLabel<WriteLabel>(graph, Event(0, 2), MemOrdering::SequentiallyConsistent,
					    x, ASize(4), SVal(1));
	second->addCo(first);

	const auto adapted = genmc::rvf::buildGraphProblem(graph, {});
	ASSERT_TRUE(adapted.error.empty()) << adapted.error;
	const auto firstId = static_cast<genmc::rvf::EventId>(
		std::ranges::find(adapted.denseToStable, cat::StableEventKey{first->getPos()}) -
		adapted.denseToStable.begin());
	const auto secondId = static_cast<genmc::rvf::EventId>(
		std::ranges::find(adapted.denseToStable, cat::StableEventKey{second->getPos()}) -
		adapted.denseToStable.begin());
	const auto initId = static_cast<genmc::rvf::EventId>(
		std::ranges::find(adapted.denseToStable, cat::StableEventKey{x}) -
		adapted.denseToStable.begin());
	genmc::rvf::Result witness;
	witness.status = genmc::rvf::Status::witness;
	witness.witness = {initId, secondId, firstId};
	witness.readsFrom.resize(adapted.problem.events.size());

	ASSERT_TRUE(genmc::rvf::applyGraphWitness(graph, adapted, witness).empty());
	ASSERT_EQ(graph.co_imm_pred(first), second);
	EXPECT_EQ(graph.co_max(x), first);
}

} /* namespace */
