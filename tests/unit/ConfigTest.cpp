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

#include "genmc/Verification/Config.hpp"
#include "genmc/Execution/Consistency/CATChecker.hpp"
#include "genmc/Execution/Consistency/ConsistencyChecker.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>
#include <utility>

namespace {

/** Test graph exposing GenMC's protected label insertion primitive. */
class CheckerTestGraph : public ExecutionGraph {
public:
	using ExecutionGraph::addLabelToGraph;
	using ExecutionGraph::ExecutionGraph;
};

/** Add one owned label to a checker integration fixture. */
template <typename Label, typename... Args>
auto addCheckerLabel(CheckerTestGraph &graph, Args &&...args) -> Label *
{
	auto label = std::make_unique<Label>(std::forward<Args>(args)...);
	return static_cast<Label *>(graph.addLabelToGraph(std::move(label)));
}

/**
 * Return whether validation produced an error containing @p fragment.
 *
 * @param status Result returned by `Config::validate`.
 * @param fragment Stable diagnostic fragment expected by the test.
 * @return True when any reported error contains the requested fragment.
 */
static auto hasError(const ValidationStatus &status, std::string_view fragment) -> bool
{
	const auto *errors = std::get_if<ConfigErrorList>(&status);
	return errors && std::ranges::any_of(*errors, [&](const auto &error) {
		       return error.find(fragment) != std::string::npos;
	       });
}

/**
 * Create a small readable CAT file in GoogleTest's temporary directory.
 *
 * Each caller supplies a unique filename so independently discovered tests do
 * not race when CTest runs them in parallel.
 *
 * @param filename Unique basename for the temporary model.
 * @return Path to the created model file.
 */
static auto createModelFile(std::string_view filename) -> std::filesystem::path
{
	auto path = std::filesystem::path(testing::TempDir()) / filename;
	std::ofstream output(path);
	output << "SC\nacyclic po | rf | fr | co as sc\n";
	output.close();
	return path;
}

} /* namespace */

/* A readable model is canonicalized, typed, and selects the generic checker. */
TEST(ConfigModelFileTest, CanonicalizesReadableFile)
{
	auto path = createModelFile("genmc-config-readable.cat");
	Config config;
	config.modelFile = path.parent_path() / "." / path.filename();
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	ASSERT_TRUE(config.modelFile.has_value());
	EXPECT_NE(config.catModel, nullptr);
	EXPECT_EQ(config.model, ModelType::SC);
	auto checker = ConsistencyChecker::create(&config);
	EXPECT_NE(dynamic_cast<CATChecker *>(checker.get()), nullptr);
	EXPECT_EQ(*config.modelFile, std::filesystem::canonical(path));
	std::filesystem::remove(path);
}

/* Explicit metadata selects TSO views while retaining the generic CAT evaluator. */
TEST(ConfigModelFileTest, SelectsExplicitTSOHostProfile)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-tso-profile.cat";
	std::ofstream output(path);
	output << "(* @genmc host-profile tso *)\nUnrelatedName\nacyclic po | rf | fr | co\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	EXPECT_EQ(config.model, ModelType::TSO);
	auto checker = ConsistencyChecker::create(&config);
	EXPECT_NE(dynamic_cast<CATTSOChecker *>(checker.get()), nullptr);
	EXPECT_EQ(dynamic_cast<CATSCChecker *>(checker.get()), nullptr);
	std::filesystem::remove(path);
}

/* Declared recursion selects the normalized CAAT backend before exploration. */
TEST(ConfigModelFileTest, SelectsRecursiveCaatBackend)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-recursive.cat";
	std::ofstream output(path);
	output << "Recursive\nlet rec reach = po | (reach ; po)\n"
		  "irreflexive reach as order\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	EXPECT_TRUE(config.useCaatBackend);
	EXPECT_EQ(config.catModel, nullptr);
	EXPECT_NE(config.caatModel, nullptr);
	EXPECT_NE(config.caatAnalysis, nullptr);
	auto checker = ConsistencyChecker::create(&config);
	EXPECT_NE(dynamic_cast<CATChecker *>(checker.get()), nullptr);
	std::filesystem::remove(path);
}

/* Preventive pruning is a closed-world recursive-PSO certificate, not a host-checker
 * shortcut or a filename-based model selection. */
TEST(ConfigModelFileTest, CertifiesPreventivePruningOnlyForRecursivePSO)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	std::vector<std::string> warnings;
	Config pso;
	pso.modelFile = modelRoot / "recursive-pso.cat";
	pso.catPreventivePruning = true;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(pso.validate(warnings)));
	EXPECT_TRUE(pso.useCaatBackend);
	EXPECT_TRUE(pso.caatModel->certifiedAdaptiveOffline());
	EXPECT_EQ(pso.caatModel->certifiedCandidateProfile(), std::nullopt);
	auto psoChecker = ConsistencyChecker::create(&pso);
	EXPECT_NE(dynamic_cast<CATTSOChecker *>(psoChecker.get()), nullptr);

	warnings.clear();
	Config sc;
	sc.modelFile = modelRoot / "recursive-sc.cat";
	sc.catPreventivePruning = true;
	EXPECT_TRUE(hasError(sc.validate(warnings), "exact bundled recursive PSO"));
}

/* A real generic checker query drives its worker-local incremental state. */
TEST(ConfigModelFileTest, ExercisesIncrementalCaatCheckerPath)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-incremental.cat";
	std::ofstream output(path);
	output << "IncrementalChecker\nlet rec reach = po | (reach ; po)\nacyclic reach\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;
	ASSERT_TRUE(std::holds_alternative<std::monostate>(config.validate(warnings)));
	auto checker = ConsistencyChecker::create(&config);
	auto *catChecker = dynamic_cast<CATSCChecker *>(checker.get());
	ASSERT_NE(catChecker, nullptr);
	ASSERT_NE(catChecker->incrementalStatistics(), nullptr);

	CheckerTestGraph graph{{nullptr, checker.get(), true}};
	EXPECT_TRUE(checker->isConsistent(graph));
	addCheckerLabel<FenceLabel>(graph, Event(0, 1), MemOrdering::Relaxed);
	EXPECT_TRUE(checker->isConsistent(graph));
	addCheckerLabel<FenceLabel>(graph, Event(0, 2), MemOrdering::Relaxed);
	EXPECT_TRUE(checker->isConsistent(graph));
	EXPECT_EQ(catChecker->incrementalStatistics()->initializations, 1U);
	EXPECT_EQ(catChecker->incrementalStatistics()->insertions, 2U);
	EXPECT_EQ(catChecker->incrementalStatistics()->rebuilds, 0U);
	std::filesystem::remove(path);
}

/* A positive witnessed violation persists as the prefix grows and may prune it. */
TEST(ConfigModelFileTest, RejectsMonotoneViolationDuringIncrementalGrowth)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-early-prune.cat";
	std::ofstream output(path);
	output << "EarlyPrune\nlet rec reach = po | (reach ; po)\nempty F\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;
	ASSERT_TRUE(std::holds_alternative<std::monostate>(config.validate(warnings)));
	auto checker = ConsistencyChecker::create(&config);
	auto *catChecker = dynamic_cast<CATSCChecker *>(checker.get());
	ASSERT_NE(catChecker, nullptr);

	CheckerTestGraph graph{{nullptr, checker.get(), true}};
	EXPECT_TRUE(checker->isConsistent(graph));
	addCheckerLabel<FenceLabel>(graph, Event(0, 1), MemOrdering::Relaxed);
	EXPECT_FALSE(checker->isConsistent(graph));
	addCheckerLabel<FenceLabel>(graph, Event(0, 2), MemOrdering::Relaxed);
	EXPECT_FALSE(checker->isConsistent(graph));
	ASSERT_NE(catChecker->incrementalStatistics(), nullptr);
	EXPECT_EQ(catChecker->incrementalStatistics()->insertions, 2U);
	EXPECT_EQ(catChecker->incrementalStatistics()->rebuilds, 0U);
	std::filesystem::remove(path);
}

/* Acyclic forward references use normalized IDs when the Phase 1 DAG cannot. */
TEST(ConfigModelFileTest, SelectsCaatBackendForForwardReference)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-forward.cat";
	std::ofstream output(path);
	output << "Forward\nlet first = later | po\nlet later = rf\nacyclic first\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	EXPECT_TRUE(config.useCaatBackend);
	EXPECT_NE(config.caatModel, nullptr);
	std::filesystem::remove(path);
}

/* Offline semi-positive difference is not silently used for prefix pruning. */
TEST(ConfigModelFileTest, RejectsRecursiveDifferenceAtOnlineBoundary)
{
	auto path =
		std::filesystem::path(testing::TempDir()) / "genmc-config-recursive-difference.cat";
	std::ofstream output(path);
	output << "Difference\nlet rec reach = (po \\ rf) | (reach ; po)\n"
		  "irreflexive reach\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "offline-admissible but not prefix-monotone"));
	EXPECT_FALSE(config.useCaatBackend);
	std::filesystem::remove(path);
}

/* Non-monotone checks remain evaluable offline but cannot prune graph prefixes safely. */
TEST(ConfigModelFileTest, RejectsOnlineInadmissibleDifference)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-difference.cat";
	std::ofstream output(path);
	output << "Difference\nacyclic po \\ rf\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "difference affecting a check is not online-admissible"));
	EXPECT_EQ(config.catModel, nullptr);
	std::filesystem::remove(path);
}

/* Relinche cannot silently use a generated host model's refinement coherence. */
TEST(ConfigModelFileTest, RejectsRelincheOptions)
{
	auto path = createModelFile("genmc-config-relinche.cat");
	Config config;
	config.modelFile = path;
	config.collectLinSpec = "unused-output-path";
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "cannot be combined with Relinche"));
	EXPECT_EQ(config.catModel, nullptr);
	std::filesystem::remove(path);
}

/* A CAT file and an explicitly selected built-in model are unambiguously conflicting. */
TEST(ConfigModelFileTest, RejectsExplicitBuiltInModel)
{
	auto path = createModelFile("genmc-config-conflict.cat");
	Config config;
	config.modelFile = path;
	config.modelExplicit = true;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "cannot be combined with an explicit built-in"));
	EXPECT_FALSE(hasError(status, "CAT model typed"));
	std::filesystem::remove(path);
}

/* Repeated scalar options are rejected instead of silently accepting the final path. */
TEST(ConfigModelFileTest, RejectsDuplicateOption)
{
	auto path = createModelFile("genmc-config-duplicate.cat");
	Config config;
	config.modelFile = path;
	config.modelFileOccurrences = 2;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "--model-file may only be specified once"));
	EXPECT_FALSE(hasError(status, "CAT model typed"));
	std::filesystem::remove(path);
}

/* Missing input is rejected before the parser or program compiler can run. */
TEST(ConfigModelFileTest, RejectsMissingFile)
{
	Config config;
	config.modelFile =
		std::filesystem::path(testing::TempDir()) / "genmc-config-definitely-missing.cat";
	std::filesystem::remove(*config.modelFile);
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "CAT model file does not exist"));
	EXPECT_FALSE(hasError(status, "CAT model typed"));
}

/* Directories are not accepted as model inputs even when they are readable. */
TEST(ConfigModelFileTest, RejectsDirectory)
{
	Config config;
	config.modelFile = std::filesystem::path(testing::TempDir());
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "CAT model file is not a regular file"));
	EXPECT_FALSE(hasError(status, "CAT model typed"));
}

/* Syntax errors from the CAT frontend retain their file/line/column through Config. */
TEST(ConfigModelFileTest, ReportsParserDiagnosticBeforeExecution)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-invalid.cat";
	std::ofstream output(path);
	output << "Broken\nacyclic po |\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, path.string() + ":3:1: parse:"));
	EXPECT_FALSE(hasError(status, "CAT model typed"));
	std::filesystem::remove(path);
}

/* Name/type compilation errors also stop before publishing a worker-shared model. */
TEST(ConfigModelFileTest, ReportsTypeDiagnosticBeforeExecution)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-invalid-type.cat";
	std::ofstream output(path);
	output << "BrokenType\nacyclic R\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, path.string() + ":2:1: type:"));
	EXPECT_EQ(config.catModel, nullptr);
	std::filesystem::remove(path);
}

/* Accepted portability notes use Config's warning channel without blocking typed lowering. */
TEST(ConfigModelFileTest, SurfacesDeprecatedMoNote)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-mo-note.cat";
	std::ofstream output(path);
	output << "Portability\nacyclic mo as portable\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	ASSERT_EQ(warnings.size(), 1U);
	EXPECT_NE(warnings[0].find(":2:9: note:"), std::string::npos);
	EXPECT_NE(config.catModel, nullptr);
	std::filesystem::remove(path);
}

/* Existing invocations without a CAT file retain the legacy validation path. */
TEST(ConfigModelFileTest, PreservesLegacyConfiguration)
{
	Config config;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	EXPECT_FALSE(config.modelFile.has_value());
}
