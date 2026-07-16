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

#include <chrono>
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
	const auto &candidateModel = conf->useCaatBackend ? conf->caatModel : nullptr;
	const auto certifiedCandidates =
		candidateModel &&
		candidateModel->certifiedCandidateProfile() == candidateModel->hostProfile();
	const auto certifiedAdaptiveOffline = candidateModel &&
					      candidateModel->certifiedAdaptiveOffline();
	if (conf->catPreventivePruning && certifiedAdaptiveOffline && !certifiedCandidates) {
		for (const auto &predicate : candidateModel->predicates()) {
			if (predicate.name == "reach") {
				preventiveOrderId_ = predicate.id;
				preventiveOrderNeedsClosure_ = false;
				break;
			}
			if (predicate.name == "order") {
				preventiveOrderId_ = predicate.id;
				preventiveOrderNeedsClosure_ = true;
			}
		}
		VERIFY(preventiveOrderId_, "certified preventive model lacks order relation");
	}
	/* Oracle mode deliberately enumerates the generic superset so mutation
	 * stress still compares every candidate with a fresh offline evaluation. */
	if (!conf->catOracle && certifiedCandidates)
		pruningHost_ = std::make_unique<HostChecker>(conf);
	if (!conf->useCaatBackend || !conf->caatModel || !conf->caatAnalysis)
		return;
	incrementalEvaluator_ = std::make_unique<cat::IncrementalCaatEvaluator>(
		*conf->caatModel, *conf->caatAnalysis, conf->catStats);
	if (incrementalEvaluator_->supportsInsertions()) {
		/* The explicit oracle option is intentionally independent of build mode. */
		const std::size_t oracleInterval = conf->catOracle ? 1 : 0;
		std::vector<std::string> requiredPrimitives;
		for (const auto &predicate : conf->caatModel->predicates()) {
			if (predicate.kind == cat::Predicate::Kind::Base)
				requiredPrimitives.push_back(predicate.name);
		}
		graphSynchronizer_ = std::make_unique<cat::GraphSynchronizer>(
			*incrementalEvaluator_, 32, oracleInterval, conf->catStats,
			std::move(requiredPrimitives),
			!conf->catOracle && certifiedAdaptiveOffline ? 512 : 0);
	}
}

template <typename HostChecker> BasicCATChecker<HostChecker>::~BasicCATChecker()
{
	if (!this->getConf()->catStats || !graphSynchronizer_)
		return;
	const auto &stats = graphSynchronizer_->statistics();
	const auto &evaluatorStats = incrementalEvaluator_->statistics();
	std::ostringstream line;
	line << "CAT incremental statistics: initialize=" << stats.initializations
	     << " unchanged=" << stats.unchanged << " insert=" << stats.insertions
	     << " rollback=" << stats.rollbacks << " rollback-insert=" << stats.rollbackInsertions
	     << " replace=" << stats.replacements << " rebuild=" << stats.rebuilds
	     << " evicted=" << stats.evictedCheckpoints << " oracle=" << stats.oracleChecks
	     << " adaptive-offline=" << stats.adaptiveOfflineSelections
	     << " eval-ops=" << evaluatorStats.operationEvaluations
	     << " value-changes=" << evaluatorStats.valueChanges
	     << " queue-pushes=" << evaluatorStats.worklistPushes
	     << " offline-evals=" << evaluatorStats.offlineEvaluations
	     << " adapter-ns=" << adapterNanoseconds_ << " sync-ns=" << synchronizationNanoseconds_
	     << " offline-ns=" << evaluatorStats.offlineNanoseconds
	     << " copy-ns=" << evaluatorStats.transactionalCopyNanoseconds
	     << " worklist-ns=" << evaluatorStats.worklistNanoseconds
	     << " checkpoint-ns=" << evaluatorStats.checkpointNanoseconds
	     << " rollback-ns=" << evaluatorStats.rollbackNanoseconds
	     << " retained-undo-bytes=" << evaluatorStats.retainedUndoBytes
	     << " peak-undo-bytes=" << evaluatorStats.peakRetainedUndoBytes
	     << " retained-snapshot-equivalent-bytes="
	     << evaluatorStats.retainedSnapshotEquivalentBytes << " peak-snapshot-equivalent-bytes="
	     << evaluatorStats.peakRetainedSnapshotEquivalentBytes
	     << " materialize-ns=" << stats.materializeNanoseconds
	     << " equality-ns=" << stats.equalityNanoseconds
	     << " insert-attempt-ns=" << stats.insertionAttemptNanoseconds
	     << " history-ns=" << stats.historySearchNanoseconds
	     << " rebuild-ns=" << stats.rebuildNanoseconds
	     << " max-active-events=" << stats.maximumActiveEvents
	     << " max-stable-events=" << stats.maximumStableEvents
	     << " max-inactive-events=" << stats.maximumInactiveEvents
	     << " max-current-base-bytes=" << stats.maximumCurrentBaseBytes
	     << " max-history-base-bytes=" << stats.maximumHistoryBaseBytes
	     << " max-base-relation-pairs=" << stats.maximumBaseRelationPairs
	     << " max-base-relation-density-ppm=" << stats.maximumBaseRelationDensityPpm
	     << " profiled-queries=" << profiledQueries_
	     << " preventive-prefix-queries=" << preventivePrefixQueries_
	     << " preventive-prefix-inconsistent=" << preventivePrefixInconsistent_
	     << " preventive-rf-candidates=" << preventiveRfCandidates_
	     << " preventive-rf-pruned=" << preventiveRfPruned_
	     << " preventive-co-candidates=" << preventiveCoCandidates_
	     << " preventive-co-pruned=" << preventiveCoPruned_
	     << " preventive-all-pruned-fallbacks=" << preventiveAllPrunedFallbacks_
	     << " preventive-lookup-ns=" << preventiveLookupNanoseconds_;
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
	const auto profile = this->getConf()->catStats;
	if (this->getConf()->useCaatBackend && graphSynchronizer_) {
		const auto syncStart = profile ? std::chrono::steady_clock::now()
					       : std::chrono::steady_clock::time_point{};
		(void)graphSynchronizer_->synchronize(graph);
		if (profile) {
			synchronizationNanoseconds_ += static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - syncStart)
					.count());
			++profiledQueries_;
		}
		const auto &result = incrementalEvaluator_->result();
		VERIFY(result.errors.empty(), "validated incremental CAAT evaluation failed");
		if (this->getConf()->explainCat && !result.violations.empty()) {
			auto explained = cat::Reasoner().explain(
				*caatModel, *caatAnalysis, incrementalEvaluator_->eventCount(),
				result.values, result.violations);
			VERIFY(explained.ok(), "validated CAT violation explanation failed");
			for (const auto &violation : explained.violations)
				std::cerr << "CAT explanation: " << cat::Reasoner::format(violation)
					  << '\n';
		}
		return result.violations.empty();
	}
	const auto adapterStart = profile ? std::chrono::steady_clock::now()
					  : std::chrono::steady_clock::time_point{};
	const cat::GraphAdapter adapter(graph);
	if (profile) {
		adapterNanoseconds_ += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - adapterStart)
				.count());
		++profiledQueries_;
	}
#ifdef ENABLE_GENMC_DEBUG
	const auto invariantErrors = adapter.validate();
	VERIFY(invariantErrors.empty(), "invalid CAT graph adapter state");
#endif
	std::vector<cat::Violation> violations;
	std::vector<std::optional<cat::Value>> caatValues;
	std::size_t caatEventCount = adapter.eventCount();
	if (this->getConf()->useCaatBackend) {
		if (graphSynchronizer_) {
			const auto syncStart = profile ? std::chrono::steady_clock::now()
						       : std::chrono::steady_clock::time_point{};
			(void)graphSynchronizer_->synchronize(adapter);
			if (profile)
				synchronizationNanoseconds_ += static_cast<std::uint64_t>(
					std::chrono::duration_cast<std::chrono::nanoseconds>(
						std::chrono::steady_clock::now() - syncStart)
						.count());
			const auto &result = incrementalEvaluator_->result();
			VERIFY(result.errors.empty(),
			       "validated incremental CAAT evaluation failed");
			violations = result.violations;
			/* Predicate values are only consumed by the opt-in explanation path.
			 * Avoid copying every fixed-point relation for ordinary consistency
			 * queries, which otherwise turns a read-only result lookup into an
			 * O(total predicate state) operation. */
			if (this->getConf()->explainCat)
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
auto BasicCATChecker<HostChecker>::preparePreventivePrefix(const ExecutionGraph &graph)
	-> const cat::Relation *
{
	if (!preventiveOrderId_ || !graphSynchronizer_ || !incrementalEvaluator_)
		return nullptr;
	++preventivePrefixQueries_;
	const auto profile = this->getConf()->catStats;
	const auto started = profile ? std::chrono::steady_clock::now()
				     : std::chrono::steady_clock::time_point{};
	(void)graphSynchronizer_->synchronize(graph);
	if (profile) {
		synchronizationNanoseconds_ += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started)
				.count());
		++profiledQueries_;
	}
	const auto &result = incrementalEvaluator_->result();
	VERIFY(result.errors.empty(), "preventive prefix evaluation failed");
	if (!result.violations.empty()) {
		++preventivePrefixInconsistent_;
		return nullptr;
	}
	const auto &value = result.values.at(*preventiveOrderId_);
	VERIFY(value.has_value(), "preventive order is not evaluated");
	const auto *order = std::get_if<cat::Relation>(&*value);
	VERIFY(order, "preventive order has unexpected type");
	if (!preventiveOrderNeedsClosure_)
		return order;
	/* Cycle-only closure slicing removes the published `reach=order+` value.
	 * Preventive pruning is an extra-model reachability consumer, so reconstruct
	 * that exact finite closure only when the opt-in pruning path requests it. */
	preventiveReachCache_ = cat::transitiveClosure(*order);
	return &*preventiveReachCache_;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::preventsRf(const ReadLabel &read, const EventLabel &source,
					      const cat::Relation &reach) const -> bool
{
	const auto &stable = graphSynchronizer_->stableAdapter();
	const auto readId = stable.id(cat::StableEventKey{read.getPos()});
	const auto sourceId = genmc::isa<InitLabel>(&source)
				      ? stable.id(cat::StableEventKey{read.getAddr()})
				      : stable.id(cat::StableEventKey{source.getPos()});
	VERIFY(readId && sourceId, "preventive RF endpoint lacks stable identity");
	/* Recursive PSO contributes only external RF directly to order. */
	if ((genmc::isa<InitLabel>(&source) || source.getThread() != read.getThread()) &&
	    reach.contains(*readId, *sourceId))
		return true;

	const auto &graph = *read.getParent();
	if (genmc::isa<InitLabel>(&source)) {
		for (const auto &successor : graph.co(read.getAddr())) {
			const auto successorId = stable.id(cat::StableEventKey{successor.getPos()});
			VERIFY(successorId, "preventive FR target lacks stable identity");
			if (reach.contains(*successorId, *readId))
				return true;
		}
		return false;
	}
	for (const auto &successor : graph.co_succs(&source)) {
		const auto successorId = stable.id(cat::StableEventKey{successor.getPos()});
		VERIFY(successorId, "preventive FR target lacks stable identity");
		if (reach.contains(*successorId, *readId))
			return true;
	}
	return false;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::preventsCo(const WriteLabel &write,
					      const EventLabel &predecessor,
					      const cat::Relation &reach) const -> bool
{
	const auto &graph = *write.getParent();
	const auto &stable = graphSynchronizer_->stableAdapter();
	const auto writeId = stable.id(cat::StableEventKey{write.getPos()});
	VERIFY(writeId, "preventive CO write lacks stable identity");
	std::vector<const EventLabel *> ordered{graph.getInitLabel()};
	for (const auto &existing : graph.co(write.getAddr()))
		ordered.push_back(&existing);
	const auto predecessorIt = std::ranges::find(ordered, &predecessor);
	VERIFY(predecessorIt != ordered.end(), "preventive CO predecessor is not ordered");
	const auto predecessorIndex =
		static_cast<std::size_t>(std::distance(ordered.begin(), predecessorIt));
	const auto stableId = [&](const EventLabel &label) {
		const auto id = genmc::isa<InitLabel>(&label)
					? stable.id(cat::StableEventKey{write.getAddr()})
					: stable.id(cat::StableEventKey{label.getPos()});
		VERIFY(id, "preventive CO endpoint lacks stable identity");
		return *id;
	};

	/* Strict total CO adds every earlier->write and write->later pair. */
	for (std::size_t index = 0; index < ordered.size(); ++index) {
		const auto otherId = stableId(*ordered[index]);
		if (index <= predecessorIndex) {
			if (reach.contains(*writeId, otherId))
				return true;
		} else if (reach.contains(otherId, *writeId)) {
			return true;
		}
	}
	/* Reads from an earlier source gain FR to the newly inserted write. */
	for (std::size_t index = 0; index <= predecessorIndex; ++index) {
		if (const auto *initial = genmc::dyn_cast<InitLabel>(ordered[index])) {
			for (const auto &reader : initial->rfs(write.getAddr())) {
				const auto readerId =
					stable.id(cat::StableEventKey{reader.getPos()});
				VERIFY(readerId, "preventive FR reader lacks stable identity");
				if (reach.contains(*writeId, *readerId))
					return true;
			}
		} else {
			const auto *earlier = genmc::cast<WriteLabel>(ordered[index]);
			for (const auto &reader : earlier->readers()) {
				const auto readerId =
					stable.id(cat::StableEventKey{reader.getPos()});
				VERIFY(readerId, "preventive FR reader lacks stable identity");
				if (reach.contains(*writeId, *readerId))
					return true;
			}
		}
	}
	return false;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::getCoherentStores(ReadLabel *read) -> std::vector<EventLabel *>
{
	if (pruningHost_) {
		ConsistencyChecker &host = *pruningHost_;
		return host.getCoherentStores(read);
	}
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
	/* With zero/one offered source, filtering cannot shrink the search: an empty
	 * result would immediately take the all-pruned fallback. More importantly,
	 * avoid materializing recursive reach for large deterministic prefixes. */
	if (!read->getRf() && result.size() > 1) {
		if (const auto *reach = preparePreventivePrefix(graph)) {
			const auto original = result;
			preventiveRfCandidates_ += result.size();
			const auto started = std::chrono::steady_clock::now();
			std::erase_if(result, [&](const auto *source) {
				const auto prune = preventsRf(*read, *source, *reach);
				preventiveRfPruned_ += prune ? 1U : 0U;
				return prune;
			});
			preventiveLookupNanoseconds_ += static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - started)
					.count());
			if (result.empty()) {
				++preventiveAllPrunedFallbacks_;
				result = original;
			}
		}
	}
	return result;
}

template <typename HostChecker>
void BasicCATChecker<HostChecker>::filterCoherentRevisits(WriteLabel *write,
							  std::vector<ReadLabel *> &reads)
{
	if (pruningHost_) {
		ConsistencyChecker &host = *pruningHost_;
		host.filterCoherentRevisits(write, reads);
		return;
	}
	/* No generic CAT theorem currently justifies removing a revisit. */
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::getCoherentPlacings(WriteLabel *write)
	-> std::vector<EventLabel *>
{
	if (pruningHost_) {
		ConsistencyChecker &host = *pruningHost_;
		return host.getCoherentPlacings(write);
	}
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
	/* A sole placement is likewise unfilterable under the safety fallback. */
	if (!write->isInCo() && result.size() > 1) {
		if (const auto *reach = preparePreventivePrefix(graph)) {
			const auto original = result;
			preventiveCoCandidates_ += result.size();
			const auto started = std::chrono::steady_clock::now();
			std::erase_if(result, [&](const auto *predecessor) {
				const auto prune = preventsCo(*write, *predecessor, *reach);
				preventiveCoPruned_ += prune ? 1U : 0U;
				return prune;
			});
			preventiveLookupNanoseconds_ += static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - started)
					.count());
			if (result.empty()) {
				++preventiveAllPrunedFallbacks_;
				result = original;
			}
		}
	}
	return result;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::shouldReportCoherenceWarning(
	WriteLabel *write, const std::vector<EventLabel *> &placements) -> bool
{
	if (pruningHost_) {
		ConsistencyChecker &host = *pruningHost_;
		return host.shouldReportCoherenceWarning(write, placements);
	}
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
