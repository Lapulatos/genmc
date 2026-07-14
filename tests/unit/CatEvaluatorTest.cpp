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
#include "genmc/CAT/CaatEvaluator.hpp"
#include "genmc/CAT/Evaluator.hpp"
#include "genmc/CAT/Frontend.hpp"
#include "genmc/CAT/IncrementalEvaluator.hpp"
#include "genmc/CAT/Model.hpp"
#include "genmc/CAT/Normalized.hpp"
#include "genmc/CAT/Reasoner.hpp"
#include "genmc/CAT/Value.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

/** Compile one isolated valid model for evaluator-only tests. */
static auto compileModel(std::string_view source) -> std::shared_ptr<const cat::ModelIR>
{
	static std::atomic<std::uint64_t> nextFixture{};
	auto path = std::filesystem::path(testing::TempDir()) /
		    ("genmc-cat-evaluator-" + std::to_string(static_cast<std::uint64_t>(getpid())) +
		     "-" + std::to_string(nextFixture.fetch_add(1, std::memory_order_relaxed)) +
		     ".cat");
	std::ofstream output(path);
	output << source;
	output.close();
	auto parsed = cat::Frontend().parseFile(path);
	EXPECT_TRUE(parsed.ok());
	if (!parsed.ok())
		return nullptr;
	auto compiled = cat::Compiler().compile(*parsed.model);
	std::filesystem::remove(path);
	EXPECT_TRUE(compiled.ok());
	return compiled.model;
}

struct AnalyzedModel {
	std::shared_ptr<const cat::NormalizedModel> model;
	std::shared_ptr<const cat::ModelAnalysis> analysis;
};

/** Parse, normalize, and analyze one admissible CAAT model fixture. */
static auto analyzeModel(std::string_view source) -> AnalyzedModel
{
	static std::atomic<std::uint64_t> nextFixture{};
	auto path = std::filesystem::path(testing::TempDir()) /
		    ("genmc-caat-evaluator-" +
		     std::to_string(static_cast<std::uint64_t>(getpid())) + "-" +
		     std::to_string(nextFixture.fetch_add(1, std::memory_order_relaxed)) + ".cat");
	std::ofstream output(path);
	output << source;
	output.close();
	auto parsed = cat::Frontend().parseFile(path);
	EXPECT_TRUE(parsed.ok());
	if (!parsed.ok())
		return {};
	auto normalized = cat::Normalizer().normalize(*parsed.model);
	std::filesystem::remove(path);
	EXPECT_TRUE(normalized.ok());
	if (!normalized.ok())
		return {};
	auto analyzed = cat::Analyzer().analyze(*normalized.model);
	EXPECT_TRUE(analyzed.ok())
		<< (analyzed.diagnostics.empty() ? "" : analyzed.diagnostics.front().format());
	return {std::move(normalized.model), std::move(analyzed.analysis)};
}

/** Find one named predicate in a normalized model used by a focused test. */
static auto findPredicate(const cat::NormalizedModel &model, std::string_view name)
	-> cat::PredicateId
{
	for (const auto &predicate : model.predicates()) {
		if (predicate.name == name)
			return predicate.id;
	}
	return static_cast<cat::PredicateId>(model.predicates().size());
}

/** Build a packed set from explicit event IDs. */
static auto makeSet(std::size_t size, std::initializer_list<std::size_t> events) -> cat::EventSet
{
	cat::EventSet result(size);
	for (const auto event : events)
		result.insert(event);
	return result;
}

/** Build a packed relation from explicit ordered pairs. */
static auto makeRelation(std::size_t size,
			 std::initializer_list<std::pair<std::size_t, std::size_t>> pairs)
	-> cat::Relation
{
	cat::Relation result(size);
	for (const auto [from, to] : pairs)
		result.insert(from, to);
	return result;
}

/** Convert a packed relation to the deliberately simple property-test oracle. */
static auto toReference(const cat::Relation &relation)
	-> std::set<std::pair<std::size_t, std::size_t>>
{
	std::set<std::pair<std::size_t, std::size_t>> result;
	for (std::size_t from = 0; from < relation.size(); ++from) {
		for (std::size_t to = 0; to < relation.size(); ++to) {
			if (relation.contains(from, to))
				result.emplace(from, to);
		}
	}
	return result;
}

/** Compare every maintained predicate and verdict with a fresh Phase 2 run. */
static void expectIncrementalEqualsOffline(const cat::IncrementalCaatEvaluator &incremental,
					   const cat::NormalizedModel &model,
					   const cat::ModelAnalysis &analysis,
					   std::size_t eventCount, const cat::BaseValues &base)
{
	auto offline = cat::CaatEvaluator().evaluate(model, analysis, eventCount, base);
	EXPECT_EQ(incremental.result().values, offline.values);
	EXPECT_EQ(incremental.result().consistent(), offline.consistent());
	EXPECT_EQ(incremental.result().violations.size(), offline.violations.size());
	EXPECT_EQ(incremental.result().errors.size(), offline.errors.size());
}

} /* namespace */

/* Packed sets retain bits across word boundaries and implement all three Boolean operations. */
TEST(CatValueTest, EvaluatesSetAlgebraAcrossWords)
{
	auto lhs = makeSet(130, {0, 63, 64, 129});
	auto rhs = makeSet(130, {1, 63, 65, 129});

	EXPECT_EQ(lhs.count(), 4U);
	EXPECT_EQ(setUnion(lhs, rhs).count(), 6U);
	EXPECT_EQ(setIntersection(lhs, rhs), makeSet(130, {63, 129}));
	EXPECT_EQ(setDifference(lhs, rhs), makeSet(130, {0, 64}));
	EXPECT_EQ(lhs.first(), 0U);
}

/* Universe growth preserves old members and zero-initializes every new event. */
TEST(CatValueTest, GrowsEventSetsAcrossPackedWordBoundaries)
{
	cat::EventSet events;
	for (const auto size : {1U, 63U, 64U, 65U, 130U}) {
		events.grow(size);
		events.insert(size - 1);
		EXPECT_EQ(events.size(), size);
		EXPECT_TRUE(events.contains(size - 1));
	}
	EXPECT_EQ(events.count(), 5U);
	for (const auto event : {0U, 62U, 63U, 64U, 129U})
		EXPECT_TRUE(events.contains(event));
}

/* Relation Boolean algebra, product, identity, and inverse preserve exact pairs. */
TEST(CatValueTest, EvaluatesBasicRelationAlgebra)
{
	auto lhs = makeRelation(4, {{0, 1}, {1, 2}, {2, 3}});
	auto rhs = makeRelation(4, {{0, 1}, {2, 0}});
	auto wide = makeRelation(130, {{0, 129}, {64, 63}, {129, 64}});

	EXPECT_EQ(wide.count(), 3U);
	EXPECT_TRUE(wide.contains(0, 129));
	EXPECT_TRUE(wide.successors(129).contains(64));
	EXPECT_EQ(relationIntersection(lhs, rhs), makeRelation(4, {{0, 1}}));
	EXPECT_EQ(relationDifference(lhs, rhs), makeRelation(4, {{1, 2}, {2, 3}}));
	EXPECT_EQ(relationUnion(lhs, rhs).count(), 4U);
	EXPECT_EQ(inverse(lhs), makeRelation(4, {{1, 0}, {2, 1}, {3, 2}}));
	EXPECT_EQ(product(makeSet(4, {0, 2}), makeSet(4, {1, 3})),
		  makeRelation(4, {{0, 1}, {0, 3}, {2, 1}, {2, 3}}));
	EXPECT_EQ(identity(makeSet(4, {1, 3})), makeRelation(4, {{1, 1}, {3, 3}}));
	EXPECT_EQ(domain(lhs), makeSet(4, {0, 1, 2}));
	EXPECT_EQ(range(lhs), makeSet(4, {1, 2, 3}));
}

/* Relation growth repacks rows without moving or inventing existing pairs. */
TEST(CatValueTest, GrowsRelationsAcrossPackedRowBoundaries)
{
	cat::Relation relation;
	relation.grow(1);
	relation.insert(0, 0);
	relation.grow(64);
	relation.insert(63, 0);
	relation.insert(0, 63);
	relation.grow(65);
	relation.insert(64, 64);
	relation.grow(130);
	relation.insert(129, 64);

	EXPECT_EQ(relation.size(), 130U);
	EXPECT_EQ(relation.count(), 5U);
	EXPECT_EQ(toReference(relation), (std::set<std::pair<std::size_t, std::size_t>>{
						 {0, 0}, {0, 63}, {63, 0}, {64, 64}, {129, 64}}));
}

/* Composition and closures cover empty, chain, cycle, singleton, and disconnected cases. */
TEST(CatValueTest, EvaluatesCompositionAndClosures)
{
	auto chain = makeRelation(5, {{0, 1}, {1, 2}, {3, 4}});
	auto step = makeRelation(5, {{1, 3}, {2, 4}});

	EXPECT_EQ(compose(chain, step), makeRelation(5, {{0, 3}, {1, 4}}));
	EXPECT_EQ(transitiveClosure(chain), makeRelation(5, {{0, 1}, {0, 2}, {1, 2}, {3, 4}}));
	EXPECT_EQ(transitiveClosure(cat::Relation(1)), cat::Relation(1));
	EXPECT_EQ(reflexiveTransitiveClosure(cat::Relation(1)), makeRelation(1, {{0, 0}}));
	auto cycle = makeRelation(3, {{0, 1}, {1, 2}, {2, 0}});
	EXPECT_EQ(transitiveClosure(cycle).count(), 9U);
	EXPECT_EQ(optional(makeRelation(3, {{0, 1}})),
		  makeRelation(3, {{0, 0}, {0, 1}, {1, 1}, {2, 2}}));
}

/* A recursive relation reaches the same least fixed point as naive Kleene iteration. */
TEST(CaatEvaluatorTest, EvaluatesRecursiveTransitiveClosure)
{
	auto analyzed = analyzeModel(R"CAT(RecursiveClosure
let rec reach = po | (reach ; po)
empty reach as populated
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	auto po = makeRelation(5, {{0, 1}, {1, 2}, {2, 3}, {1, 4}});
	cat::BaseValues base{{"po", po}};

	auto result = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 5, base);
	const auto reach = findPredicate(*analyzed.model, "reach");
	ASSERT_LT(reach, result.values.size());
	ASSERT_TRUE(result.values[reach].has_value());

	cat::Relation naive(5);
	for (;;) {
		auto next = relationUnion(po, compose(naive, po));
		if (next == naive)
			break;
		naive = std::move(next);
	}
	EXPECT_EQ(std::get<cat::Relation>(*result.values[reach]), naive);
	EXPECT_FALSE(result.consistent());
	EXPECT_GT(result.statistics.valueChanges, 0U);
}

/* Incremental state initialization publishes the exact Phase 2 fixed point. */
TEST(IncrementalCaatEvaluatorTest, InitializesFromOfflineOracle)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalInitialization
let rec reach = po | (reach ; po)
acyclic reach as order
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"po", makeRelation(5, {{0, 1}, {1, 2}, {3, 4}})}};
	auto offline = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 5, base);

	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	EXPECT_FALSE(incremental.initialized());
	const auto &initialized = incremental.initialize(5, base);

	EXPECT_TRUE(incremental.initialized());
	EXPECT_EQ(incremental.eventCount(), 5U);
	EXPECT_EQ(incremental.baseValues(), base);
	EXPECT_EQ(initialized.values, offline.values);
	EXPECT_EQ(initialized.consistent(), offline.consistent());
	EXPECT_EQ(initialized.violations.size(), offline.violations.size());
	EXPECT_EQ(initialized.errors.size(), offline.errors.size());
	EXPECT_EQ(&incremental.result(), &initialized);
	EXPECT_EQ(incremental.statistics().initializations, 1U);
	EXPECT_EQ(incremental.statistics().offlineEvaluations, 1U);
}

/* Reinitialization atomically replaces the old universe, bases, and result. */
TEST(IncrementalCaatEvaluatorTest, ReinitializesAfterFallback)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalReinitialization
let rec reach = po | (reach ; po)
empty reach as no-order
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(2, {{"po", makeRelation(2, {})}});
	EXPECT_TRUE(incremental.result().consistent());

	cat::BaseValues replacement{{"po", makeRelation(3, {{0, 1}})}};
	auto offline =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 3, replacement);
	const auto &current = incremental.initialize(3, replacement);

	EXPECT_EQ(incremental.eventCount(), 3U);
	EXPECT_EQ(incremental.baseValues(), replacement);
	EXPECT_EQ(current.values, offline.values);
	EXPECT_EQ(current.consistent(), offline.consistent());
	EXPECT_EQ(current.violations.size(), offline.violations.size());
	EXPECT_EQ(incremental.statistics().initializations, 2U);
	EXPECT_EQ(incremental.statistics().offlineEvaluations, 2U);
}

/* Every positive normalized operator reaches the same value after staged insertions. */
TEST(IncrementalCaatEvaluatorTest, PropagatesCompletePositiveOperatorSurface)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalOperators
let restricted = [R]
let backwards = po^-1
let maybe = po?
let closure = po+
let reflexive = po*
let sources = domain(po)
let targets = range(rf)
let pairs = sources * targets
let sequence = po ; rf
let either = po | rf
let both = po & rf
let rec reach = po | (reach ; po)
acyclic reach as recursive-order
empty sequence as composition-empty
empty pairs as product-empty
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{
		{"R", makeSet(2, {})}, {"po", makeRelation(2, {})}, {"rf", makeRelation(2, {})}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(2, base);
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 2, base);

	base = {{"R", makeSet(2, {0})},
		{"po", makeRelation(2, {{0, 1}})},
		{"rf", makeRelation(2, {{1, 0}})}};
	auto first = incremental.tryInsert(2, base);
	ASSERT_TRUE(first.applied()) << first.reason;
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 2, base);

	/* Growing without changing `po` specifically tests the implicit identity
	 * additions in optional and reflexive-transitive closure. */
	base = {{"R", makeSet(3, {0, 2})},
		{"po", makeRelation(3, {{0, 1}})},
		{"rf", makeRelation(3, {{1, 0}})}};
	auto grown = incremental.tryInsert(3, base);
	ASSERT_TRUE(grown.applied()) << grown.reason;
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 3, base);

	base.at("po") = makeRelation(3, {{0, 1}, {1, 2}});
	base.at("rf") = makeRelation(3, {{1, 0}, {2, 1}});
	auto recursive = incremental.tryInsert(3, base);
	ASSERT_TRUE(recursive.applied()) << recursive.reason;
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 3, base);
	EXPECT_EQ(incremental.statistics().insertionUpdates, 3U);
	EXPECT_GT(incremental.statistics().operationEvaluations, 0U);
	EXPECT_GT(incremental.statistics().worklistPushes, 0U);
}

/* Mutual set recursion and duplicate updates converge without repeated publication. */
TEST(IncrementalCaatEvaluatorTest, PropagatesMutualSetRecursionAndDuplicateFacts)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalMutualSets
let rec left = R | right
and right = W | left
empty left as populated
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"R", makeSet(4, {})}, {"W", makeSet(4, {})}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(4, base);

	base.at("R") = makeSet(4, {2});
	ASSERT_TRUE(incremental.tryInsert(4, base).applied());
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 4, base);
	const auto afterFirst = incremental.result().values;
	const auto beforeDuplicateChanges = incremental.statistics().valueChanges;
	ASSERT_TRUE(incremental.tryInsert(4, base).applied());
	EXPECT_EQ(incremental.result().values, afterFirst);
	EXPECT_EQ(incremental.statistics().valueChanges, beforeDuplicateChanges);

	base.at("W") = makeSet(4, {1, 3});
	ASSERT_TRUE(incremental.tryInsert(4, base).applied());
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 4, base);
	const auto left = findPredicate(*analyzed.model, "left");
	EXPECT_EQ(std::get<cat::EventSet>(*incremental.result().values[left]),
		  makeSet(4, {1, 2, 3}));
}

/* Unsupported deletion and difference updates preserve the last exact state. */
TEST(IncrementalCaatEvaluatorTest, RejectsNonMonotoneUpdatesTransactionally)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalRejection
let rec reach = po | (reach ; po)
acyclic reach
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"po", makeRelation(3, {{0, 1}, {1, 2}})}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(3, base);
	const auto oldValues = incremental.result().values;
	const auto oldBase = incremental.baseValues();

	auto deleted = incremental.tryInsert(3, {{"po", makeRelation(3, {{0, 1}})}});
	EXPECT_FALSE(deleted.applied());
	EXPECT_NE(deleted.reason.find("removed a fact"), std::string::npos);
	EXPECT_EQ(incremental.result().values, oldValues);
	EXPECT_EQ(incremental.baseValues(), oldBase);
	auto shrunk = incremental.tryInsert(2, {{"po", makeRelation(2, {{0, 1}})}});
	EXPECT_FALSE(shrunk.applied());
	EXPECT_NE(shrunk.reason.find("shrank"), std::string::npos);
	EXPECT_EQ(incremental.eventCount(), 3U);

	auto withDifference = analyzeModel(R"CAT(IncrementalDifference
let remaining = po \ rf
empty remaining
)CAT");
	ASSERT_NE(withDifference.model, nullptr);
	cat::IncrementalCaatEvaluator nonMonotone(*withDifference.model, *withDifference.analysis);
	cat::BaseValues differenceBase{{"po", makeRelation(2, {})}, {"rf", makeRelation(2, {})}};
	nonMonotone.initialize(2, differenceBase);
	EXPECT_FALSE(nonMonotone.supportsInsertions());
	differenceBase.at("po") = makeRelation(2, {{0, 1}});
	auto rejected = nonMonotone.tryInsert(2, differenceBase);
	EXPECT_FALSE(rejected.applied());
	EXPECT_NE(rejected.reason.find("difference"), std::string::npos);
}

/* Random edge arrival orders match a fresh recursive reachability fixed point each step. */
RC_GTEST_PROP(IncrementalCaatEvaluatorPropertyTest, MatchesOfflineAfterEveryInsertion,
	      (const std::vector<std::uint8_t> &bytes))
{
	auto analyzed = analyzeModel(R"CAT(RandomIncrementalReachability
let rec reach = po | (reach ; po)
acyclic reach
)CAT");
	RC_ASSERT(analyzed.model != nullptr);
	constexpr std::size_t size = 6;
	cat::Relation po(size);
	cat::BaseValues base{{"po", po}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(size, base);
	for (std::size_t index = 0; index + 1 < bytes.size(); index += 2) {
		po.insert(static_cast<std::size_t>(bytes[index]) % size,
			  static_cast<std::size_t>(bytes[index + 1]) % size);
		base.at("po") = po;
		const auto updated = incremental.tryInsert(size, base);
		RC_ASSERT(updated.applied());
		auto offline = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis,
							     size, base);
		RC_ASSERT(incremental.result().values == offline.values);
		RC_ASSERT(incremental.result().consistent() == offline.consistent());
	}
}

/* Mutual relation recursion converges to the union of both base relations. */
TEST(CaatEvaluatorTest, EvaluatesMutualAndEmptyFixedPoints)
{
	auto mutual = analyzeModel(R"CAT(Mutual
let rec x = po | y
and y = rf | x
empty x as populated
)CAT");
	auto empty = analyzeModel(R"CAT(Empty
let rec x = x ; po
empty x as empty-fixed-point
)CAT");
	ASSERT_NE(mutual.model, nullptr);
	ASSERT_NE(empty.model, nullptr);
	auto po = makeRelation(3, {{0, 1}});
	auto rf = makeRelation(3, {{1, 2}});
	cat::BaseValues base{{"po", po}, {"rf", rf}};

	auto mutualResult = cat::CaatEvaluator().evaluate(*mutual.model, *mutual.analysis, 3, base);
	auto emptyResult = cat::CaatEvaluator().evaluate(*empty.model, *empty.analysis, 3, base);
	const auto expected = relationUnion(po, rf);
	EXPECT_EQ(std::get<cat::Relation>(*mutualResult.values[findPredicate(*mutual.model, "x")]),
		  expected);
	EXPECT_EQ(std::get<cat::Relation>(*mutualResult.values[findPredicate(*mutual.model, "y")]),
		  expected);
	EXPECT_TRUE(emptyResult.consistent());
	EXPECT_TRUE(std::get<cat::Relation>(*emptyResult.values[findPredicate(*empty.model, "x")])
			    .empty());
}

/* Recursive sets use relation projections and converge over the same finite domain. */
TEST(CaatEvaluatorTest, EvaluatesRecursiveSetProjection)
{
	auto analyzed = analyzeModel(R"CAT(SetRecursion
let rec reached = R | range([reached] ; po)
empty reached as populated
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"R", makeSet(4, {0})},
			     {"po", makeRelation(4, {{0, 1}, {1, 2}, {2, 3}})}};

	auto result = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 4, base);
	EXPECT_EQ(
		std::get<cat::EventSet>(*result.values[findPredicate(*analyzed.model, "reached")]),
		makeSet(4, {0, 1, 2, 3}));
}

/* Recursive cycle explanations reduce fixed-point edges to positive base literals. */
TEST(CaatReasonerTest, ExplainsRecursiveAcyclicViolation)
{
	auto analyzed = analyzeModel(R"CAT(RecursiveCycle
let rec reach = po | (reach ; po)
acyclic reach as cycle
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"po", makeRelation(3, {{0, 1}, {1, 2}, {2, 0}})}};
	auto evaluated =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 3, base);

	ASSERT_EQ(evaluated.violations.size(), 1U);
	auto explained = cat::Reasoner().explain(*analyzed.model, *analyzed.analysis, 3,
						 evaluated.values, evaluated.violations);
	ASSERT_TRUE(explained.ok());
	ASSERT_EQ(explained.violations.size(), 1U);
	EXPECT_EQ(explained.violations[0].violation.checkName, "cycle");
	ASSERT_EQ(explained.violations[0].explanation.size(), 3U);
	for (const auto &literal : explained.violations[0].explanation) {
		EXPECT_EQ(literal.predicateName, "po");
		EXPECT_TRUE(literal.positive);
		EXPECT_EQ(literal.type, cat::ValueType::Relation);
		ASSERT_TRUE(literal.second.has_value());
	}
	cat::Relation replayPo(3);
	for (const auto &literal : explained.violations[0].explanation)
		replayPo.insert(literal.first, *literal.second);
	const auto replayReach = transitiveClosure(replayPo);
	EXPECT_TRUE(replayReach.contains(0, 0) || replayReach.contains(1, 1) ||
		    replayReach.contains(2, 2));
}

/* Semi-positive difference explanations retain both membership and absence facts. */
TEST(CaatReasonerTest, ExplainsNegativeBaseLiteralAndReplaysViolation)
{
	auto analyzed = analyzeModel(R"CAT(Difference
let kept = po \ rf
empty kept as difference
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"po", makeRelation(2, {{0, 1}})}, {"rf", cat::Relation(2)}};
	auto evaluated =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 2, base);
	auto explained = cat::Reasoner().explain(*analyzed.model, *analyzed.analysis, 2,
						 evaluated.values, evaluated.violations);

	ASSERT_TRUE(explained.ok());
	ASSERT_EQ(explained.violations.size(), 1U);
	const auto &literals = explained.violations[0].explanation;
	ASSERT_EQ(literals.size(), 2U);
	EXPECT_EQ(literals[0].predicateName, "po");
	EXPECT_TRUE(literals[0].positive);
	EXPECT_EQ(literals[1].predicateName, "rf");
	EXPECT_FALSE(literals[1].positive);
	EXPECT_EQ(cat::Reasoner::format(explained.violations[0]),
		  "3:1: CAT check 'difference' failed; witness=0->1; because po(0,1) & "
		  "!rf(0,1)");

	/* Replay the two literals with direct relation algebra, not the CAAT evaluator. */
	cat::Relation replayPo(2);
	cat::Relation replayRf(2);
	for (const auto &literal : literals) {
		if (literal.positive && literal.predicateName == "po")
			replayPo.insert(literal.first, *literal.second);
		if (literal.positive && literal.predicateName == "rf")
			replayRf.insert(literal.first, *literal.second);
	}
	EXPECT_TRUE(relationDifference(replayPo, replayRf).contains(0, 1));
}

/* Set and irreflexive witnesses are projected with their exact ground arity. */
TEST(CaatReasonerTest, ExplainsSetAndIrreflexiveWitnesses)
{
	auto analyzed = analyzeModel(R"CAT(WitnessKinds
let rec reached = R | range([reached] ; po)
let diagonal = po ; rf
empty reached as set-check
irreflexive diagonal as irreflexive-check
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"R", makeSet(2, {0})},
			     {"po", makeRelation(2, {{0, 1}})},
			     {"rf", makeRelation(2, {{1, 0}})}};
	auto evaluated =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 2, base);
	auto explained = cat::Reasoner().explain(*analyzed.model, *analyzed.analysis, 2,
						 evaluated.values, evaluated.violations);

	ASSERT_TRUE(explained.ok());
	ASSERT_EQ(explained.violations.size(), 2U);
	ASSERT_EQ(explained.violations[0].explanation.size(), 1U);
	EXPECT_EQ(explained.violations[0].explanation[0].predicateName, "R");
	EXPECT_FALSE(explained.violations[0].explanation[0].second.has_value());
	ASSERT_EQ(explained.violations[1].explanation.size(), 2U);
	EXPECT_TRUE(explained.violations[1].explanation[0].second.has_value());
	EXPECT_EQ(explained.violations[0].explanation[0].first, 0U);
	cat::Relation replayPo(2);
	cat::Relation replayRf(2);
	for (const auto &literal : explained.violations[1].explanation) {
		auto &relation = literal.predicateName == "po" ? replayPo : replayRf;
		relation.insert(literal.first, *literal.second);
	}
	EXPECT_TRUE(compose(replayPo, replayRf).contains(0, 0));
}

/* A stale violation cannot be paired with a misleading explanation. */
TEST(CaatReasonerTest, RejectsStaleViolation)
{
	auto analyzed = analyzeModel("Stale\nempty po as present\n");
	ASSERT_NE(analyzed.model, nullptr);
	cat::Violation stale{"missing", cat::Statement::CheckKind::Empty, {}, {0, 1}};
	cat::BaseValues base{{"po", makeRelation(2, {{0, 1}})}};
	auto evaluated =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, 2, base);

	auto explained = cat::Reasoner().explain(*analyzed.model, *analyzed.analysis, 2,
						 evaluated.values, {stale});
	EXPECT_FALSE(explained.ok());
	EXPECT_TRUE(explained.violations.empty());
	ASSERT_EQ(explained.errors.size(), 1U);
	EXPECT_NE(explained.errors[0].find("unknown check"), std::string::npos);
	auto malformed = cat::Reasoner().explain(*analyzed.model, *analyzed.analysis, 2, {},
						 evaluated.violations);
	EXPECT_FALSE(malformed.ok());
	EXPECT_NE(malformed.errors[0].find("stale"), std::string::npos);
}

/* Worklist fixed points agree with an independent naive Kleene recurrence. */
RC_GTEST_PROP(CaatEvaluatorPropertyTest, MatchesNaiveKleeneReachability,
	      (const std::vector<std::uint8_t> &edgeBytes))
{
	static const auto analyzed = analyzeModel(R"CAT(Property
let rec reach = po | (reach ; po)
empty reach
)CAT");
	RC_ASSERT(analyzed.model != nullptr);
	constexpr std::size_t size = 4;
	cat::Relation po(size);
	if (!edgeBytes.empty()) {
		for (std::size_t from = 0; from < size; ++from) {
			for (std::size_t target = 0; target < size; ++target) {
				const auto index = from * size + target;
				if ((edgeBytes[index % edgeBytes.size()] & 1U) != 0)
					po.insert(from, target);
			}
		}
	}
	cat::BaseValues base{{"po", po}};
	const auto result =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, size, base);

	cat::Relation naive(size);
	for (;;) {
		auto next = relationUnion(po, compose(naive, po));
		if (next == naive)
			break;
		naive = std::move(next);
	}
	const auto reach = findPredicate(*analyzed.model, "reach");
	RC_ASSERT(std::get<cat::Relation>(*result.values[reach]) == naive);
}

/*
 * Chain reachability gives a reproducible worst-shaped fixed-point measurement:
 * each recurrence can extend the longest known path by only one edge.  The
 * counters make later worklist or incremental implementations comparable
 * without treating wall-clock time as a correctness assertion.
 */
TEST(CaatEvaluatorTest, BenchmarkRecursiveChainFixedPoints)
{
	auto analyzed = analyzeModel(R"CAT(Benchmark
let rec reach = po | (reach ; po)
acyclic reach
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	std::size_t operationEvaluations{};
	std::size_t valueChanges{};
	std::size_t worklistPushes{};
	std::size_t packedBytes{};
	const auto start = std::chrono::steady_clock::now();
	for (const auto size : {32U, 64U, 128U}) {
		cat::Relation po(size);
		for (std::size_t event = 1; event < size; ++event)
			po.insert(event - 1, event);
		cat::BaseValues base{{"po", po}};
		auto result = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis,
							    size, base);
		ASSERT_TRUE(result.errors.empty());
		ASSERT_TRUE(result.violations.empty());
		const auto reach = findPredicate(*analyzed.model, "reach");
		ASSERT_LT(reach, result.values.size());
		ASSERT_TRUE(result.values[reach].has_value());
		const auto &value = std::get<cat::Relation>(*result.values[reach]);
		EXPECT_EQ(value.count(), size * (size - 1) / 2);
		operationEvaluations += result.statistics.operationEvaluations;
		valueChanges += result.statistics.valueChanges;
		worklistPushes += result.statistics.worklistPushes;
		packedBytes += value.storageBytes();
	}
	const auto elapsed = std::chrono::steady_clock::now() - start;
	RecordProperty("elapsed_microseconds",
		       std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
	RecordProperty("operation_evaluations", operationEvaluations);
	RecordProperty("value_changes", valueChanges);
	RecordProperty("worklist_pushes", worklistPushes);
	RecordProperty("packed_result_bytes", packedBytes);
}

/* Random set Boolean operations satisfy identities and a direct membership oracle. */
RC_GTEST_PROP(CatValuePropertyTest, MatchesReferenceSetAlgebra,
	      (const std::vector<std::uint8_t> &lhsBytes,
	       const std::vector<std::uint8_t> &rhsBytes))
{
	constexpr std::size_t size = 19;
	cat::EventSet lhs(size);
	cat::EventSet rhs(size);
	for (std::size_t event = 0; event < size; ++event) {
		if (!lhsBytes.empty() && (lhsBytes[event % lhsBytes.size()] & 1U) != 0)
			lhs.insert(event);
		if (!rhsBytes.empty() && (rhsBytes[event % rhsBytes.size()] & 1U) != 0)
			rhs.insert(event);
	}
	const auto joined = setUnion(lhs, rhs);
	const auto common = setIntersection(lhs, rhs);
	const auto removed = setDifference(lhs, rhs);
	for (std::size_t event = 0; event < size; ++event) {
		RC_ASSERT(joined.contains(event) == (lhs.contains(event) || rhs.contains(event)));
		RC_ASSERT(common.contains(event) == (lhs.contains(event) && rhs.contains(event)));
		RC_ASSERT(removed.contains(event) == (lhs.contains(event) && !rhs.contains(event)));
	}
	RC_ASSERT(setUnion(lhs, lhs) == lhs);
	RC_ASSERT(setIntersection(lhs, lhs) == lhs);
	RC_ASSERT(setDifference(lhs, lhs).empty());
}

/* Random growth preserves every packed membership and leaves new cells empty. */
RC_GTEST_PROP(CatValuePropertyTest, GrowthMatchesReferenceValues,
	      (const std::vector<std::uint8_t> &bytes))
{
	const auto oldSize = bytes.empty() ? 0U : static_cast<std::size_t>(bytes.front() % 131U);
	const auto extra = bytes.size() < 2 ? 0U : static_cast<std::size_t>(bytes[1] % 66U);
	const auto newSize = oldSize + extra;
	cat::EventSet events(oldSize);
	cat::Relation relation(oldSize);
	std::set<std::size_t> eventReference;
	std::set<std::pair<std::size_t, std::size_t>> relationReference;
	if (oldSize != 0) {
		for (std::size_t index = 0; index < bytes.size(); ++index) {
			const auto event = static_cast<std::size_t>(bytes[index]) % oldSize;
			events.insert(event);
			eventReference.insert(event);
			const auto target =
				static_cast<std::size_t>(bytes[(index + 1) % bytes.size()]) %
				oldSize;
			relation.insert(event, target);
			relationReference.emplace(event, target);
		}
	}
	events.grow(newSize);
	relation.grow(newSize);

	RC_ASSERT(events.size() == newSize);
	RC_ASSERT(relation.size() == newSize);
	RC_ASSERT(events.count() == eventReference.size());
	RC_ASSERT(toReference(relation) == relationReference);
	for (std::size_t event = oldSize; event < newSize; ++event)
		RC_ASSERT(!events.contains(event));
}

/* Random packed operations agree with a std::set relation oracle. */
RC_GTEST_PROP(CatValuePropertyTest, MatchesReferenceRelationAlgebra,
	      (const std::vector<std::uint8_t> &lhsBytes,
	       const std::vector<std::uint8_t> &rhsBytes))
{
	constexpr std::size_t size = 6;
	cat::Relation lhs(size);
	cat::Relation rhs(size);
	for (std::size_t pair = 0; pair < size * size; ++pair) {
		if (!lhsBytes.empty() && (lhsBytes[pair % lhsBytes.size()] & 1U) != 0)
			lhs.insert(pair / size, pair % size);
		if (!rhsBytes.empty() && (rhsBytes[pair % rhsBytes.size()] & 1U) != 0)
			rhs.insert(pair / size, pair % size);
	}
	const auto lhsRef = toReference(lhs);
	const auto rhsRef = toReference(rhs);
	std::set<std::pair<std::size_t, std::size_t>> unionRef = lhsRef;
	unionRef.insert(rhsRef.begin(), rhsRef.end());
	std::set<std::pair<std::size_t, std::size_t>> intersectionRef;
	std::set_intersection(lhsRef.begin(), lhsRef.end(), rhsRef.begin(), rhsRef.end(),
			      std::inserter(intersectionRef, intersectionRef.end()));
	std::set<std::pair<std::size_t, std::size_t>> differenceRef;
	std::set_difference(lhsRef.begin(), lhsRef.end(), rhsRef.begin(), rhsRef.end(),
			    std::inserter(differenceRef, differenceRef.end()));
	std::set<std::pair<std::size_t, std::size_t>> compositionRef;
	for (const auto [from, middle] : lhsRef) {
		for (const auto [candidate, to] : rhsRef) {
			if (middle == candidate)
				compositionRef.emplace(from, to);
		}
	}
	bool reachable[size][size]{};
	for (const auto [from, to] : lhsRef)
		reachable[from][to] = true;
	for (std::size_t pivot = 0; pivot < size; ++pivot) {
		for (std::size_t from = 0; from < size; ++from) {
			for (std::size_t to = 0; to < size; ++to)
				reachable[from][to] |= reachable[from][pivot] &&
						       reachable[pivot][to];
		}
	}
	std::set<std::pair<std::size_t, std::size_t>> closureRef;
	for (std::size_t from = 0; from < size; ++from) {
		for (std::size_t to = 0; to < size; ++to) {
			if (reachable[from][to])
				closureRef.emplace(from, to);
		}
	}

	RC_ASSERT(toReference(relationUnion(lhs, rhs)) == unionRef);
	RC_ASSERT(toReference(relationIntersection(lhs, rhs)) == intersectionRef);
	RC_ASSERT(toReference(relationDifference(lhs, rhs)) == differenceRef);
	RC_ASSERT(toReference(compose(lhs, rhs)) == compositionRef);
	RC_ASSERT(toReference(transitiveClosure(lhs)) == closureRef);
	RC_ASSERT(inverse(inverse(lhs)) == lhs);
}

/* Named checks produce set, pair, diagonal, and closed-cycle witnesses with source spans. */
TEST(CatEvaluatorTest, ReportsStructuredViolations)
{
	auto model = compileModel(R"CAT(Witnesses
let communication = po | rf
empty R as reads-empty
empty communication as relation-empty
irreflexive communication as diagonal
acyclic communication as cycle
)CAT");
	cat::BaseValues base;
	base.emplace("R", makeSet(3, {2}));
	base.emplace("po", makeRelation(3, {{0, 1}, {1, 0}}));
	base.emplace("rf", makeRelation(3, {{2, 2}}));

	auto result = cat::Evaluator().evaluate(*model, 3, base);

	ASSERT_TRUE(result.errors.empty());
	ASSERT_EQ(result.violations.size(), 4U);
	EXPECT_EQ(result.violations[0].witness, std::vector<std::size_t>({2}));
	EXPECT_EQ(result.violations[1].witness.size(), 2U);
	EXPECT_EQ(result.violations[2].witness, std::vector<std::size_t>({2}));
	EXPECT_EQ(result.violations[3].witness.front(), result.violations[3].witness.back());
	EXPECT_EQ(result.violations[3].checkName, "cycle");
	EXPECT_EQ(result.violations[3].span.begin.line, 6U);
}

/* Acyclic graphs satisfy checks, and shared DAG nodes are evaluated exactly once. */
TEST(CatEvaluatorTest, MemoizesSharedNodesOnConsistentModel)
{
	auto model = compileModel(R"CAT(Memo
let shared = po | rf
let reused = shared ; shared
acyclic shared as first
irreflexive reused as second
)CAT");
	cat::BaseValues base;
	base.emplace("po", makeRelation(4, {{0, 1}, {1, 2}}));
	base.emplace("rf", makeRelation(4, {{2, 3}}));

	auto result = cat::Evaluator().evaluate(*model, 4, base);

	EXPECT_TRUE(result.consistent());
	ASSERT_EQ(result.evaluationCounts.size(), model->nodes().size());
	for (const auto count : result.evaluationCounts)
		EXPECT_LE(count, 1U);
	EXPECT_EQ(result.evaluationCounts[model->bindings()[0].value], 1U);
}

/* The bundled SC equation is interpreted generically from primitive values. */
TEST(CatEvaluatorTest, EvaluatesScModelWithoutSpecialDispatch)
{
	auto model = compileModel(R"CAT(SC
let com = rf | fr | co
empty rmw & (fre ; coe) as atomicity
acyclic po | tc | tj | com as sc
)CAT");
	cat::BaseValues base;
	base.emplace("rf", cat::Relation(2));
	base.emplace("fr", cat::Relation(2));
	base.emplace("co", cat::Relation(2));
	base.emplace("rmw", cat::Relation(2));
	base.emplace("ext", cat::Relation(2));
	base.emplace("po", makeRelation(2, {{0, 1}}));
	base.emplace("tc", cat::Relation(2));
	base.emplace("tj", cat::Relation(2));

	auto consistent = cat::Evaluator().evaluate(*model, 2, base);
	EXPECT_TRUE(consistent.consistent());
	base["rf"] = makeRelation(2, {{1, 0}});
	auto cyclic = cat::Evaluator().evaluate(*model, 2, base);
	ASSERT_EQ(cyclic.violations.size(), 1U);
	EXPECT_EQ(cyclic.violations[0].checkName, "sc");
}

/* Missing or wrong-shaped primitives fail once and never fabricate a consistency result. */
TEST(CatEvaluatorTest, RejectsInvalidBaseValues)
{
	auto model = compileModel("Errors\nlet shared = po | po\nacyclic shared as check\n");
	auto missing = cat::Evaluator().evaluate(*model, 3, {});
	cat::BaseValues wrong{{"po", makeSet(3, {0})}};
	auto mismatchedType = cat::Evaluator().evaluate(*model, 3, wrong);
	cat::BaseValues wrongSize{{"po", makeRelation(2, {{0, 1}})}};
	auto mismatchedSize = cat::Evaluator().evaluate(*model, 3, wrongSize);

	ASSERT_EQ(missing.errors.size(), 1U);
	EXPECT_EQ(missing.evaluationCounts[model->bindings()[0].value], 1U);
	EXPECT_EQ(mismatchedType.errors.size(), 1U);
	EXPECT_EQ(mismatchedSize.errors.size(), 1U);
	EXPECT_FALSE(missing.consistent());
}

/* Sparse/dense composition and closure measurements cover increasing universes. */
TEST(CatValueTest, BenchmarkIncreasingRelationSizes)
{
	std::size_t packedBytes{};
	const auto start = std::chrono::steady_clock::now();
	for (const auto size : {64U, 128U, 256U, 512U}) {
		cat::Relation sparse(size);
		cat::Relation dense(size);
		for (std::size_t from = 0; from < size; ++from) {
			if (from + 1 < size)
				sparse.insert(from, from + 1);
			for (std::size_t to = 0; to < size; ++to) {
				if ((from + to) % 3 == 0)
					dense.insert(from, to);
			}
		}
		packedBytes += sparse.storageBytes() + dense.storageBytes();
		EXPECT_FALSE(compose(sparse, dense).empty());
		EXPECT_FALSE(transitiveClosure(sparse).empty());
		EXPECT_FALSE(transitiveClosure(dense).empty());
	}
	const auto elapsed = std::chrono::steady_clock::now() - start;
	RecordProperty("elapsed_microseconds",
		       std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
	RecordProperty("packed_bytes", packedBytes);
}
