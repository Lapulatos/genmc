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

#ifndef GENMC_CAT_CHECKER_HPP
#define GENMC_CAT_CHECKER_HPP

#include "genmc/CAT/GraphSynchronizer.hpp"
#include "genmc/CAT/ConflictCore.hpp"
#include "genmc/Execution/Consistency/SCChecker.hpp"
#include "genmc/Execution/Consistency/TSOChecker.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

/**
 * Correctness-first graph checker for a validated CAT model.
 *
 * HostChecker supplies established view, prefix, race, and warning machinery
 * selected by explicit typed model metadata. CAT consistency itself is never
 * delegated. Positive normalized CAAT models synchronize each candidate into a
 * worker-local incremental evaluator; Phase 1 models keep their from-scratch
 * evaluator. Model-specific candidate/revisit pruning is disabled in favor of
 * conservative enumeration. Like ConsistencyChecker, one instance belongs to
 * one verification worker and is not thread-safe.
 *
 * @tparam HostChecker Generated SC/TSO checker used only for exploration views
 * and language-level diagnostics; its consistency predicate is overridden.
 */
template <typename HostChecker> class BasicCATChecker final : public HostChecker {
public:
	/**
	 * Construct a checker borrowing the worker's immutable configuration.
	 *
	 * @param conf Non-null Config that outlives this checker and owns catModel.
	 */
	explicit BasicCATChecker(const Config *conf);
	/** Print opt-in, atomically emitted worker statistics before releasing state. */
	~BasicCATChecker() override;

	/**
	 * Return online transition counters when this model has a monotone CAAT path.
	 *
	 * @return Worker-local counters, or null for the offline fallback.
	 */
	[[nodiscard]] auto incrementalStatistics() const
		-> const cat::GraphSynchronizationStatistics *;

private:
	/**
	 * Evaluate the complete graph containing newly selected @p lab.
	 *
	 * @param lab Non-null graph-owned label whose rf/co choice is already installed.
	 * @return True exactly when every CAT check accepts the containing graph.
	 * @complexity One adapter build plus the classified incremental delta or fallback.
	 */
	[[nodiscard]] auto isConsistent(const EventLabel *lab) const -> bool override;
	/**
	 * Evaluate all checks in Config::catModel against @p graph.
	 *
	 * @param graph Read-only candidate snapshot.
	 * @return True exactly when evaluation succeeds without a violation.
	 * @errors Internal invariant failure if validated IR/primitives cannot evaluate.
	 */
	[[nodiscard]] auto isConsistent(const ExecutionGraph &graph) const -> bool override;
	/** Exact forward/reverse reach around one candidate's newly chosen event. */
	struct PreventiveFocusReach {
		cat::EventSet forward;
		cat::EventSet reverse;
	};
	/** Synchronize one prefix and derive exact focus reach from a retained sparse order. */
	[[nodiscard]] auto preparePreventivePrefix(const ExecutionGraph &graph,
					       const EventLabel &focus)
		-> bool;
	/** Whether assigning @p read to @p source inserts an edge reversing prefix reach. */
	[[nodiscard]] auto preventsRf(const ReadLabel &read, const EventLabel &source,
				      const cat::PreventiveOrderCertificate &certificate,
				      const PreventiveFocusReach &reach) const -> bool;
	/** Whether placing @p write after @p predecessor reverses prefix reach. */
	[[nodiscard]] auto preventsCo(const WriteLabel &write, const EventLabel &predecessor,
				      const cat::PreventiveOrderCertificate &certificate,
				      const PreventiveFocusReach &reach) const -> bool;
	/** Materialize only selected-root base leaves for pre-root exact core matching. */
	[[nodiscard]] auto prepareConflictCorePrefix(const ExecutionGraph &graph) -> bool;
	/** Exact positive base delta introduced by one prospective RF choice. */
	[[nodiscard]] auto conflictRfDelta(const ReadLabel &read, const EventLabel &source) const
		-> std::vector<cat::ConflictLiteral>;
	/** Exact positive base delta introduced by one prospective CO placement. */
	[[nodiscard]] auto conflictCoDelta(const WriteLabel &write,
					   const EventLabel &predecessor) const
		-> std::vector<cat::ConflictLiteral>;
	/** Derive and admit a sufficient cycle core from current base plus @p proposed. */
	void learnConflictCore(std::span<const cat::ConflictLiteral> proposed);

	/**
	 * Return every currently available same-location rf source.
	 *
	 * @param read Graph-owned read whose address selects the co list.
	 * @return Init followed by every real write in current coherence order.
	 */
	auto getCoherentStores(ReadLabel *read) -> std::vector<EventLabel *> override;
	/**
	 * Keep every generic revisit candidate; CAT evaluation filters resulting graphs.
	 *
	 * @param write Newly added write that triggered revisits.
	 * @param reads Candidate list left unchanged by this conservative profile.
	 */
	void filterCoherentRevisits(WriteLabel *write, std::vector<ReadLabel *> &reads) override;
	/**
	 * Return every coherence predecessor, subject only to RMW adjacency.
	 *
	 * @param write Graph-owned write not yet assigned its final co position.
	 * @return All predecessor choices, or the unique rf source for an RMW write.
	 */
	auto getCoherentPlacings(WriteLabel *write) -> std::vector<EventLabel *> override;
	/**
	 * Suppress warnings caused solely by correctness-first over-enumeration.
	 *
	 * @param write Store whose raw predecessor candidates were enumerated.
	 * @param placements Unfiltered candidates, including CAT-inconsistent ones.
	 * @return Whether the host's diagnostic-only placement range is ambiguous.
	 */
	auto shouldReportCoherenceWarning(WriteLabel *write,
					  const std::vector<EventLabel *> &placements)
		-> bool override;

	/** Mutable worker-local online state; const consistency queries update caches. */
	mutable std::unique_ptr<cat::IncrementalCaatEvaluator> incrementalEvaluator_;
	/** Synchronizer points to incrementalEvaluator_, whose allocation never moves. */
	mutable std::unique_ptr<cat::GraphSynchronizer> graphSynchronizer_;
	/** Independent stable universe used only by direct preventive root evaluation. */
	std::unique_ptr<cat::StableGraphAdapter> preventiveAdapter_;
	/** Base predicate IDs in the selected analyzer-certified lazy root cone. */
	std::vector<cat::PredicateId> preventiveBasePredicates_;
	std::optional<cat::PredicateId> preventiveRfPredicate_;
	std::optional<cat::PredicateId> preventiveFrPredicate_;
	std::optional<cat::PredicateId> preventiveCoPredicate_;
	std::vector<std::optional<cat::Value>> conflictBaseValues_;
	std::size_t conflictEventCount_{};
	std::unique_ptr<cat::ConflictCoreDatabase> conflictCores_;
	mutable std::size_t conflictCoreDirectChecksAvoided_{};
	mutable std::size_t conflictCoreRfPruned_{};
	mutable std::size_t conflictCoreCoPruned_{};
	/** Separate generated checker used only under an exact semantic certificate. */
	std::unique_ptr<HostChecker> pruningHost_;
	/** Opt-in wall-time attribution; only updated under --cat-stats. */
	mutable std::uint64_t adapterNanoseconds_{};
	mutable std::uint64_t synchronizationNanoseconds_{};
	mutable std::uint64_t consistencyNanoseconds_{};
	mutable std::size_t profiledQueries_{};
	mutable std::size_t consistentQueries_{};
	mutable std::size_t inconsistentQueries_{};
	/** Structurally certified checked orders used only under the opt-in path. */
	std::vector<cat::PreventiveOrderCertificate> preventiveOrders_;
	/** Exact focus-specific reachability for the selected checked order. */
	std::vector<PreventiveFocusReach> preventiveReachCache_;
	mutable std::size_t preventivePrefixQueries_{};
	mutable std::size_t preventivePrefixInconsistent_{};
	mutable std::size_t preventiveDirectChecks_{};
	mutable std::size_t preventiveDirectEdgeCandidates_{};
	mutable std::size_t preventiveDirectBaseCandidates_{};
	mutable std::size_t preventiveDirectDepthFallbacks_{};
	mutable std::size_t focusReachAttempts_{};
	mutable std::size_t focusReachSuccesses_{};
	mutable std::size_t focusReachFallbacks_{};
	mutable std::size_t focusReachEdgeCandidates_{};
	mutable std::size_t focusReachBaseCandidates_{};
	mutable std::size_t preventiveRfCandidates_{};
	mutable std::size_t preventiveRfPruned_{};
	mutable std::size_t preventiveCoCandidates_{};
	mutable std::size_t preventiveCoPruned_{};
	mutable std::size_t preventiveAllPrunedFallbacks_{};
	mutable std::uint64_t preventiveLookupNanoseconds_{};
};

/** CAT evaluator hosted by SC causal views for models declaring/defaulting to SC. */
using CATSCChecker = BasicCATChecker<SCChecker>;
/** CAT evaluator hosted by TSO causal views for models explicitly declaring TSO. */
using CATTSOChecker = BasicCATChecker<TSOChecker>;
/** Backward-compatible name for the original Phase 1.6 SC-hosted checker. */
using CATChecker = CATSCChecker;

extern template class BasicCATChecker<SCChecker>;
extern template class BasicCATChecker<TSOChecker>;

#endif /* GENMC_CAT_CHECKER_HPP */
