/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#ifndef GENMC_SC_READS_VALUE_FROM_FRAME_HPP
#define GENMC_SC_READS_VALUE_FROM_FRAME_HPP

#include "genmc/Execution/Event.hpp"
#include "genmc/Verification/SCExecutionGraphAdapter.hpp"
#include "genmc/Verification/SCReadsValueFromState.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace genmc::rvf {

/** State owned by one RVF-SMC recursion node and copied into its children. */
struct Frame {
	StableGoodWrites goodWrites{};
	/* Dense adapter IDs are prefix-local and may change when a parent continuation
	 * schedules another thread. Persist causal cutoffs by stable GenMC identity. */
	std::map<cat::StableEventKey, std::vector<std::uint32_t>> causalCutoffs{};
	std::vector<::Event> processedReads{};

	[[nodiscard]] auto processed(::Event read) const -> bool
	{
		return std::ranges::find(processedReads, read) != processedReads.end();
	}
};

} /* namespace genmc::rvf */

#endif /* GENMC_SC_READS_VALUE_FROM_FRAME_HPP */
