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

TEST(ConfigModelFileTest, RequiresStructuralCertificateAndDefersRVFOverrides)
{
	const auto acceptedPath =
		std::filesystem::path(testing::TempDir()) / "genmc-config-rvf-structural.cat";
	{
		std::ofstream output(acceptedPath);
		output << "RenamedSC\nlet a = co | rf\nlet b = tj | (fr | tc)\n"
			  "acyclic (b | po | a)\n";
	}
	Config accepted;
	accepted.modelFile = acceptedPath;
	accepted.scRvfExploration = true;
	accepted.symmetryReduction = true;
	accepted.instructionCaching = true;
	accepted.disableBAM = false;
	accepted.ipr = true;
	accepted.estimate = true;
	accepted.finalWrite = true;
	std::vector<std::string> warnings;
	auto acceptedStatus = accepted.validate(warnings);
	EXPECT_TRUE(std::holds_alternative<std::monostate>(acceptedStatus));
	ASSERT_NE(accepted.caatAnalysis, nullptr);
	EXPECT_TRUE(accepted.caatAnalysis->certifiesSCValueExploration());
	/* Program-level certification happens after transformation. Validation must not
	 * mutate native options before an unsupported program can fail open exactly. */
	EXPECT_TRUE(accepted.symmetryReduction);
	EXPECT_TRUE(accepted.instructionCaching);
	EXPECT_FALSE(accepted.disableBAM);
	EXPECT_TRUE(accepted.ipr);
	EXPECT_TRUE(accepted.estimate);
	EXPECT_TRUE(accepted.finalWrite);

	Config bounded;
	bounded.modelFile = acceptedPath;
	bounded.scRvfExploration = true;
	bounded.bound = 2U;
	warnings.clear();
	auto boundedStatus = bounded.validate(warnings);
	EXPECT_TRUE(hasError(boundedStatus, "does not support context or round bounds"));

	Config liveness;
	liveness.modelFile = acceptedPath;
	liveness.scRvfExploration = true;
	liveness.checkLiveness = true;
	warnings.clear();
	auto livenessStatus = liveness.validate(warnings);
	EXPECT_TRUE(hasError(livenessStatus, "local safety properties only"));

	auto rejectedPath = createModelFile("genmc-config-rvf-incomplete.cat");
	Config rejected;
	rejected.modelFile = rejectedPath;
	rejected.scRvfExploration = true;
	warnings.clear();
	auto rejectedStatus = rejected.validate(warnings);
	EXPECT_TRUE(hasError(rejectedStatus, "structurally certified plain-read/write SC"));
	std::filesystem::remove(acceptedPath);
	std::filesystem::remove(rejectedPath);
}

TEST(ConfigModelFileTest, QuotientDisabledControlRequiresRVFExploration)
{
	Config config;
	config.scRvfDisableQuotient = true;
	std::vector<std::string> warnings;
	auto status = config.validate(warnings);
	EXPECT_TRUE(hasError(status,
			     "--sc-rvf-disable-quotient requires --sc-rvf-exploration"));
}

TEST(ConfigModelFileTest, AnnotatedReadExperimentRequiresRVFExploration)
{
	Config config;
	config.scRvfAnnotatedReads = true;
	std::vector<std::string> warnings;
	auto status = config.validate(warnings);
	EXPECT_TRUE(hasError(status,
			     "--sc-rvf-annotated-reads requires --sc-rvf-exploration"));
}

TEST(ConfigModelFileTest, RegionalExperimentRequiresRVFExploration)
{
	Config config;
	config.scRvfRegional = true;
	std::vector<std::string> warnings;
	auto status = config.validate(warnings);
	EXPECT_TRUE(hasError(status, "--sc-rvf-regional requires --sc-rvf-exploration"));
}

TEST(ConfigModelFileTest, ConflictCoresRequirePreventiveCertificatePath)
{
	Config config;
	config.catConflictCores = true;
	std::vector<std::string> warnings;
	auto status = config.validate(warnings);
	EXPECT_TRUE(hasError(status, "--cat-conflict-cores requires --cat-preventive-pruning"));
}

TEST(ConfigModelFileTest, BackjumpCensusRequiresModelStatsAndPreventiveCertificate)
{
	Config config;
	config.catBackjumpCensus = true;
	std::vector<std::string> warnings;
	auto status = config.validate(warnings);
	EXPECT_TRUE(hasError(status, "--cat-backjump-census requires"));
}

TEST(ConfigModelFileTest, FocusReachRequiresPreventiveCertificatePath)
{
	Config config;
	config.catFocusReach = true;
	std::vector<std::string> warnings;
	auto status = config.validate(warnings);
	EXPECT_TRUE(hasError(status, "--cat-focus-reach requires --cat-preventive-pruning"));
}

/* Preventive pruning requires a structural CAT proof and never routes a generated
 * SC/TSO candidate checker as the claimed optimization. */
TEST(ConfigModelFileTest, CertifiesPreventivePruningFromAcyclicChoiceStructure)
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
	EXPECT_TRUE(std::ranges::any_of(
		pso.caatAnalysis->preventiveOrders(), [](const auto &certificate) {
			return certificate.rfMode == cat::PreventiveRfMode::External &&
			       certificate.unassignedReadSink && certificate.unplacedWriteSink &&
			       certificate.lazyReachSupported;
		}));
	auto psoChecker = ConsistencyChecker::create(&pso);
	EXPECT_NE(dynamic_cast<CATTSOChecker *>(psoChecker.get()), nullptr);

	const auto renamedPath =
		std::filesystem::path(testing::TempDir()) / "renamed-structural-preventive.cat";
	{
		std::ofstream renamed(renamedPath);
		renamed << "(* @genmc host-profile tso *)\nRenamedPreventive\n"
			   "let fixed = po | tc\nlet com = (ext & rf) | fr | co\n"
			   "let order = fixed | com\n"
			   "let rec closure = order | (order ; closure)\n"
			   "irreflexive closure\n";
	}
	warnings.clear();
	Config renamed;
	renamed.modelFile = renamedPath;
	renamed.catPreventivePruning = true;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(renamed.validate(warnings)));
	ASSERT_NE(renamed.caatAnalysis, nullptr);
	EXPECT_FALSE(renamed.caatAnalysis->preventiveOrders().empty());
	std::filesystem::remove(renamedPath);

	warnings.clear();
	Config sc;
	sc.modelFile = modelRoot / "recursive-sc.cat";
	sc.catPreventivePruning = true;
	EXPECT_TRUE(hasError(sc.validate(warnings), "structurally certified"));
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

/* The diagnostic switch changes only the certified small-graph synchronization policy. */
TEST(ConfigModelFileTest, DisablesCertifiedAdaptiveOfflineHeuristic)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config config;
	config.modelFile = modelRoot / "recursive-sc.cat";
	config.catDisableAdaptiveOffline = true;
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
	EXPECT_EQ(catChecker->incrementalStatistics()->initializations, 1U);
	EXPECT_EQ(catChecker->incrementalStatistics()->insertions, 1U);
	EXPECT_EQ(catChecker->incrementalStatistics()->adaptiveOfflineSelections, 0U);
}

TEST(ConfigModelFileTest, AdaptiveOfflineSwitchRequiresCertifiedCaatModel)
{
	Config missing;
	missing.catDisableAdaptiveOffline = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(hasError(missing.validate(warnings),
			     "--cat-disable-adaptive-offline requires --model-file"));

	auto path = createModelFile("genmc-config-adaptive-offline-acyclic.cat");
	Config acyclic;
	acyclic.modelFile = path;
	acyclic.catDisableAdaptiveOffline = true;
	warnings.clear();
	EXPECT_TRUE(hasError(acyclic.validate(warnings), "adaptive-offline structural certificate"));
	std::filesystem::remove(path);
}

TEST(ConfigModelFileTest, PrimitiveCacheRequiresCertifiedCaatModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catPrimitiveCache = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missing;
	missing.catPrimitiveCache = true;
	EXPECT_TRUE(hasError(missing.validate(warnings), "--cat-primitive-cache requires --model-file"));

	auto path = createModelFile("genmc-config-primitive-cache-acyclic.cat");
	Config acyclic;
	acyclic.modelFile = path;
	acyclic.catPrimitiveCache = true;
	warnings.clear();
	EXPECT_TRUE(hasError(acyclic.validate(warnings), "adaptive-offline structural certificate"));
	std::filesystem::remove(path);
}

TEST(ConfigModelFileTest, FastPrimitiveBuildRequiresCertifiedCaatModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catFastPrimitiveBuild = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missing;
	missing.catFastPrimitiveBuild = true;
	EXPECT_TRUE(hasError(missing.validate(warnings),
			     "--cat-fast-primitive-build requires --model-file"));

	auto path = createModelFile("genmc-config-fast-primitive-build-acyclic.cat");
	Config acyclic;
	acyclic.modelFile = path;
	acyclic.catFastPrimitiveBuild = true;
	warnings.clear();
	EXPECT_TRUE(hasError(acyclic.validate(warnings), "adaptive-offline structural certificate"));
	std::filesystem::remove(path);
}

TEST(ConfigModelFileTest, FastCoherenceBuildRequiresCertifiedCaatModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catFastCoherenceBuild = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missing;
	missing.catFastCoherenceBuild = true;
	EXPECT_TRUE(hasError(missing.validate(warnings),
			     "--cat-fast-coherence-build requires --model-file"));

	auto path = createModelFile("genmc-config-fast-coherence-build-acyclic.cat");
	Config acyclic;
	acyclic.modelFile = path;
	acyclic.catFastCoherenceBuild = true;
	warnings.clear();
	EXPECT_TRUE(hasError(acyclic.validate(warnings),
			     "adaptive-offline structural certificate"));
	std::filesystem::remove(path);
}

TEST(ConfigModelFileTest, FastDescriptorBuildRequiresPrimitiveCacheAndCertifiedModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catPrimitiveCache = true;
	certified.catFastDescriptorBuild = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missingCache = certified;
	missingCache.catPrimitiveCache = false;
	EXPECT_TRUE(hasError(missingCache.validate(warnings),
			     "--cat-fast-descriptor-build requires --cat-primitive-cache"));

	Config missingModel;
	missingModel.catPrimitiveCache = true;
	missingModel.catFastDescriptorBuild = true;
	EXPECT_TRUE(hasError(missingModel.validate(warnings),
			     "--cat-fast-descriptor-build requires --model-file"));
}

TEST(ConfigModelFileTest, FastDescriptorReuseRequiresPrimitiveCacheAndCertifiedModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catPrimitiveCache = true;
	certified.catFastDescriptorReuse = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missingCache = certified;
	missingCache.catPrimitiveCache = false;
	EXPECT_TRUE(hasError(missingCache.validate(warnings),
			     "--cat-fast-descriptor-reuse requires --cat-primitive-cache"));
}

TEST(ConfigModelFileTest, FastChecksRequireCaatModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catFastChecks = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missing;
	missing.catFastChecks = true;
	EXPECT_TRUE(hasError(missing.validate(warnings), "--cat-fast-checks requires --model-file"));
}

TEST(ConfigModelFileTest, FastCompositionRequiresCaatModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-sc.cat";
	certified.catFastComposition = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missing;
	missing.catFastComposition = true;
	EXPECT_TRUE(hasError(missing.validate(warnings),
			     "--cat-fast-composition requires --model-file"));
}

TEST(ConfigModelFileTest, FastCycleChecksRequireCaatModel)
{
	const auto modelRoot =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat";
	Config certified;
	certified.modelFile = modelRoot / "recursive-tso.cat";
	certified.catFastCycleChecks = true;
	std::vector<std::string> warnings;
	EXPECT_TRUE(std::holds_alternative<std::monostate>(certified.validate(warnings)));

	Config missing;
	missing.catFastCycleChecks = true;
	EXPECT_TRUE(hasError(missing.validate(warnings),
			     "--cat-fast-cycle-checks requires --model-file"));
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
