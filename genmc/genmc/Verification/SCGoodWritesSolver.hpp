/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#ifndef GENMC_SC_GOOD_WRITES_SOLVER_HPP
#define GENMC_SC_GOOD_WRITES_SOLVER_HPP

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace genmc::rvf {

using EventId = std::uint32_t;
using ThreadId = std::uint32_t;
using VariableId = std::uint32_t;

inline constexpr auto noVariable = std::numeric_limits<VariableId>::max();

/** Event category relevant to SC witness construction. */
enum class EventKind : std::uint8_t { other, read, write };

/** One event in a proper bounded SC event set. IDs must be dense and equal to the
 * event's index in Problem::events. Events of one thread must use dense threadIndex
 * values; extraPredecessors encode spawn/join or other non-thread-local prerequisites. */
struct Event {
	EventId id{};
	ThreadId thread{};
	std::uint32_t threadIndex{};
	VariableId variable{noVariable};
	EventKind kind{EventKind::other};
	std::vector<EventId> extraPredecessors{};
	/** Successful RMW read's immediately adjacent write. The paired write must be the
	 * next event of the same thread and variable; failed RMW reads leave this empty. */
	std::optional<EventId> atomicSuccessor{};
};

/** A VSC instance. goodWrites[r] contains every write that read r may observe. */
struct Problem {
	std::vector<Event> events{};
	std::vector<std::vector<EventId>> goodWrites{};
};

/** Exact search-effort counters for one solver invocation. */
struct Metrics {
	std::uint64_t statesDiscovered{};
	std::uint64_t statesExpanded{};
	std::uint64_t executableTransitions{};
	std::uint64_t duplicateStates{};
	std::uint64_t maximumWorklist{};
};

/** Outcome of input validation and SC witness search. */
enum class Status : std::uint8_t { witness, noWitness, invalidInput };

/** Solver outcome. A witness is populated only for Status::witness, while error is
 * populated only for Status::invalidInput. */
struct Result {
	Status status{Status::invalidInput};
	std::vector<EventId> witness{};
	/** Concrete active write selected for each read in witness; other entries are empty. */
	std::vector<std::optional<EventId>> readsFrom{};
	Metrics metrics{};
	std::string error{};
};

/** Decide whether the proper event set has an SC linearization in which every read
 * observes one of its good writes. This is an independent representative generator;
 * it does not use GenMC's built-in SC consistency checker. */
[[nodiscard]] auto verifySC(const Problem &problem) -> Result;

} /* namespace genmc::rvf */

#endif /* GENMC_SC_GOOD_WRITES_SOLVER_HPP */
