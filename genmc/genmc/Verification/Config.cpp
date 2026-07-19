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

#include "genmc/config.h"

#include "genmc/CAT/Analysis.hpp"
#include "genmc/CAT/Frontend.hpp"
#include "genmc/CAT/Model.hpp"
#include "genmc/CAT/Normalized.hpp"
#include "genmc/Support/Error.hpp"
#include "genmc/Verification/Config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

static auto doesPolicySupportSeed(const SchedulePolicy policy) -> bool
{
	switch (policy) {
	case SchedulePolicy::Arbitrary:
	case SchedulePolicy::WFR:
		return true;
	case SchedulePolicy::LTR:
	case SchedulePolicy::WF:
		return false;
	}
	UNREACHABLE(); /* Unknown SchedulePolicy */
}

auto Config::validate(std::vector<std::string> &warnings) -> ValidationStatus
{
	ConfigErrorList errors;
	if (explainCat && !modelFile)
		errors.emplace_back("--explain-cat requires --model-file.");
	if (catStats && !modelFile)
		errors.emplace_back("--cat-stats requires --model-file.");
	if (catDisableAdaptiveOffline && !modelFile)
		errors.emplace_back("--cat-disable-adaptive-offline requires --model-file.");
	if (catPrimitiveCache && !modelFile)
		errors.emplace_back("--cat-primitive-cache requires --model-file.");
	if (catFastPrimitiveBuild && !modelFile)
		errors.emplace_back("--cat-fast-primitive-build requires --model-file.");
	if (catFastCoherenceBuild && !modelFile)
		errors.emplace_back("--cat-fast-coherence-build requires --model-file.");
	if (catFastDescriptorBuild && !modelFile)
		errors.emplace_back("--cat-fast-descriptor-build requires --model-file.");
	if (catFastDescriptorBuild && !catPrimitiveCache)
		errors.emplace_back("--cat-fast-descriptor-build requires --cat-primitive-cache.");
	if (catFastDescriptorReuse && !modelFile)
		errors.emplace_back("--cat-fast-descriptor-reuse requires --model-file.");
	if (catFastDescriptorReuse && !catPrimitiveCache)
		errors.emplace_back("--cat-fast-descriptor-reuse requires --cat-primitive-cache.");
	if (catFastChecks && !modelFile)
		errors.emplace_back("--cat-fast-checks requires --model-file.");
	if (catFastComposition && !modelFile)
		errors.emplace_back("--cat-fast-composition requires --model-file.");
	if (catFastCycleChecks && !modelFile)
		errors.emplace_back("--cat-fast-cycle-checks requires --model-file.");
	if (catPreventivePruning && !modelFile)
		errors.emplace_back("--cat-preventive-pruning requires --model-file.");
	if (catConflictCores && !catPreventivePruning)
		errors.emplace_back("--cat-conflict-cores requires --cat-preventive-pruning.");
	if (catBackjumpCensus && (!modelFile || !catStats || !catPreventivePruning))
		errors.emplace_back(
			"--cat-backjump-census requires --model-file, --cat-stats, and "
			"--cat-preventive-pruning.");
	if (catFocusReach && !catPreventivePruning)
		errors.emplace_back("--cat-focus-reach requires --cat-preventive-pruning.");
	if (catOracle && !modelFile)
		errors.emplace_back("--cat-oracle requires --model-file.");
	if (scRvfExploration && !modelFile)
		errors.emplace_back("SC RVF exploration requires --model-file.");
	if (scRvfDisableQuotient && !scRvfExploration)
		errors.emplace_back("--sc-rvf-disable-quotient requires --sc-rvf-exploration.");
	if (scRvfAnnotatedReads && !scRvfExploration)
		errors.emplace_back("--sc-rvf-annotated-reads requires --sc-rvf-exploration.");
	if (scRvfRegional && !scRvfExploration)
		errors.emplace_back("--sc-rvf-regional requires --sc-rvf-exploration.");

	/* Check exploration options */
	if (modelFile.has_value()) {
		bool usable = !modelExplicit && modelFileOccurrences <= 1;
		if (modelExplicit) {
			errors.emplace_back(
				"--model-file cannot be combined with an explicit built-in memory "
				"model option.");
		}
		if (modelFileOccurrences > 1) {
			errors.emplace_back("--model-file may only be specified once.");
		}
		if (collectLinSpec.has_value() || checkLinSpec.has_value()) {
			errors.emplace_back(
				"--model-file cannot be combined with Relinche collection or "
				"checking options in Phase 1.");
			usable = false;
		}

		/* Validate and canonicalize the path before compilation. Keeping the
		 * canonical path in Config makes later parser caches independent of the
		 * process working directory and symlink spelling. */
		std::error_code ec;
		if (usable) {
			const auto exists = std::filesystem::exists(*modelFile, ec);
			if (ec) {
				errors.emplace_back("Cannot inspect CAT model file '" +
						    modelFile->string() + "': " + ec.message() +
						    ".");
				usable = false;
			} else if (!exists) {
				errors.emplace_back("CAT model file does not exist: '" +
						    modelFile->string() + "'.");
				usable = false;
			}
		}

		ec.clear();
		if (usable) {
			const auto isRegular = std::filesystem::is_regular_file(*modelFile, ec);
			if (ec) {
				errors.emplace_back("Cannot inspect CAT model file '" +
						    modelFile->string() + "': " + ec.message() +
						    ".");
				usable = false;
			} else if (!isRegular) {
				errors.emplace_back("CAT model file is not a regular file: '" +
						    modelFile->string() + "'.");
				usable = false;
			}
		}

		if (usable) {
			auto canonicalPath = std::filesystem::canonical(*modelFile, ec);
			if (ec) {
				errors.emplace_back("Cannot resolve CAT model file '" +
						    modelFile->string() + "': " + ec.message() +
						    ".");
				usable = false;
			} else {
				std::ifstream input(canonicalPath);
				if (!input.good()) {
					errors.emplace_back("CAT model file is not readable: '" +
							    canonicalPath.string() + "'.");
					usable = false;
				} else {
					modelFile = std::move(canonicalPath);
				}
			}
		}

		/* Parse, resolve, and type before LLVM execution. The immutable model is
		 * retained in Config for read-only sharing by worker-local CAT checkers. */
		if (usable) {
			auto parseResult = cat::Frontend().parseFile(*modelFile);
			for (const auto &diagnostic : parseResult.diagnostics)
				errors.push_back(diagnostic.format());
			if (parseResult.ok()) {
				const bool recursive = std::ranges::any_of(
					parseResult.model->statements, [](const auto &statement) {
						return statement.recursiveGroup != 0;
					});
				/* Build the normalized form for every file. Phase 1 remains the
				 * preferred backend for its acyclic subset; this second form
				 * enables recursion, forward references, and requested
				 * explanations. */
				{
					auto normalized =
						cat::Normalizer().normalize(*parseResult.model);
					for (const auto &diagnostic : normalized.diagnostics)
						errors.push_back(diagnostic.format());
					if (normalized.ok()) {
						auto analyzed =
							cat::Analyzer().analyze(*normalized.model);
						for (const auto &diagnostic : analyzed.diagnostics)
							errors.push_back(diagnostic.format());
						if (analyzed.ok()) {
							caatModel = std::move(normalized.model);
							caatAnalysis = std::move(analyzed.analysis);
						}
					}
				}

				if (recursive && caatModel && caatAnalysis) {
					/* A from-scratch CAAT call is safe for a growing GenMC
					 * prefix only when checked predicates are monotone. Phase 3
					 * will provide a trail-aware contract for negative
					 * literals. */
					const auto difference = std::ranges::find_if(
						caatModel->predicates(), [](const auto &predicate) {
							return predicate.kind ==
							       cat::Predicate::Kind::Difference;
						});
					if (difference != caatModel->predicates().end()) {
						errors.push_back(cat::Diagnostic{
							cat::DiagnosticKind::Unsupported,
							difference->span,
							"recursive CAT difference is "
							"offline-admissible "
							"but not prefix-monotone in Phase 2",
							{}}.format());
					} else {
						useCaatBackend = true;
					}
				} else if (!recursive) {
					auto compiled = cat::Compiler().compile(*parseResult.model);
					if (compiled.ok()) {
						for (const auto &note : compiled.notes)
							warnings.push_back(note.format());
						const auto inadmissible =
							compiled.model
								->firstOnlineInadmissibleNode();
						if (inadmissible) {
							const auto &node =
								compiled.model->nodes().at(
									*inadmissible);
							errors.push_back(cat::Diagnostic{
								cat::DiagnosticKind::Unsupported,
								node.span,
								"CAT difference affecting a check "
								"is not "
								"online-admissible in Phase 1",
								{}}.format());
						} else {
							catModel = std::move(compiled.model);
						}
					} else if (caatModel && caatAnalysis) {
						const auto difference = std::ranges::find_if(
							caatModel->predicates(),
							[](const auto &predicate) {
								return predicate.kind ==
								       cat::Predicate::Kind::
									       Difference;
							});
						if (difference == caatModel->predicates().end())
							useCaatBackend = true;
						else
							errors.push_back(cat::Diagnostic{
								cat::DiagnosticKind::Unsupported,
								difference->span,
								"forward-reference CAT difference "
								"is not "
								"prefix-monotone in Phase 2",
								{}}.format());
					} else {
						for (const auto &diagnostic : compiled.diagnostics)
							errors.push_back(diagnostic.format());
					}
				}

				/* Metadata chooses only the established causal-view host. */
				if (useCaatBackend || catModel) {
					const auto host = useCaatBackend ? caatModel->hostProfile()
									 : catModel->hostProfile();
					model = host == cat::HostProfile::SC ? ModelType::SC
									     : ModelType::TSO;
				}
			}
		}
	}
	if (catPreventivePruning &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     caatModel->certifiedCandidateProfile() || caatAnalysis->preventiveOrders().empty()))
		errors.emplace_back(
			"--cat-preventive-pruning requires a structurally certified acyclic "
			"RF/FR/CO order.");
	if (catDisableAdaptiveOffline &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     !caatModel->certifiedAdaptiveOffline()))
		errors.emplace_back(
			"--cat-disable-adaptive-offline requires a CAAT model with the "
			"adaptive-offline structural certificate.");
	if (catPrimitiveCache &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     !caatModel->certifiedAdaptiveOffline()))
		errors.emplace_back(
			"--cat-primitive-cache requires a CAAT model with the "
			"adaptive-offline structural certificate.");
	if (catFastPrimitiveBuild &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     !caatModel->certifiedAdaptiveOffline()))
		errors.emplace_back(
			"--cat-fast-primitive-build requires a CAAT model with the "
			"adaptive-offline structural certificate.");
	if (catFastCoherenceBuild &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     !caatModel->certifiedAdaptiveOffline()))
		errors.emplace_back(
			"--cat-fast-coherence-build requires a CAAT model with the "
			"adaptive-offline structural certificate.");
	if (catFastDescriptorBuild &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     !caatModel->certifiedAdaptiveOffline()))
		errors.emplace_back(
			"--cat-fast-descriptor-build requires a CAAT model with the "
			"adaptive-offline structural certificate.");
	if (catFastDescriptorReuse &&
	    (!useCaatBackend || !caatModel || !caatAnalysis ||
	     !caatModel->certifiedAdaptiveOffline()))
		errors.emplace_back(
			"--cat-fast-descriptor-reuse requires a CAAT model with the "
			"adaptive-offline structural certificate.");
	if (catFastChecks && (!useCaatBackend || !caatModel || !caatAnalysis))
		errors.emplace_back("--cat-fast-checks requires the CAAT backend.");
	if (catFastComposition && (!useCaatBackend || !caatModel || !caatAnalysis))
		errors.emplace_back("--cat-fast-composition requires the CAAT backend.");
	if (catFastCycleChecks && (!useCaatBackend || !caatModel || !caatAnalysis))
		errors.emplace_back("--cat-fast-cycle-checks requires the CAAT backend.");
	if (scRvfExploration && (!caatAnalysis || !caatAnalysis->certifiesSCValueExploration()))
		errors.emplace_back(
			"SC RVF exploration requires a structurally certified plain-read/write "
			"SC model.");
	if (scRvfExploration && mode != ExplorationMode::verify)
		errors.emplace_back("SC RVF exploration requires exhaustive verification mode.");
	if (scRvfExploration && bound.has_value())
		errors.emplace_back("SC RVF exploration does not support context or round bounds.");
	if (scRvfExploration && checkLiveness)
		errors.emplace_back(
			"SC RVF exploration currently supports local safety properties only.");
	if (scRvfExploration && catPreventivePruning)
		errors.emplace_back(
			"SC RVF exploration cannot be combined with CAT preventive pruning until "
			"their completeness arguments are composed.");
	if (LAPOR) {
		errors.emplace_back("LAPOR is temporarily disabled.");
	}
	if (confirmation) {
		errors.emplace_back("Confirmation is temporarily disabled.");
	}
	if (model == ModelType::IMM && (ipr || symmetryReduction)) {
		warnings.emplace_back(
			"In-place revisiting and symmetry reduction have no effect under IMM");
		symmetryReduction = false;
		ipr = false;
	}
	if (!emitNALabels && instructionCaching)
		errors.emplace_back("Instruction caching implies NA-label emission");

	/* Check sampling options */
	if (mode == ExplorationMode::random && randomMax == 0)
		errors.emplace_back("Random exploration budget must be greater than 0.");

	/* Check debugging options */
	if (!doesPolicySupportSeed(schedulePolicy) && printRandomScheduleSeed)
		warnings.emplace_back(
			"--print-schedule-seed used without --schedule-policy={{arbitrary,wfr}}.");
	if (!doesPolicySupportSeed(schedulePolicy) && randomScheduleSeed.has_value())
		warnings.emplace_back(
			"--schedule-seed used without --schedule-policy={{arbitrary,wfr}}.");

	/* Populate seed if not provided by user */
	if (!randomScheduleSeed)
		randomScheduleSeed = std::random_device()();

	/* Check bounding options */
	if (bound.has_value() && model != ModelType::SC) {
		errors.emplace_back("Bounding can only be used with --sc.");
	}
	GENMC_DEBUG(if (bound.has_value() && boundsHistogram)
			    errors.emplace_back("Bounds histogram cannot be used when bounding."););
	if (!bound.has_value() && boundType != BoundType::none) {
		warnings.emplace_back("--bound-type used without --bound.");
	}

	/* Sanitize bounding options */
	auto bounding = bound.has_value();
	GENMC_DEBUG(bounding |= boundsHistogram;);
	if (bounding && (LAPOR || !disableBAM || symmetryReduction || ipr ||
			 schedulePolicy != SchedulePolicy::LTR)) {
		warnings.emplace_back(
			"LAPOR/BAM/SR/IPR have no effect when --bound is used. Scheduling "
			"defaults to LTR.");
		LAPOR = symmetryReduction = ipr = false;
		disableBAM = true;
		schedulePolicy = SchedulePolicy::LTR;
	}

	/* Check Relinche options */
	if (collectLinSpec.has_value() && checkLinSpec.has_value()) {
		errors.emplace_back(
			"Cannot collect and analyze linearizability specification in a single "
			"run.");
	}
	if (checkLinSpec.has_value() && (!std::filesystem::exists(*checkLinSpec) ||
					 !std::filesystem::is_regular_file(*checkLinSpec))) {
		errors.emplace_back("Specification file is not a regular file!");
	}
	return errors.empty() ? ValidationStatus() : ValidationStatus(std::move(errors));
}
