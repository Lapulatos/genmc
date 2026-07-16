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
#include "genmc/CAT/GraphSynchronizer.hpp"
#include "genmc/CAT/IncrementalEvaluator.hpp"
#include "genmc/CAT/Model.hpp"
#include "genmc/CAT/Normalized.hpp"
#include "genmc/CAT/Reasoner.hpp"
#include "genmc/CAT/Value.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

/** Test graph exposing the protected label insertion used by synchronization fixtures. */
class SynchronizerTestGraph : public ExecutionGraph {
public:
	using ExecutionGraph::addLabelToGraph;
	using ExecutionGraph::ExecutionGraph;
};

/** Insert one owned label into a synchronization fixture. */
template <typename Label, typename... Args>
auto addSynchronizerLabel(SynchronizerTestGraph &graph, Args &&...args) -> Label *
{
	auto label = std::make_unique<Label>(std::forward<Args>(args)...);
	return static_cast<Label *>(graph.addLabelToGraph(std::move(label)));
}

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

/* Packed, CSR, and structural rows expose the same ascending successor cursor. */
TEST(CatValueTest, IteratesRelationSuccessorsWithoutMaterializingRows)
{
	const std::vector<std::pair<std::size_t, std::size_t>> edges{{0, 1}, {0, 64},
							      {0, 129}, {64, 0}};
	cat::Relation dense(130);
	for (const auto [from, target] : edges)
		dense.insert(from, target);
	auto sparse = cat::Relation::sparse(130, edges);
	for (const auto &relation : {dense, sparse}) {
		EXPECT_EQ(relation.nextSuccessor(0, 0), 1U);
		EXPECT_EQ(relation.nextSuccessor(0, 2), 64U);
		EXPECT_EQ(relation.nextSuccessor(0, 65), 129U);
		EXPECT_EQ(relation.nextSuccessor(0, 130), 130U);
		EXPECT_EQ(relation.nextSuccessor(1, 0), 130U);
	}
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

/* Packed row insertion is exactly equivalent to inserting every selected pair. */
TEST(CatValueTest, InsertsPackedSuccessorRows)
{
	cat::Relation relation(130);
	relation.insert(64, 0);
	relation.insertSuccessors(64, makeSet(130, {1, 63, 64, 65, 129}));

	EXPECT_EQ(relation,
		  makeRelation(130, {{64, 0}, {64, 1}, {64, 63}, {64, 64}, {64, 65}, {64, 129}}));
}

/* Structural primitive views are exact operands and fail closed to dense on mutation. */
TEST(CatValueTest, StructuralRelationsMatchDenseAlgebraAndMutation)
{
	constexpr std::size_t size = 7;
	const auto threadKey = [](std::uint64_t group, std::uint32_t index = 0) {
		return (group << 32) | index;
	};
	const std::vector<std::uint64_t> threads{threadKey(2, 0), threadKey(2, 2), threadKey(3, 0),
						 threadKey(1),	  threadKey(1),	   0,
						 threadKey(2, 1)};
	const std::vector<std::uint64_t> locations{1, 2, 1, 1, 2, 0, 0};
	const std::array kinds{
		cat::StructuralRelationKind::ProgramOrder, cat::StructuralRelationKind::Internal,
		cat::StructuralRelationKind::External, cat::StructuralRelationKind::Location};
	cat::Relation extra(size);
	extra.insert(0, 2);
	extra.insert(2, 6);
	extra.insert(4, 0);

	for (const auto kind : kinds) {
		const auto &keys = kind == cat::StructuralRelationKind::Location ? locations
										 : threads;
		auto view = cat::Relation::structural(kind, keys);
		cat::Relation dense(size);
		for (std::size_t from = 0; from < size; ++from) {
			for (std::size_t to = 0; to < size; ++to) {
				const auto lhs = keys[from], rhs = keys[to];
				const auto lhsGroup = lhs >> 32, rhsGroup = rhs >> 32;
				const bool present =
					kind == cat::StructuralRelationKind::Location
						? lhs != 0 && lhs == rhs
					: kind == cat::StructuralRelationKind::ProgramOrder
						? lhsGroup >= 2 && lhsGroup == rhsGroup &&
							  static_cast<std::uint32_t>(lhs) <
								  static_cast<std::uint32_t>(rhs)
					: kind == cat::StructuralRelationKind::Internal
						? (lhsGroup == 1 && rhsGroup == 1) ||
							  (lhsGroup >= 2 && lhsGroup == rhsGroup)
						: (lhsGroup == 1 && rhsGroup >= 2) ||
							  (lhsGroup >= 2 && rhsGroup == 1) ||
							  (lhsGroup >= 2 && rhsGroup >= 2 &&
							   lhsGroup != rhsGroup);
				if (present)
					dense.insert(from, to);
			}
		}

		EXPECT_TRUE(view.isStructural());
		EXPECT_EQ(view, dense);
		EXPECT_EQ(dense, view);
		EXPECT_TRUE(view.isSubsetOf(dense));
		EXPECT_TRUE(dense.isSubsetOf(view));
		EXPECT_EQ(view.count(), dense.count());
		EXPECT_EQ(view.empty(), dense.empty());
		EXPECT_EQ(domain(view), domain(dense));
		EXPECT_EQ(range(view), range(dense));
		EXPECT_EQ(inverse(view), inverse(dense));
		EXPECT_EQ(optional(view), optional(dense));
		EXPECT_EQ(transitiveClosure(view), transitiveClosure(dense));
		EXPECT_EQ(relationUnion(view, extra), relationUnion(dense, extra));
		EXPECT_EQ(relationIntersection(view, extra), relationIntersection(dense, extra));
		EXPECT_EQ(relationDifference(view, extra), relationDifference(dense, extra));
		EXPECT_EQ(compose(view, extra), compose(dense, extra));
		EXPECT_EQ(compose(extra, view), compose(extra, dense));

		auto grownView = view;
		auto grownDense = dense;
		grownView.grow(9);
		grownDense.grow(9);
		EXPECT_TRUE(grownView.isStructural());
		EXPECT_EQ(grownView, grownDense);
		grownView.shrink(size);
		EXPECT_EQ(grownView, dense);

		auto mutated = view;
		auto expected = dense;
		bool erased = false;
		for (std::size_t from = 0; from < size && !erased; ++from) {
			for (std::size_t to = 0; to < size; ++to) {
				if (!dense.contains(from, to))
					continue;
				mutated.erase(from, to);
				expected.erase(from, to);
				erased = true;
				break;
			}
		}
		ASSERT_TRUE(erased);
		EXPECT_FALSE(mutated.isStructural());
		EXPECT_EQ(mutated, expected);
	}

	auto oldInternal =
		cat::Relation::structural(cat::StructuralRelationKind::Internal, threads);
	auto extendedThreads = threads;
	extendedThreads[5] = threadKey(4, 0);
	auto newInternal =
		cat::Relation::structural(cat::StructuralRelationKind::Internal, extendedThreads);
	EXPECT_TRUE(oldInternal.isSubsetOf(newInternal));
	EXPECT_FALSE(newInternal.isSubsetOf(oldInternal));
	auto relabeledLocations = locations;
	for (auto &key : relabeledLocations) {
		if (key == 1)
			key = 9;
		else if (key == 2)
			key = 4;
	}
	auto oldLocation =
		cat::Relation::structural(cat::StructuralRelationKind::Location, locations);
	auto relabeledLocation = cat::Relation::structural(cat::StructuralRelationKind::Location,
							   relabeledLocations);
	EXPECT_TRUE(oldLocation.isSubsetOf(relabeledLocation));
	EXPECT_TRUE(relabeledLocation.isSubsetOf(oldLocation));
}

/* Explicit CSR relations preserve arbitrary sparse edges and exact dense fallback. */
TEST(CatValueTest, SparseRelationsMatchDenseAlgebraGrowthAndMutation)
{
	const std::vector<std::pair<std::size_t, std::size_t>> edges{
		{6, 1}, {0, 4}, {2, 2}, {0, 1}, {6, 1}, {4, 6}};
	auto sparse = cat::Relation::sparse(7, edges);
	auto dense = makeRelation(7, {{0, 1}, {0, 4}, {2, 2}, {4, 6}, {6, 1}});
	auto other = makeRelation(7, {{1, 3}, {2, 2}, {4, 0}, {6, 5}});

	EXPECT_TRUE(sparse.isStructural());
	EXPECT_EQ(sparse.storageBytes(), 13U * sizeof(std::uint32_t));
	EXPECT_EQ(sparse, dense);
	EXPECT_TRUE(sparse.isSubsetOf(dense));
	EXPECT_TRUE(dense.isSubsetOf(sparse));
	EXPECT_EQ(sparse.count(), dense.count());
	EXPECT_EQ(domain(sparse), domain(dense));
	EXPECT_EQ(range(sparse), range(dense));
	EXPECT_EQ(inverse(sparse), inverse(dense));
	EXPECT_EQ(optional(sparse), optional(dense));
	EXPECT_EQ(transitiveClosure(sparse), transitiveClosure(dense));
	EXPECT_EQ(relationUnion(sparse, other), relationUnion(dense, other));
	EXPECT_EQ(relationIntersection(sparse, other), relationIntersection(dense, other));
	EXPECT_EQ(relationDifference(sparse, other), relationDifference(dense, other));
	EXPECT_EQ(compose(sparse, other), compose(dense, other));
	EXPECT_EQ(compose(other, sparse), compose(other, dense));

	auto subset = cat::Relation::sparse(7, {{0, 1}, {4, 6}});
	EXPECT_TRUE(subset.isSubsetOf(sparse));
	EXPECT_FALSE(sparse.isSubsetOf(subset));
	subset.grow(10);
	EXPECT_TRUE(subset.isStructural());
	EXPECT_TRUE(subset.contains(4, 6));
	subset.shrink(5);
	EXPECT_TRUE(subset.contains(0, 1));
	EXPECT_FALSE(subset.contains(4, 4));

	auto mutated = sparse;
	mutated.erase(2, 2);
	mutated.insert(3, 5);
	dense.erase(2, 2);
	dense.insert(3, 5);
	EXPECT_FALSE(mutated.isStructural());
	EXPECT_EQ(mutated, dense);
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

/* The canonical closure value matches an independent Kleene construction for random
 * finite relations, preventing the optimized offline/incremental paths from serving as
 * each other's only oracle. */
RC_GTEST_PROP(CaatOptimizationPropertyTest, LinearClosureMatchesNaiveKleene,
	      (const std::vector<std::uint8_t> &bytes))
{
	const std::size_t size = 1 + std::min<std::size_t>(bytes.size(), 15);
	cat::Relation po(size);
	for (std::size_t pair = 0; pair < size * size; ++pair) {
		if (!bytes.empty() && (bytes[pair % bytes.size()] & 1U) != 0)
			po.insert(pair / size, pair % size);
	}
	cat::Relation naive(size);
	for (;;) {
		auto next = relationUnion(po, compose(naive, po));
		if (next == naive)
			break;
		naive = std::move(next);
	}
	for (const auto source : {
		     "LeftA\nlet rec path = po | (path ; po)\nempty path\n",
		     "LeftB\nlet rec path = (path ; po) | po\nempty path\n",
		     "RightA\nlet rec path = po | (po ; path)\nempty path\n",
		     "RightB\nlet rec path = (po ; path) | po\nempty path\n"}) {
		auto analyzed = analyzeModel(source);
		RC_ASSERT(analyzed.model != nullptr);
		const auto result = cat::CaatEvaluator().evaluate(
			*analyzed.model, *analyzed.analysis, size, {{"po", po}});
		const auto path = findPredicate(*analyzed.model, "path");
		RC_ASSERT(analyzed.model->predicates()[path].kind ==
			  cat::Predicate::Kind::TransitiveClosure);
		RC_ASSERT(std::get<cat::Relation>(*result.values[path]) == naive);
	}
}

/* Sliced closure checks agree with an independent DFS over every random seed graph. */
RC_GTEST_PROP(CaatOptimizationPropertyTest, AcyclicClosureSliceMatchesNaiveDfs,
	      (const std::vector<std::uint8_t> &bytes))
{
	const std::size_t size = 1 + std::min<std::size_t>(bytes.size(), 15);
	cat::Relation po(size);
	for (std::size_t pair = 0; pair < size * size; ++pair) {
		if (!bytes.empty() && (bytes[pair % bytes.size()] & 1U) != 0)
			po.insert(pair / size, pair % size);
	}
	std::vector<std::uint8_t> color(size);
	const auto visit = [&](auto &self, std::size_t event) -> bool {
		color[event] = 1;
		for (std::size_t target = 0; target < size; ++target) {
			if (!po.contains(event, target))
				continue;
			if (color[target] == 1 || (color[target] == 0 && self(self, target)))
				return true;
		}
		color[event] = 2;
		return false;
	};
	bool hasCycle = false;
	for (std::size_t event = 0; event < size && !hasCycle; ++event) {
		if (color[event] == 0)
			hasCycle = visit(visit, event);
	}
	for (const auto source : {
		     "AcyclicSliceProperty\nlet path = po+\nacyclic path as cycle\n",
		     "IrreflexiveSliceProperty\nlet path = po+\nirreflexive path as cycle\n"}) {
		auto analyzed = analyzeModel(source);
		RC_ASSERT(analyzed.model != nullptr);
		RC_ASSERT(std::ranges::none_of(
			analyzed.model->predicates(), [](const auto &predicate) {
				return predicate.kind == cat::Predicate::Kind::TransitiveClosure;
			}));
		RC_ASSERT(analyzed.model->checks()[0].kind ==
			  cat::Statement::CheckKind::Acyclic);
		const auto result = cat::CaatEvaluator().evaluate(
			*analyzed.model, *analyzed.analysis, size, {{"po", po}});
		RC_ASSERT(result.consistent() == !hasCycle);
	}
}

/* The lazy plan interprets a positive union/composition/filter DAG extensionally and
 * agrees with independently materialized CAAT values for every random finite input. */
RC_GTEST_PROP(CaatOptimizationPropertyTest, LazyCyclePlanMatchesMaterializedRelation,
	      (const std::vector<std::uint8_t> &bytes))
{
	static const auto analyzed = analyzeModel(R"CAT(LazyCycleProperty
let order = ([R] ; po) | (po ; [W]) | ((po ; rf) & loc)
acyclic order as cycle
)CAT");
	RC_ASSERT(analyzed.model != nullptr);
	RC_ASSERT(analyzed.analysis->lazyCycleRoots().size() == 1);
	RC_ASSERT(analyzed.analysis->lazyCycleRoots()[0].has_value());
	const std::size_t size = 1 + std::min<std::size_t>(bytes.size(), 14);
	cat::Relation po(size), rf(size), loc(size);
	cat::EventSet reads(size), writes(size);
	const auto byte = [&](std::size_t index) {
		return bytes.empty() ? std::uint8_t{} : bytes[index % bytes.size()];
	};
	for (std::size_t event = 0; event < size; ++event) {
		if ((byte(event) & 1U) != 0)
			reads.insert(event);
		if ((byte(event) & 2U) != 0)
			writes.insert(event);
	}
	for (std::size_t pair = 0; pair < size * size; ++pair) {
		const auto from = pair / size;
		const auto target = pair % size;
		const auto bits = byte(size + pair);
		if ((bits & 1U) != 0)
			po.insert(from, target);
		if ((bits & 2U) != 0)
			rf.insert(from, target);
		if ((bits & 4U) != 0)
			loc.insert(from, target);
	}
	cat::BaseValues base{{"R", reads}, {"W", writes}, {"po", po}, {"rf", rf},
			     {"loc", loc}};
	const auto materialized = cat::CaatEvaluator().evaluate(
		*analyzed.model, *analyzed.analysis, size, base, false);
	const auto lazy = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis,
						       size, base, true);
	RC_ASSERT(materialized.errors.empty());
	RC_ASSERT(lazy.errors.empty());
	RC_ASSERT(materialized.consistent() == lazy.consistent());
	RC_ASSERT(materialized.violations.size() == lazy.violations.size());
	for (std::size_t index = 0; index < materialized.violations.size(); ++index) {
		RC_ASSERT(materialized.violations[index].checkName ==
			  lazy.violations[index].checkName);
		RC_ASSERT(materialized.violations[index].checkKind ==
			  lazy.violations[index].checkKind);
		RC_ASSERT(materialized.violations[index].span == lazy.violations[index].span);
		RC_ASSERT(materialized.violations[index].witness ==
			  lazy.violations[index].witness);
	}
	const auto root = *analyzed.analysis->lazyCycleRoots()[0];
	RC_ASSERT(materialized.values[root].has_value());
	RC_ASSERT(!lazy.values[root].has_value());
}

/* Shared, recursive, and expensive-membership cones retain the generic evaluator. */
TEST(CaatOptimizationTest, LazyCyclePlanFailsClosedForNearNeighbours)
{
	auto exact = analyzeModel(
		"LazyExact\nlet order = po | (rf ; co)\nacyclic order\n");
	ASSERT_NE(exact.model, nullptr);
	ASSERT_EQ(exact.analysis->lazyCycleRoots().size(), 1U);
	EXPECT_TRUE(exact.analysis->lazyCycleRoots()[0].has_value());

	for (const auto source : {
		     "LazyCheap\nlet order = po | rf\nacyclic order\n",
		     "LazyShared\nlet order = po | rf\nacyclic order\nempty order\n",
		     "LazyRecursive\nlet rec order = po | (order ; rf)\nacyclic order\n",
		     "LazyIntersection\nlet order = (po ; rf) & (rf ; po)\nacyclic order\n"}) {
		auto fallback = analyzeModel(source);
		ASSERT_NE(fallback.model, nullptr) << source;
		EXPECT_TRUE(std::ranges::none_of(fallback.analysis->lazyCycleRoots(),
						[](const auto &root) { return root.has_value(); }))
			<< source;
		EXPECT_TRUE(std::ranges::none_of(fallback.analysis->lazyCycleElided(),
						[](bool elided) { return elided; }))
			<< source;
	}
}

/* Only a dead non-reflexive closure observed exclusively by cycle checks is sliced. */
TEST(CaatOptimizationTest, SlicesOnlyExactCycleCheckedClosures)
{
	const auto closureCount = [](const cat::NormalizedModel &model) {
		return std::ranges::count_if(model.predicates(), [](const auto &predicate) {
			return predicate.kind == cat::Predicate::Kind::TransitiveClosure ||
			       predicate.kind == cat::Predicate::Kind::ReflexiveTransitiveClosure;
		});
	};

	auto exact = analyzeModel(R"CAT(ExactSlice
let first = po+
let second = first+
acyclic second as a
irreflexive second as b
)CAT");
	ASSERT_NE(exact.model, nullptr);
	EXPECT_EQ(closureCount(*exact.model), 0);
	ASSERT_EQ(exact.model->checks().size(), 2U);
	for (const auto &check : exact.model->checks()) {
		EXPECT_EQ(check.kind, cat::Statement::CheckKind::Acyclic);
		EXPECT_EQ(exact.model->predicates()[check.predicate].name, "po");
	}

	for (const auto source : {
		     "EmptyUse\nlet path = po+\nempty path\n",
		     "PredicateUse\nlet path = po+\nlet copy = path\nacyclic path\n",
		     "ReflexiveUse\nlet path = po*\nirreflexive path\n",
		     "MixedUse\nlet path = po+\nacyclic path\nempty path\n",
		     "Unused\nlet path = po+\nacyclic po\n"}) {
		auto retained = analyzeModel(source);
		ASSERT_NE(retained.model, nullptr) << source;
		EXPECT_EQ(closureCount(*retained.model), 1) << source;
	}
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

/* Lazy predicates remain absent across monotone insertion and exact rollback while the
 * extensional cycle verdict tracks a full materialized evaluation. */
TEST(IncrementalCaatEvaluatorTest, MaintainsLazyCycleAcrossInsertionAndRollback)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalLazyCycle
let order = po | (rf ; co)
acyclic order as cycle
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	ASSERT_TRUE(analyzed.analysis->lazyCycleRoots()[0].has_value());
	constexpr std::size_t size = 513;
	cat::BaseValues root{{"po", cat::Relation(size)},
			     {"rf", cat::Relation(size)},
			     {"co", cat::Relation(size)}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis, true,
						 true);
	incremental.initialize(size, root);
	const auto order = *analyzed.analysis->lazyCycleRoots()[0];
	ASSERT_FALSE(incremental.result().values[order].has_value());
	ASSERT_TRUE(incremental.result().consistent());
	const auto checkpoint = incremental.checkpoint();

	auto forward = root;
	std::get<cat::Relation>(forward["po"]).insert(0, 1);
	ASSERT_TRUE(incremental.tryInsert(size, forward).applied());
	EXPECT_TRUE(incremental.result().consistent());
	auto cycle = forward;
	std::get<cat::Relation>(cycle["po"]).insert(1, 0);
	ASSERT_TRUE(incremental.tryInsert(size, cycle).applied());
	EXPECT_FALSE(incremental.result().consistent());
	const auto materialized =
		cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis, size, cycle);
	EXPECT_EQ(incremental.result().consistent(), materialized.consistent());

	const auto restored = incremental.rollback(checkpoint);
	ASSERT_TRUE(restored.restored) << restored.reason;
	EXPECT_TRUE(incremental.result().consistent());
	EXPECT_EQ(incremental.eventCount(), size);
	EXPECT_GT(incremental.statistics().lazyCycleChecks, 0U);
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

TEST(IncrementalCaatEvaluatorTest, PreservesUnionFactsUntilLastReplacementSupport)
{
	auto analyzed = analyzeModel(R"CAT(SupportAwareReplacement
let order = rf | co
empty order
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues base{{"rf", makeRelation(3, {{0, 1}})}, {"co", makeRelation(3, {{0, 1}})}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(3, base);

	base.at("rf") = makeRelation(3, {});
	ASSERT_TRUE(incremental.tryReplace(3, base).applied());
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 3, base);
	EXPECT_FALSE(incremental.result().consistent());

	base.at("co") = makeRelation(3, {});
	ASSERT_TRUE(incremental.tryReplace(3, base).applied());
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 3, base);
	EXPECT_TRUE(incremental.result().consistent());
	EXPECT_EQ(incremental.statistics().replacementUpdates, 2U);
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

/* Nested checkpoints restore recursive values, witnesses, and word-boundary growth exactly. */
TEST(IncrementalCaatEvaluatorTest, RestoresNestedCheckpointsAndInvalidatesDescendants)
{
	auto analyzed = analyzeModel(R"CAT(IncrementalRollback
let rec reach = po | (reach ; po)
acyclic reach as cycle
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	cat::BaseValues rootBase{{"po", makeRelation(63, {{0, 1}})}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(63, rootBase);
	const auto root = incremental.checkpoint();

	cat::BaseValues middleBase{{"po", makeRelation(65, {{0, 1}, {1, 64}})}};
	ASSERT_TRUE(incremental.tryInsert(65, middleBase).applied());
	const auto middle = incremental.checkpoint();
	cat::BaseValues leafBase{{"po", makeRelation(65, {{0, 1}, {1, 64}, {64, 0}})}};
	ASSERT_TRUE(incremental.tryInsert(65, leafBase).applied());
	ASSERT_FALSE(incremental.result().consistent());

	auto restoredMiddle = incremental.rollback(middle);
	ASSERT_TRUE(restoredMiddle.restored) << restoredMiddle.reason;
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 65,
				       middleBase);
	EXPECT_TRUE(incremental.result().consistent());
	const auto descendant = incremental.checkpoint();
	ASSERT_TRUE(incremental.tryInsert(65, leafBase).applied());
	const auto cyclic = incremental.checkpoint();
	ASSERT_TRUE(incremental.rollback(cyclic).restored);
	auto explanation = cat::Reasoner().explain(
		*analyzed.model, *analyzed.analysis, incremental.eventCount(),
		incremental.result().values, incremental.result().violations);
	ASSERT_TRUE(explanation.ok());
	ASSERT_EQ(explanation.violations.size(), 1U);
	EXPECT_FALSE(explanation.violations[0].explanation.empty());
	ASSERT_TRUE(incremental.rollback(root).restored);
	expectIncrementalEqualsOffline(incremental, *analyzed.model, *analyzed.analysis, 63,
				       rootBase);
	EXPECT_FALSE(incremental.rollback(middle).restored);
	EXPECT_FALSE(incremental.rollback(descendant).restored);
	ASSERT_TRUE(incremental.rollback(root).restored);
	EXPECT_FALSE(incremental.rollback(cyclic).restored);
	EXPECT_EQ(incremental.statistics().rollbacks, 4U);
	EXPECT_EQ(incremental.statistics().rejectedRollbacks, 3U);
}

/* Reinitialization starts a new checkpoint epoch and rejects every old handle. */
TEST(IncrementalCaatEvaluatorTest, RejectsCheckpointFromPreviousInitialization)
{
	auto analyzed = analyzeModel("IncrementalCheckpointEpoch\nacyclic po\n");
	ASSERT_NE(analyzed.model, nullptr);
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(2, {{"po", makeRelation(2, {})}});
	const auto stale = incremental.checkpoint();
	cat::IncrementalCaatEvaluator other(*analyzed.model, *analyzed.analysis);
	other.initialize(2, {{"po", makeRelation(2, {})}});
	const auto foreign = other.checkpoint();
	EXPECT_FALSE(incremental.rollback(foreign).restored);
	incremental.initialize(2, {{"po", makeRelation(2, {{0, 1}})}});
	const auto before = incremental.result().values;
	auto rejected = incremental.rollback(stale);
	EXPECT_FALSE(rejected.restored);
	EXPECT_NE(rejected.reason.find("stale"), std::string::npos);
	EXPECT_EQ(incremental.result().values, before);
}

/* Graph synchronization selects insertion, rollback-plus-insert, and mixed rebuild exactly. */
TEST(CaatGraphSynchronizerTest, ClassifiesStableGraphTransitionsAndBoundsHistory)
{
	auto analyzed = analyzeModel(R"CAT(GraphSynchronization
let rec order = po | (order ; po)
acyclic order
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	SynchronizerTestGraph graph{{nullptr, nullptr, true}};
	cat::IncrementalCaatEvaluator evaluator(*analyzed.model, *analyzed.analysis);
	/* Check every transition so initialization, insertion, rollback-insert and
	 * rebuild all compare their published values with the Phase 2 oracle. */
	cat::GraphSynchronizer synchronizer(evaluator, 2, 1, true);

	cat::GraphAdapter empty(graph);
	EXPECT_EQ(synchronizer.synchronize(empty).transition, cat::GraphTransition::Initialize);
	addSynchronizerLabel<FenceLabel>(graph, Event(0, 1), MemOrdering::Relaxed);
	cat::GraphAdapter one(graph);
	EXPECT_EQ(synchronizer.synchronize(one).transition, cat::GraphTransition::Insert);
	addSynchronizerLabel<FenceLabel>(graph, Event(0, 2), MemOrdering::Relaxed);
	cat::GraphAdapter two(graph);
	EXPECT_EQ(synchronizer.synchronize(two).transition, cat::GraphTransition::Insert);
	EXPECT_EQ(synchronizer.retainedCheckpoints(), 2U);

	auto removedSecond = graph.removeLast(0);
	ASSERT_NE(removedSecond, nullptr);
	cat::GraphAdapter cut(graph);
	EXPECT_EQ(synchronizer.synchronize(cut).transition, cat::GraphTransition::RollbackInsert);
	EXPECT_EQ(evaluator.eventCount(), 2U);
	EXPECT_EQ(std::get<cat::EventSet>(evaluator.baseValues().at("_")).count(), 1U);
	expectIncrementalEqualsOffline(evaluator, *analyzed.model, *analyzed.analysis,
				       evaluator.eventCount(), evaluator.baseValues());

	/* Replacing the same EventPos with a different label category removes F and
	 * adds W, so no retained snapshot is a semantic predecessor. */
	auto removedFirst = graph.removeLast(0);
	ASSERT_NE(removedFirst, nullptr);
	auto *write = addSynchronizerLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::Relaxed,
						       SAddr(0x1000), ASize(4), SVal(1));
	write->addCo(graph.getInitLabel());
	cat::GraphAdapter replaced(graph);
	const auto rebuilt = synchronizer.synchronize(replaced);
	EXPECT_EQ(rebuilt.transition, cat::GraphTransition::Rebuild);
	EXPECT_FALSE(rebuilt.reason.empty());
	EXPECT_EQ(synchronizer.statistics().initializations, 1U);
	EXPECT_EQ(synchronizer.statistics().insertions, 2U);
	EXPECT_EQ(synchronizer.statistics().rollbackInsertions, 1U);
	EXPECT_EQ(synchronizer.statistics().rebuilds, 1U);
	EXPECT_EQ(synchronizer.statistics().evictedCheckpoints, 1U);
	EXPECT_EQ(synchronizer.statistics().oracleChecks, 5U);
	EXPECT_EQ(synchronizer.statistics().maximumActiveEvents, 2U);
	EXPECT_EQ(synchronizer.statistics().maximumStableEvents, 3U);
	EXPECT_GE(synchronizer.statistics().maximumInactiveEvents, 1U);
	EXPECT_GT(synchronizer.statistics().maximumCurrentBaseBytes, 0U);
	EXPECT_GT(synchronizer.statistics().maximumHistoryBaseBytes, 0U);
}

/* Direct stable materialization is exactly equivalent to dense-build/remap. */
TEST(CatStableGraphAdapterTest, DirectMaterializationMatchesDenseRemapAcrossMutations)
{
	SynchronizerTestGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	const SAddr y{0x2000};
	auto *first = addSynchronizerLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::Relaxed, x,
						       ASize(4), SVal(1));
	first->addCo(graph.getInitLabel());
	auto *second = addSynchronizerLabel<WriteLabel>(graph, Event(0, 2), MemOrdering::Relaxed, x,
							ASize(4), SVal(2));
	second->addCo(first);
	auto *read = addSynchronizerLabel<ReadLabel>(graph, Event(0, 3), MemOrdering::Relaxed, x,
						     ASize(4));
	read->setRf(first);
	graph.addNewThread();
	auto *otherThread = addSynchronizerLabel<WriteLabel>(
		graph, Event(1, 0), MemOrdering::Relaxed, y, ASize(4), SVal(3));
	otherThread->addCo(graph.getInitLabel());
	addSynchronizerLabel<FenceLabel>(graph, Event(1, 1), MemOrdering::Acquire);

	cat::StableGraphAdapter legacy;
	cat::StableGraphAdapter direct;
	const auto compare = [&] {
		const cat::GraphAdapter dense(graph);
		const auto expected = legacy.materialize(dense);
		const auto actual = direct.materialize(graph);
		EXPECT_EQ(actual.eventCount, expected.eventCount);
		EXPECT_EQ(actual.activeEventCount, expected.activeEventCount);
		EXPECT_EQ(actual.denseToStable, expected.denseToStable);
		EXPECT_EQ(actual.base, expected.base);
		for (const auto name : {"po", "int", "ext", "loc"})
			EXPECT_FALSE(std::get<cat::Relation>(actual.base.at(name)).isStructural())
				<< name;
	};
	compare();
	read->setRf(second);
	compare();
	second->moveCo(graph.getInitLabel());
	compare();
	auto removed = graph.removeLast(0);
	ASSERT_NE(removed, nullptr);
	compare();
}

/* Structural bases start only beyond the adaptive-offline event boundary. */
TEST(CatStableGraphAdapterTest, UsesStructuralViewsAboveAdaptiveOfflineBoundary)
{
	SynchronizerTestGraph graph{{nullptr, nullptr, true}};
	cat::StableGraphAdapter direct;
	for (int index = 1; index <= 512; ++index)
		addSynchronizerLabel<FenceLabel>(graph, Event(0, index), MemOrdering::Relaxed);
	const auto boundary = direct.materialize(graph);
	ASSERT_EQ(boundary.eventCount, 512U);
	for (const auto name : {"po", "int", "ext", "loc"})
		EXPECT_FALSE(std::get<cat::Relation>(boundary.base.at(name)).isStructural())
			<< name;

	addSynchronizerLabel<FenceLabel>(graph, Event(0, 513), MemOrdering::Relaxed);
	const auto large = direct.materialize(graph);
	ASSERT_EQ(large.eventCount, 513U);
	for (const auto name : {"po", "int", "ext", "loc"})
		EXPECT_TRUE(std::get<cat::Relation>(large.base.at(name)).isStructural()) << name;
	const cat::GraphAdapter dense(graph);
	cat::StableGraphAdapter remapped;
	EXPECT_EQ(large.base, remapped.materialize(dense).base);
}

/* Large stable snapshots emit exact CSR rf/co/fr and empty lifecycle primitives. */
TEST(CatStableGraphAdapterTest, SparseEdgePrimitivesMatchDenseLargeSnapshot)
{
	SynchronizerTestGraph graph{{nullptr, nullptr, true}};
	for (int index = 1; index <= 509; ++index)
		addSynchronizerLabel<FenceLabel>(graph, Event(0, index), MemOrdering::Relaxed);
	const SAddr x{0x1000};
	auto *first = addSynchronizerLabel<WriteLabel>(
		graph, Event(0, 510), MemOrdering::Relaxed, x, ASize(4), SVal(1));
	first->addCo(graph.getInitLabel());
	auto *second = addSynchronizerLabel<WriteLabel>(
		graph, Event(0, 511), MemOrdering::Relaxed, x, ASize(4), SVal(2));
	second->addCo(first);
	auto *read = addSynchronizerLabel<ReadLabel>(
		graph, Event(0, 512), MemOrdering::Relaxed, x, ASize(4));
	read->setRf(first);

	cat::StableGraphAdapter adapter({"rf", "co", "fr", "rmw", "tc", "tj"});
	const auto snapshot = adapter.materialize(graph);
	ASSERT_EQ(snapshot.eventCount, 513U);
	const std::map<std::string_view, cat::Relation> expected{
		{"rf", makeRelation(513, {{509, 511}})},
		{"co", makeRelation(513, {{512, 509}, {512, 510}, {509, 510}})},
		{"fr", makeRelation(513, {{511, 510}})},
		{"rmw", cat::Relation(513)},
		{"tc", cat::Relation(513)},
		{"tj", cat::Relation(513)}};
	for (const auto &[name, dense] : expected) {
		const auto &actual = std::get<cat::Relation>(snapshot.base.at(std::string(name)));
		EXPECT_TRUE(actual.isStructural()) << name;
		EXPECT_EQ(actual, dense) << name;
		EXPECT_LE(actual.storageBytes(), dense.storageBytes() / 2) << name;
	}
}

TEST(CaatOptimizationTest, CertifiesOnlyExactBundledRecursiveModels)
{
	for (const auto &[name, expected] :
	     {std::pair{"sc", cat::HostProfile::SC}, std::pair{"tso", cat::HostProfile::TSO}}) {
		auto parsed = cat::Frontend().parseFile(
			std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
			"models/cat" / ("recursive-" + std::string(name) + ".cat"));
		ASSERT_TRUE(parsed.ok());
		auto normalized = cat::Normalizer().normalize(*parsed.model);
		ASSERT_TRUE(normalized.ok());
		EXPECT_EQ(normalized.model->certifiedCandidateProfile(), expected)
			<< normalized.model->summary();
		EXPECT_TRUE(normalized.model->certifiedAdaptiveOffline());
	}

	auto psoParsed = cat::Frontend().parseFile(
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat/recursive-pso.cat");
	ASSERT_TRUE(psoParsed.ok());
	auto pso = cat::Normalizer().normalize(*psoParsed.model);
	ASSERT_TRUE(pso.ok());
	EXPECT_EQ(pso.model->certifiedCandidateProfile(), std::nullopt) << pso.model->summary();
	EXPECT_TRUE(pso.model->certifiedAdaptiveOffline()) << pso.model->summary();

	/* Check kinds are part of the closed-world certificate: changing coherence
	 * from acyclic to irreflexive retains the same predicate DAG but changes the
	 * accepted executions. */
	const auto tsoPath =
		std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
		"models/cat/recursive-tso.cat";
	std::ifstream tsoInput(tsoPath);
	ASSERT_TRUE(tsoInput.good());
	std::string changedKind{std::istreambuf_iterator<char>{tsoInput},
				std::istreambuf_iterator<char>{}};
	const auto oldCheck = changedKind.find("acyclic (po & loc)");
	ASSERT_NE(oldCheck, std::string::npos);
	changedKind.replace(oldCheck, std::string_view{"acyclic"}.size(), "irreflexive");
	auto changed = analyzeModel(changedKind);
	ASSERT_NE(changed.model, nullptr);
	EXPECT_EQ(changed.model->certifiedCandidateProfile(), std::nullopt);
	EXPECT_FALSE(changed.model->certifiedAdaptiveOffline());

	auto modified = analyzeModel(R"CAT(RecursiveSC
let rec reach = order | (reach ; order)
let com = rf | fr | co
let order = po | tc | tj
acyclic reach as sc
)CAT");
	ASSERT_NE(modified.model, nullptr);
	EXPECT_EQ(modified.model->certifiedCandidateProfile(), std::nullopt);
	EXPECT_FALSE(modified.model->certifiedAdaptiveOffline());
}

/* Edge-only rf replacement and coherence reorder cannot masquerade as insertion. */
TEST(CaatGraphSynchronizerTest, ReplacesSupportedReadsFromAndCoherenceMutations)
{
	auto analyzed = analyzeModel(R"CAT(GraphEdgeMutation
acyclic (po | rf | co)
)CAT");
	ASSERT_NE(analyzed.model, nullptr);
	SynchronizerTestGraph graph{{nullptr, nullptr, true}};
	const SAddr x{0x1000};
	auto *first = addSynchronizerLabel<WriteLabel>(graph, Event(0, 1), MemOrdering::Relaxed, x,
						       ASize(4), SVal(1));
	first->addCo(graph.getInitLabel());
	auto *second = addSynchronizerLabel<WriteLabel>(graph, Event(0, 2), MemOrdering::Relaxed, x,
							ASize(4), SVal(2));
	second->addCo(first);
	auto *read = addSynchronizerLabel<ReadLabel>(graph, Event(0, 3), MemOrdering::Relaxed, x,
						     ASize(4));
	read->setRf(first);
	cat::IncrementalCaatEvaluator evaluator(*analyzed.model, *analyzed.analysis);
	cat::GraphSynchronizer synchronizer(evaluator);
	cat::GraphAdapter initial(graph);
	ASSERT_EQ(synchronizer.synchronize(initial).transition, cat::GraphTransition::Initialize);

	read->setRf(second);
	cat::GraphAdapter changedRf(graph);
	EXPECT_EQ(synchronizer.synchronize(changedRf).transition, cat::GraphTransition::Replace);
	expectIncrementalEqualsOffline(evaluator, *analyzed.model, *analyzed.analysis,
				       evaluator.eventCount(), evaluator.baseValues());

	second->moveCo(graph.getInitLabel());
	cat::GraphAdapter changedCo(graph);
	EXPECT_EQ(synchronizer.synchronize(changedCo).transition, cat::GraphTransition::Replace);
	expectIncrementalEqualsOffline(evaluator, *analyzed.model, *analyzed.analysis,
				       evaluator.eventCount(), evaluator.baseValues());
}

/* Stamp cuts restore a prefix and same-position revisits are classified semantically. */
TEST(CaatGraphSynchronizerTest, HandlesCutToStampAndNonLifoRevisit)
{
	auto analyzed = analyzeModel("GraphCutAndRevisit\nacyclic po\n");
	ASSERT_NE(analyzed.model, nullptr);
	SynchronizerTestGraph graph{{nullptr, nullptr, true}};
	auto *first = addSynchronizerLabel<FenceLabel>(graph, Event(0, 1), MemOrdering::Relaxed);
	ASSERT_EQ(first->getStamp(), Stamp(1));
	cat::IncrementalCaatEvaluator evaluator(*analyzed.model, *analyzed.analysis);
	cat::GraphSynchronizer synchronizer(evaluator);
	cat::GraphAdapter prefix(graph);
	ASSERT_EQ(synchronizer.synchronize(prefix).transition, cat::GraphTransition::Initialize);
	auto *second = addSynchronizerLabel<FenceLabel>(graph, Event(0, 2), MemOrdering::Acquire);
	ASSERT_EQ(second->getStamp(), Stamp(2));
	cat::GraphAdapter extension(graph);
	ASSERT_EQ(synchronizer.synchronize(extension).transition, cat::GraphTransition::Insert);

	graph.cutToStamp(Stamp(1));
	cat::GraphAdapter cut(graph);
	EXPECT_EQ(synchronizer.synchronize(cut).transition, cat::GraphTransition::RollbackInsert);
	EXPECT_EQ(std::get<cat::EventSet>(evaluator.baseValues().at("_")).count(), 1U);

	/* A new label at the old position reuses its reserved stable key; reactivating
	 * that ID and its po edges is an insertion rather than a fresh identity. */
	auto *revisit =
		addSynchronizerLabel<FenceLabel>(graph, Event(0, 2), MemOrdering::AcquireRelease);
	ASSERT_EQ(revisit->getStamp(), Stamp(2));
	cat::GraphAdapter revisited(graph);
	EXPECT_EQ(synchronizer.synchronize(revisited).transition, cat::GraphTransition::Insert);
	expectIncrementalEqualsOffline(evaluator, *analyzed.model, *analyzed.analysis,
				       evaluator.eventCount(), evaluator.baseValues());
}

/* Random insertion and LIFO rollback trees equal a fresh Phase 2 evaluation at every node. */
RC_GTEST_PROP(IncrementalCaatEvaluatorPropertyTest, MatchesOfflineAcrossPushPopTree,
	      (const std::vector<std::uint8_t> &commands))
{
	auto analyzed = analyzeModel(R"CAT(RandomIncrementalRollback
let rec reach = po | (reach ; po)
acyclic reach
)CAT");
	RC_ASSERT(analyzed.model != nullptr);
	constexpr std::size_t size = 65;
	cat::BaseValues base{{"po", makeRelation(size, {})}};
	cat::IncrementalCaatEvaluator incremental(*analyzed.model, *analyzed.analysis);
	incremental.initialize(size, base);
	std::vector<std::pair<cat::IncrementalCheckpoint, cat::BaseValues>> stack;
	stack.emplace_back(incremental.checkpoint(), base);
	for (std::size_t index = 0; index < commands.size(); ++index) {
		if (commands[index] % 5 == 0 && stack.size() > 1) {
			stack.pop_back();
			base = stack.back().second;
			RC_ASSERT(incremental.rollback(stack.back().first).restored);
		} else if (commands[index] % 5 == 1) {
			stack.emplace_back(incremental.checkpoint(), base);
		} else if (index + 1 < commands.size()) {
			auto relation = std::get<cat::Relation>(base.at("po"));
			const auto from = commands[index] % size;
			const auto to = commands[++index] % size;
			relation.insert(from, to);
			base.at("po") = relation;
			RC_ASSERT(incremental.tryInsert(size, base).applied());
		}
		auto offline = cat::CaatEvaluator().evaluate(*analyzed.model, *analyzed.analysis,
							     size, base);
		RC_ASSERT(incremental.result().values == offline.values);
		RC_ASSERT(incremental.result().violations.size() == offline.violations.size());
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
let observed = reach
acyclic observed
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
