/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_SKELETON_SC_COMPLETION_HPP
#define GENMC_FINITE_SKELETON_SC_COMPLETION_HPP

#include "genmc/Verification/FiniteSkeletonEncoder.hpp"
#include "genmc/Verification/SCGoodWritesSolver.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace genmc::symbolic {

/** Outcome of exact SC coherence completion for one fixed control/RF assignment. */
enum class FiniteSCCompletionStatus : std::uint8_t {
	completed,
	noWitness,
	invalidInput,
	witnessRejected
};

struct FiniteSCCompletionResult {
	FiniteSCCompletionStatus status{FiniteSCCompletionStatus::invalidInput};
	std::optional<FiniteAssignment> assignment{};
	rvf::Metrics metrics{};
	std::vector<skeleton::NodeID> rfCoreLoads{};
	std::uint64_t coreChecks{};
	bool refinementSafe{};
	std::string error{};
};

/** Complete a fixed finite control/RF candidate with one exact SC coherence order.
 *
 * The input assignment is first materialized through the same CAT adapter used for final
 * checking. The independent VerifySC representative generator then searches for a total
 * order with the fixed RF choices. RMW atomicity is deliberately relaxed during witness
 * search: this makes `noWitness` and its RF core sound even for lock-containing programs.
 * A witness is returned only after the generated CO passes the exact recursive-SC checks,
 * including RMW atomicity. `noWitness` must never be used as a whole-program safety proof.
 */
[[nodiscard]] auto completeFiniteSC(const skeleton::Program &program,
				    const FiniteAssignment &assignment)
	-> FiniteSCCompletionResult;

/** Exact active-candidate SC ordering query with one Boolean assumption per fixed RF.
 * SAT ranks are projected to CO and rechecked by recursive SC; UNSAT returns the native
 * assumption core without repeated completion calls. */
[[nodiscard]] auto completeFiniteSCWithOrdering(const skeleton::Program &program,
					const FiniteAssignment &assignment)
	-> FiniteSCCompletionResult;

} /* namespace genmc::symbolic */

#endif
