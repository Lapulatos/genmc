/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_SKELETON_ENCODER_HPP
#define GENMC_FINITE_SKELETON_ENCODER_HPP

#include "genmc/Verification/FiniteSymbolicSolver.hpp"
#include "genmc/Verification/FiniteSkeletonIR.hpp"
#include "genmc/CAT/Reasoner.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace genmc::symbolic {

struct FiniteDenseEvent;

struct FiniteAssignment {
	std::vector<skeleton::NodeID> activeEvents{};
	std::vector<std::optional<std::uint64_t>> values{};
	/** Per event: selected store event, invalidNode for initial write, nullopt for non-load. */
	std::vector<std::optional<skeleton::NodeID>> readsFrom{};
	/** Active stores in increasing per-location coherence rank. */
	std::vector<skeleton::NodeID> coherenceOrder{};
};

struct FiniteStep {
	CheckResult status{CheckResult::unknown};
	std::optional<FiniteAssignment> assignment{};
};

enum class RfCardinalityEncoding : std::uint8_t { pairwise, native };

struct FiniteEncodingOptions {
	bool requireActiveError{};
	bool encodeCo{true};
	RfCardinalityEncoding rfCardinality{RfCardinalityEncoding::pairwise};
};

/** Solver-free size census for the current eager encoding and two literature-motivated
 * alternatives. Counts describe the skeleton representation only and never affect a verdict. */
struct FiniteRepresentationCensus {
	std::uint64_t blocks{};
	std::uint64_t cfgEdges{};
	std::uint64_t valueVariables{};
	std::uint64_t valueBits{};
	std::uint64_t errorEvents{};
	std::uint64_t rfReads{};
	std::uint64_t rfReadsWithoutSource{};
	std::uint64_t rfSelectors{};
	std::uint64_t rfPairs{};
	std::uint64_t rfLoadActivations{};
	std::uint64_t rfStoreActivations{};
	std::uint64_t rfValueConstraints{};
	std::uint64_t coRanks{};
	std::uint64_t coRankBits{};
	std::uint64_t coPairs{};
	std::uint64_t poPairs{};
	std::uint64_t potentialFrDerivations{};
	std::uint64_t maximumRfSources{};
	std::uint64_t maximumWritesPerAddress{};
};

[[nodiscard]] auto censusFiniteRepresentation(const skeleton::Program &program)
	-> FiniteRepresentationCensus;
[[nodiscard]] auto format(const FiniteRepresentationCensus &census) -> std::string;

/** Exact finite CFG/value/RF/CO assignment enumerator. CAT consistency and interpreter
 * replay deliberately remain outside this class and must validate every assignment. */
class FiniteSkeletonEncoder {
public:
	explicit FiniteSkeletonEncoder(const skeleton::Program &program,
				       FiniteEncodingOptions options = {});
	~FiniteSkeletonEncoder();
	FiniteSkeletonEncoder(const FiniteSkeletonEncoder &) = delete;
	auto operator=(const FiniteSkeletonEncoder &) -> FiniteSkeletonEncoder & = delete;
	FiniteSkeletonEncoder(FiniteSkeletonEncoder &&) noexcept;
	auto operator=(FiniteSkeletonEncoder &&) noexcept -> FiniteSkeletonEncoder &;

	[[nodiscard]] auto supported() const -> bool;
	[[nodiscard]] auto blockers() const -> const std::vector<std::string> &;
	/** Return the next complete assignment and block it from subsequent calls. */
	[[nodiscard]] auto next() -> FiniteStep;
	/** Block every remaining SSA/input assignment with the same active-event, RF and CO
	 * graph as the most recently returned model. Safe after exact CAT classification. */
	void blockCurrentGraph();
	/** Block the current active-control assignment together with only the selected RF
	 * choices for CORELOADS. Sound after an exact SC no-witness core. */
	void blockCurrentRfCore(std::span<const skeleton::NodeID> coreLoads);
	/** Block one reasoner-proved conjunction of CAT base literals. Returns false if a
	 * literal is outside the exact first-encoder translation. */
	[[nodiscard]] auto blockCurrentExplanation(
		std::span<const cat::BaseLiteral> explanation,
		std::span<const FiniteDenseEvent> denseEvents) -> bool;
	/** Stable diagnostic for the most recent failed explanation translation. */
	[[nodiscard]] auto explanationFailure() const -> const std::string &;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} /* namespace genmc::symbolic */

#endif
