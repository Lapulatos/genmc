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

#ifndef GENMC_CONFIG_HPP
#define GENMC_CONFIG_HPP

#include "genmc/Verification/MemoryModel.hpp"
#include "genmc/config.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cat {
class ModelAnalysis;
class ModelIR;
class NormalizedModel;
} /* namespace cat */

enum class ExplorationMode : std::uint8_t { verify, random, estimate };
enum class SchedulePolicy : std::uint8_t { LTR, WF, WFR, Arbitrary };
enum class BoundType : std::uint8_t { none, context, round };

using ConfigErrorList = std::vector<std::string>;
using ValidationStatus = std::variant<std::monostate, ConfigErrorList>;

struct Config {
	/*** Exploration options ***/
	ExplorationMode mode{};
	ModelType model{};
	/** Canonical CAT model path, when the user selected `--model-file`. */
	std::optional<std::filesystem::path> modelFile;
	/** Immutable typed CAT model shared by verification workers after validation. */
	std::shared_ptr<const cat::ModelIR> catModel;
	/** Immutable normalized equations for recursive CAAT or requested explanations. */
	std::shared_ptr<const cat::NormalizedModel> caatModel;
	/** Stratification/admissibility result matching caatModel. */
	std::shared_ptr<const cat::ModelAnalysis> caatAnalysis;
	/** Select the CAAT evaluator instead of the Phase 1 topological evaluator. */
	bool useCaatBackend{};
	/** Print source-located base-literal reasons for rejected CAT candidates. */
	bool explainCat{};
	/** Print per-worker incremental synchronization counters at shutdown. */
	bool catStats{};
	/** Whether the user also explicitly selected one of GenMC's built-in models. */
	bool modelExplicit{};
	/** Number of `--model-file` occurrences, retained for stable duplicate diagnostics. */
	unsigned int modelFileOccurrences{};
	bool estimate{};
	bool isDepTrackingModel{};
	std::optional<unsigned int> bound;
	BoundType boundType{};
	bool LAPOR{};
	bool symmetryReduction{};
	bool helper{};
	bool confirmation{};
	bool finalWrite{};
	bool checkLiveness{};
	bool printErrorTrace{};
	std::string dotFile;
	bool instructionCaching{};
	bool disableRaceDetection{};
	bool disableInitializationChecks{};
	bool disableStaticValidityChecks{};
	bool disableBAM{};
	bool ipr{};
	bool warnUnfreedMemory{};
	std::optional<std::string> collectLinSpec;
	std::optional<std::string> checkLinSpec;
	unsigned int maxExtSize{};
	bool dotPrintOnlyClientEvents{};
	bool replayCompletedThreads{true};
	static constexpr bool emitNALabels = EMIT_NA_LABELS;

	/*** Debugging options ***/
	unsigned int randomMax{};
	unsigned int estimationMax{};
	unsigned int estimationMin{};
	unsigned int sdThreshold{};
	bool printExecGraphs{};
	bool printBlockedExecs{};
	SchedulePolicy schedulePolicy{};
	std::optional<unsigned long long> randomScheduleSeed;
	bool printRandomScheduleSeed{};
	unsigned int warnOnGraphSize{};
#ifdef ENABLE_GENMC_DEBUG
	bool validateExecGraphs{};
	bool countDuplicateExecs{};
	bool countMootExecs{};
	bool printEstimationStats{};
	bool boundsHistogram{};
	bool relincheDebug{};
#endif

	auto validate(std::vector<std::string> &warnings) -> ValidationStatus;
};

#endif /* GENMC_CONFIG_HPP */
