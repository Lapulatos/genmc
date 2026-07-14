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

#include "genmc/CAT/CaatEvaluator.hpp"
#include "genmc/CAT/Evaluator.hpp"
#include "genmc/CAT/GraphAdapter.hpp"
#include "genmc/CAT/Reasoner.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"
#include "genmc/Support/Error.hpp"
#include "genmc/Verification/Config.hpp"

#include <iostream>
#include <mutex>
#include <ranges>
#include <sstream>

namespace {

/** Serialize opt-in statistics emitted by independently destroyed workers. */
std::mutex catStatisticsMutex;

} /* namespace */

template <typename HostChecker>
BasicCATChecker<HostChecker>::BasicCATChecker(const Config *conf) : HostChecker(conf)
{
	VERIFY(conf, "CAT checker requires configuration");
	if (!conf->useCaatBackend || !conf->caatModel || !conf->caatAnalysis)
		return;
	incrementalEvaluator_ = std::make_unique<cat::IncrementalCaatEvaluator>(
		*conf->caatModel, *conf->caatAnalysis);
	if (incrementalEvaluator_->supportsInsertions())
		graphSynchronizer_ =
			std::make_unique<cat::GraphSynchronizer>(*incrementalEvaluator_);
}

template <typename HostChecker> BasicCATChecker<HostChecker>::~BasicCATChecker()
{
	if (!this->getConf()->catStats || !graphSynchronizer_)
		return;
	const auto &stats = graphSynchronizer_->statistics();
	std::ostringstream line;
	line << "CAT incremental statistics: initialize=" << stats.initializations
	     << " unchanged=" << stats.unchanged << " insert=" << stats.insertions
	     << " rollback=" << stats.rollbacks << " rollback-insert=" << stats.rollbackInsertions
	     << " rebuild=" << stats.rebuilds << " evicted=" << stats.evictedCheckpoints;
	/* A process-wide lock keeps worker records parseable under --nthreads. */
	const std::lock_guard lock(catStatisticsMutex);
	std::cerr << line.str() << '\n';
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::incrementalStatistics() const
	-> const cat::GraphSynchronizationStatistics *
{
	return graphSynchronizer_ ? &graphSynchronizer_->statistics() : nullptr;
}

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
	const auto &caatModel = this->getConf()->caatModel;
	const auto &caatAnalysis = this->getConf()->caatAnalysis;
	VERIFY(model || (caatModel && caatAnalysis),
	       "CATChecker requires a validated CAT or CAAT model");
	const cat::GraphAdapter adapter(graph);
#ifdef ENABLE_GENMC_DEBUG
	const auto invariantErrors = adapter.validate();
	VERIFY(invariantErrors.empty(), "invalid CAT graph adapter state");
#endif
	std::vector<cat::Violation> violations;
	std::vector<std::optional<cat::Value>> caatValues;
	std::size_t caatEventCount = adapter.eventCount();
	if (this->getConf()->useCaatBackend) {
		if (graphSynchronizer_) {
			(void)graphSynchronizer_->synchronize(adapter);
			const auto &result = incrementalEvaluator_->result();
			VERIFY(result.errors.empty(),
			       "validated incremental CAAT evaluation failed");
			violations = result.violations;
			caatValues = result.values;
			caatEventCount = incrementalEvaluator_->eventCount();
		} else {
			auto result = cat::CaatEvaluator().evaluate(*caatModel, *caatAnalysis,
								    adapter.eventCount(),
								    adapter.baseValues());
			VERIFY(result.errors.empty(), "validated CAAT evaluation failed");
			violations = std::move(result.violations);
			caatValues = std::move(result.values);
		}
	} else {
		auto result = cat::Evaluator().evaluate(*model, adapter.eventCount(),
							adapter.baseValues());
		VERIFY(result.errors.empty(), "validated CAT evaluation failed");
		violations = std::move(result.violations);
	}
	if (this->getConf()->explainCat && !violations.empty()) {
		VERIFY(caatModel && caatAnalysis, "CAT explanation requires validated CAAT IR");
		if (caatValues.empty()) {
			auto evaluated = cat::CaatEvaluator().evaluate(*caatModel, *caatAnalysis,
								       adapter.eventCount(),
								       adapter.baseValues());
			VERIFY(evaluated.errors.empty(),
			       "validated CAAT explanation evaluation failed");
			caatValues = std::move(evaluated.values);
			violations = std::move(evaluated.violations);
		}
		auto explained = cat::Reasoner().explain(*caatModel, *caatAnalysis, caatEventCount,
							 caatValues, violations);
		VERIFY(explained.ok(), "validated CAT violation explanation failed");
		/* One line per violation minimizes interleaving when explicitly enabled
		 * during multi-worker exploration. */
		for (const auto &violation : explained.violations)
			std::cerr << "CAT explanation: " << cat::Reasoner::format(violation)
				  << '\n';
	}
	return violations.empty();
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::getCoherentStores(ReadLabel *read) -> std::vector<EventLabel *>
{
	VERIFY(read && read->getParent(), "rf enumeration requires a graph-owned read");
	auto &graph = *read->getParent();
	std::vector<EventLabel *> result;
	/* Dynamic storage has no address-polymorphic C initializer. Its initialized
	 * bytes are represented by real allocation/library writes; offering Init as
	 * an rf source can fabricate initialized heap data and violates GenMC's NA
	 * access invariant when the value is read. */
	if (read->getAddr().isStatic())
		result.push_back(graph.getInitLabel());
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

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::shouldReportCoherenceWarning(
	WriteLabel *write, const std::vector<EventLabel *> & /*placements*/) -> bool
{
	/* Raw generic candidates include choices that the CAT model will reject.
	 * Recompute only the host's proven candidate range for the diagnostic; this
	 * preserves real unordered-write warnings without using host pruning to
	 * remove executions from the generic CAT search. */
	HostChecker diagnosticHost(this->getConf());
	ConsistencyChecker &hostInterface = diagnosticHost;
	return hostInterface.getCoherentPlacings(write).size() > 1;
}

template class BasicCATChecker<SCChecker>;
template class BasicCATChecker<TSOChecker>;
