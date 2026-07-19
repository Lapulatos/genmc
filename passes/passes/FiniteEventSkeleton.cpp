/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "passes/FiniteEventSkeleton.hpp"

#include "passes/InternalFunctions.hpp"
#include "passes/LLVMUtils.hpp"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <algorithm>
#include <functional>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace genmc::skeleton {
namespace {

void addBlocker(Report &report, std::string blocker)
{
	if (std::ranges::find(report.blockers, blocker) == report.blockers.end())
		report.blockers.push_back(std::move(blocker));
}

auto hasStaticAddress(const llvm::Value *pointer) -> bool
{
	/* The first exact lane has no byte-offset or allocation-lifetime model. Admit only
	 * a direct global (possibly through pointer casts), never a GEP or alloca. */
	return llvm::isa<llvm::GlobalVariable>(pointer->stripPointerCasts());
}

void classifyCall(const llvm::CallBase &call, Report &report)
{
	const auto *callee = call.getCalledFunction();
	if (!callee) {
		++report.indirectCalls;
		addBlocker(report, "indirect-call");
		return;
	}
	if (callee->isIntrinsic() || !callee->isDeclaration())
		return;

	const auto name = callee->getName().str();
	if (name == "__VERIFIER_nondet_int")
		++report.nondetCalls;
	else if (isAssumeFunction(name))
		++report.assumeCalls;
	else if (name == "pthread_create" || name == "__VERIFIER_thread_create" ||
		 name == "__VERIFIER_thread_create_symmetric")
		++report.threadCreateCalls;
	else if (name == "pthread_join" || name == "__VERIFIER_thread_join")
		++report.threadJoinCalls;
	else if (name == "__assert_fail" || name == "__VERIFIER_error" ||
		 name == "__VERIFIER_assert_fail")
		++report.assertionCalls;

	/* Internal calls have explicit interpreter semantics and are admitted individually
	 * by the future encoder. Unknown external effects cannot enter the first subset. */
	if (!isInternalFunction(name) && name != "__assert_fail" &&
	    name != "__VERIFIER_error") {
		++report.unsupportedExternalCalls;
		addBlocker(report, "external-call:" + name);
	}
}

using FunctionSet = std::unordered_set<const llvm::Function *>;

auto referencedThreadEntry(const llvm::CallBase &call) -> const llvm::Function *
{
	const auto *callee = call.getCalledFunction();
	if (!callee || (callee->getName() != "__VERIFIER_thread_create" &&
			callee->getName() != "__VERIFIER_thread_create_symmetric") ||
	    call.arg_size() < 2)
		return nullptr;
	return llvm::dyn_cast<llvm::Function>(call.getArgOperand(1)->stripPointerCasts());
}

auto reachableFunctions(const llvm::Module &module) -> FunctionSet
{
	FunctionSet reachable;
	std::queue<const llvm::Function *> pending;
	if (const auto *main = module.getFunction("main")) {
		reachable.insert(main);
		pending.push(main);
	}
	while (!pending.empty()) {
		const auto *function = pending.front();
		pending.pop();
		for (const auto &instruction : llvm::instructions(function)) {
			const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
			if (!call)
				continue;
			const auto enqueue = [&](const llvm::Function *target) {
				if (target && !target->isDeclaration() && reachable.insert(target).second)
					pending.push(target);
			};
			enqueue(call->getCalledFunction());
			enqueue(referencedThreadEntry(*call));
		}
	}
	return reachable;
}

void findRecursion(const FunctionSet &reachable, Report &report)
{
	std::unordered_map<const llvm::Function *, FunctionSet> edges;
	for (const auto *function : reachable) {
		for (const auto &instruction : llvm::instructions(function)) {
			const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
			const auto *callee = call ? call->getCalledFunction() : nullptr;
			if (callee && reachable.contains(callee))
				edges[function].insert(callee);
		}
	}

	FunctionSet visiting;
	FunctionSet finished;
	FunctionSet recursive;
	std::function<void(const llvm::Function *)> visit = [&](const auto *function) {
		if (finished.contains(function))
			return;
		if (!visiting.insert(function).second) {
			recursive.insert(function);
			return;
		}
		for (const auto *callee : edges[function]) {
			if (visiting.contains(callee)) {
				recursive.insert(function);
				recursive.insert(callee);
			} else {
				visit(callee);
			}
		}
		visiting.erase(function);
		finished.insert(function);
	};
	for (const auto &[function, unused] : edges)
		visit(function);
	report.recursiveFunctions = recursive.size();
	if (!recursive.empty())
		addBlocker(report, "recursive-call-graph");
}

auto bitWidth(const llvm::Type *type) -> std::uint32_t
{
	return type->isIntegerTy() ? type->getIntegerBitWidth() : 0;
}

auto staticAddress(const llvm::Value *pointer) -> std::string
{
	if (const auto *global =
		    llvm::dyn_cast<llvm::GlobalVariable>(pointer->stripPointerCasts()))
		return global->getName().str();
	return {};
}

void addBuildBlocker(BuildResult &result, std::string blocker)
{
	if (std::ranges::find(result.blockers, blocker) == result.blockers.end())
		result.blockers.push_back(std::move(blocker));
}

} /* namespace */

auto analyze(const llvm::Module &module) -> Report
{
	Report report;
	const auto reachable = reachableFunctions(module);
	for (const auto *function : reachable) {
		++report.functions;
		report.basicBlocks += function->size();
		llvm::SmallVector<std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>, 4>
			backedges;
		llvm::FindFunctionBackedges(*function, backedges);
		report.backedges += backedges.size();
		for (const auto &instruction : llvm::instructions(function)) {
			++report.instructions;
			if (const auto *branch = llvm::dyn_cast<llvm::BranchInst>(&instruction);
			    branch && branch->isConditional())
				++report.conditionalBranches;
			if (const auto *load = llvm::dyn_cast<llvm::LoadInst>(&instruction)) {
				++report.loads;
				if (!hasStaticAddress(load->getPointerOperand()))
					++report.dynamicMemoryAddresses;
			} else if (const auto *store = llvm::dyn_cast<llvm::StoreInst>(&instruction)) {
				++report.stores;
				if (!hasStaticAddress(store->getPointerOperand()))
					++report.dynamicMemoryAddresses;
			} else if (llvm::isa<llvm::AtomicRMWInst, llvm::AtomicCmpXchgInst>(instruction)) {
				++report.atomicRmw;
			} else if (const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
				classifyCall(*call, report);
			}
		}
	}
	if (report.backedges != 0)
		addBlocker(report, "control-flow-backedge");
	if (report.dynamicMemoryAddresses != 0)
		addBlocker(report, "dynamic-memory-address");
	findRecursion(reachable, report);
	std::ranges::sort(report.blockers);
	return report;
}

auto build(const llvm::Module &module) -> BuildResult
{
	BuildResult result;
	Program program;
	program.report = analyze(module);
	if (!program.report.encodableSubset()) {
		result.blockers = program.report.blockers;
		return result;
	}

	const auto reachable = reachableFunctions(module);
	std::unordered_set<const llvm::Function *> threadEntries;
	for (const auto *function : reachable)
		for (const auto &instruction : llvm::instructions(function))
			if (const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction))
				if (const auto *entry = referencedThreadEntry(*call))
					threadEntries.insert(entry);

	std::unordered_map<const llvm::Function *, NodeID> functionIDs;
	std::unordered_map<const llvm::BasicBlock *, NodeID> blockIDs;
	std::unordered_map<const llvm::Value *, NodeID> valueIDs;
	std::unordered_map<const llvm::Instruction *, std::uint32_t> instructionOrdinals;
	for (const auto &global : module.globals()) {
		const auto *initializer = global.hasInitializer()
						  ? llvm::dyn_cast<llvm::ConstantInt>(global.getInitializer())
						  : nullptr;
		if (!initializer)
			continue;
		program.initialValues.push_back({.address = global.getName().str(),
						 .width = bitWidth(initializer->getType()),
						 .value = initializer->getValue().getLimitedValue()});
	}

	/* Assign IDs before lowering operands so PHIs and forward CFG edges are valid. */
	for (const auto *function : reachable) {
		const auto functionID = static_cast<NodeID>(program.functions.size());
		functionIDs.emplace(function, functionID);
		FunctionNode node{.id = functionID,
				  .name = function->getName().str(),
				  .isMain = function->getName() == "main",
				  .isThreadEntry = threadEntries.contains(function)};
		for (const auto &argument : function->args()) {
			if (!argument.getType()->isIntegerTy())
				continue;
			const auto id = static_cast<NodeID>(program.values.size());
			valueIDs.emplace(&argument, id);
			node.arguments.push_back(id);
			program.values.push_back({.id = id,
						  .opcode = ValueOpcode::argument,
						  .width = bitWidth(argument.getType())});
		}
		program.functions.push_back(std::move(node));
		for (const auto &block : *function) {
			const auto id = static_cast<NodeID>(program.blocks.size());
			blockIDs.emplace(&block, id);
			program.blocks.push_back({.id = id, .function = functionID});
			program.functions[functionID].blocks.push_back(id);
		}
		program.functions[functionID].entry = blockIDs.at(&function->getEntryBlock());
		std::uint32_t instructionOrdinal{};
		for (const auto &instruction : llvm::instructions(function)) {
			instructionOrdinals.emplace(&instruction, instructionOrdinal++);
			if (!instruction.getType()->isIntegerTy())
				continue;
			const auto id = static_cast<NodeID>(program.values.size());
			valueIDs.emplace(&instruction, id);
			program.values.push_back({.id = id,
						  .block = blockIDs.at(instruction.getParent()),
						  .instruction = instructionOrdinals.at(&instruction),
						  .width = bitWidth(instruction.getType())});
		}
	}

	const auto valueRef = [&](const llvm::Value *value) -> NodeID {
		if (const auto found = valueIDs.find(value); found != valueIDs.end())
			return found->second;
		if (const auto *constant = llvm::dyn_cast<llvm::ConstantInt>(value)) {
			const auto id = static_cast<NodeID>(program.values.size());
			valueIDs.emplace(value, id);
			program.values.push_back({.id = id,
						  .opcode = ValueOpcode::constant,
						  .width = bitWidth(constant->getType()),
						  .constant = constant->getValue().getLimitedValue()});
			return id;
		}
		return invalidNode;
	};
	const auto addEvent = [&](EventSite event) {
		event.id = static_cast<NodeID>(program.events.size());
		program.events.push_back(std::move(event));
	};

	for (const auto *function : reachable) {
		const auto functionID = functionIDs.at(function);
		for (const auto &block : *function) {
			auto &blockNode = program.blocks[blockIDs.at(&block)];
			for (const auto *predecessor : llvm::predecessors(&block))
				blockNode.predecessors.push_back(blockIDs.at(predecessor));
			for (const auto *successor : llvm::successors(&block))
				blockNode.successors.push_back(blockIDs.at(successor));
			if (const auto *branch = llvm::dyn_cast<llvm::BranchInst>(block.getTerminator());
			    branch && branch->isConditional())
				blockNode.condition = valueRef(branch->getCondition());
			if (!llvm::isa<llvm::BranchInst, llvm::ReturnInst, llvm::UnreachableInst>(
				    block.getTerminator()))
				addBuildBlocker(result, std::string{"unsupported-terminator:"} +
							 block.getTerminator()->getOpcodeName());

			for (const auto &instruction : block) {
				const auto instructionOrdinal = instructionOrdinals.at(&instruction);
				const auto own = valueIDs.contains(&instruction)
							 ? valueIDs.at(&instruction)
							 : invalidNode;
				auto setOperands = [&](ValueOpcode opcode) {
					std::vector<NodeID> operands;
					for (const auto &operand : instruction.operands()) {
						const auto ref = valueRef(operand.get());
						if (ref == invalidNode)
							addBuildBlocker(result, "unsupported-value-operand");
						operands.push_back(ref);
					}
					/* valueRef() may append a constant and reallocate program.values.
					 * Do not retain a ValueNode reference across those calls. */
					program.values[own].opcode = opcode;
					program.values[own].operands = std::move(operands);
				};

				if (const auto *load = llvm::dyn_cast<llvm::LoadInst>(&instruction)) {
					if (own == invalidNode) {
						addBuildBlocker(result, "unsupported-load-type");
						continue;
					}
					program.values[own].opcode = ValueOpcode::load;
					addEvent({.kind = EventKind::load,
						  .function = functionID,
						  .block = blockNode.id,
						  .instruction = instructionOrdinal,
						  .value = own,
						  .address = staticAddress(load->getPointerOperand()),
						  .width = bitWidth(load->getType())});
					program.events.back().sequentiallyConsistent =
						load->isAtomic() &&
						load->getOrdering() == llvm::AtomicOrdering::SequentiallyConsistent;
				} else if (const auto *store = llvm::dyn_cast<llvm::StoreInst>(&instruction)) {
					if (!store->getValueOperand()->getType()->isIntegerTy()) {
						addBuildBlocker(result, "unsupported-store-type");
						continue;
					}
					const auto stored = valueRef(store->getValueOperand());
					if (stored == invalidNode)
						addBuildBlocker(result, "unsupported-store-value");
					addEvent({.kind = EventKind::store,
						  .function = functionID,
						  .block = blockNode.id,
						  .instruction = instructionOrdinal,
						  .value = stored,
						  .address = staticAddress(store->getPointerOperand()),
						  .width = bitWidth(store->getValueOperand()->getType())});
					program.events.back().sequentiallyConsistent =
						store->isAtomic() && store->getOrdering() ==
								     llvm::AtomicOrdering::SequentiallyConsistent;
				} else if (const auto *fence =
						   llvm::dyn_cast<llvm::FenceInst>(&instruction)) {
					addEvent({.kind = EventKind::fence,
						  .function = functionID,
						  .block = blockNode.id,
						  .instruction = instructionOrdinal,
						  .sequentiallyConsistent =
							  fence->getOrdering() ==
								  llvm::AtomicOrdering::SequentiallyConsistent});
				} else if (const auto *phi = llvm::dyn_cast<llvm::PHINode>(&instruction)) {
					std::vector<NodeID> operands;
					std::vector<NodeID> incomingBlocks;
					for (auto i = 0U; i < phi->getNumIncomingValues(); ++i) {
						operands.push_back(valueRef(phi->getIncomingValue(i)));
						incomingBlocks.push_back(
							blockIDs.at(phi->getIncomingBlock(i)));
					}
					program.values[own].opcode = ValueOpcode::phi;
					program.values[own].operands = std::move(operands);
					program.values[own].incomingBlocks = std::move(incomingBlocks);
				} else if (const auto *compare =
						   llvm::dyn_cast<llvm::ICmpInst>(&instruction)) {
					setOperands(ValueOpcode::icmp);
					program.values[own].predicate = compare->getPredicate();
				} else if (llvm::isa<llvm::TruncInst>(instruction)) {
					setOperands(ValueOpcode::trunc);
				} else if (llvm::isa<llvm::ZExtInst>(instruction)) {
					setOperands(ValueOpcode::zext);
				} else if (llvm::isa<llvm::SExtInst>(instruction)) {
					setOperands(ValueOpcode::sext);
				} else if (llvm::isa<llvm::FreezeInst>(instruction)) {
					setOperands(ValueOpcode::freeze);
				} else if (const auto *binary =
						   llvm::dyn_cast<llvm::BinaryOperator>(&instruction)) {
					const auto opcode = [&]() -> std::optional<ValueOpcode> {
						switch (binary->getOpcode()) {
						case llvm::Instruction::Add: return ValueOpcode::add;
						case llvm::Instruction::Sub: return ValueOpcode::sub;
						case llvm::Instruction::And: return ValueOpcode::bitAnd;
						case llvm::Instruction::Or: return ValueOpcode::bitOr;
						case llvm::Instruction::Xor: return ValueOpcode::bitXor;
						default: return std::nullopt;
						}
					}();
					if (opcode)
						setOperands(*opcode);
					else
						addBuildBlocker(result, std::string{"unsupported-instruction:"} +
									 instruction.getOpcodeName());
				} else if (const auto *call =
						   llvm::dyn_cast<llvm::CallBase>(&instruction)) {
					const auto *callee = call->getCalledFunction();
					if (!callee) {
						addBuildBlocker(result, "indirect-call");
						continue;
					}
					if (callee->isIntrinsic()) {
						if (own != invalidNode)
							addBuildBlocker(result, "unsupported-intrinsic:" +
										 callee->getName().str());
						continue;
					}
					const auto name = callee->getName().str();
					if (name == "__VERIFIER_nondet_int") {
						program.values[own].opcode = ValueOpcode::nondet;
						continue;
					}
					EventSite event{.kind = EventKind::directCall,
							.function = functionID,
							.block = blockNode.id,
							.instruction = instructionOrdinal,
							.value = own,
							.callee = name};
					if (isAssumeFunction(name))
						event.kind = EventKind::assume;
					else if (isErrorFunction(name) || name == "__VERIFIER_error" ||
						 name == "__assert_fail")
						event.kind = EventKind::error;
					else if (name == "__VERIFIER_thread_create" ||
						 name == "__VERIFIER_thread_create_symmetric")
						event.kind = EventKind::threadCreate;
					else if (name == "__VERIFIER_thread_join")
						event.kind = EventKind::threadJoin;
					else if (name == "__VERIFIER_mutex_lock")
						event.kind = EventKind::lock;
					else if (name == "__VERIFIER_mutex_unlock")
						event.kind = EventKind::unlock;
					if (event.kind == EventKind::threadCreate)
						if (const auto *entry = referencedThreadEntry(*call))
							event.threadEntry = entry->getName().str();
					if ((event.kind == EventKind::lock ||
					     event.kind == EventKind::unlock) && call->arg_size() != 0)
						event.address = staticAddress(call->getArgOperand(0));
					for (const auto &argument : call->args())
						if (argument->getType()->isIntegerTy())
							event.arguments.push_back(valueRef(argument.get()));
					if (own != invalidNode) {
						program.values[own].opcode = callee->isDeclaration()
										     ? ValueOpcode::constant
										     : ValueOpcode::call;
						program.values[own].constant = 0;
					}
					addEvent(std::move(event));
				} else if (const auto *ret =
						   llvm::dyn_cast<llvm::ReturnInst>(&instruction)) {
					if (ret->getReturnValue() && ret->getReturnValue()->getType()->isIntegerTy())
						addEvent({.kind = EventKind::returnValue,
							  .function = functionID,
							  .block = blockNode.id,
							  .instruction = instructionOrdinal,
							  .value = valueRef(ret->getReturnValue())});
				} else if (own != invalidNode) {
					addBuildBlocker(result, std::string{"unsupported-instruction:"} +
								 instruction.getOpcodeName());
				}
			}
		}
	}

	std::ranges::sort(result.blockers);
	std::unordered_map<std::string, std::uint32_t> threadInstances;
	for (const auto &function : program.functions)
		/* Call/argument binding is not encoded yet. Treating integer arguments as
		 * independent symbolic inputs would over-approximate TRUE and is therefore
		 * forbidden by the first exact lane. */
		if (!function.arguments.empty())
			addBuildBlocker(result, "integer-function-argument:" + function.name);
	for (const auto &event : program.events) {
		if (event.kind == EventKind::threadCreate && !event.threadEntry.empty() &&
		    ++threadInstances[event.threadEntry] > 1)
			addBuildBlocker(result, "reused-thread-entry:" + event.threadEntry);
		if (event.kind == EventKind::threadCreate && event.threadEntry.empty())
			addBuildBlocker(result, "unresolved-thread-entry");
		if (event.kind == EventKind::threadJoin)
			addBuildBlocker(result, "unmodeled-thread-join");
		if (event.kind == EventKind::directCall)
			addBuildBlocker(result, "unmodeled-direct-call:" + event.callee);
		if ((event.kind == EventKind::lock || event.kind == EventKind::unlock) &&
		    event.address.empty())
			addBuildBlocker(result, "dynamic-mutex-address");
	}
	std::ranges::sort(result.blockers);
	if (result.blockers.empty())
		result.program = std::move(program);
	return result;
}

auto format(const Report &r) -> std::string
{
	std::ostringstream out;
	out << "finite-control=" << r.finiteControl() << " encodable-subset="
	    << r.encodableSubset() << " functions=" << r.functions << " blocks="
	    << r.basicBlocks << " instructions=" << r.instructions << " branches="
	    << r.conditionalBranches << " loads=" << r.loads << " stores=" << r.stores
	    << " rmw=" << r.atomicRmw << " nondet=" << r.nondetCalls << " assumes="
	    << r.assumeCalls << " creates=" << r.threadCreateCalls << " joins="
	    << r.threadJoinCalls << " assertions=" << r.assertionCalls << " backedges="
	    << r.backedges << " indirect-calls=" << r.indirectCalls << " recursive-functions="
	    << r.recursiveFunctions << " dynamic-addresses=" << r.dynamicMemoryAddresses
	    << " unsupported-externals=" << r.unsupportedExternalCalls << " blockers=";
	for (std::size_t i = 0; i < r.blockers.size(); ++i) {
		if (i != 0)
			out << ',';
		out << r.blockers[i];
	}
	return out.str();
}

} /* namespace genmc::skeleton */
