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

#include "genmc/CAT/Analysis.hpp"
#include "genmc/CAT/Frontend.hpp"
#include "genmc/CAT/Model.hpp"
#include "genmc/CAT/Normalized.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>

#include <unistd.h>

namespace {

static auto repositoryRoot() -> std::filesystem::path
{
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

/** Parse and compile one isolated source fixture, preserving diagnostics for assertions. */
static auto compileText(std::string_view source) -> cat::CompileResult
{
	static std::atomic<std::uint64_t> nextFixture{};
	const auto nonce = nextFixture.fetch_add(1, std::memory_order_relaxed);
	auto path = std::filesystem::path(testing::TempDir()) /
		    ("genmc-cat-model-" + std::to_string(static_cast<std::uint64_t>(getpid())) +
		     "-" + std::to_string(nonce) + ".cat");
	std::ofstream output(path);
	output << source;
	output.close();
	auto parsed = cat::Frontend().parseFile(path);
	EXPECT_TRUE(parsed.ok()) << (parsed.diagnostics.empty()
					     ? ""
					     : parsed.diagnostics.front().format());
	if (!parsed.ok())
		return {};
	auto compiled = cat::Compiler().compile(*parsed.model);
	std::filesystem::remove(path);
	return compiled;
}

/** Parse and normalize one source while preserving recursive forward references. */
static auto normalizeText(std::string_view source) -> cat::NormalizeResult
{
	static std::atomic<std::uint64_t> nextFixture{};
	const auto nonce = nextFixture.fetch_add(1, std::memory_order_relaxed);
	auto path =
		std::filesystem::path(testing::TempDir()) /
		("genmc-caat-normalized-" + std::to_string(static_cast<std::uint64_t>(getpid())) +
		 "-" + std::to_string(nonce) + ".cat");
	std::ofstream output(path);
	output << source;
	output.close();
	auto parsed = cat::Frontend().parseFile(path);
	EXPECT_TRUE(parsed.ok()) << (parsed.diagnostics.empty()
					     ? ""
					     : parsed.diagnostics.front().format());
	if (!parsed.ok())
		return {};
	auto normalized = cat::Normalizer().normalize(*parsed.model);
	std::filesystem::remove(path);
	return normalized;
}

/** Normalize and analyze one fixture for concise CAAT admissibility tests. */
static auto analyzeText(std::string_view source) -> cat::AnalysisResult
{
	auto normalized = normalizeText(source);
	EXPECT_TRUE(normalized.ok())
		<< (normalized.diagnostics.empty() ? "" : normalized.diagnostics.front().format());
	if (!normalized.ok())
		return {};
	return cat::Analyzer().analyze(*normalized.model);
}

/** Find a diagnostic category without coupling tests to secondary messages. */
static auto hasDiagnostic(const cat::CompileResult &result, cat::DiagnosticKind kind) -> bool
{
	for (const auto &diagnostic : result.diagnostics) {
		if (diagnostic.kind == kind)
			return true;
	}
	return false;
}

/** Read one checked-in platform-independent IR summary. */
static auto readGolden(std::string_view name) -> std::string
{
	std::ifstream input(repositoryRoot() / "tests/unit/cat-golden" /
			    (std::string(name) + ".ir"));
	return std::string(std::istreambuf_iterator<char>(input), {});
}

} /* namespace */

/* Every frozen operator receives its valid set/relation type and lowers successfully. */
TEST(CatModelTest, TypesCompleteOperatorSurface)
{
	auto result = compileText(R"CAT(Types
let sets = (R | W) & (M \ IW)
let product = R * W
let identity = [sets]
let relations = (po | rf) & (loc \ 0)
let sources = domain(relations)
let targets = range(relations)
let composed = identity ; relations
let postfix = (composed^-1)? | composed+ | composed*
empty sets as set-empty
empty postfix as rel-empty
acyclic relations as relation-acyclic
irreflexive product as product-irreflexive
)CAT");

	ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
					     ? ""
					     : result.diagnostics.front().format());
	EXPECT_EQ(result.model->bindings().size(), 8U);
	EXPECT_EQ(result.model->checks().size(), 4U);
	for (const auto &node : result.model->nodes()) {
		EXPECT_EQ(node.id, &node - result.model->nodes().data());
		EXPECT_FALSE(node.span.begin.file.empty());
		for (const auto operand : node.operands)
			EXPECT_LT(operand, node.id);
	}
}

/* Random valid expressions never leave unresolved names or forward DAG edges in ModelIR. */
RC_GTEST_PROP(CatModelPropertyTest, ProducesResolvedTopologicalIR, ())
{
	const auto expression = *rc::gen::element(
		std::string("po | rf"), std::string("po ; rf"), std::string("po \\ rf"),
		std::string("po & loc"), std::string("R * W"), std::string("[R]"),
		std::string("po^-1"), std::string("po?"), std::string("po+"), std::string("po*"));
	auto result = compileText(std::string("Property\nlet value = ") + expression +
				  "\nempty value as property\n");

	RC_ASSERT(result.ok());
	for (const auto &node : result.model->nodes()) {
		RC_ASSERT(!node.span.begin.file.empty());
		for (const auto operand : node.operands)
			RC_ASSERT(operand < node.id);
	}
}

/* Conventional aliases lower to generic primitive/intersection nodes, never filename switches. */
TEST(CatModelTest, ExpandsAliasesAndMo)
{
	auto result = compileText(R"CAT(Aliases
let old = mo
let canonical = co
let communication = rfi | rfe | coi | coe | fri | fre | po-loc
acyclic communication as aliases
)CAT");

	ASSERT_TRUE(result.ok());
	ASSERT_GE(result.model->bindings().size(), 2U);
	EXPECT_EQ(result.model->bindings()[0].value, result.model->bindings()[1].value);
	EXPECT_EQ(result.model->nodes()[result.model->bindings()[2].value].type,
		  cat::ValueType::Relation);
	ASSERT_EQ(result.notes.size(), 1U);
	EXPECT_EQ(result.notes[0].kind, cat::DiagnosticKind::Note);
	EXPECT_NE(result.notes[0].message.find("deprecated"), std::string::npos);
}

/* Undefined and forward names fail in the name-resolution gate. */
TEST(CatModelTest, RejectsUndefinedAndForwardNames)
{
	auto result = compileText("Names\nlet first = later\nlet later = po\nacyclic unknown\n");

	EXPECT_FALSE(result.ok());
	EXPECT_TRUE(hasDiagnostic(result, cat::DiagnosticKind::Name));
	EXPECT_GE(result.diagnostics.size(), 2U);
}

/* Bindings and checks cannot duplicate user names or shadow the read-only prelude. */
TEST(CatModelTest, RejectsDuplicateAndReservedNames)
{
	auto result =
		compileText("Duplicates\nlet po = rf\nlet x = po\nlet x = rf\nacyclic rf as x\n");

	EXPECT_FALSE(result.ok());
	EXPECT_TRUE(hasDiagnostic(result, cat::DiagnosticKind::Name));
}

/* Each invalid operand combination is rejected specifically by the type gate. */
TEST(CatModelTest, RejectsInvalidOperatorTypes)
{
	for (const auto *expression : {"R | po", "R ; W", "po * rf", "R^-1", "[po]"}) {
		auto result = compileText(std::string("BadType\nlet bad = ") + expression + "\n");
		EXPECT_FALSE(result.ok()) << expression;
		EXPECT_TRUE(hasDiagnostic(result, cat::DiagnosticKind::Type)) << expression;
	}
}

/* Relational checks reject sets while empty deliberately accepts both value types. */
TEST(CatModelTest, TypesCheckOperands)
{
	auto acyclic = compileText("BadCheck\nacyclic R\n");
	auto irreflexive = compileText("BadCheck\nirreflexive W\n");
	auto empty = compileText("GoodCheck\nempty R\nempty po\n");

	EXPECT_TRUE(hasDiagnostic(acyclic, cat::DiagnosticKind::Type));
	EXPECT_TRUE(hasDiagnostic(irreflexive, cat::DiagnosticKind::Type));
	EXPECT_TRUE(empty.ok());
	EXPECT_EQ(empty.model->checks().size(), 2U);
	EXPECT_NE(empty.model->checks()[0].name, empty.model->checks()[1].name);
}

/* Bundled models lower to exact checked-in summaries without pointer/platform paths. */
TEST(CatModelTest, MatchesBundledGoldenSummaries)
{
	for (const auto *name : {"sc", "tso", "pso"}) {
		auto parsed = cat::Frontend().parseFile(repositoryRoot() / "models/cat" /
							(std::string(name) + ".cat"));
		ASSERT_TRUE(parsed.ok());
		auto result = cat::Compiler().compile(*parsed.model);
		ASSERT_TRUE(result.ok());
		EXPECT_EQ(result.model->summary(), readGolden(name))
			<< "actual summary for " << name << ":\n"
			<< result.model->summary();
	}
}

/* Recompiling identical syntax produces byte-identical IDs and summaries. */
TEST(CatModelTest, ProducesStableNodeIds)
{
	const auto source = "Stable\nlet x = po | rf\nacyclic x as stable\n";
	auto first = compileText(source);
	auto second = compileText(source);

	ASSERT_TRUE(first.ok());
	ASSERT_TRUE(second.ok());
	EXPECT_EQ(first.model->summary(), second.model->summary());
}

/* Online pruning rejects only non-monotone difference nodes reachable from checks. */
TEST(CatModelTest, ClassifiesOnlineAdmissibility)
{
	auto reachable =
		compileText("Reachable\nlet nonmonotone = po \\ rf\nacyclic nonmonotone\n");
	auto unused = compileText("Unused\nlet offlineOnly = po \\ rf\nacyclic po\n");

	ASSERT_TRUE(reachable.ok());
	ASSERT_TRUE(unused.ok());
	const auto node = reachable.model->firstOnlineInadmissibleNode();
	ASSERT_TRUE(node.has_value());
	EXPECT_EQ(reachable.model->nodes()[*node].kind, cat::Node::Kind::Difference);
	EXPECT_FALSE(unused.model->firstOnlineInadmissibleNode().has_value());
}

/* All three Phase 1 acceptance models satisfy the conservative online gate. */
TEST(CatModelTest, AcceptsBundledModelsForOnlineChecking)
{
	for (const auto *name : {"sc", "tso", "pso"}) {
		auto parsed = cat::Frontend().parseFile(repositoryRoot() / "models/cat" /
							(std::string(name) + ".cat"));
		ASSERT_TRUE(parsed.ok()) << name;
		auto result = cat::Compiler().compile(*parsed.model);
		ASSERT_TRUE(result.ok()) << name;
		EXPECT_FALSE(result.model->firstOnlineInadmissibleNode().has_value()) << name;
	}
}

/* Normalization reserves named IDs before lowering mutually recursive bodies. */
TEST(CaatNormalizedModelTest, ResolvesAndNormalizesMutualRecursion)
{
	auto result = normalizeText(R"CAT(Recursive
let rec x = po | (y ; rf)
and y = x^-1 | co
acyclic x as recursive
)CAT");

	ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
					     ? ""
					     : result.diagnostics.front().format());
	const auto &predicates = result.model->predicates();
	ASSERT_GE(predicates.size(), 7U);
	EXPECT_EQ(predicates[0].name, "x");
	EXPECT_EQ(predicates[1].name, "y");
	EXPECT_NE(predicates[0].declaredRecursiveGroup, 0U);
	EXPECT_EQ(predicates[0].declaredRecursiveGroup, predicates[1].declaredRecursiveGroup);
	for (const auto &predicate : predicates)
		EXPECT_LE(predicate.operands.size(), 2U);
	ASSERT_EQ(result.model->checks().size(), 1U);
	EXPECT_EQ(result.model->checks()[0].predicate, 0U);
}

/* CAAT projections produce set predicates and can feed a Cartesian product. */
TEST(CaatNormalizedModelTest, TypesProjectionAndCartesianEquations)
{
	auto result = normalizeText(R"CAT(Projections
let sources = domain(po | rf)
let targets = range(co)
let pairs = sources * targets
empty pairs as projection-product
)CAT");

	ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
					     ? ""
					     : result.diagnostics.front().format());
	EXPECT_EQ(result.model->predicates()[0].type, cat::ValueType::Set);
	EXPECT_EQ(result.model->predicates()[1].type, cat::ValueType::Set);
	EXPECT_EQ(result.model->predicates()[2].type, cat::ValueType::Relation);
	EXPECT_NE(result.model->summary().find("domain"), std::string::npos);
	EXPECT_NE(result.model->summary().find("range"), std::string::npos);
}

/* A recursive type with no constraining operator is rejected instead of guessed. */
TEST(CaatNormalizedModelTest, RejectsAmbiguousRecursiveType)
{
	auto result = normalizeText("Ambiguous\nlet rec x = x | x\nempty x\n");

	EXPECT_FALSE(result.ok());
	ASSERT_FALSE(result.diagnostics.empty());
	EXPECT_EQ(result.diagnostics.front().kind, cat::DiagnosticKind::Type);
	EXPECT_NE(result.diagnostics.front().message.find("cannot infer"), std::string::npos);
}

/* Positive mutual recursion forms one admitted SCC with deterministic strata. */
TEST(CaatAnalysisTest, StratifiesPositiveMutualRecursion)
{
	auto normalized = normalizeText(R"CAT(Strata
let rec x = po | (y ; rf)
and y = co | x^-1
acyclic x as recursive
)CAT");
	ASSERT_TRUE(normalized.ok());
	auto result = cat::Analyzer().analyze(*normalized.model);

	ASSERT_TRUE(result.ok()) << (result.diagnostics.empty()
					     ? ""
					     : result.diagnostics.front().format());
	const auto &components = result.analysis->componentOf();
	EXPECT_EQ(components[0], components[1]);
	for (const auto &dependency : result.analysis->dependencies()) {
		EXPECT_LE(components[dependency.source], components[dependency.target]);
	}
}

/* A cycle not introduced by `let rec` is rejected before fixed-point solving. */
TEST(CaatAnalysisTest, RejectsUndeclaredAndSplitRecursion)
{
	auto undeclared = analyzeText("Bad\nlet x = x | po\nacyclic x\n");
	auto split = analyzeText("Bad\nlet rec x = y | po\nlet rec y = x | rf\nacyclic x\n");

	EXPECT_FALSE(undeclared.ok());
	EXPECT_FALSE(split.ok());
	ASSERT_FALSE(undeclared.diagnostics.empty());
	ASSERT_FALSE(split.diagnostics.empty());
	EXPECT_NE(undeclared.diagnostics.front().message.find("let rec"), std::string::npos);
	EXPECT_NE(split.diagnostics.front().message.find("declaration group"), std::string::npos);
}

/* Negative recursion is non-stratifiable and derived RHS difference needs a cut. */
TEST(CaatAnalysisTest, RejectsNegativeRecursionAndNonSemiPositiveDifference)
{
	auto negative = analyzeText("Negative\nlet rec x = po \\ x\nacyclic x\n");
	auto needsCut =
		analyzeText("Cut\nlet derived = rf ; co\nlet bad = po \\ derived\nacyclic bad\n");

	EXPECT_FALSE(negative.ok());
	EXPECT_FALSE(needsCut.ok());
	ASSERT_FALSE(negative.diagnostics.empty());
	ASSERT_FALSE(needsCut.diagnostics.empty());
	EXPECT_NE(negative.diagnostics.front().message.find("negative dependency"),
		  std::string::npos);
	EXPECT_NE(needsCut.diagnostics.front().message.find("requires cutting"), std::string::npos);
}

/* Unguarded use of the full domain fails CAAT's syntactic DI criterion. */
TEST(CaatAnalysisTest, RejectsDomainDependentAxiom)
{
	auto bad = analyzeText("Domain\nempty _ * _ as all-pairs\n");
	auto guarded = analyzeText("Guarded\nempty po & (_ * _) as guarded\n");

	EXPECT_FALSE(bad.ok());
	EXPECT_TRUE(guarded.ok());
	ASSERT_FALSE(bad.diagnostics.empty());
	EXPECT_NE(bad.diagnostics.front().message.find("not domain-independent"),
		  std::string::npos);
}
