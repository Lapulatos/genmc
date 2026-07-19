/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSkeletonEncoder.hpp"

#include <gtest/gtest.h>

#include <set>

using namespace genmc;

TEST(FiniteSkeletonEncoderTest, EnumeratesBothFiniteBranchActivations)
{
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back({.id = 0, .name = "main", .entry = 0, .isMain = true,
				     .blocks = {0, 1, 2}});
	program.blocks.push_back({.id = 0, .function = 0, .successors = {1, 2}, .condition = 0});
	program.blocks.push_back({.id = 1, .function = 0, .predecessors = {0}});
	program.blocks.push_back({.id = 2, .function = 0, .predecessors = {0}});
	program.values.push_back(
		{.id = 0, .block = 0, .opcode = skeleton::ValueOpcode::nondet, .width = 1});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::error,
				  .function = 0, .block = 1});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::error,
				  .function = 0, .block = 2});

	symbolic::FiniteSkeletonEncoder encoder(program);
	ASSERT_TRUE(encoder.supported());
	std::set<skeleton::NodeID> activeErrors;
	for (;;) {
		auto step = encoder.next();
		if (step.status == symbolic::CheckResult::unsat)
			break;
		ASSERT_EQ(step.status, symbolic::CheckResult::sat);
		ASSERT_TRUE(step.assignment);
		ASSERT_EQ(step.assignment->activeEvents.size(), 1U);
		activeErrors.insert(step.assignment->activeEvents.front());
	}
	EXPECT_EQ(activeErrors, (std::set<skeleton::NodeID>{0, 1}));
}

TEST(FiniteSkeletonEncoderTest, EnumeratesInitialAndStoreRfSources)
{
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.values.push_back({.id = 0, .block = 0, .opcode = skeleton::ValueOpcode::constant,
				  .width = 8, .constant = 7});
	program.values.push_back(
		{.id = 1, .block = 0, .opcode = skeleton::ValueOpcode::load, .width = 8});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::load,
				  .function = 0, .block = 0, .value = 1,
				  .address = "x", .width = 8});
	program.initialValues.push_back({.address = "x", .width = 8, .value = 0});

	symbolic::FiniteSkeletonEncoder encoder(program);
	ASSERT_TRUE(encoder.supported());
	std::set<skeleton::NodeID> sources;
	std::set<std::uint64_t> readValues;
	for (;;) {
		auto step = encoder.next();
		if (step.status == symbolic::CheckResult::unsat)
			break;
		ASSERT_EQ(step.status, symbolic::CheckResult::sat);
		ASSERT_TRUE(step.assignment);
		ASSERT_TRUE(step.assignment->readsFrom[1].has_value());
		sources.insert(*step.assignment->readsFrom[1]);
		readValues.insert(*step.assignment->values[1]);
	}
	EXPECT_EQ(sources,
		  (std::set<skeleton::NodeID>{skeleton::invalidNode, 0}));
	EXPECT_EQ(readValues, (std::set<std::uint64_t>{0, 7}));
}

TEST(FiniteSkeletonEncoderTest, ReportsSolverFreeRepresentationSize)
{
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0, 1}});
	program.blocks.push_back({.id = 0, .function = 0, .successors = {1}});
	program.blocks.push_back({.id = 1, .function = 0, .predecessors = {0}});
	program.values.push_back({.id = 0, .block = 0,
				  .opcode = skeleton::ValueOpcode::constant,
				  .width = 8, .constant = 7});
	program.values.push_back(
		{.id = 1, .block = 1, .opcode = skeleton::ValueOpcode::load, .width = 8});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 1, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 2, .kind = skeleton::EventKind::load,
				  .function = 0, .block = 1, .value = 1,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 3, .kind = skeleton::EventKind::error,
				  .function = 0, .block = 1});
	program.initialValues.push_back({.address = "x", .width = 8, .value = 0});

	const auto census = symbolic::censusFiniteRepresentation(program);
	EXPECT_EQ(census.blocks, 2U);
	EXPECT_EQ(census.cfgEdges, 1U);
	EXPECT_EQ(census.valueVariables, 1U);
	EXPECT_EQ(census.valueBits, 8U);
	EXPECT_EQ(census.errorEvents, 1U);
	EXPECT_EQ(census.rfReads, 1U);
	EXPECT_EQ(census.rfReadsWithoutSource, 0U);
	EXPECT_EQ(census.rfSelectors, 3U);
	EXPECT_EQ(census.rfPairs, 3U);
	EXPECT_EQ(census.rfValueClasses, 2U);
	EXPECT_EQ(census.rfValueClassPairs, 1U);
	EXPECT_EQ(census.rfValueProvenanceClasses, 2U);
	EXPECT_EQ(census.rfValueProvenanceClassPairs, 1U);
	EXPECT_EQ(census.rfValueMergeableSources, 1U);
	EXPECT_EQ(census.rfValueProvenanceMergeableSources, 1U);
	EXPECT_EQ(census.rfReadsWithValueMerge, 1U);
	EXPECT_EQ(census.rfReadsWithValueProvenanceMerge, 1U);
	EXPECT_EQ(census.rfLoadActivations, 3U);
	EXPECT_EQ(census.rfStoreActivations, 2U);
	EXPECT_EQ(census.rfValueConstraints, 3U);
	EXPECT_EQ(census.coRanks, 2U);
	EXPECT_EQ(census.coRankBits, 2U);
	EXPECT_EQ(census.coPairs, 1U);
	EXPECT_EQ(census.poPairs, 3U);
	EXPECT_EQ(census.potentialFrDerivations, 6U);
	EXPECT_EQ(census.maximumRfSources, 3U);
	EXPECT_EQ(census.maximumRfValueClassSize, 2U);
	EXPECT_EQ(census.maximumRfValueProvenanceClassSize, 2U);
	EXPECT_EQ(census.maximumWritesPerAddress, 2U);
}

TEST(FiniteSkeletonEncoderTest, AbstractCardinalityRequiresErrorAndPreservesRfChoice)
{
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.values.push_back({.id = 0, .block = 0,
				  .opcode = skeleton::ValueOpcode::constant,
				  .width = 8, .constant = 7});
	program.values.push_back(
		{.id = 1, .block = 0, .opcode = skeleton::ValueOpcode::load, .width = 8});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::load,
				  .function = 0, .block = 0, .value = 1,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 2, .kind = skeleton::EventKind::error,
				  .function = 0, .block = 0});
	program.initialValues.push_back({.address = "x", .width = 8, .value = 0});

	symbolic::FiniteSkeletonEncoder encoder(
		program, {.requireActiveError = true,
			  .encodeCo = false,
			  .rfCardinality = symbolic::RfCardinalityEncoding::native});
	std::set<skeleton::NodeID> sources;
	for (;;) {
		auto step = encoder.next();
		if (step.status == symbolic::CheckResult::unsat)
			break;
		ASSERT_EQ(step.status, symbolic::CheckResult::sat);
		ASSERT_TRUE(step.assignment);
		ASSERT_TRUE(step.assignment->readsFrom[1].has_value());
		EXPECT_TRUE(step.assignment->coherenceOrder.empty());
		sources.insert(*step.assignment->readsFrom[1]);
		encoder.blockCurrentGraph();
	}
	EXPECT_EQ(sources,
		  (std::set<skeleton::NodeID>{skeleton::invalidNode, 0}));
}

TEST(FiniteSkeletonEncoderTest, ValueClassFirstModelDefersSameValueSources)
{
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.values.push_back({.id = 0, .block = 0,
				  .opcode = skeleton::ValueOpcode::constant,
				  .width = 8, .constant = 7});
	program.values.push_back(
		{.id = 1, .block = 0, .opcode = skeleton::ValueOpcode::load, .width = 8});
	program.events.push_back({.id = 0, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 1, .kind = skeleton::EventKind::store,
				  .function = 0, .block = 0, .value = 0,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 2, .kind = skeleton::EventKind::load,
				  .function = 0, .block = 0, .value = 1,
				  .address = "x", .width = 8});
	program.events.push_back({.id = 3, .kind = skeleton::EventKind::error,
				  .function = 0, .block = 0});
	program.initialValues.push_back({.address = "x", .width = 8, .value = 0});

	symbolic::FiniteSkeletonEncoder encoder(
		program, {.requireActiveError = true,
			  .encodeCo = false,
			  .rfCardinality = symbolic::RfCardinalityEncoding::native,
			  .rfAbstraction = symbolic::RfAbstractionEncoding::value});
	std::set<std::uint64_t> readValues;
	for (;;) {
		auto step = encoder.next();
		if (step.status == symbolic::CheckResult::unsat)
			break;
		ASSERT_EQ(step.status, symbolic::CheckResult::sat);
		ASSERT_TRUE(step.assignment);
		EXPECT_TRUE(step.assignment->abstractReadsFrom);
		readValues.insert(*step.assignment->values[1]);
	}
	EXPECT_EQ(readValues, (std::set<std::uint64_t>{0, 7}));
}

TEST(FiniteSkeletonEncoderTest, GraphBlockingQuotientsUnusedInputValues)
{
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.values.push_back(
		{.id = 0, .block = 0, .opcode = skeleton::ValueOpcode::nondet, .width = 8});
	symbolic::FiniteSkeletonEncoder encoder(program);
	auto first = encoder.next();
	ASSERT_EQ(first.status, symbolic::CheckResult::sat);
	encoder.blockCurrentGraph();
	EXPECT_EQ(encoder.next().status, symbolic::CheckResult::unsat);
}

TEST(FiniteSkeletonEncoderTest, MalformedValueArityFailsOpen)
{
	if (!symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	skeleton::Program program;
	program.functions.push_back(
		{.id = 0, .name = "main", .entry = 0, .isMain = true, .blocks = {0}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.values.push_back({.id = 0, .block = 0,
				  .opcode = skeleton::ValueOpcode::constant,
				  .width = 1, .constant = 0});
	program.values.push_back({.id = 1, .block = 0,
				  .opcode = skeleton::ValueOpcode::icmp,
				  .width = 1, .operands = {0}});

	symbolic::FiniteSkeletonEncoder encoder(program);
	EXPECT_FALSE(encoder.supported());
	EXPECT_EQ(encoder.blockers(), (std::vector<std::string>{"value-arity:1"}));
	EXPECT_EQ(encoder.next().status, symbolic::CheckResult::unavailable);
}
