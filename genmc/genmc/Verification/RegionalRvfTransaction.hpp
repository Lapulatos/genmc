/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#ifndef GENMC_REGIONAL_RVF_TRANSACTION_HPP
#define GENMC_REGIONAL_RVF_TRANSACTION_HPP

#include <cstdint>
#include <mutex>
#include <string>

namespace genmc::rvf {

/** Stable capability carried by every task owned by one speculative region. */
struct RegionToken {
	std::uint64_t id{};
	std::uint64_t epoch{};

	auto operator<=>(const RegionToken &) const = default;
};

/** Pool-visible lifetime state of a speculative regional quotient. */
enum class RegionState : std::uint8_t { active, revoking, committed, revoked };

/** State transition produced when a task retires or revocation is requested. */
struct RegionTransition {
	bool accepted{};
	bool publish{};
	bool replayNative{};
};

/**
 * Synchronizes descendant ownership and isolates speculative task results.
 *
 * The native entry execution deliberately remains owned by ThreadPool: this class
 * decides exactly when it may be replayed without depending on GenMCDriver's nested
 * Execution type.
 */
class RegionTransaction {
public:
	explicit RegionTransaction(RegionToken token) : token_(token) {}

	RegionTransaction(const RegionTransaction &) = delete;
	RegionTransaction(RegionTransaction &&) = delete;
	auto operator=(const RegionTransaction &) -> RegionTransaction & = delete;
	auto operator=(RegionTransaction &&) -> RegionTransaction & = delete;

	/** Register a descendant before it becomes visible in a queue. */
	[[nodiscard]] auto tryAddDescendant(RegionToken token) -> bool;

	/** Retire one registered descendant and decide whether buffered pool state may publish. */
	[[nodiscard]] auto retireDescendant(RegionToken token) -> RegionTransition;

	/** Begin fail-open revocation. Native replay becomes available after the last retirement. */
	[[nodiscard]] auto requestRevocation(RegionToken token, std::string reason)
		-> RegionTransition;
	[[nodiscard]] auto isActive(RegionToken token) const -> bool;

	[[nodiscard]] auto token() const -> RegionToken { return token_; }
	[[nodiscard]] auto state() const -> RegionState;
	[[nodiscard]] auto outstanding() const -> std::uint64_t;
	[[nodiscard]] auto revocationReason() const -> std::string;

private:
	[[nodiscard]] auto matches(RegionToken token) const -> bool { return token == token_; }
	RegionToken token_;
	mutable std::mutex mutex_;
	RegionState state_{RegionState::active};
	std::uint64_t outstanding_{};
	std::string revocationReason_{};
};

} /* namespace genmc::rvf */

#endif /* GENMC_REGIONAL_RVF_TRANSACTION_HPP */
