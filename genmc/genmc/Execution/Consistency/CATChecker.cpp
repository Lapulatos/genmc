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
#include "genmc/CAT/LazyCycle.hpp"
#include "genmc/CAT/Reasoner.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"
#include "genmc/Support/Error.hpp"
#include "genmc/Verification/Config.hpp"

#include <chrono>
#include <iostream>
#include <limits>
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
	const auto enableLazyCycles = !conf->explainCat && !conf->catOracle;
	std::optional<cat::PredicateId> retainedLazyRoot;
	if (conf->catPreventivePruning && conf->caatAnalysis) {
		const auto &orders = conf->caatAnalysis->preventiveOrders();
		if (!orders.empty()) {
			/* One sufficient checked order preserves soundness and avoids replaying
			 * multiple lazy edge programs at every choice. Prefer the structurally
			 * narrower external-RF seed; ties retain source check order. */
			const auto rank = [](const auto &certificate) {
				return certificate.rfMode == cat::PreventiveRfMode::External ? 0 :
				       certificate.rfMode == cat::PreventiveRfMode::None     ? 1 :
									      2;
			};
			const auto selected = std::ranges::min_element(
				orders, {}, [&](const auto &certificate) { return rank(certificate); });
			preventiveOrders_.push_back(*selected);
			const auto directV9 = enableLazyCycles &&
				      conf->caatAnalysis->lazyCycleElided()[selected->order];
			const auto directV11 = enableLazyCycles && conf->catFocusReach &&
				       selected->lazyReachSupported &&
				       (selected->unassignedReadSink || selected->unplacedWriteSink);
			if (directV9 || directV11) {
				/* The exact lazy interpreter can evaluate this root from its primitive
				 * cone without publishing unrelated predicates or mutating the
				 * authoritative consistency state. */
				std::vector<std::string> requiredPrimitives;
				std::vector<bool> seen(candidateModel->predicates().size());
				std::vector<cat::PredicateId> pending{selected->order};
				while (!pending.empty()) {
					const auto id = pending.back();
					pending.pop_back();
					if (seen[id])
						continue;
					seen[id] = true;
					const auto &predicate = candidateModel->predicates()[id];
					if (predicate.kind == cat::Predicate::Kind::Base) {
						preventiveBasePredicates_.push_back(id);
						requiredPrimitives.push_back(predicate.name);
						if (predicate.name == "rf")
							preventiveRfPredicate_ = id;
						else if (predicate.name == "fr")
							preventiveFrPredicate_ = id;
						else if (predicate.name == "co")
							preventiveCoPredicate_ = id;
						continue;
					}
					pending.insert(pending.end(), predicate.operands.begin(),
						       predicate.operands.end());
				}
				if (!requiredPrimitives.empty())
					preventiveAdapter_ = std::make_unique<cat::StableGraphAdapter>(
						std::move(requiredPrimitives));
				else
					retainedLazyRoot = selected->order;
			}
		}
	}
	if (conf->catConflictCores && preventiveAdapter_)
		conflictCores_ = std::make_unique<cat::ConflictCoreDatabase>();
	/* Oracle mode deliberately enumerates the generic superset so mutation
	 * stress still compares every candidate with a fresh offline evaluation. */
	if (!conf->catOracle && certifiedCandidates)
		pruningHost_ = std::make_unique<HostChecker>(conf);
	if (!conf->useCaatBackend || !conf->caatModel || !conf->caatAnalysis)
		return;
	incrementalEvaluator_ = std::make_unique<cat::IncrementalCaatEvaluator>(
		*conf->caatModel, *conf->caatAnalysis, conf->catStats,
		enableLazyCycles, enableLazyCycles ? retainedLazyRoot : std::nullopt,
		conf->catFastChecks, conf->catFastComposition, conf->catFastCycleChecks);
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
			!conf->catOracle && !conf->catDisableAdaptiveOffline &&
					certifiedAdaptiveOffline
				? 512
				: 0,
			conf->catPrimitiveCache, conf->catFastPrimitiveBuild,
			conf->catFastCoherenceBuild, conf->catFastDescriptorBuild,
			conf->catFastDescriptorReuse);
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
	     << " primitive-cache-hits=" << graphSynchronizer_->stableAdapter().cacheHits()
	     << " primitive-cache-misses=" << graphSynchronizer_->stableAdapter().cacheMisses()
	     << " primitive-cache-oracle="
	     << graphSynchronizer_->stableAdapter().cacheOracleChecks()
	     << " primitive-scan-ns=" << graphSynchronizer_->stableAdapter().scanNanoseconds()
	     << " primitive-label-ns=" << graphSynchronizer_->stableAdapter().labelNanoseconds()
	     << " primitive-structural-ns="
	     << graphSynchronizer_->stableAdapter().structuralNanoseconds()
	     << " primitive-coherence-ns="
	     << graphSynchronizer_->stableAdapter().coherenceNanoseconds()
	     << " primitive-co-edges-ns="
	     << graphSynchronizer_->stableAdapter().coherenceEdgeNanoseconds()
	     << " primitive-fr-ns=" << graphSynchronizer_->stableAdapter().fromReadNanoseconds()
	     << " primitive-pack-ns="
	     << graphSynchronizer_->stableAdapter().relationPackNanoseconds()
	     << " primitive-assembly-ns="
	     << graphSynchronizer_->stableAdapter().assemblyNanoseconds()
	     << " insertion-rejections=" << stats.insertionRejections
	     << " history-entries-examined=" << stats.historyEntriesExamined
	     << " history-subset-matches=" << stats.historySubsetMatches
	     << " history-rollback-attempts=" << stats.historyRollbackAttempts
	     << " history-rollback-failures=" << stats.historyRollbackFailures
	     << " history-advance-attempts=" << stats.historyAdvanceAttempts
	     << " history-advance-failures=" << stats.historyAdvanceFailures
	     << " replacement-rejections=" << stats.replacementRejections
	     << " eval-ops=" << evaluatorStats.operationEvaluations
	     << " value-changes=" << evaluatorStats.valueChanges
	     << " queue-pushes=" << evaluatorStats.worklistPushes
	     << " lazy-cycle-checks=" << evaluatorStats.lazyCycleChecks
	     << " lazy-edge-candidates=" << evaluatorStats.lazyEdgeCandidates
	     << " lazy-base-candidates=" << evaluatorStats.lazyBaseCandidates
	     << " lazy-depth-fallbacks=" << evaluatorStats.lazyDepthFallbacks
	     << " offline-evals=" << evaluatorStats.offlineEvaluations
	     << " adapter-ns=" << adapterNanoseconds_ << " sync-ns=" << synchronizationNanoseconds_
	     << " consistency-total-ns=" << consistencyNanoseconds_
	     << " consistency-accepted=" << consistentQueries_
	     << " consistency-rejected=" << inconsistentQueries_
	     << " offline-ns=" << evaluatorStats.offlineNanoseconds
	     << " offline-evaluation-ns=" << evaluatorStats.offlineEvaluationNanoseconds
	     << " initialization-base-copy-ns="
	     << evaluatorStats.initializationBaseCopyNanoseconds
	     << " offline-init-ns=" << evaluatorStats.offlineInitializationNanoseconds
	     << " offline-check-ns=" << evaluatorStats.offlineCheckNanoseconds
	     << " offline-compare-ns=" << evaluatorStats.offlineValueComparisonNanoseconds
	     << " offline-alias-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[1]
	     << " offline-union-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[2]
	     << " offline-compose-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[3]
	     << " offline-difference-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[4]
	     << " offline-intersection-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[5]
	     << " offline-product-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[6]
	     << " offline-identity-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[7]
	     << " offline-domain-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[8]
	     << " offline-range-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[9]
	     << " offline-inverse-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[10]
	     << " offline-optional-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[11]
	     << " offline-tc-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[12]
	     << " offline-rtc-ns=" << evaluatorStats.offlineOperationNanosecondsByKind[13]
	     << " offline-compose-count="
	     << evaluatorStats.offlineOperationEvaluationsByKind[3]
	     << " offline-union-count=" << evaluatorStats.offlineOperationEvaluationsByKind[2]
	     << " offline-difference-count="
	     << evaluatorStats.offlineOperationEvaluationsByKind[4]
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
	     << " history-base-copy-ns=" << stats.historyBaseCopyNanoseconds
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
	     << " preventive-direct-checks=" << preventiveDirectChecks_
	     << " preventive-direct-edge-candidates=" << preventiveDirectEdgeCandidates_
	     << " preventive-direct-base-candidates=" << preventiveDirectBaseCandidates_
	     << " preventive-direct-depth-fallbacks=" << preventiveDirectDepthFallbacks_
	     << " focus-reach-attempts=" << focusReachAttempts_
	     << " focus-reach-successes=" << focusReachSuccesses_
	     << " focus-reach-fallbacks=" << focusReachFallbacks_
	     << " focus-reach-edge-candidates=" << focusReachEdgeCandidates_
	     << " focus-reach-base-candidates=" << focusReachBaseCandidates_
	     << " preventive-rf-candidates=" << preventiveRfCandidates_
	     << " preventive-rf-pruned=" << preventiveRfPruned_
	     << " preventive-co-candidates=" << preventiveCoCandidates_
	     << " preventive-co-pruned=" << preventiveCoPruned_
	     << " preventive-all-pruned-fallbacks=" << preventiveAllPrunedFallbacks_
	     << " preventive-lookup-ns=" << preventiveLookupNanoseconds_;
	if (conflictCores_) {
		const auto &cores = conflictCores_->statistics();
		line << " conflict-core-learn-attempts=" << cores.learnAttempts
		     << " conflict-core-learned=" << cores.learned
		     << " conflict-core-unsupported=" << cores.unsupported
		     << " conflict-core-duplicate-subsumed=" << cores.duplicateOrSubsumed
		     << " conflict-core-evicted=" << cores.evicted
		     << " conflict-core-match-queries=" << cores.matchQueries
		     << " conflict-core-literal-checks=" << cores.literalChecks
		     << " conflict-core-hits=" << cores.hits
		     << " conflict-core-direct-checks-avoided="
		     << conflictCoreDirectChecksAvoided_
		     << " conflict-core-rf-pruned=" << conflictCoreRfPruned_
		     << " conflict-core-co-pruned=" << conflictCoreCoPruned_
		     << " conflict-core-current-clauses=" << conflictCores_->size()
		     << " conflict-core-current-literals=" << conflictCores_->literalCount()
		     << " conflict-core-max-clauses=" << cores.maximumClauses
		     << " conflict-core-max-literals=" << cores.maximumLiterals
		     << " conflict-core-max-bytes=" << cores.maximumBytes;
	}
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
	const auto queryStarted = profile ? std::chrono::steady_clock::now()
					  : std::chrono::steady_clock::time_point{};
	const auto finish = [&](bool consistent) {
		if (profile) {
			consistencyNanoseconds_ += static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - queryStarted)
					.count());
			consistent ? ++consistentQueries_ : ++inconsistentQueries_;
		}
		return consistent;
	};
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
		return finish(result.violations.empty());
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
								    adapter.baseValues(), false,
								    std::nullopt, false,
								    this->getConf()->catFastChecks,
								    this->getConf()->catFastComposition,
								    this->getConf()->catFastCycleChecks);
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
	return finish(violations.empty());
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::prepareConflictCorePrefix(const ExecutionGraph &graph) -> bool
{
	if (!conflictCores_ || !preventiveAdapter_)
		return false;
	auto snapshot = preventiveAdapter_->materialize(graph);
	conflictBaseValues_.assign(this->getConf()->caatModel->predicates().size(), std::nullopt);
	for (const auto id : preventiveBasePredicates_) {
		const auto &predicate = this->getConf()->caatModel->predicates()[id];
		auto found = snapshot.base.find(predicate.name);
		if (found == snapshot.base.end()) {
			conflictBaseValues_.clear();
			return false;
		}
		conflictBaseValues_[id] = std::move(found->second);
	}
	conflictEventCount_ = snapshot.eventCount;
	return true;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::conflictRfDelta(const ReadLabel &read,
						    const EventLabel &source) const
	-> std::vector<cat::ConflictLiteral>
{
	std::vector<cat::ConflictLiteral> result;
	const auto &stable = *preventiveAdapter_;
	const auto readId = stable.id(cat::StableEventKey{read.getPos()});
	const auto sourceId = genmc::isa<InitLabel>(&source)
			      ? stable.id(cat::StableEventKey{read.getAddr()})
			      : stable.id(cat::StableEventKey{source.getPos()});
	VERIFY(readId && sourceId, "conflict-core RF endpoint lacks stable identity");
	const auto literal = [](cat::PredicateId predicate, std::size_t from, std::size_t to) {
		VERIFY(from <= std::numeric_limits<std::uint32_t>::max() &&
			       to <= std::numeric_limits<std::uint32_t>::max(),
		       "conflict-core endpoint exceeds 32-bit stable IDs");
		return cat::ConflictLiteral{predicate, static_cast<std::uint32_t>(from),
					 static_cast<std::uint32_t>(to), false};
	};
	if (preventiveRfPredicate_)
		result.push_back(literal(*preventiveRfPredicate_, *sourceId, *readId));
	if (preventiveFrPredicate_) {
		const auto &graph = *read.getParent();
		const auto appendSuccessor = [&](const EventLabel &successor) {
			const auto id = stable.id(cat::StableEventKey{successor.getPos()});
			VERIFY(id, "conflict-core FR target lacks stable identity");
			result.push_back(literal(*preventiveFrPredicate_, *readId, *id));
		};
		if (genmc::isa<InitLabel>(&source)) {
			for (const auto &successor : graph.co(read.getAddr()))
				appendSuccessor(successor);
		} else {
			for (const auto &successor : graph.co_succs(&source))
				appendSuccessor(successor);
		}
	}
	std::ranges::sort(result);
	result.erase(std::ranges::unique(result).begin(), result.end());
	return result;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::conflictCoDelta(const WriteLabel &write,
						    const EventLabel &predecessor) const
	-> std::vector<cat::ConflictLiteral>
{
	std::vector<cat::ConflictLiteral> result;
	const auto &graph = *write.getParent();
	const auto &stable = *preventiveAdapter_;
	const auto writeId = stable.id(cat::StableEventKey{write.getPos()});
	VERIFY(writeId, "conflict-core CO write lacks stable identity");
	const auto literal = [](cat::PredicateId predicate, std::size_t from, std::size_t to) {
		VERIFY(from <= std::numeric_limits<std::uint32_t>::max() &&
			       to <= std::numeric_limits<std::uint32_t>::max(),
		       "conflict-core endpoint exceeds 32-bit stable IDs");
		return cat::ConflictLiteral{predicate, static_cast<std::uint32_t>(from),
					 static_cast<std::uint32_t>(to), false};
	};
	std::vector<const EventLabel *> ordered{graph.getInitLabel()};
	for (const auto &existing : graph.co(write.getAddr()))
		ordered.push_back(&existing);
	const auto predecessorIt = std::ranges::find(ordered, &predecessor);
	VERIFY(predecessorIt != ordered.end(), "conflict-core CO predecessor is not ordered");
	const auto predecessorIndex =
		static_cast<std::size_t>(std::distance(ordered.begin(), predecessorIt));
	const auto stableId = [&](const EventLabel &label) {
		const auto id = genmc::isa<InitLabel>(&label)
				? stable.id(cat::StableEventKey{write.getAddr()})
				: stable.id(cat::StableEventKey{label.getPos()});
		VERIFY(id, "conflict-core CO endpoint lacks stable identity");
		return *id;
	};
	if (preventiveCoPredicate_) {
		for (std::size_t index = 0; index < ordered.size(); ++index) {
			const auto otherId = stableId(*ordered[index]);
			result.push_back(index <= predecessorIndex
					 ? literal(*preventiveCoPredicate_, otherId, *writeId)
					 : literal(*preventiveCoPredicate_, *writeId, otherId));
		}
	}
	if (preventiveFrPredicate_) {
		for (std::size_t index = 0; index <= predecessorIndex; ++index) {
			const auto appendReaders = [&](const auto &readers) {
				for (const auto &reader : readers) {
					const auto id = stable.id(
						cat::StableEventKey{reader.getPos()});
					VERIFY(id, "conflict-core FR reader lacks stable identity");
					result.push_back(literal(*preventiveFrPredicate_, *id,
								 *writeId));
				}
			};
			if (const auto *initial = genmc::dyn_cast<InitLabel>(ordered[index]))
				appendReaders(initial->rfs(write.getAddr()));
			else
				appendReaders(genmc::cast<WriteLabel>(ordered[index])->readers());
		}
	}
	std::ranges::sort(result);
	result.erase(std::ranges::unique(result).begin(), result.end());
	return result;
}

template <typename HostChecker>
void BasicCATChecker<HostChecker>::learnConflictCore(
	std::span<const cat::ConflictLiteral> proposed)
{
	if (!conflictCores_ || proposed.empty() || conflictBaseValues_.empty())
		return;
	auto augmented = conflictBaseValues_;
	for (const auto &literal : proposed) {
		if (literal.predicate >= augmented.size() || !augmented[literal.predicate]) {
			(void)conflictCores_->learn({});
			return;
		}
		auto *relation = std::get_if<cat::Relation>(&*augmented[literal.predicate]);
		if (!relation) {
			(void)conflictCores_->learn({});
			return;
		}
		relation->insert(literal.from, literal.to);
	}
	const auto root = preventiveOrders_.front().order;
	auto cycle = cat::findLazyCycle(*this->getConf()->caatModel, root, augmented,
					conflictEventCount_);
	if (cycle.empty()) {
		(void)conflictCores_->learn({});
		return;
	}
	std::vector<cat::ConflictLiteral> core;
	for (std::size_t edge = 1; edge < cycle.size(); ++edge) {
		auto derivation = cat::deriveLazyEdge(*this->getConf()->caatModel, root, augmented,
						      conflictEventCount_, cycle[edge - 1],
						      cycle[edge]);
		if (!derivation) {
			(void)conflictCores_->learn({});
			return;
		}
		core.insert(core.end(), derivation->begin(), derivation->end());
	}
	std::ranges::sort(core);
	core.erase(std::ranges::unique(core).begin(), core.end());
	if (!std::ranges::any_of(proposed, [&](const auto &literal) {
		    return std::ranges::binary_search(core, literal);
	    })) {
		(void)conflictCores_->learn({});
		return;
	}
	(void)conflictCores_->learn(std::move(core));
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::preparePreventivePrefix(const ExecutionGraph &graph,
						    const EventLabel &focus)
	-> bool
{
	if (preventiveOrders_.empty() || !graphSynchronizer_ || !incrementalEvaluator_)
		return false;
	++preventivePrefixQueries_;
	const auto profile = this->getConf()->catStats;
	const auto started = profile ? std::chrono::steady_clock::now()
				     : std::chrono::steady_clock::time_point{};
	std::optional<cat::Relation> directOrder;
	std::optional<PreventiveFocusReach> directReach;
	const cat::StableGraphAdapter *stable{};
	bool usable = true;
	if (preventiveAdapter_) {
		auto snapshot = preventiveAdapter_->materialize(graph);
		std::vector<std::optional<cat::Value>> values(
			this->getConf()->caatModel->predicates().size());
		for (const auto id : preventiveBasePredicates_) {
			const auto &predicate = this->getConf()->caatModel->predicates()[id];
			auto found = snapshot.base.find(predicate.name);
			if (found == snapshot.base.end()) {
				usable = false;
				break;
			}
			values[id] = std::move(found->second);
		}
		if (usable) {
			const auto &certificate = preventiveOrders_.front();
			const auto suffix = graph.getLastThreadLabel(focus.getThread()) == &focus;
			const auto *read = genmc::dyn_cast<ReadLabel>(&focus);
			const auto *write = genmc::dyn_cast<WriteLabel>(&focus);
			const auto certifiedRead = read && !read->getRf() &&
						   certificate.unassignedReadSink;
			const auto certifiedWrite =
				write && !write->isInCo() && std::ranges::empty(write->readers()) &&
				certificate.unplacedWriteSink;
			const auto useFocusReach = this->getConf()->catFocusReach &&
					       certificate.lazyReachSupported && suffix &&
					       (certifiedRead || certifiedWrite);
			if (this->getConf()->catFocusReach)
				++focusReachAttempts_;
			if (useFocusReach) {
				const auto focusId = preventiveAdapter_->id(
					cat::StableEventKey{focus.getPos()});
				VERIFY(focusId, "focus-directed reach lacks stable identity");
				cat::LazyCycleStatistics lazyStatistics;
				auto forward = cat::findLazyReach(
					*this->getConf()->caatModel, certificate.order, values,
					snapshot.eventCount, *focusId, false, &lazyStatistics);
				auto reverse = cat::findLazyReach(
					*this->getConf()->caatModel, certificate.order, values,
					snapshot.eventCount, *focusId, true, &lazyStatistics);
				focusReachEdgeCandidates_ += lazyStatistics.emittedCandidates;
				focusReachBaseCandidates_ += lazyStatistics.baseCandidates;
				directReach.emplace(PreventiveFocusReach{
					.forward = std::move(forward), .reverse = std::move(reverse)});
				++focusReachSuccesses_;
			} else {
				if (this->getConf()->catFocusReach)
					++focusReachFallbacks_;
				cat::LazyCycleStatistics lazyStatistics;
				directOrder.emplace(snapshot.eventCount);
				auto cycle = cat::findLazyCycle(
					*this->getConf()->caatModel, certificate.order, values,
					snapshot.eventCount, &lazyStatistics, &*directOrder);
				preventiveDirectChecks_ += lazyStatistics.checks;
				preventiveDirectEdgeCandidates_ += lazyStatistics.emittedCandidates;
				preventiveDirectBaseCandidates_ += lazyStatistics.baseCandidates;
				preventiveDirectDepthFallbacks_ += lazyStatistics.depthFallbacks;
				if (!cycle.empty()) {
					++preventivePrefixInconsistent_;
					usable = false;
				}
			}
		}
		stable = preventiveAdapter_.get();
	} else {
		(void)graphSynchronizer_->synchronize(graph);
		const auto &result = incrementalEvaluator_->result();
		VERIFY(result.errors.empty(), "preventive prefix evaluation failed");
		if (!result.violations.empty()) {
			++preventivePrefixInconsistent_;
			usable = false;
		}
		stable = &graphSynchronizer_->stableAdapter();
	}
	if (profile) {
		synchronizationNanoseconds_ += static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - started)
				.count());
		++profiledQueries_;
	}
	if (!usable)
		return false;
	preventiveReachCache_.clear();
	preventiveReachCache_.reserve(preventiveOrders_.size());
	if (directReach) {
		preventiveReachCache_.push_back(std::move(*directReach));
		return true;
	}
	const auto focusId = stable->id(cat::StableEventKey{focus.getPos()});
	VERIFY(focusId, "preventive focus lacks stable identity");
	for (const auto &certificate : preventiveOrders_) {
		const cat::Relation *order{};
		if (directOrder) {
			VERIFY(certificate.order == preventiveOrders_.front().order,
			       "direct preventive root does not match selected certificate");
			order = &*directOrder;
		} else {
			const auto &value = incrementalEvaluator_->result().values.at(certificate.order);
			VERIFY(value.has_value(), "preventive order is not evaluated");
			order = std::get_if<cat::Relation>(&*value);
			VERIFY(order, "preventive order has unexpected type");
		}
		const auto size = order->size();
		cat::EventSet forward(size), reverse(size), visitedForward(size), visitedReverse(size);
		std::vector<std::size_t> queue;
		queue.reserve(size);
		visitedForward.insert(*focusId);
		queue.push_back(*focusId);
		for (std::size_t next = 0; next < queue.size(); ++next) {
			const auto from = queue[next];
			for (auto target = order->nextSuccessor(from, 0); target < size;
			     target = order->nextSuccessor(from, target + 1)) {
				if (visitedForward.contains(target))
					continue;
				visitedForward.insert(target);
				forward.insert(target);
				queue.push_back(target);
			}
		}

		queue.clear();
		visitedReverse.insert(*focusId);
		queue.push_back(*focusId);
		for (std::size_t next = 0; next < queue.size(); ++next) {
			const auto target = queue[next];
			for (auto from = order->nextPredecessor(target, 0); from < size;
			     from = order->nextPredecessor(target, from + 1)) {
				if (visitedReverse.contains(from))
					continue;
				visitedReverse.insert(from);
				reverse.insert(from);
				queue.push_back(from);
			}
		}
		preventiveReachCache_.push_back(
			{.forward = std::move(forward), .reverse = std::move(reverse)});
	}
	return true;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::preventsRf(const ReadLabel &read, const EventLabel &source,
					      const cat::PreventiveOrderCertificate &certificate,
					      const PreventiveFocusReach &reach) const -> bool
{
	const auto &stable = preventiveAdapter_ ? *preventiveAdapter_
					       : graphSynchronizer_->stableAdapter();
	const auto readId = stable.id(cat::StableEventKey{read.getPos()});
	const auto sourceId = genmc::isa<InitLabel>(&source)
				      ? stable.id(cat::StableEventKey{read.getAddr()})
				      : stable.id(cat::StableEventKey{source.getPos()});
	VERIFY(readId && sourceId, "preventive RF endpoint lacks stable identity");
	const auto external = genmc::isa<InitLabel>(&source) ||
			      source.getThread() != read.getThread();
	if ((certificate.rfMode == cat::PreventiveRfMode::All ||
	     (certificate.rfMode == cat::PreventiveRfMode::External && external)) &&
	    reach.forward.contains(*sourceId))
		return true;

	if (!certificate.includesFr)
		return false;
	const auto &graph = *read.getParent();
	if (genmc::isa<InitLabel>(&source)) {
		for (const auto &successor : graph.co(read.getAddr())) {
			const auto successorId = stable.id(cat::StableEventKey{successor.getPos()});
			VERIFY(successorId, "preventive FR target lacks stable identity");
			if (reach.reverse.contains(*successorId))
				return true;
		}
		return false;
	}
	for (const auto &successor : graph.co_succs(&source)) {
		const auto successorId = stable.id(cat::StableEventKey{successor.getPos()});
		VERIFY(successorId, "preventive FR target lacks stable identity");
		if (reach.reverse.contains(*successorId))
			return true;
	}
	return false;
}

template <typename HostChecker>
auto BasicCATChecker<HostChecker>::preventsCo(const WriteLabel &write,
					      const EventLabel &predecessor,
					      const cat::PreventiveOrderCertificate &certificate,
					      const PreventiveFocusReach &reach) const -> bool
{
	const auto &graph = *write.getParent();
	const auto &stable = preventiveAdapter_ ? *preventiveAdapter_
					       : graphSynchronizer_->stableAdapter();
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

	if (certificate.includesCo) {
		/* Strict total CO adds every earlier->write and write->later pair. */
		for (std::size_t index = 0; index < ordered.size(); ++index) {
			const auto otherId = stableId(*ordered[index]);
			if (index <= predecessorIndex) {
				if (reach.forward.contains(otherId))
					return true;
			} else if (reach.reverse.contains(otherId)) {
				return true;
			}
		}
	}
	if (!certificate.includesFr)
		return false;
	/* Reads from an earlier source gain FR to the newly inserted write. */
	for (std::size_t index = 0; index <= predecessorIndex; ++index) {
		if (const auto *initial = genmc::dyn_cast<InitLabel>(ordered[index])) {
			for (const auto &reader : initial->rfs(write.getAddr())) {
				const auto readerId =
					stable.id(cat::StableEventKey{reader.getPos()});
				VERIFY(readerId, "preventive FR reader lacks stable identity");
				if (reach.forward.contains(*readerId))
					return true;
			}
		} else {
			const auto *earlier = genmc::cast<WriteLabel>(ordered[index]);
			for (const auto &reader : earlier->readers()) {
				const auto readerId =
					stable.id(cat::StableEventKey{reader.getPos()});
				VERIFY(readerId, "preventive FR reader lacks stable identity");
				if (reach.forward.contains(*readerId))
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
		const auto original = result;
		preventiveRfCandidates_ += result.size();
		const auto corePrepared = prepareConflictCorePrefix(graph);
		if (corePrepared) {
			std::erase_if(result, [&](const auto *source) {
				auto proposed = conflictRfDelta(*read, *source);
				const auto prune = conflictCores_->matches(
					*this->getConf()->caatModel, conflictBaseValues_, proposed);
				if (prune) {
					++preventiveRfPruned_;
					++conflictCoreRfPruned_;
				}
				return prune;
			});
			if (result.empty()) {
				++conflictCoreDirectChecksAvoided_;
				++preventiveAllPrunedFallbacks_;
				result.push_back(original.front());
				return result;
			}
		}
		if (preparePreventivePrefix(graph, *read)) {
			const auto started = std::chrono::steady_clock::now();
			std::erase_if(result, [&](const auto *source) {
				bool prune{};
				for (std::size_t index = 0; index < preventiveOrders_.size() && !prune;
				     ++index)
					prune = preventsRf(*read, *source, preventiveOrders_[index],
							   preventiveReachCache_[index]);
				if (prune) {
					++preventiveRfPruned_;
					if (corePrepared)
						learnConflictCore(conflictRfDelta(*read, *source));
				}
				return prune;
			});
			preventiveLookupNanoseconds_ += static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - started)
					.count());
			if (result.empty()) {
				++preventiveAllPrunedFallbacks_;
				result.push_back(original.front());
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
		const auto original = result;
		preventiveCoCandidates_ += result.size();
		const auto corePrepared = prepareConflictCorePrefix(graph);
		if (corePrepared) {
			std::erase_if(result, [&](const auto *predecessor) {
				auto proposed = conflictCoDelta(*write, *predecessor);
				const auto prune = conflictCores_->matches(
					*this->getConf()->caatModel, conflictBaseValues_, proposed);
				if (prune) {
					++preventiveCoPruned_;
					++conflictCoreCoPruned_;
				}
				return prune;
			});
			if (result.empty()) {
				++conflictCoreDirectChecksAvoided_;
				++preventiveAllPrunedFallbacks_;
				result.push_back(original.front());
				return result;
			}
		}
		if (preparePreventivePrefix(graph, *write)) {
			const auto started = std::chrono::steady_clock::now();
			std::erase_if(result, [&](const auto *predecessor) {
				bool prune{};
				for (std::size_t index = 0; index < preventiveOrders_.size() && !prune;
				     ++index)
					prune = preventsCo(*write, *predecessor, preventiveOrders_[index],
							   preventiveReachCache_[index]);
				if (prune) {
					++preventiveCoPruned_;
					if (corePrepared)
						learnConflictCore(
							conflictCoDelta(*write, *predecessor));
				}
				return prune;
			});
			preventiveLookupNanoseconds_ += static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - started)
					.count());
			if (result.empty()) {
				++preventiveAllPrunedFallbacks_;
				result.push_back(original.front());
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
