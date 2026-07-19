/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/Verification/RegionalRvfTransaction.hpp"

#include "genmc/Support/Error.hpp"

namespace genmc::rvf {

auto RegionTransaction::tryAddDescendant(RegionToken token) -> bool
{
	const std::lock_guard lock(mutex_);
	if (!matches(token) || state_ != RegionState::active)
		return false;
	++outstanding_;
	return true;
}

auto RegionTransaction::retireDescendant(RegionToken token) -> RegionTransition
{
	const std::lock_guard lock(mutex_);
	if (!matches(token) || state_ == RegionState::committed || state_ == RegionState::revoked)
		return {};
	VERIFY(outstanding_ > 0, "retiring an unregistered regional descendant");
	--outstanding_;
	if (state_ == RegionState::revoking) {
		if (outstanding_ != 0)
			return {.accepted = true};
		state_ = RegionState::revoked;
		return {.accepted = true, .replayNative = true};
	}

	if (outstanding_ != 0)
		return {.accepted = true};
	state_ = RegionState::committed;
	return {.accepted = true, .publish = true};
}

auto RegionTransaction::requestRevocation(RegionToken token, std::string reason)
	-> RegionTransition
{
	const std::lock_guard lock(mutex_);
	if (!matches(token) || state_ == RegionState::committed || state_ == RegionState::revoked)
		return {};
	if (state_ == RegionState::active) {
		state_ = RegionState::revoking;
		revocationReason_ = std::move(reason);
	}
	if (outstanding_ != 0)
		return {.accepted = true};
	state_ = RegionState::revoked;
	return {.accepted = true, .replayNative = true};
}

auto RegionTransaction::isActive(RegionToken token) const -> bool
{
	const std::lock_guard lock(mutex_);
	return matches(token) && state_ == RegionState::active;
}

auto RegionTransaction::state() const -> RegionState
{
	const std::lock_guard lock(mutex_);
	return state_;
}

auto RegionTransaction::outstanding() const -> std::uint64_t
{
	const std::lock_guard lock(mutex_);
	return outstanding_;
}

auto RegionTransaction::revocationReason() const -> std::string
{
	const std::lock_guard lock(mutex_);
	return revocationReason_;
}

} /* namespace genmc::rvf */
