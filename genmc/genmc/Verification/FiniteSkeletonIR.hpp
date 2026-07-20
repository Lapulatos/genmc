/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_SKELETON_IR_HPP
#define GENMC_FINITE_SKELETON_IR_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace genmc::skeleton {

struct Report {
	std::uint64_t functions{};
	std::uint64_t basicBlocks{};
	std::uint64_t instructions{};
	std::uint64_t conditionalBranches{};
	std::uint64_t loads{};
	std::uint64_t stores{};
	std::uint64_t atomicRmw{};
	std::uint64_t nondetCalls{};
	std::uint64_t assumeCalls{};
	std::uint64_t threadCreateCalls{};
	std::uint64_t threadJoinCalls{};
	std::uint64_t assertionCalls{};
	std::uint64_t backedges{};
	std::uint64_t indirectCalls{};
	std::uint64_t recursiveFunctions{};
	std::uint64_t dynamicMemoryAddresses{};
	std::uint64_t unsupportedExternalCalls{};
	std::vector<std::string> blockers{};

	[[nodiscard]] auto finiteControl() const -> bool
	{
		return backedges == 0 && indirectCalls == 0 && recursiveFunctions == 0;
	}
	[[nodiscard]] auto encodableSubset() const -> bool
	{
		return finiteControl() && dynamicMemoryAddresses == 0 &&
		       unsupportedExternalCalls == 0;
	}
};

using NodeID = std::uint32_t;
inline constexpr auto invalidNode = static_cast<NodeID>(-1);
inline constexpr auto invalidInstruction = static_cast<std::uint32_t>(-1);

enum class ValueOpcode : std::uint8_t {
	argument,
	constant,
	phi,
	icmp,
	trunc,
	zext,
	sext,
	freeze,
	add,
	sub,
	bitAnd,
	bitOr,
	bitXor,
	load,
	nondet,
	call
};

struct ValueNode {
	NodeID id{invalidNode};
	NodeID block{invalidNode};
	/** Stable ordinal among LLVM instructions in the containing function. */
	std::uint32_t instruction{invalidInstruction};
	ValueOpcode opcode{};
	std::uint32_t width{};
	std::uint64_t constant{};
	std::uint32_t predicate{};
	std::vector<NodeID> operands{};
	std::vector<NodeID> incomingBlocks{};
};

enum class EventKind : std::uint8_t {
	load,
	store,
	assume,
	error,
	threadCreate,
	threadJoin,
	lock,
	unlock,
	fence,
	directCall,
	returnValue
};

struct EventSite {
	NodeID id{invalidNode};
	EventKind kind{};
	NodeID function{invalidNode};
	NodeID block{invalidNode};
	/** Stable ordinal among LLVM instructions in the containing function. */
	std::uint32_t instruction{invalidInstruction};
	NodeID value{invalidNode};
	std::string address{};
	std::string callee{};
	std::string threadEntry{};
	/** Statically resolved target entry for a threadJoin event. */
	std::string joinedThreadEntry{};
	/** Exact threadCreate event whose handle is consumed by threadJoin. */
	NodeID joinedThreadCreate{invalidNode};
	std::uint32_t width{};
	bool sequentiallyConsistent{};
	std::vector<NodeID> arguments{};
};

struct InitialValue {
	std::string address{};
	std::uint32_t width{};
	std::uint64_t value{};
};

struct BlockNode {
	NodeID id{invalidNode};
	NodeID function{invalidNode};
	std::vector<NodeID> predecessors{};
	std::vector<NodeID> successors{};
	NodeID condition{invalidNode};
};

struct FunctionNode {
	NodeID id{invalidNode};
	std::string name{};
	NodeID entry{invalidNode};
	bool isMain{};
	bool isThreadEntry{};
	std::vector<NodeID> arguments{};
	std::vector<NodeID> blocks{};
};

struct Program {
	Report report{};
	std::vector<FunctionNode> functions{};
	std::vector<BlockNode> blocks{};
	std::vector<ValueNode> values{};
	std::vector<EventSite> events{};
	std::vector<InitialValue> initialValues{};
};

struct BuildResult {
	std::optional<Program> program{};
	std::vector<std::string> blockers{};
};

} /* namespace genmc::skeleton */

#endif
