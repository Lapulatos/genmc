#include <gtest/gtest.h>

#include "genmc/Verification/Revisit.hpp"

TEST(RevisitTest, StoresPlainViewInlineWithoutChangingContents)
{
	auto view = std::make_unique<View>();
	view->updateIdx(Event{2, 7});
	view->updateIdx(Event{4, 3});

	auto revisit = BackwardRevisit::create(Event{1, 5}, Event{3, 8}, std::move(view));

	ASSERT_NE(revisit, nullptr);
	EXPECT_EQ(revisit->getPos(), (Event{1, 5}));
	EXPECT_EQ(revisit->getRev(), (Event{3, 8}));
	ASSERT_EQ(revisit->getViewNoRel()->getKind(), VectorClock::VC_View);
	EXPECT_TRUE(revisit->getViewNoRel()->contains(Event{2, 7}));
	EXPECT_TRUE(revisit->getViewNoRel()->contains(Event{4, 3}));
	EXPECT_LT(sizeof(TypedBackwardRevisit<View>),
		  sizeof(BackwardRevisit) + sizeof(View) + 2 * sizeof(void *));
}

TEST(RevisitTest, StoresDependencyViewInlineWithoutChangingHoles)
{
	auto view = std::make_unique<DepView>();
	view->updateIdx(Event{1, 4});
	view->removeHole(Event{1, 2});

	auto revisit = BackwardRevisit::create(Event{0, 3}, Event{2, 6}, std::move(view));

	ASSERT_NE(revisit, nullptr);
	ASSERT_EQ(revisit->getViewNoRel()->getKind(), VectorClock::VC_DepView);
	auto *saved = genmc::dyn_cast<DepView>(revisit->getViewNoRel());
	ASSERT_NE(saved, nullptr);
	EXPECT_TRUE(saved->contains(Event{1, 2}));
	EXPECT_FALSE(saved->contains(Event{1, 3}));
	EXPECT_LT(sizeof(TypedBackwardRevisit<DepView>),
		  sizeof(BackwardRevisit) + sizeof(DepView) + 2 * sizeof(void *));
}
