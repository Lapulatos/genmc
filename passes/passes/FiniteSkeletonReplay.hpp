/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_SKELETON_REPLAY_HPP
#define GENMC_FINITE_SKELETON_REPLAY_HPP

#include "genmc/Verification/FiniteSkeletonEncoder.hpp"

#include <string>
#include <vector>

namespace llvm {
class Module;
}

namespace genmc::skeleton {

/** Constrain a transformed-module clone to one finite assignment's nondeterministic
 * inputs and active load values. The ordinary interpreter and checker must still find
 * and report the error; this function never validates a verdict by itself. */
[[nodiscard]] auto constrainReplayModule(llvm::Module &module, const Program &program,
					 const symbolic::FiniteAssignment &assignment)
	-> std::vector<std::string>;

} /* namespace genmc::skeleton */

#endif
