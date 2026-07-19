#include "genmc/Verification/RegionalRvfTransaction.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace {

using genmc::rvf::RegionState;
using genmc::rvf::RegionToken;
using genmc::rvf::RegionTransaction;

TEST(RegionalRvfTransaction, PublishesOnlyAfterEveryDescendantRetires)
{
	const RegionToken token{7, 3};
	RegionTransaction transaction(token);
	ASSERT_TRUE(transaction.tryAddDescendant(token));
	ASSERT_TRUE(transaction.tryAddDescendant(token));

	auto first = transaction.retireDescendant(token);
	EXPECT_TRUE(first.accepted);
	EXPECT_FALSE(first.publish);
	EXPECT_EQ(transaction.state(), RegionState::active);

	auto last = transaction.retireDescendant(token);
	ASSERT_TRUE(last.accepted);
	ASSERT_TRUE(last.publish);
	EXPECT_EQ(transaction.state(), RegionState::committed);
}

TEST(RegionalRvfTransaction, RevocationDiscardsResultsAndReplaysOnce)
{
	const RegionToken token{9, 1};
	RegionTransaction transaction(token);
	ASSERT_TRUE(transaction.tryAddDescendant(token));
	ASSERT_TRUE(transaction.tryAddDescendant(token));
	EXPECT_TRUE(transaction.retireDescendant(token).accepted);

	auto request = transaction.requestRevocation(token, "unsupported RMW");
	EXPECT_TRUE(request.accepted);
	EXPECT_FALSE(request.replayNative);
	EXPECT_EQ(transaction.revocationReason(), "unsupported RMW");

	auto last = transaction.retireDescendant(token);
	EXPECT_TRUE(last.accepted);
	EXPECT_TRUE(last.replayNative);
	EXPECT_FALSE(last.publish);
	EXPECT_EQ(transaction.state(), RegionState::revoked);
	EXPECT_FALSE(transaction.tryAddDescendant(token));
}

TEST(RegionalRvfTransaction, RejectsStaleEpochWithoutChangingOwnership)
{
	const RegionToken token{12, 4};
	const RegionToken stale{12, 3};
	RegionTransaction transaction(token);
	EXPECT_TRUE(transaction.isActive(token));
	EXPECT_FALSE(transaction.isActive(stale));
	EXPECT_FALSE(transaction.tryAddDescendant(stale));
	EXPECT_EQ(transaction.outstanding(), 0U);
	EXPECT_FALSE(transaction.requestRevocation(stale, "stale").accepted);

	ASSERT_TRUE(transaction.tryAddDescendant(token));
	auto completion = transaction.retireDescendant(stale);
	EXPECT_FALSE(completion.accepted);
	EXPECT_EQ(transaction.outstanding(), 1U);
	EXPECT_TRUE(transaction.isActive(token));
}

TEST(RegionalRvfTransaction, EmptyRegionRevokesImmediately)
{
	const RegionToken token{21, 8};
	RegionTransaction transaction(token);
	auto transition = transaction.requestRevocation(token, "entry rejected");
	EXPECT_TRUE(transition.accepted);
	EXPECT_TRUE(transition.replayNative);
	EXPECT_EQ(transaction.state(), RegionState::revoked);
}

TEST(RegionalRvfTransaction, ConcurrentLastRetirementAndRevocationChooseOneTerminalPath)
{
	for (auto repetition = 0; repetition < 100; ++repetition) {
		const RegionToken token{static_cast<std::uint64_t>(100 + repetition), 1};
		RegionTransaction transaction(token);
		ASSERT_TRUE(transaction.tryAddDescendant(token));
		std::atomic<bool> start{false};
		genmc::rvf::RegionTransition retirement;
		genmc::rvf::RegionTransition revocation;
		std::thread retiring([&] {
			while (!start.load(std::memory_order_acquire))
				;
			retirement = transaction.retireDescendant(token);
		});
		std::thread revoking([&] {
			while (!start.load(std::memory_order_acquire))
				;
			revocation = transaction.requestRevocation(token, "race");
		});
		start.store(true, std::memory_order_release);
		retiring.join();
		revoking.join();

		EXPECT_NE(retirement.publish, retirement.replayNative || revocation.replayNative);
		const auto state = transaction.state();
		EXPECT_TRUE(state == RegionState::committed || state == RegionState::revoked);
		EXPECT_EQ(state == RegionState::committed, retirement.publish);
	}
}

} // namespace
