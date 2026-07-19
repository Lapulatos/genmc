#ifndef GENMC_VERIFICATION_RESULT_HPP
#define GENMC_VERIFICATION_RESULT_HPP

#include "genmc/ADT/IndexedMap.hpp"
#include "genmc/ADT/VSet.hpp"
#include "genmc/Verification/Relinche/LinearizabilityChecker.hpp"
#include "genmc/Verification/Relinche/Specification.hpp"
#include "genmc/Verification/VerificationError.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <utility>

struct VerificationResult {
	struct ExplorationStatistics {
		std::uint64_t rfOffered{};
		std::uint64_t rfQueued{};
		/** RF decision points with at least two post-filter candidates. */
		std::uint64_t rfValueChoicePoints{};
		/** Sum of distinct (value, provenance) classes at those decision points. */
		std::uint64_t rfValueClasses{};
		/** Candidates beyond one representative per value class (opportunity only). */
		std::uint64_t rfSameValueCandidateUpperBound{};
		/** Decision points containing at least one repeated value class. */
		std::uint64_t rfSameValueChoicePoints{};
		/** Largest repeated value class seen by one worker. */
		std::uint64_t maximumRfSameValueClass{};
		std::uint64_t coOffered{};
		std::uint64_t coQueued{};
		std::uint64_t backwardOffered{};
		std::uint64_t backwardQueued{};
		std::uint64_t workItemsAdded{};
		std::uint64_t workItemsPopped{};
		std::uint64_t maximumRetainedWorkItems{};
		/** Peak retained exploration-state cardinalities sampled at progress points. */
		std::uint64_t maximumCurrentGraphLabels{};
		std::uint64_t maximumStackGraphLabels{};
		std::uint64_t maximumSchedulerCachedLabels{};
		/** Same-worker backward-prefix COW sharing; cross-worker clones remain deep. */
		std::uint64_t sharedHistoryGraphCopies{};
		std::uint64_t sharedHistoryViewBases{};
		std::uint64_t sharedHistoryViewLowerBoundBytes{};
		/** Spin-loop classification decisions behind retained graph growth. */
		std::uint64_t spinStarts{};
		std::uint64_t spinStartsAfterLoopBegin{};
		std::uint64_t spinStartsWithSideEffects{};
		std::uint64_t spinLoopBlocks{};
		std::uint64_t candidateValidityQueries{};
		std::uint64_t realizedRevisitPrefixes{};
		std::uint64_t inconsistentRevisitPrefixes{};
		/** Reads for which the RVF representative path was attempted/completed. */
		std::uint64_t rvfLoadsAttempted{};
		std::uint64_t rvfLoadsReduced{};
		/** Plain atomic reads reached after regional RVF has delegated the path to native
		 * RF-DPOR, and the subset that still exposes an RVF-compatible source merge. */
		std::uint64_t rvfNativeOnlyLoads{};
		std::uint64_t rvfNativeOnlyMergeableLoads{};
		std::uint64_t rvfNativeOnlyMergeableSources{};
		/** Reads delegated by the quotient-disabled instrumentation control. */
		std::uint64_t rvfQuotientDisabledLoads{};
		/** First-visit reads delegated to native RF-DPOR because all classes are singleton.
		 */
		std::uint64_t rvfSingletonBypass{};
		/** Native RF choices materialized as singleton GoodW constraints for VerifySC. */
		std::uint64_t rvfNativeReadsSynthesized{};
		/** Annotated infeasible groups and native revisits suppressed by RVF ownership. */
		std::uint64_t rvfAnnotatedGroupsRejected{};
		std::uint64_t rvfOwnedReadRevisitsSuppressed{};
		/** Concrete visible sources and value groups at completed RVF read frontiers. */
		std::uint64_t rvfVisibleSources{};
		std::uint64_t rvfValueGroups{};
		/** Exact VerifySC outcomes and aggregate state-space effort. */
		std::uint64_t rvfVerifyCalls{};
		std::uint64_t rvfNoWitness{};
		std::uint64_t rvfVerifyStatesDiscovered{};
		std::uint64_t rvfVerifyStatesExpanded{};
		std::uint64_t rvfVerifyTransitions{};
		std::uint64_t rvfVerifyDuplicateStates{};
		std::uint64_t rvfMaximumVerifyWorklist{};
		/** Successfully materialized representative graphs and witness replay length. */
		std::uint64_t rvfRepresentativesQueued{};
		std::uint64_t rvfWitnessEventsReplayed{};
		std::uint64_t rvfParentContinuationsQueued{};
		std::uint64_t rvfMaximumPendingExecutions{};
		/** Fail-open attempts, split by the stage that could not prove a reduction. */
		std::uint64_t rvfFailOpen{};
		std::uint64_t rvfFailOpenUnsupported{};
		std::uint64_t rvfFailOpenSources{};
		std::uint64_t rvfFailOpenAdapter{};
		std::uint64_t rvfFailOpenCausalState{};
		std::uint64_t rvfFailOpenVerify{};
		std::uint64_t rvfFailOpenModel{};
		/** Conservative finite-lane FALSE search; TRUE always falls back to native. */
		std::uint64_t finiteAssignments{};
		std::uint64_t finiteCatRejected{};
		std::uint64_t finiteCatConsistent{};
		std::uint64_t finiteReplayAttempts{};
		std::uint64_t finiteReplayConfirmed{};
		std::uint64_t finiteFailOpen{};

		auto operator+=(const ExplorationStatistics &other) -> ExplorationStatistics &
		{
			rfOffered += other.rfOffered;
			rfQueued += other.rfQueued;
			rfValueChoicePoints += other.rfValueChoicePoints;
			rfValueClasses += other.rfValueClasses;
			rfSameValueCandidateUpperBound += other.rfSameValueCandidateUpperBound;
			rfSameValueChoicePoints += other.rfSameValueChoicePoints;
			maximumRfSameValueClass =
				std::max(maximumRfSameValueClass, other.maximumRfSameValueClass);
			coOffered += other.coOffered;
			coQueued += other.coQueued;
			backwardOffered += other.backwardOffered;
			backwardQueued += other.backwardQueued;
			workItemsAdded += other.workItemsAdded;
			workItemsPopped += other.workItemsPopped;
			maximumRetainedWorkItems =
				std::max(maximumRetainedWorkItems, other.maximumRetainedWorkItems);
			maximumCurrentGraphLabels = std::max(maximumCurrentGraphLabels,
							     other.maximumCurrentGraphLabels);
			maximumStackGraphLabels =
				std::max(maximumStackGraphLabels, other.maximumStackGraphLabels);
			maximumSchedulerCachedLabels = std::max(maximumSchedulerCachedLabels,
								other.maximumSchedulerCachedLabels);
			sharedHistoryGraphCopies += other.sharedHistoryGraphCopies;
			sharedHistoryViewBases += other.sharedHistoryViewBases;
			sharedHistoryViewLowerBoundBytes += other.sharedHistoryViewLowerBoundBytes;
			spinStarts += other.spinStarts;
			spinStartsAfterLoopBegin += other.spinStartsAfterLoopBegin;
			spinStartsWithSideEffects += other.spinStartsWithSideEffects;
			spinLoopBlocks += other.spinLoopBlocks;
			candidateValidityQueries += other.candidateValidityQueries;
			realizedRevisitPrefixes += other.realizedRevisitPrefixes;
			inconsistentRevisitPrefixes += other.inconsistentRevisitPrefixes;
			rvfLoadsAttempted += other.rvfLoadsAttempted;
			rvfLoadsReduced += other.rvfLoadsReduced;
			rvfNativeOnlyLoads += other.rvfNativeOnlyLoads;
			rvfNativeOnlyMergeableLoads += other.rvfNativeOnlyMergeableLoads;
			rvfNativeOnlyMergeableSources += other.rvfNativeOnlyMergeableSources;
			rvfQuotientDisabledLoads += other.rvfQuotientDisabledLoads;
			rvfSingletonBypass += other.rvfSingletonBypass;
			rvfNativeReadsSynthesized += other.rvfNativeReadsSynthesized;
			rvfAnnotatedGroupsRejected += other.rvfAnnotatedGroupsRejected;
			rvfOwnedReadRevisitsSuppressed += other.rvfOwnedReadRevisitsSuppressed;
			rvfVisibleSources += other.rvfVisibleSources;
			rvfValueGroups += other.rvfValueGroups;
			rvfVerifyCalls += other.rvfVerifyCalls;
			rvfNoWitness += other.rvfNoWitness;
			rvfVerifyStatesDiscovered += other.rvfVerifyStatesDiscovered;
			rvfVerifyStatesExpanded += other.rvfVerifyStatesExpanded;
			rvfVerifyTransitions += other.rvfVerifyTransitions;
			rvfVerifyDuplicateStates += other.rvfVerifyDuplicateStates;
			rvfMaximumVerifyWorklist =
				std::max(rvfMaximumVerifyWorklist, other.rvfMaximumVerifyWorklist);
			rvfRepresentativesQueued += other.rvfRepresentativesQueued;
			rvfWitnessEventsReplayed += other.rvfWitnessEventsReplayed;
			rvfParentContinuationsQueued += other.rvfParentContinuationsQueued;
			rvfMaximumPendingExecutions = std::max(rvfMaximumPendingExecutions,
							       other.rvfMaximumPendingExecutions);
			rvfFailOpen += other.rvfFailOpen;
			rvfFailOpenUnsupported += other.rvfFailOpenUnsupported;
			rvfFailOpenSources += other.rvfFailOpenSources;
			rvfFailOpenAdapter += other.rvfFailOpenAdapter;
			rvfFailOpenCausalState += other.rvfFailOpenCausalState;
			rvfFailOpenVerify += other.rvfFailOpenVerify;
			rvfFailOpenModel += other.rvfFailOpenModel;
			finiteAssignments += other.finiteAssignments;
			finiteCatRejected += other.finiteCatRejected;
			finiteCatConsistent += other.finiteCatConsistent;
			finiteReplayAttempts += other.finiteReplayAttempts;
			finiteReplayConfirmed += other.finiteReplayConfirmed;
			finiteFailOpen += other.finiteFailOpen;
			return *this;
		}
	};

	std::optional<VerificationError> status{}; /**< Whether the verification
				    completed successfully */
	unsigned explored{};			   /**< Number of complete executions explored */
	unsigned exploredBlocked{};		   /**< Number of blocked executions explored */
	unsigned boundExceeding{};	  /**< Number of bound-exceeding executions explored */
	long double estimationMean{};	  /**< The mean of estimations */
	long double estimationVariance{}; /**< The (biased) variance of the estimations */
#ifdef ENABLE_GENMC_DEBUG
	unsigned exploredMoot{};		 /**< Number of moot executions _encountered_ */
	unsigned duplicates{};			 /**< Number of duplicate executions explored */
	genmc::IndexedMap<int> exploredBounds{}; /**< Number of complete executions not
			       exceeding each bound */
#endif
	std::string message;				 /**< A message to be printed */
	VSet<VerificationError> warnings;		 /**< The warnings encountered */
	std::unique_ptr<Specification> specification;	 /**< Spec collected (if any) */
	LinearizabilityChecker::Result relincheResult{}; /**< Spec analysis result */
	ExplorationStatistics explorationStatistics{};	 /**< Opt-in candidate-space census */

	VerificationResult() = default;

	auto operator+=(VerificationResult &&other) -> VerificationResult &
	{
		/* Propagate latest error */
		if (other.status.has_value())
			status = other.status;
		message += other.message;
		explored += other.explored;
		exploredBlocked += other.exploredBlocked;
		boundExceeding += other.boundExceeding;
		estimationMean += other.estimationMean;
		estimationVariance += other.estimationVariance;
		explorationStatistics += other.explorationStatistics;
#ifdef ENABLE_GENMC_DEBUG
		exploredMoot += other.exploredMoot;
		/* Bound-blocked executions are calculated at the end */
		exploredBounds.grow(other.exploredBounds.size() - 1);
		for (auto i = 0U; i < other.exploredBounds.size(); i++)
			exploredBounds[i] += other.exploredBounds[i];
		duplicates += other.duplicates;
#endif
		warnings.insert(other.warnings);
		if (other.specification)
			specification->merge(
				std::move(*other.specification)); // FIXME other is const lvalue
		relincheResult += std::move(other.relincheResult);
		return *this;
	}
};

/** Stable key-value rendering shared by final and timeout-surviving progress records. */
inline auto formatExplorationStatistics(const VerificationResult::ExplorationStatistics &statistics)
	-> std::string
{
	return std::format(
		"rf-offered={} rf-queued={} rf-value-choice-points={} rf-value-classes={} "
		"rf-same-value-candidate-upper-bound={} rf-same-value-choice-points={} "
		"max-rf-same-value-class={} co-offered={} co-queued={} backward-offered={} "
		"backward-queued={} work-added={} work-popped={} max-retained-work={} "
		"max-current-graph-labels={} max-stack-graph-labels={} "
		"max-scheduler-cached-labels={} "
		"shared-history-graph-copies={} shared-history-view-bases={} "
		"shared-history-view-lower-bound-bytes={} "
		"spin-starts={} spin-starts-after-loop-begin={} "
		"spin-starts-with-side-effects={} spin-loop-blocks={} "
		"validity-queries={} realized-revisit-prefixes={} "
		"inconsistent-revisit-prefixes={} "
		"rvf-loads-attempted={} rvf-loads-reduced={} "
		"rvf-native-only-loads={} rvf-native-only-mergeable-loads={} "
		"rvf-native-only-mergeable-sources={} "
		"rvf-quotient-disabled-loads={} rvf-singleton-bypass={} "
		"rvf-native-reads-synthesized={} "
		"rvf-annotated-groups-rejected={} "
		"rvf-owned-read-revisits-suppressed={} "
		"rvf-visible-sources={} rvf-value-groups={} rvf-verify-calls={} "
		"rvf-no-witness={} rvf-verify-states-discovered={} "
		"rvf-verify-states-expanded={} rvf-verify-transitions={} "
		"rvf-verify-duplicate-states={} rvf-max-verify-worklist={} "
		"rvf-representatives-queued={} rvf-witness-events-replayed={} "
		"rvf-parent-continuations-queued={} rvf-max-pending-executions={} "
		"rvf-fail-open={} rvf-fail-open-unsupported={} rvf-fail-open-sources={} "
		"rvf-fail-open-adapter={} rvf-fail-open-causal-state={} "
		"rvf-fail-open-verify={} rvf-fail-open-model={} "
		"finite-assignments={} finite-cat-rejected={} finite-cat-consistent={} "
		"finite-replay-attempts={} finite-replay-confirmed={} finite-fail-open={}",
		statistics.rfOffered, statistics.rfQueued, statistics.rfValueChoicePoints,
		statistics.rfValueClasses, statistics.rfSameValueCandidateUpperBound,
		statistics.rfSameValueChoicePoints, statistics.maximumRfSameValueClass,
		statistics.coOffered, statistics.coQueued, statistics.backwardOffered,
		statistics.backwardQueued, statistics.workItemsAdded, statistics.workItemsPopped,
		statistics.maximumRetainedWorkItems, statistics.maximumCurrentGraphLabels,
		statistics.maximumStackGraphLabels, statistics.maximumSchedulerCachedLabels,
		statistics.sharedHistoryGraphCopies, statistics.sharedHistoryViewBases,
		statistics.sharedHistoryViewLowerBoundBytes, statistics.spinStarts,
		statistics.spinStartsAfterLoopBegin, statistics.spinStartsWithSideEffects,
		statistics.spinLoopBlocks, statistics.candidateValidityQueries,
		statistics.realizedRevisitPrefixes, statistics.inconsistentRevisitPrefixes,
		statistics.rvfLoadsAttempted, statistics.rvfLoadsReduced,
		statistics.rvfNativeOnlyLoads, statistics.rvfNativeOnlyMergeableLoads,
		statistics.rvfNativeOnlyMergeableSources, statistics.rvfQuotientDisabledLoads,
		statistics.rvfSingletonBypass, statistics.rvfNativeReadsSynthesized,
		statistics.rvfAnnotatedGroupsRejected,
		statistics.rvfOwnedReadRevisitsSuppressed, statistics.rvfVisibleSources,
		statistics.rvfValueGroups, statistics.rvfVerifyCalls, statistics.rvfNoWitness,
		statistics.rvfVerifyStatesDiscovered, statistics.rvfVerifyStatesExpanded,
		statistics.rvfVerifyTransitions, statistics.rvfVerifyDuplicateStates,
		statistics.rvfMaximumVerifyWorklist, statistics.rvfRepresentativesQueued,
		statistics.rvfWitnessEventsReplayed, statistics.rvfParentContinuationsQueued,
		statistics.rvfMaximumPendingExecutions, statistics.rvfFailOpen,
		statistics.rvfFailOpenUnsupported, statistics.rvfFailOpenSources,
		statistics.rvfFailOpenAdapter, statistics.rvfFailOpenCausalState,
		statistics.rvfFailOpenVerify, statistics.rvfFailOpenModel,
		statistics.finiteAssignments, statistics.finiteCatRejected,
		statistics.finiteCatConsistent, statistics.finiteReplayAttempts,
		statistics.finiteReplayConfirmed, statistics.finiteFailOpen);
}

#endif /* GENMC_VERIFICATION_RESULT_HPP */
