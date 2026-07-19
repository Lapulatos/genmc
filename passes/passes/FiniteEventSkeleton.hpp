/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_EVENT_SKELETON_HPP
#define GENMC_FINITE_EVENT_SKELETON_HPP

#include "genmc/Verification/FiniteSkeletonIR.hpp"

#include <string>

namespace llvm {
class Module;
}

namespace genmc::skeleton {

/** Analyze the fully transformed module without changing it. */
[[nodiscard]] auto analyze(const llvm::Module &module) -> Report;

/** Build the pointer-free event/SSA skeleton. Any unencoded semantic operation rejects
 * the lane before search; the ordinary interpreter remains the exact fallback. */
[[nodiscard]] auto build(const llvm::Module &module) -> BuildResult;

/** Stable, one-line key/value form for benchmark logs. */
[[nodiscard]] auto format(const Report &report) -> std::string;

} /* namespace genmc::skeleton */

#endif
