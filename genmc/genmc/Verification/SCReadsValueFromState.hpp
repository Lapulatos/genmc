/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#ifndef GENMC_SC_READS_VALUE_FROM_STATE_HPP
#define GENMC_SC_READS_VALUE_FROM_STATE_HPP

#include "genmc/Verification/SCGoodWritesSolver.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace genmc::rvf {

/** Result of computing the writes visible to one read under program order. */
struct VisibleWritesResult {
	std::vector<EventId> writes{};
	std::string error{};
};

/** Compute the paper's exact VisibleW_PO(read) set for a proper event set. */
[[nodiscard]] auto visibleWrites(const std::vector<Event> &events, EventId read)
	-> VisibleWritesResult;

/** Per-read thread-prefix cutoffs used by RVF-SMC's causal map. */
class CausalMap {
public:
	CausalMap(std::size_t numEvents, std::size_t numThreads);
	/** Grow the event/thread universe while preserving every recorded cutoff. */
	[[nodiscard]] auto grow(std::size_t numEvents, std::size_t numThreads) -> bool;

	[[nodiscard]] auto isDefined(EventId read) const -> bool;
	[[nodiscard]] auto isForbidden(EventId read, const Event &write) const -> bool;
	[[nodiscard]] auto record(EventId read, std::span<const std::uint32_t> threadEventCounts)
		-> bool;
	[[nodiscard]] auto cutoffs(EventId read) const
		-> const std::optional<std::vector<std::uint32_t>> &;

private:
	std::size_t numThreads_{};
	std::vector<std::optional<std::vector<std::uint32_t>>> entries_{};
};

/** Rollback-scoped backtrack signals for reads in ancestor RVF recursion frames. */
class AncestorSignals {
public:
	struct Token {
		std::size_t depth{};
		EventId read{};
	};

	[[nodiscard]] auto push(EventId read, bool initial) -> Token;
	[[nodiscard]] auto observeWrite(const std::vector<Event> &events, EventId write) -> bool;
	[[nodiscard]] auto signal(const Token &token) const -> std::optional<bool>;
	[[nodiscard]] auto pop(const Token &token) -> std::optional<bool>;
	[[nodiscard]] auto size() const -> std::size_t { return entries_.size(); }

private:
	struct Entry {
		EventId read{};
		bool signal{};
	};

	std::vector<Entry> entries_{};
};

} /* namespace genmc::rvf */

#endif /* GENMC_SC_READS_VALUE_FROM_STATE_HPP */
