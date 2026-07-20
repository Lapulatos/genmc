/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/CAT/Frontend.hpp"
#include "genmc/Verification/FiniteSkeletonCAT.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <unistd.h>

namespace {

auto compileModel(std::string_view source) -> std::shared_ptr<const cat::ModelIR>
{
	static std::atomic<std::uint64_t> next{};
	auto path = std::filesystem::path(testing::TempDir()) /
		    ("finite-skeleton-cat-" + std::to_string(getpid()) + "-" +
		     std::to_string(next++) + ".cat");
	std::ofstream output(path);
	output << source;
	output.close();
	auto parsed = cat::Frontend().parseFile(path);
	std::filesystem::remove(path);
	EXPECT_TRUE(parsed.ok());
	if (!parsed.ok())
		return {};
	auto compiled = cat::Compiler().compile(*parsed.model);
	EXPECT_TRUE(compiled.ok());
	return compiled.model;
}

struct AnalyzedModel {
	std::shared_ptr<const cat::NormalizedModel> model;
	std::shared_ptr<const cat::ModelAnalysis> analysis;
};

auto analyzeModel(std::string_view source) -> AnalyzedModel
{
	static std::atomic<std::uint64_t> next{};
	auto path = std::filesystem::path(testing::TempDir()) /
		    ("finite-skeleton-caat-" + std::to_string(getpid()) + "-" +
		     std::to_string(next++) + ".cat");
	std::ofstream output(path);
	output << source;
	output.close();
	auto parsed = cat::Frontend().parseFile(path);
	std::filesystem::remove(path);
	EXPECT_TRUE(parsed.ok());
	if (!parsed.ok())
		return {};
	auto normalized = cat::Normalizer().normalize(*parsed.model);
	EXPECT_TRUE(normalized.ok());
	if (!normalized.ok())
		return {};
	auto analyzed = cat::Analyzer().analyze(*normalized.model);
	EXPECT_TRUE(analyzed.ok());
	return {std::move(normalized.model), std::move(analyzed.analysis)};
}

auto storeBufferingProgram() -> genmc::skeleton::Program
{
	using namespace genmc::skeleton;
	Program program;
	program.functions.push_back(
		{.id = 0, .name = "p0", .entry = 0, .isThreadEntry = true, .blocks = {0}});
	program.functions.push_back(
		{.id = 1, .name = "p1", .entry = 1, .isThreadEntry = true, .blocks = {1}});
	program.blocks.push_back({.id = 0, .function = 0});
	program.blocks.push_back({.id = 1, .function = 1});
	program.values.push_back(
		{.id = 0, .block = 0, .opcode = ValueOpcode::constant, .width = 8, .constant = 1});
	program.values.push_back({.id = 1, .block = 0, .opcode = ValueOpcode::load, .width = 8});
	program.values.push_back(
		{.id = 2, .block = 1, .opcode = ValueOpcode::constant, .width = 8, .constant = 1});
	program.values.push_back({.id = 3, .block = 1, .opcode = ValueOpcode::load, .width = 8});
	program.events.push_back({.id = 0, .kind = EventKind::store, .function = 0,
				  .block = 0, .value = 0, .address = "x", .width = 8});
	program.events.push_back({.id = 1, .kind = EventKind::load, .function = 0,
				  .block = 0, .value = 1, .address = "y", .width = 8});
	program.events.push_back({.id = 2, .kind = EventKind::store, .function = 1,
				  .block = 1, .value = 2, .address = "y", .width = 8});
	program.events.push_back({.id = 3, .kind = EventKind::load, .function = 1,
				  .block = 1, .value = 3, .address = "x", .width = 8});
	program.initialValues.push_back({.address = "x", .width = 8, .value = 0});
	program.initialValues.push_back({.address = "y", .width = 8, .value = 0});
	return program;
}

} /* namespace */

TEST(FiniteSkeletonCATTest, GenericEvaluatorRejectsStoreBufferingCycle)
{
	auto model = compileModel("M\nlet com = rf | fr | co\nacyclic po | com as sc\n");
	ASSERT_TRUE(model);
	auto program = storeBufferingProgram();
	genmc::symbolic::FiniteAssignment assignment;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(4);
	assignment.readsFrom.resize(4);
	assignment.readsFrom[1] = genmc::skeleton::invalidNode;
	assignment.readsFrom[3] = genmc::skeleton::invalidNode;
	assignment.coherenceOrder = {0, 2};
	auto result = genmc::symbolic::evaluateFiniteAssignment(program, assignment, *model);
	EXPECT_TRUE(result.errors.empty());
	EXPECT_FALSE(result.evaluation.consistent());
	ASSERT_EQ(result.evaluation.violations.size(), 1U);
	EXPECT_EQ(result.evaluation.violations.front().checkName, "sc");
}

TEST(FiniteSkeletonCATTest, GenericEvaluatorAcceptsConsistentRfAssignment)
{
	auto model = compileModel("M\nlet com = rf | fr | co\nacyclic po | com as sc\n");
	ASSERT_TRUE(model);
	auto program = storeBufferingProgram();
	genmc::symbolic::FiniteAssignment assignment;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(4);
	assignment.readsFrom.resize(4);
	assignment.readsFrom[1] = 2;
	assignment.readsFrom[3] = 0;
	assignment.coherenceOrder = {0, 2};
	auto result = genmc::symbolic::evaluateFiniteAssignment(program, assignment, *model);
	EXPECT_TRUE(result.errors.empty());
	EXPECT_TRUE(result.evaluation.consistent());
}

TEST(FiniteSkeletonCATTest, RejectsAbstractReadsFromClasses)
{
	auto model = compileModel("M\nacyclic po | rf as sc\n");
	ASSERT_TRUE(model);
	auto program = storeBufferingProgram();
	genmc::symbolic::FiniteAssignment assignment;
	assignment.abstractReadsFrom = true;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(4);
	assignment.readsFrom.resize(4);
	assignment.readsFrom[1] = 2;
	assignment.readsFrom[3] = 0;

	auto result = genmc::symbolic::evaluateFiniteAssignment(program, assignment, *model);
	ASSERT_EQ(result.errors.size(), 1U);
	EXPECT_NE(result.errors.front().find("must be refined"), std::string::npos);
}

TEST(FiniteSkeletonCATTest, MaterializesResolvedThreadJoinRelation)
{
	auto program = storeBufferingProgram();
	program.functions[0].isMain = true;
	program.functions[0].isThreadEntry = false;
	program.events.push_back({.id = 4, .kind = genmc::skeleton::EventKind::threadCreate,
				  .function = 0, .block = 0, .threadEntry = "p1"});
	program.events.push_back({.id = 5, .kind = genmc::skeleton::EventKind::returnValue,
				  .function = 1, .block = 1});
	program.events.push_back({.id = 6, .kind = genmc::skeleton::EventKind::threadJoin,
				  .function = 0, .block = 0, .joinedThreadEntry = "p1",
				  .joinedThreadCreate = 4});
	genmc::symbolic::FiniteAssignment assignment;
	assignment.activeEvents = {0, 1, 2, 3, 4, 5, 6};
	assignment.values.resize(4);
	assignment.readsFrom.resize(7);
	assignment.readsFrom[1] = 2;
	assignment.readsFrom[3] = 0;
	assignment.coherenceOrder = {0, 2};
	auto [result, base] =
		genmc::symbolic::materializeFiniteAssignment(program, assignment);
	ASSERT_TRUE(result.errors.empty());
	const auto finish = std::ranges::find(result.denseEvents, 5,
						 &genmc::symbolic::FiniteDenseEvent::site);
	const auto join = std::ranges::find(result.denseEvents, 6,
					      &genmc::symbolic::FiniteDenseEvent::site);
	ASSERT_NE(finish, result.denseEvents.end());
	ASSERT_NE(join, result.denseEvents.end());
	const auto &tj = std::get<cat::Relation>(base.at("tj"));
	EXPECT_TRUE(tj.contains(static_cast<std::size_t>(finish - result.denseEvents.begin()),
				static_cast<std::size_t>(join - result.denseEvents.begin())));
}

TEST(FiniteSkeletonCATTest, MatchesGraphAdapterSCAndFenceClassification)
{
	auto model = compileModel("Predicates\nempty SC as sc-empty\nempty F as f-empty\n");
	ASSERT_TRUE(model);
	auto program = storeBufferingProgram();
	genmc::symbolic::FiniteAssignment assignment;
	assignment.activeEvents = {0, 1, 2, 3};
	assignment.values.resize(4);
	assignment.readsFrom.resize(4);
	assignment.readsFrom[1] = genmc::skeleton::invalidNode;
	assignment.readsFrom[3] = genmc::skeleton::invalidNode;
	assignment.coherenceOrder = {0, 2};

	/* Ordinary and initial memory events are not members of GraphAdapter's SC set. */
	EXPECT_TRUE(genmc::symbolic::evaluateFiniteAssignment(program, assignment, *model)
			    .evaluation.consistent());
	program.events[0].sequentiallyConsistent = true;
	EXPECT_FALSE(genmc::symbolic::evaluateFiniteAssignment(program, assignment, *model)
			     .evaluation.consistent());

	program.events[0].sequentiallyConsistent = false;
	program.events.push_back({.id = 4, .kind = genmc::skeleton::EventKind::fence,
				  .function = 0, .block = 0});
	assignment.activeEvents.push_back(4);
	assignment.readsFrom.resize(5);
	EXPECT_FALSE(genmc::symbolic::evaluateFiniteAssignment(program, assignment, *model)
			     .evaluation.consistent());
}

TEST(FiniteSkeletonCATTest, RecursiveExplanationBlocksCurrentSolverModel)
{
	if (!genmc::symbolic::Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	auto analyzed = analyzeModel(R"CAT(RecursiveSC
let rec order = po | rf | fr | co | (order ; order)
acyclic order as sc
)CAT");
	ASSERT_TRUE(analyzed.model);
	ASSERT_TRUE(analyzed.analysis);
	auto program = storeBufferingProgram();
	program.functions[0].isMain = true;
	program.functions[0].isThreadEntry = false;
	program.events.push_back({.id = 4, .kind = genmc::skeleton::EventKind::threadCreate,
				  .function = 0, .block = 0, .threadEntry = "p1"});

	genmc::symbolic::FiniteSkeletonEncoder encoder(program);
	ASSERT_TRUE(encoder.supported());
	for (unsigned attempt = 0; attempt < 32; ++attempt) {
		auto step = encoder.next();
		ASSERT_EQ(step.status, genmc::symbolic::CheckResult::sat);
		ASSERT_TRUE(step.assignment);
		auto result = genmc::symbolic::evaluateFiniteAssignment(
			program, *step.assignment, *analyzed.model, *analyzed.analysis);
		ASSERT_TRUE(result.errors.empty());
		ASSERT_TRUE(result.evaluation.errors.empty());
		if (result.evaluation.violations.empty()) {
			encoder.blockCurrentGraph();
			continue;
		}
		ASSERT_FALSE(result.explanations.empty());
		ASSERT_TRUE(encoder.blockCurrentExplanation(result.explanations.front(),
							    result.denseEvents))
			<< encoder.explanationFailure();
		auto next = encoder.next();
		if (next.status == genmc::symbolic::CheckResult::sat) {
			ASSERT_TRUE(next.assignment);
			EXPECT_TRUE(next.assignment->readsFrom != step.assignment->readsFrom ||
				    next.assignment->coherenceOrder !=
					    step.assignment->coherenceOrder ||
				    next.assignment->activeEvents != step.assignment->activeEvents);
		}
		return;
	}
	FAIL() << "no recursive CAT violation was enumerated";
}
