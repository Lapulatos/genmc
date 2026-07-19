/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSkeletonCAT.hpp"
#include "genmc/Verification/FiniteSkeletonSCCompletion.hpp"

#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <map>
#include <set>

namespace {

auto storeBufferingProgram() -> genmc::skeleton::Program
{
	using namespace genmc;
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "t0", .entry = 0, .isMain = true, .blocks = {0}});
	program.functions.push_back(
		{.id = 1, .name = "t1", .entry = 1, .isThreadEntry = true, .blocks = {1}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.blocks.push_back({.id = 1, .function = 1});
	program.values.push_back({.id = 0, .block = 0,
				  .opcode = skeleton::ValueOpcode::constant,
				  .width = 8, .constant = 1});
	program.values.push_back({.id = 1, .block = 0,
				  .opcode = skeleton::ValueOpcode::load, .width = 8});
	program.values.push_back({.id = 2, .block = 1,
				  .opcode = skeleton::ValueOpcode::load, .width = 8});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::load,
				  .function = 0, .block = 0, .value = 1,
				  .address = "y", .width = 8});
	program.events.push_back({.id = 2, .kind = skeleton::EventKind::store,
				  .function = 1, .block = 1, .value = 0,
				  .address = "y", .width = 8});
	program.events.push_back({.id = 3, .kind = skeleton::EventKind::load,
				  .function = 1, .block = 1, .value = 2,
				  .address = "x", .width = 8});
	program.initialValues.push_back({.address = "x", .width = 8, .value = 0});
	program.initialValues.push_back({.address = "y", .width = 8, .value = 0});
	return program;
}

auto exactSCBaseAccepts(const genmc::skeleton::Program &program,
			const genmc::symbolic::FiniteAssignment &assignment) -> bool
{
	auto [snapshot, base] =
		genmc::symbolic::materializeFiniteAssignment(program, assignment);
	if (!snapshot.errors.empty())
		return false;
	const auto &po = std::get<cat::Relation>(base.at("po"));
	const auto &tc = std::get<cat::Relation>(base.at("tc"));
	const auto &tj = std::get<cat::Relation>(base.at("tj"));
	const auto &rf = std::get<cat::Relation>(base.at("rf"));
	const auto &fr = std::get<cat::Relation>(base.at("fr"));
	const auto &co = std::get<cat::Relation>(base.at("co"));
	auto order = cat::relationUnion(po, tc);
	order = cat::relationUnion(order, tj);
	order = cat::relationUnion(order, rf);
	order = cat::relationUnion(order, fr);
	order = cat::relationUnion(order, co);
	const auto closure = cat::transitiveClosure(order);
	for (std::size_t event = 0; event < snapshot.eventCount; ++event)
		if (closure.contains(event, event))
			return false;
	return true;
}

TEST(FiniteSkeletonSCCompletionTest, RejectsSCStoreBufferingZeroZero)
{
	using namespace genmc;
	const auto program = storeBufferingProgram();
	symbolic::FiniteAssignment assignment;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(program.values.size());
	assignment.readsFrom.resize(program.events.size());
	assignment.readsFrom[1] = skeleton::invalidNode;
	assignment.readsFrom[3] = skeleton::invalidNode;

	const auto result = symbolic::completeFiniteSC(program, assignment);
	EXPECT_EQ(result.status, symbolic::FiniteSCCompletionStatus::noWitness);
	EXPECT_FALSE(result.assignment.has_value());
	EXPECT_TRUE(result.refinementSafe);
	EXPECT_EQ(result.rfCoreLoads.size(), 2U);
	EXPECT_GT(result.coreChecks, 0U);
	const auto ordering = symbolic::completeFiniteSCWithOrdering(program, assignment);
	EXPECT_EQ(ordering.status, symbolic::FiniteSCCompletionStatus::noWitness);
	EXPECT_TRUE(ordering.refinementSafe);
	EXPECT_FALSE(ordering.rfCoreLoads.empty());
	EXPECT_EQ(ordering.coreChecks, 1U);
}

TEST(FiniteSkeletonSCCompletionTest, RejectsAbstractReadsFromClasses)
{
	using namespace genmc;
	const auto program = storeBufferingProgram();
	symbolic::FiniteAssignment assignment;
	assignment.abstractReadsFrom = true;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(program.values.size());
	assignment.readsFrom.resize(program.events.size());

	const auto result = symbolic::completeFiniteSC(program, assignment);
	EXPECT_EQ(result.status, symbolic::FiniteSCCompletionStatus::invalidInput);
	EXPECT_NE(result.error.find("must be refined"), std::string::npos);
	const auto ordering = symbolic::completeFiniteSCWithOrdering(program, assignment);
	EXPECT_EQ(ordering.status, symbolic::FiniteSCCompletionStatus::invalidInput);
	EXPECT_NE(ordering.error.find("must be refined"), std::string::npos);
}

TEST(FiniteSkeletonSCCompletionTest, ValueClassRefinementMatchesConcreteRfOracle)
{
	using namespace genmc;
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	auto program = storeBufferingProgram();
	program.events.push_back({.id = 4, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 5, .kind = skeleton::EventKind::store,
				  .function = 1, .block = 1, .value = 0,
				  .address = "y", .width = 8});
	program.events.push_back({.id = 6, .kind = skeleton::EventKind::threadCreate,
				  .function = 0, .block = 0, .threadEntry = "t1"});
	using Key = std::pair<skeleton::NodeID, skeleton::NodeID>;
	using Status = symbolic::FiniteSCCompletionStatus;
	const auto options = [](symbolic::RfAbstractionEncoding abstraction) {
		return symbolic::FiniteEncodingOptions{
			.encodeCo = false,
			.rfCardinality = symbolic::RfCardinalityEncoding::native,
			.rfAbstraction = abstraction};
	};

	std::map<Key, Status> concreteOracle;
	symbolic::FiniteSkeletonEncoder concrete(
		program, options(symbolic::RfAbstractionEncoding::concrete));
	ASSERT_TRUE(concrete.supported());
	for (unsigned guard = 0; guard < 32; ++guard) {
		auto step = concrete.next();
		if (!step.assignment) {
			EXPECT_EQ(step.status, symbolic::CheckResult::unsat);
			break;
		}
		auto completion = symbolic::completeFiniteSCWithOrdering(program, *step.assignment);
		concreteOracle[{*step.assignment->readsFrom[1],
				*step.assignment->readsFrom[3]}] = completion.status;
		concrete.blockCurrentGraph();
	}

	std::map<Key, Status> refinedOracle;
	symbolic::FiniteSkeletonEncoder abstract(
		program, options(symbolic::RfAbstractionEncoding::value));
	ASSERT_TRUE(abstract.supported());
	for (unsigned guard = 0; guard < 32; ++guard) {
		auto step = abstract.next();
		if (!step.assignment) {
			EXPECT_EQ(step.status, symbolic::CheckResult::unsat);
			break;
		}
		if (step.assignment->abstractReadsFrom)
			step = abstract.refineCurrentRfClasses();
		ASSERT_TRUE(step.assignment);
		ASSERT_FALSE(step.assignment->abstractReadsFrom);
		auto completion =
			symbolic::completeFiniteSCWithOrdering(program, *step.assignment);
		refinedOracle[{*step.assignment->readsFrom[1],
				*step.assignment->readsFrom[3]}] = completion.status;
		abstract.blockCurrentGraph();
	}
	EXPECT_EQ(refinedOracle, concreteOracle);
	EXPECT_EQ(concreteOracle.size(), 9U);

	std::set<std::pair<std::uint64_t, std::uint64_t>> concreteObservations;
	for (const auto &[sources, status] : concreteOracle)
		if (status == Status::completed)
			concreteObservations.emplace(
				sources.first == skeleton::invalidNode ? 0 : 1,
				sources.second == skeleton::invalidNode ? 0 : 1);
	std::set<std::pair<std::uint64_t, std::uint64_t>> classOrderObservations;
	symbolic::FiniteSkeletonEncoder classOrder(
		program,
		{.encodeCo = false,
		 .encodeSCOrder = true,
		 .rfCardinality = symbolic::RfCardinalityEncoding::native,
		 .rfAbstraction = symbolic::RfAbstractionEncoding::value});
	ASSERT_TRUE(classOrder.supported());
	for (unsigned guard = 0; guard < 16; ++guard) {
		auto step = classOrder.next();
		if (!step.assignment) {
			EXPECT_EQ(step.status, symbolic::CheckResult::unsat);
			break;
		}
		EXPECT_FALSE(step.assignment->abstractReadsFrom);
		EXPECT_TRUE(exactSCBaseAccepts(program, *step.assignment));
		classOrderObservations.emplace(*step.assignment->values[1],
					       *step.assignment->values[2]);
		classOrder.blockCurrentGraph();
	}
	EXPECT_EQ(classOrderObservations, concreteObservations);
}

TEST(FiniteSkeletonSCCompletionTest, SCOrderEncodingFailsOpenForThreadJoin)
{
	using namespace genmc;
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	auto program = storeBufferingProgram();
	program.events[0].kind = skeleton::EventKind::threadJoin;
	symbolic::FiniteSkeletonEncoder encoder(
		program,
		{.encodeCo = false,
		 .encodeSCOrder = true,
		 .rfCardinality = symbolic::RfCardinalityEncoding::native,
		 .rfAbstraction = symbolic::RfAbstractionEncoding::value});
	EXPECT_FALSE(encoder.supported());
	EXPECT_NE(std::ranges::find(encoder.blockers(), "sc-order-unsupported-thread-join"),
		  encoder.blockers().end());
}

TEST(FiniteSkeletonSCCompletionTest, SCOrderEncodingMaterializesAtomicSections)
{
	using namespace genmc;
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::lock,
				  .function = 0, .block = 0, .address = "mutex"});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::unlock,
				  .function = 0, .block = 0, .address = "mutex"});
	program.events.push_back({.id = 2, .kind = skeleton::EventKind::lock,
				  .function = 0, .block = 0, .address = "mutex"});

	symbolic::FiniteSkeletonEncoder encoder(
		program,
		{.encodeCo = false,
		 .encodeSCOrder = true,
		 .rfCardinality = symbolic::RfCardinalityEncoding::native,
		 .rfAbstraction = symbolic::RfAbstractionEncoding::value});
	ASSERT_TRUE(encoder.supported());
	auto step = encoder.next();
	ASSERT_TRUE(step.assignment);
	EXPECT_FALSE(step.assignment->abstractReadsFrom);
	EXPECT_EQ(*step.assignment->readsFrom[0], skeleton::invalidNode);
	EXPECT_EQ(*step.assignment->readsFrom[2], 1U);
	EXPECT_TRUE(exactSCBaseAccepts(program, *step.assignment));
}

TEST(FiniteSkeletonSCCompletionTest, SCOrderEncodingMatchesConcreteObservationOracle)
{
	using namespace genmc;
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	for (unsigned firstLoadPosition = 0; firstLoadPosition < 3;
	     ++firstLoadPosition)
		for (unsigned secondLoadPosition = 0; secondLoadPosition < 3;
		     ++secondLoadPosition)
			for (unsigned firstMixed = 0; firstMixed < 2; ++firstMixed)
				for (unsigned secondMixed = 0; secondMixed < 2;
				     ++secondMixed) {
					skeleton::Program program;
					program.functions.push_back(
						{.id = 0, .name = "t0", .entry = 0,
						 .isMain = true, .blocks = {0}});
					program.functions.push_back(
						{.id = 1, .name = "t1", .entry = 1,
						 .isThreadEntry = true, .blocks = {1}});
					program.blocks.push_back({.id = 0, .function = 0});
					program.blocks.push_back({.id = 1, .function = 1});
					program.values.push_back(
						{.id = 0, .block = 0,
						 .opcode = skeleton::ValueOpcode::constant,
						 .width = 8, .constant = 1});
					program.values.push_back(
						{.id = 1, .block = 0,
						 .opcode = skeleton::ValueOpcode::constant,
						 .width = 8, .constant = 2});
					program.values.push_back(
						{.id = 2, .block = 0,
						 .opcode = skeleton::ValueOpcode::load, .width = 8});
					program.values.push_back(
						{.id = 3, .block = 1,
						 .opcode = skeleton::ValueOpcode::load, .width = 8});
					program.events.push_back(
						{.id = 0, .kind = skeleton::EventKind::threadCreate,
						 .function = 0, .block = 0, .threadEntry = "t1"});
					const auto addThread = [&](skeleton::NodeID function,
								   unsigned loadPosition,
								   bool mixed,
								   std::string ownAddress,
								   std::string otherAddress,
								   skeleton::NodeID loadValue) {
						unsigned storeIndex{};
						for (unsigned position = 0; position < 3; ++position) {
							const auto id = static_cast<skeleton::NodeID>(
								program.events.size());
							if (position == loadPosition) {
								program.events.push_back(
									{.id = id,
									 .kind = skeleton::EventKind::load,
									 .function = function,
									 .block = function,
									 .value = loadValue,
									 .address = otherAddress,
									 .width = 8});
							} else {
								program.events.push_back(
									{.id = id,
									 .kind = skeleton::EventKind::store,
									 .function = function,
									 .block = function,
									 .value = mixed && storeIndex == 1 ? 1U : 0U,
									 .address = ownAddress,
									 .width = 8});
								++storeIndex;
							}
						}
					};
					addThread(0, firstLoadPosition, firstMixed, "x", "y", 2);
					addThread(1, secondLoadPosition, secondMixed, "y", "x", 3);
					program.initialValues.push_back(
						{.address = "x", .width = 8, .value = 0});
					program.initialValues.push_back(
						{.address = "y", .width = 8, .value = 0});

					using Observation = std::pair<std::uint64_t, std::uint64_t>;
					std::set<Observation> concrete;
					symbolic::FiniteSkeletonEncoder exact(
						program,
						{.encodeCo = false,
						 .rfCardinality = symbolic::RfCardinalityEncoding::native});
					ASSERT_TRUE(exact.supported());
					for (unsigned guard = 0; guard < 32; ++guard) {
						auto step = exact.next();
						if (!step.assignment)
							break;
						auto completion = symbolic::completeFiniteSCWithOrdering(
							program, *step.assignment);
						if (completion.status ==
						    symbolic::FiniteSCCompletionStatus::completed)
							concrete.emplace(*step.assignment->values[2],
									 *step.assignment->values[3]);
						exact.blockCurrentGraph();
					}

					std::set<Observation> abstract;
					symbolic::FiniteSkeletonEncoder scOrder(
						program,
						{.encodeCo = false,
						 .encodeSCOrder = true,
						 .rfCardinality = symbolic::RfCardinalityEncoding::native,
						 .rfAbstraction = symbolic::RfAbstractionEncoding::value});
					ASSERT_TRUE(scOrder.supported());
					for (unsigned guard = 0; guard < 16; ++guard) {
						auto step = scOrder.next();
						if (!step.assignment)
							break;
						ASSERT_FALSE(step.assignment->abstractReadsFrom);
						ASSERT_TRUE(exactSCBaseAccepts(program, *step.assignment));
						abstract.emplace(*step.assignment->values[2],
								 *step.assignment->values[3]);
						scOrder.blockCurrentGraph();
					}
					EXPECT_EQ(abstract, concrete)
						<< "load positions=" << firstLoadPosition << ","
						<< secondLoadPosition << " mixed=" << firstMixed << ","
						<< secondMixed;
				}
}

TEST(FiniteSkeletonSCCompletionTest, CompletesEverySCFeasibleStoreBufferingRfShape)
{
	using namespace genmc;
	const auto program = storeBufferingProgram();
	const std::array<skeleton::NodeID, 2> firstSources = {skeleton::invalidNode, 2};
	const std::array<skeleton::NodeID, 2> secondSources = {skeleton::invalidNode, 0};
	for (const auto first : firstSources) {
		for (const auto second : secondSources) {
			if (first == skeleton::invalidNode && second == skeleton::invalidNode)
				continue;
			symbolic::FiniteAssignment assignment;
			assignment.activeEvents = {0, 1, 2, 3};
			assignment.values.resize(program.values.size());
			assignment.readsFrom.resize(program.events.size());
			assignment.readsFrom[1] = first;
			assignment.readsFrom[3] = second;

			const auto result = symbolic::completeFiniteSC(program, assignment);
			ASSERT_EQ(result.status, symbolic::FiniteSCCompletionStatus::completed)
				<< "first=" << first << " second=" << second << " error="
				<< result.error;
			ASSERT_TRUE(result.assignment);
			EXPECT_EQ(result.assignment->coherenceOrder.size(), 2U);
			auto [snapshot, base] =
				symbolic::materializeFiniteAssignment(program, *result.assignment);
			EXPECT_TRUE(snapshot.errors.empty());
			EXPECT_EQ(std::get<cat::Relation>(base.at("co")).count(), 2U);
		}
	}
}

TEST(FiniteSkeletonSCCompletionTest, FailsOpenWhenAnActiveReadHasNoRfSource)
{
	using namespace genmc;
	const auto program = storeBufferingProgram();
	symbolic::FiniteAssignment assignment;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(program.values.size());
	assignment.readsFrom.resize(program.events.size());
	assignment.readsFrom[1] = skeleton::invalidNode;

	const auto result = symbolic::completeFiniteSC(program, assignment);
	EXPECT_EQ(result.status, symbolic::FiniteSCCompletionStatus::invalidInput);
	EXPECT_FALSE(result.error.empty());
}

TEST(FiniteSkeletonSCCompletionTest, MatchesExhaustiveCoOracleForAllFixedRfShapes)
{
	using namespace genmc;
	auto program = storeBufferingProgram();
	program.events[1].address = "x";
	program.events[2].address = "x";
	program.events[3].address = "x";
	program.initialValues.resize(1U);
	const std::array<skeleton::NodeID, 3> sources = {skeleton::invalidNode, 0, 2};
	for (const auto first : sources) {
		for (const auto second : sources) {
			symbolic::FiniteAssignment assignment;
			assignment.activeEvents = {0, 1, 2, 3};
			assignment.values.resize(program.values.size());
			assignment.readsFrom.resize(program.events.size());
			assignment.readsFrom[1] = first;
			assignment.readsFrom[3] = second;

			bool oracle = false;
			for (const auto &co : {std::array<skeleton::NodeID, 2>{0, 2},
					       std::array<skeleton::NodeID, 2>{2, 0}}) {
				auto concrete = assignment;
				concrete.coherenceOrder.assign(co.begin(), co.end());
				oracle = oracle || exactSCBaseAccepts(program, concrete);
			}
			const auto result = symbolic::completeFiniteSC(program, assignment);
			EXPECT_EQ(result.status == symbolic::FiniteSCCompletionStatus::completed,
				  oracle)
				<< "first=" << first << " second=" << second
				<< " error=" << result.error;
			const auto ordering =
				symbolic::completeFiniteSCWithOrdering(program, assignment);
			EXPECT_EQ(ordering.status ==
					  symbolic::FiniteSCCompletionStatus::completed,
				  oracle)
				<< "ordering first=" << first << " second=" << second
				<< " error=" << ordering.error;
		}
	}
}

TEST(FiniteSkeletonSCCompletionTest, RfCoreRefinementFindsACompletableGraph)
{
	using namespace genmc;
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	auto program = storeBufferingProgram();
	/* This unit fixture has no explicit create event; enable its second finite entry
	 * directly so the encoder can enumerate the same two-thread event set. */
	program.functions[1].isMain = true;
	program.events.push_back({.id = 4, .kind = skeleton::EventKind::error,
				  .function = 0, .block = 0});
	symbolic::FiniteSkeletonEncoder encoder(
		program, {.requireActiveError = true,
			  .encodeCo = false,
			  .rfCardinality = symbolic::RfCardinalityEncoding::native});
	bool completed = false;
	for (std::size_t attempt = 0; attempt < 16U; ++attempt) {
		auto step = encoder.next();
		ASSERT_EQ(step.status, symbolic::CheckResult::sat);
		ASSERT_TRUE(step.assignment);
		const auto completion = symbolic::completeFiniteSC(program, *step.assignment);
		if (completion.status == symbolic::FiniteSCCompletionStatus::completed) {
			completed = true;
			break;
		}
		ASSERT_EQ(completion.status, symbolic::FiniteSCCompletionStatus::noWitness);
		ASSERT_TRUE(completion.refinementSafe);
		encoder.blockCurrentRfCore(completion.rfCoreLoads);
	}
	EXPECT_TRUE(completed);
}

} /* namespace */
