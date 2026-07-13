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

#include "genmc/Execution/Consistency/CATChecker.hpp"

#include "genmc/CAT/Evaluator.hpp"
#include "genmc/CAT/GraphAdapter.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"
#include "genmc/Support/Error.hpp"
#include "genmc/Verification/Config.hpp"

#include <ranges>

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::isConsistent(const EventLabel *lab) const -> bool
{
	VERIFY(lab && lab->getParent(), "CAT consistency requires a graph-owned label");
	return isConsistent(*lab->getParent());
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::isConsistent(const ExecutionGraph &graph) const -> bool
{
	const auto &model = this->getConf()->catModel;
	VERIFY(model, "CATChecker requires a validated CAT model");
	const cat::GraphAdapter adapter(graph);
#ifdef ENABLE_GENMC_DEBUG
	const auto invariantErrors = adapter.validate();
	VERIFY(invariantErrors.empty(), "invalid CAT graph adapter state");
#endif
	const auto result =
		cat::Evaluator().evaluate(*model, adapter.eventCount(), adapter.baseValues());
	VERIFY(result.errors.empty(), "validated CAT evaluation failed");
	return result.violations.empty();
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::getCoherentStores(ReadLabel *read) -> std::vector<EventLabel *>
{
	VERIFY(read && read->getParent(), "rf enumeration requires a graph-owned read");
	auto &graph = *read->getParent();
	std::vector<EventLabel *> result{graph.getInitLabel()};
	for (auto &write : graph.co(read->getAddr()))
		result.push_back(&write);
	return result;
}

template <typename HostChecker>
void BasicCATChecker<HostChecker>::filterCoherentRevisits(WriteLabel * /*write*/,
							  std::vector<ReadLabel *> & /*reads*/)
{
	/* No generic CAT theorem currently justifies removing a revisit. */
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::getCoherentPlacings(WriteLabel *write)
	-> std::vector<EventLabel *>
{
	VERIFY(write && write->getParent(), "co enumeration requires a graph-owned write");
	auto &graph = *write->getParent();

	/* A successful split-label RMW must be placed immediately after its rf source. */
	if (write->isRMW()) {
		auto *read = genmc::dyn_cast<ReadLabel>(graph.po_imm_pred(write));
		VERIFY(read && read->getRf(), "RMW write requires an adjacent read with rf");
		return {read->getRf()};
	}

	/* Every existing write denotes the position immediately after it; Init is first. */
	std::vector<EventLabel *> result{graph.getInitLabel()};
	for (auto &predecessor : graph.co(write->getAddr()))
		result.push_back(&predecessor);
	return result;
}

template class BasicCATChecker<SCChecker>;
template class BasicCATChecker<TSOChecker>;
