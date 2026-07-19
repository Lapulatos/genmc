/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "passes/FiniteSkeletonReplay.hpp"

#include "genmc/Execution/LoadAnnotation.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace genmc::skeleton {
namespace {

void addError(std::vector<std::string> &errors, std::string error)
{
	if (std::ranges::find(errors, error) == errors.end())
		errors.push_back(std::move(error));
}

using InstructionMap =
	std::unordered_map<std::string, std::vector<llvm::Instruction *>>;

auto indexInstructions(llvm::Module &module) -> InstructionMap
{
	InstructionMap result;
	for (auto &function : module) {
		if (function.isDeclaration())
			continue;
		auto &instructions = result[function.getName().str()];
		for (auto &block : function)
			for (auto &instruction : block)
				instructions.push_back(&instruction);
	}
	return result;
}

auto findInstruction(const Program &program, const InstructionMap &instructions,
		     NodeID function, std::uint32_t ordinal) -> llvm::Instruction *
{
	if (function >= program.functions.size() || ordinal == invalidInstruction)
		return nullptr;
	const auto found = instructions.find(program.functions[function].name);
	if (found == instructions.end() || ordinal >= found->second.size())
		return nullptr;
	return found->second[ordinal];
}

} /* namespace */

auto constrainReplayModule(llvm::Module &module, const Program &program,
			   const symbolic::FiniteAssignment &assignment)
	-> std::vector<std::string>
{
	std::vector<std::string> errors;
	const auto instructions = indexInstructions(module);
	if (assignment.values.size() != program.values.size()) {
		addError(errors, "replay-value-count-mismatch");
		return errors;
	}

	/* Freeze nondeterministic inputs before inserting any calls, preserving the stable
	 * instruction ordinals recorded from the unmodified transformed module. */
	std::vector<llvm::Instruction *> erase;
	for (const auto &value : program.values) {
		if (value.opcode != ValueOpcode::nondet)
			continue;
		if (!assignment.values[value.id]) {
			addError(errors, "replay-nondet-without-model-value");
			continue;
		}
		if (value.block >= program.blocks.size()) {
			addError(errors, "replay-nondet-invalid-block");
			continue;
		}
		auto *instruction = findInstruction(program, instructions,
			program.blocks[value.block].function, value.instruction);
		if (!instruction || !instruction->getType()->isIntegerTy()) {
			addError(errors, "replay-nondet-instruction-mismatch");
			continue;
		}
		auto *constant = llvm::ConstantInt::get(instruction->getType(),
						 *assignment.values[value.id]);
		instruction->replaceAllUsesWith(constant);
		erase.push_back(instruction);
	}
	for (auto *instruction : erase)
		instruction->eraseFromParent();

	auto *assume = module.getFunction("__VERIFIER_assume_internal");
	if (!assume) {
		addError(errors, "replay-assume-function-missing");
		return errors;
	}
	std::vector<bool> active(program.events.size(), false);
	for (const auto event : assignment.activeEvents) {
		if (event >= active.size()) {
			addError(errors, "replay-active-event-out-of-range");
			continue;
		}
		active[event] = true;
	}
	for (const auto &event : program.events) {
		if (!active[event.id] || event.kind != EventKind::load)
			continue;
		if (event.value >= assignment.values.size() || !assignment.values[event.value]) {
			addError(errors, "replay-load-without-model-value");
			continue;
		}
		auto *instruction = findInstruction(program, instructions, event.function,
						    event.instruction);
		auto *load = llvm::dyn_cast_or_null<llvm::LoadInst>(instruction);
		if (!load || !load->getType()->isIntegerTy()) {
			addError(errors, "replay-load-instruction-mismatch");
			continue;
		}
		auto *expected = llvm::ConstantInt::get(load->getType(),
						 *assignment.values[event.value]);
		auto *equal = new llvm::ICmpInst(load->getNextNode(), llvm::CmpInst::ICMP_EQ,
						 load, expected, "finite.replay.value");
		auto *type = llvm::ConstantInt::get(llvm::Type::getInt8Ty(module.getContext()),
			static_cast<std::underlying_type_t<AssumeType>>(AssumeType::User));
		auto *call = llvm::CallInst::Create(assume, {equal, type}, "", equal->getNextNode());
		call->setDebugLoc(load->getDebugLoc());
	}
	std::ranges::sort(errors);
	return errors;
}

} /* namespace genmc::skeleton */
