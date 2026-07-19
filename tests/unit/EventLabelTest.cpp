#include <gtest/gtest.h>

#include "genmc/Execution/EventLabel.hpp"

TEST(EventLabelTest, EmptyDependenciesShareCanonicalStorage)
{
	OptionalLabel first(Event{0, 0});
	OptionalLabel second(Event{0, 1});

	EXPECT_TRUE(first.getDeps().addr.empty());
	EXPECT_EQ(&first.getDeps(), &second.getDeps());
}

TEST(EventLabelTest, NonEmptyDependenciesSurviveCloneAndReplacement)
{
	const Event dependency{1, 2};
	EventDeps deps;
	deps.data = DepInfo(dependency);
	OptionalLabel original(Event{0, 0}, deps);
	auto clone = original.clone();

	ASSERT_TRUE(original.getDeps().data.contains(dependency));
	EXPECT_EQ(&original.getDeps(), &clone->getDeps());

	clone->setDeps(EventDeps{});
	EXPECT_TRUE(clone->getDeps().data.empty());
	EXPECT_TRUE(original.getDeps().data.contains(dependency));
	EXPECT_NE(&original.getDeps(), &clone->getDeps());
}
