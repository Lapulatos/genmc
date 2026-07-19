/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/CAT/ConflictCore.hpp"

#include <gtest/gtest.h>

namespace {

auto baseModel() -> cat::NormalizedModel
{
	std::vector<cat::Predicate> predicates;
	predicates.push_back({.kind = cat::Predicate::Kind::Base,
			      .type = cat::ValueType::Relation,
			      .name = "po"});
	predicates.push_back({.kind = cat::Predicate::Kind::Base,
			      .type = cat::ValueType::Relation,
			      .name = "rf"});
	return cat::NormalizedModel("cores", cat::HostProfile::SC, std::move(predicates), {});
}

TEST(ConflictCoreDatabase, MatchesOnlyCompleteExactConjunction)
{
	auto model = baseModel();
	std::vector<std::optional<cat::Value>> values(model.predicates().size());
	cat::Relation po(3);
	po.insert(0, 1);
	values[0] = po;
	values[1] = cat::Relation(3);
	cat::ConflictCoreDatabase database;
	ASSERT_TRUE(database.learn({{0, 0, 1, false}, {1, 2, 0, false}}));
	EXPECT_FALSE(database.matches(model, values, {}));
	std::vector<cat::ConflictLiteral> proposed{{1, 2, 0, false}};
	EXPECT_TRUE(database.matches(model, values, proposed));
	EXPECT_EQ(database.statistics().hits, 1U);
}

TEST(ConflictCoreDatabase, MinimizesSubsumedClausesAndRejectsUnsupported)
{
	cat::ConflictCoreDatabase database;
	EXPECT_FALSE(database.learn({}));
	ASSERT_TRUE(database.learn({{0, 0, 1, false}, {1, 2, 0, false}}));
	ASSERT_TRUE(database.learn({{0, 0, 1, false}}));
	EXPECT_EQ(database.size(), 1U);
	EXPECT_FALSE(database.learn({{0, 0, 1, false}, {1, 1, 2, false}}));
	EXPECT_EQ(database.statistics().learned, 2U);
	EXPECT_EQ(database.statistics().duplicateOrSubsumed, 2U);
}

} /* namespace */
