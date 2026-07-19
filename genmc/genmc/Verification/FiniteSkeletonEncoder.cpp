/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSkeletonEncoder.hpp"
#include "genmc/Verification/FiniteSkeletonCAT.hpp"

#include <algorithm>
#include <bit>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace genmc::symbolic {
namespace {

struct Edge {
	skeleton::NodeID from{};
	skeleton::NodeID to{};
	auto operator==(const Edge &) const -> bool = default;
};

struct EdgeHash {
	auto operator()(const Edge &edge) const -> std::size_t
	{
		return (static_cast<std::size_t>(edge.from) << 32U) ^ edge.to;
	}
};

struct RfChoice {
	skeleton::NodeID load{};
	skeleton::NodeID store{skeleton::invalidNode};
	Expr selected{};
	std::vector<skeleton::NodeID> members{};
	std::vector<std::pair<skeleton::NodeID, Expr>> refinedMembers{};
};

struct CoRank {
	skeleton::NodeID store{};
	Expr rank{};
};

struct BvDecision {
	Expr expression{};
	std::uint32_t width{};
};

using StaticValueKey = std::tuple<bool, std::uint32_t, std::uint64_t>;
using StaticValueProvenanceKey = std::tuple<StaticValueKey, skeleton::NodeID, bool>;

auto constantKey(std::uint32_t width, std::uint64_t value) -> StaticValueKey
{
	return {true, width, value};
}

auto storeValueKey(const skeleton::Program &program, const skeleton::EventSite &store,
		   std::uint32_t width) -> StaticValueKey
{
	if (store.kind == skeleton::EventKind::unlock)
		return constantKey(width, 0);
	const auto &value = program.values.at(store.value);
	return value.opcode == skeleton::ValueOpcode::constant
		       ? constantKey(value.width, value.constant)
		       : StaticValueKey{false, value.width, value.id};
}

auto isOrderingEvent(const skeleton::EventSite &event) -> bool
{
	using enum skeleton::EventKind;
	return event.kind == load || event.kind == store || event.kind == lock ||
	       event.kind == unlock || event.kind == fence || event.kind == threadCreate ||
	       event.kind == threadJoin;
}

void addUnique(std::vector<std::string> &blockers, std::string blocker)
{
	if (std::ranges::find(blockers, blocker) == blockers.end())
		blockers.push_back(std::move(blocker));
}

} /* namespace */

auto censusFiniteRepresentation(const skeleton::Program &program)
	-> FiniteRepresentationCensus
{
	FiniteRepresentationCensus result;
	result.blocks = program.blocks.size();
	for (const auto &block : program.blocks)
		result.cfgEdges += block.successors.size();
	for (const auto &value : program.values) {
		if (value.opcode != skeleton::ValueOpcode::constant) {
			++result.valueVariables;
			result.valueBits += value.width;
		}
	}
	for (const auto &event : program.events)
		if (event.kind == skeleton::EventKind::error)
			++result.errorEvents;

	std::map<std::string, std::vector<const skeleton::EventSite *>> writes;
	for (const auto &event : program.events)
		if (event.kind == skeleton::EventKind::store ||
		    event.kind == skeleton::EventKind::unlock ||
		    event.kind == skeleton::EventKind::lock)
			writes[event.address].push_back(&event);

	const auto hasInitial = [&](const skeleton::EventSite &load, std::uint32_t width) {
		if (load.kind == skeleton::EventKind::lock)
			return true;
		return std::ranges::any_of(program.initialValues, [&](const auto &initial) {
			return initial.address == load.address && initial.width == width;
		});
	};
	for (const auto &load : program.events) {
		if (load.kind != skeleton::EventKind::load &&
		    load.kind != skeleton::EventKind::lock)
			continue;
		++result.rfReads;
		const auto width = load.kind == skeleton::EventKind::lock ? 1U : load.width;
		std::vector<StaticValueProvenanceKey> sourceClasses;
		if (hasInitial(load, width)) {
			const auto initialValue = load.kind == skeleton::EventKind::lock
						  ? 0
						  : std::ranges::find_if(
							    program.initialValues,
							    [&](const auto &initial) {
								    return initial.address == load.address &&
									   initial.width == width;
							    })->value;
			sourceClasses.emplace_back(constantKey(width, initialValue),
						   skeleton::invalidNode, true);
		}
		for (const auto &store : program.events)
			if (store.address == load.address &&
			    ((load.kind == skeleton::EventKind::load &&
			      store.kind == skeleton::EventKind::store && store.width == width) ||
			     (load.kind == skeleton::EventKind::lock &&
			      store.kind == skeleton::EventKind::unlock)))
				sourceClasses.emplace_back(storeValueKey(program, store, width),
							   store.function, false);
		const auto sources = static_cast<std::uint64_t>(sourceClasses.size());
		std::map<StaticValueKey, std::uint64_t> valueClassSizes;
		std::map<StaticValueProvenanceKey, std::uint64_t> provenanceClassSizes;
		for (const auto &source : sourceClasses) {
			++valueClassSizes[std::get<0>(source)];
			++provenanceClassSizes[source];
		}
		const auto valueClasses = static_cast<std::uint64_t>(valueClassSizes.size());
		const auto provenanceClasses =
			static_cast<std::uint64_t>(provenanceClassSizes.size());
		result.rfSelectors += sources;
		if (sources == 0)
			++result.rfReadsWithoutSource;
		else
			result.rfPairs += sources * (sources - 1) / 2;
		result.rfValueClasses += valueClasses;
		result.rfValueClassPairs += valueClasses * (valueClasses - 1) / 2;
		result.rfValueProvenanceClasses += provenanceClasses;
		result.rfValueProvenanceClassPairs +=
			provenanceClasses * (provenanceClasses - 1) / 2;
		result.rfValueMergeableSources += sources - valueClasses;
		result.rfValueProvenanceMergeableSources += sources - provenanceClasses;
		result.rfReadsWithValueMerge += valueClasses < sources;
		result.rfReadsWithValueProvenanceMerge += provenanceClasses < sources;
		for (const auto &[unusedKey, size] : valueClassSizes)
			result.maximumRfValueClassSize =
				std::max(result.maximumRfValueClassSize, size);
		for (const auto &[unusedKey, size] : provenanceClassSizes)
			result.maximumRfValueProvenanceClassSize =
				std::max(result.maximumRfValueProvenanceClassSize, size);
		result.rfLoadActivations += sources;
		result.rfStoreActivations += sources - (hasInitial(load, width) ? 1 : 0);
		if (load.kind == skeleton::EventKind::load)
			result.rfValueConstraints += sources;
		result.maximumRfSources = std::max(result.maximumRfSources, sources);
		const auto found = writes.find(load.address);
		const auto writeCount = found == writes.end() ? 0 : found->second.size();
		result.potentialFrDerivations += sources * writeCount;
	}

	for (const auto &[unusedAddress, addressWrites] : writes) {
		const auto count = static_cast<std::uint64_t>(addressWrites.size());
		const auto width = std::max(1U, static_cast<unsigned>(std::bit_width(count - 1)));
		result.coRanks += count;
		result.coRankBits += count * width;
		result.coPairs += count * (count - 1) / 2;
		result.maximumWritesPerAddress =
			std::max(result.maximumWritesPerAddress, count);
	}
	for (std::size_t i = 0; i < program.events.size(); ++i)
		for (std::size_t j = i + 1; j < program.events.size(); ++j)
			if (isOrderingEvent(program.events[i]) && isOrderingEvent(program.events[j]) &&
			    program.events[i].function == program.events[j].function)
				++result.poPairs;
	return result;
}

auto format(const FiniteRepresentationCensus &c) -> std::string
{
	std::ostringstream out;
	out << "blocks=" << c.blocks << " cfg-edges=" << c.cfgEdges
	    << " value-vars=" << c.valueVariables << " value-bits=" << c.valueBits
	    << " error-events=" << c.errorEvents << " rf-reads=" << c.rfReads
	    << " rf-reads-without-source=" << c.rfReadsWithoutSource
	    << " rf-selectors=" << c.rfSelectors << " rf-pairs=" << c.rfPairs
	    << " rf-value-classes=" << c.rfValueClasses
	    << " rf-value-class-pairs=" << c.rfValueClassPairs
	    << " rf-value-provenance-classes=" << c.rfValueProvenanceClasses
	    << " rf-value-provenance-class-pairs=" << c.rfValueProvenanceClassPairs
	    << " rf-value-mergeable-sources=" << c.rfValueMergeableSources
	    << " rf-value-provenance-mergeable-sources="
	    << c.rfValueProvenanceMergeableSources
	    << " rf-reads-with-value-merge=" << c.rfReadsWithValueMerge
	    << " rf-reads-with-value-provenance-merge="
	    << c.rfReadsWithValueProvenanceMerge
	    << " rf-load-activations=" << c.rfLoadActivations
	    << " rf-store-activations=" << c.rfStoreActivations
	    << " rf-value=" << c.rfValueConstraints << " co-ranks=" << c.coRanks
	    << " co-rank-bits=" << c.coRankBits << " co-pairs=" << c.coPairs
	    << " co-before-vars=" << c.coPairs << " po-pairs=" << c.poPairs
	    << " potential-fr=" << c.potentialFrDerivations
	    << " max-rf-sources=" << c.maximumRfSources
	    << " max-rf-value-class-size=" << c.maximumRfValueClassSize
	    << " max-rf-value-provenance-class-size="
	    << c.maximumRfValueProvenanceClassSize
	    << " max-writes-address=" << c.maximumWritesPerAddress
	    << " yogar-removed-co-ranks=" << c.coRanks
	    << " yogar-removed-co-rank-bits=" << c.coRankBits
	    << " yogar-removed-co-pairs=" << c.coPairs
	    << " yogar-removed-co-before-vars=" << c.coPairs;
	return out.str();
}

class FiniteSkeletonEncoder::Impl {
public:
	explicit Impl(const skeleton::Program &source, FiniteEncodingOptions encodingOptions)
		: program(source), options(encodingOptions)
	{
		if (!Solver::backendAvailable()) {
			addUnique(reasons, "solver-unavailable");
			return;
		}
		validateValueShapes();
		if (!reasons.empty())
			return;
		encode();
	}

	const skeleton::Program &program;
	FiniteEncodingOptions options{};
	Solver solver{};
	std::vector<std::string> reasons{};
	std::vector<Expr> blocks{};
	std::vector<Expr> values{};
	std::unordered_map<Edge, Expr, EdgeHash> edges{};
	std::vector<RfChoice> rf{};
	std::vector<CoRank> co{};
	std::vector<Expr> boolDecisions{};
	std::vector<Expr> graphDecisions{};
	std::vector<BvDecision> bvDecisions{};
	Expr lastGraphClause{};
	Expr pendingAssignmentClause{};
	std::string lastExplanationFailure{};

	void validateValueShapes()
	{
		for (const auto &node : program.values) {
			const auto require = [&](std::size_t count) {
				if (node.operands.size() != count)
					addUnique(reasons, "value-arity:" + std::to_string(node.id));
			};
			switch (node.opcode) {
			case skeleton::ValueOpcode::argument:
			case skeleton::ValueOpcode::constant:
			case skeleton::ValueOpcode::load:
			case skeleton::ValueOpcode::nondet: require(0); break;
			case skeleton::ValueOpcode::phi:
				if (node.operands.empty() ||
				    node.operands.size() != node.incomingBlocks.size())
					addUnique(reasons, "value-arity:" + std::to_string(node.id));
				break;
			case skeleton::ValueOpcode::icmp:
			case skeleton::ValueOpcode::add:
			case skeleton::ValueOpcode::sub:
			case skeleton::ValueOpcode::bitAnd:
			case skeleton::ValueOpcode::bitOr:
			case skeleton::ValueOpcode::bitXor: require(2); break;
			case skeleton::ValueOpcode::trunc:
			case skeleton::ValueOpcode::zext:
			case skeleton::ValueOpcode::sext:
			case skeleton::ValueOpcode::freeze: require(1); break;
			case skeleton::ValueOpcode::call: break;
			}
			for (const auto operand : node.operands)
				if (operand >= program.values.size())
					addUnique(reasons, "value-operand-range:" +
							       std::to_string(node.id));
			for (const auto block : node.incomingBlocks)
				if (block >= program.blocks.size())
					addUnique(reasons, "value-block-range:" +
							       std::to_string(node.id));
		}
	}

	auto truth(Expr value, std::uint32_t width) -> Expr
	{
		return solver.logicalNot(solver.equal(value, solver.bitVectorConstant(0, width)));
	}

	auto compare(const skeleton::ValueNode &node) -> Expr
	{
		const auto lhs = values.at(node.operands.at(0));
		const auto rhs = values.at(node.operands.at(1));
		/* LLVM's stable ICmp predicate encoding (llvm::CmpInst::Predicate). */
		switch (node.predicate) {
		case 32: return solver.equal(lhs, rhs); /* eq */
		case 33: return solver.logicalNot(solver.equal(lhs, rhs)); /* ne */
		case 36: return solver.unsignedLess(lhs, rhs); /* ult */
		case 37: /* ule */
			return solver.logicalNot(solver.unsignedLess(rhs, lhs));
		case 34: return solver.unsignedLess(rhs, lhs); /* ugt */
		case 35: /* uge */
			return solver.logicalNot(solver.unsignedLess(lhs, rhs));
		case 40: return solver.signedLess(lhs, rhs); /* slt */
		case 41: /* sle */
			return solver.logicalNot(solver.signedLess(rhs, lhs));
		case 38: return solver.signedLess(rhs, lhs); /* sgt */
		case 39: /* sge */
			return solver.logicalNot(solver.signedLess(lhs, rhs));
		default:
			addUnique(reasons, "unsupported-icmp-predicate");
			return solver.boolean("invalid_icmp");
		}
	}

	auto initial(const std::string &address, std::uint32_t width) const
		-> std::optional<std::uint64_t>
	{
		for (const auto &value : program.initialValues)
			if (value.address == address && value.width == width)
				return value.value;
		return std::nullopt;
	}

	void encodeCFG()
	{
		blocks.reserve(program.blocks.size());
		for (const auto &block : program.blocks)
			blocks.push_back(solver.boolean("block_" + std::to_string(block.id)));

		for (const auto &block : program.blocks) {
			for (std::size_t index = 0; index < block.successors.size(); ++index) {
				const auto successor = block.successors[index];
				auto edge = solver.boolean("edge_" + std::to_string(block.id) + "_" +
							   std::to_string(successor));
				edges.emplace(Edge{block.id, successor}, edge);
				auto enabled = blocks[block.id];
				if (block.condition != skeleton::invalidNode) {
					auto condition = truth(values[block.condition],
							       program.values[block.condition].width);
					if (index != 0)
						condition = solver.logicalNot(condition);
					const Expr terms[]{enabled, condition};
					enabled = solver.allOf(terms);
				}
				solver.constrain(solver.equal(edge, enabled));
			}
		}

		for (const auto &function : program.functions) {
			std::vector<Expr> entrySources;
			if (function.isMain)
				entrySources.push_back(solver.boolean("main_enabled"));
			for (const auto &event : program.events) {
				if ((event.kind == skeleton::EventKind::threadCreate &&
				     event.threadEntry == function.name) ||
				    (event.kind == skeleton::EventKind::directCall &&
				     event.callee == function.name))
					entrySources.push_back(blocks[event.block]);
			}
			if (function.isMain)
				solver.constrain(entrySources.front());
			if (entrySources.empty()) {
				addUnique(reasons, "function-entry-without-call:" + function.name);
				continue;
			}
			solver.constrain(
				solver.equal(blocks[function.entry], solver.anyOf(entrySources)));
		}

		for (const auto &block : program.blocks) {
			const auto &function = program.functions[block.function];
			if (block.id == function.entry)
				continue;
			std::vector<Expr> incoming;
			for (const auto predecessor : block.predecessors)
				incoming.push_back(edges.at(Edge{predecessor, block.id}));
			solver.constrain(solver.equal(blocks[block.id], solver.anyOf(incoming)));
		}
	}

	void encodeValues()
	{
		values.reserve(program.values.size());
		for (const auto &node : program.values) {
			if (node.width == 0) {
				addUnique(reasons, "zero-width-value");
				values.push_back(solver.bitVector("invalid_value", 1));
				continue;
			}
			if (node.opcode == skeleton::ValueOpcode::constant)
				values.push_back(solver.bitVectorConstant(node.constant, node.width));
			else
				values.push_back(solver.bitVector("value_" + std::to_string(node.id),
								  node.width));
			if (node.opcode == skeleton::ValueOpcode::nondet ||
			    node.opcode == skeleton::ValueOpcode::argument)
				bvDecisions.push_back({values.back(), node.width});
		}
	}

	void encodeDefinitions()
	{
		for (const auto &node : program.values) {
			if (node.opcode == skeleton::ValueOpcode::constant ||
			    node.opcode == skeleton::ValueOpcode::argument ||
			    node.opcode == skeleton::ValueOpcode::load ||
			    node.opcode == skeleton::ValueOpcode::nondet)
				continue;
			if (node.opcode == skeleton::ValueOpcode::call) {
				addUnique(reasons, "live-call-result");
				continue;
			}
			Expr expression;
			switch (node.opcode) {
			case skeleton::ValueOpcode::phi: {
				if (node.operands.empty()) {
					addUnique(reasons, "empty-phi");
					continue;
				}
				expression = values[node.operands.back()];
				for (std::size_t i = node.operands.size() - 1; i-- > 0;) {
					const auto edge = edges.at(Edge{node.incomingBlocks[i], node.block});
					expression = solver.select(edge, values[node.operands[i]], expression);
				}
				break;
			}
			case skeleton::ValueOpcode::icmp: {
				const auto condition = compare(node);
				expression = solver.select(condition, solver.bitVectorConstant(1, 1),
							   solver.bitVectorConstant(0, 1));
				break;
			}
			case skeleton::ValueOpcode::trunc:
				expression = solver.bitVectorTruncate(values[node.operands.at(0)],
								      node.width);
				break;
			case skeleton::ValueOpcode::zext:
				expression = solver.bitVectorZeroExtend(values[node.operands.at(0)],
									 node.width);
				break;
			case skeleton::ValueOpcode::sext:
				expression = solver.bitVectorSignExtend(values[node.operands.at(0)],
									 node.width);
				break;
			case skeleton::ValueOpcode::freeze:
				expression = values[node.operands.at(0)];
				break;
			case skeleton::ValueOpcode::add:
				expression = solver.bitVectorAdd(values[node.operands.at(0)],
								 values[node.operands.at(1)]);
				break;
			case skeleton::ValueOpcode::sub:
				expression = solver.bitVectorSub(values[node.operands.at(0)],
								 values[node.operands.at(1)]);
				break;
			case skeleton::ValueOpcode::bitAnd:
				expression = solver.bitVectorAnd(values[node.operands.at(0)],
								 values[node.operands.at(1)]);
				break;
			case skeleton::ValueOpcode::bitOr:
				expression = solver.bitVectorOr(values[node.operands.at(0)],
								values[node.operands.at(1)]);
				break;
			case skeleton::ValueOpcode::bitXor:
				expression = solver.bitVectorXor(values[node.operands.at(0)],
								 values[node.operands.at(1)]);
				break;
			default:
				addUnique(reasons, "unsupported-value-opcode");
				continue;
			}
			solver.constrain(solver.equal(values[node.id], expression));
		}
	}

	void encodeAssumes()
	{
		for (const auto &event : program.events) {
			if (event.kind != skeleton::EventKind::assume)
				continue;
			if (event.arguments.empty()) {
				addUnique(reasons, "assume-without-condition");
				continue;
			}
			const auto argument = event.arguments.front();
			solver.constrain(solver.implies(
				blocks[event.block], truth(values[argument], program.values[argument].width)));
		}
	}

	void encodeErrorObjective()
	{
		if (!options.requireActiveError)
			return;
		std::vector<Expr> errors;
		for (const auto &event : program.events)
			if (event.kind == skeleton::EventKind::error)
				errors.push_back(blocks[event.block]);
		if (errors.empty()) {
			addUnique(reasons, "error-objective-without-event");
			return;
		}
		solver.constrain(solver.anyOf(errors));
	}

	void encodeRF()
	{
		for (const auto &load : program.events) {
			if (load.kind != skeleton::EventKind::load &&
			    load.kind != skeleton::EventKind::lock)
				continue;
			std::vector<std::pair<skeleton::NodeID, Expr>> sources;
			const auto width = load.kind == skeleton::EventKind::lock ? 1U : load.width;
			if (load.kind == skeleton::EventKind::lock)
				sources.emplace_back(skeleton::invalidNode,
						     solver.bitVectorConstant(0, width));
			else if (const auto value = initial(load.address, width))
				sources.emplace_back(skeleton::invalidNode,
						     solver.bitVectorConstant(*value, width));
			for (const auto &store : program.events)
				if (store.address == load.address &&
				    ((load.kind == skeleton::EventKind::load &&
				      store.kind == skeleton::EventKind::store && store.width == width) ||
				     (load.kind == skeleton::EventKind::lock &&
				      store.kind == skeleton::EventKind::unlock)))
					sources.emplace_back(
						store.id, load.kind == skeleton::EventKind::lock
								  ? solver.bitVectorConstant(0, width)
								  : values[store.value]);
			if (sources.empty()) {
				addUnique(reasons, "load-without-rf-source:" + load.address);
				continue;
			}
			std::vector<Expr> selectors;
			if (options.rfAbstraction != RfAbstractionEncoding::concrete) {
				using Source = std::pair<skeleton::NodeID, Expr>;
				std::map<StaticValueProvenanceKey, std::vector<Source>> classes;
				for (const auto &[store, sourceValue] : sources) {
					const auto isInitial = store == skeleton::invalidNode;
					const auto valueKey = isInitial
							      ? constantKey(
									width,
									load.kind == skeleton::EventKind::lock
										? 0
										: *initial(load.address, width))
							      : storeValueKey(program,
									      program.events[store], width);
					const auto provenance =
						options.rfAbstraction ==
								RfAbstractionEncoding::valueProvenance
							? std::tuple{valueKey,
								     isInitial ? skeleton::invalidNode
									     : program.events[store].function,
								     isInitial}
							: std::tuple{valueKey, skeleton::invalidNode, false};
					classes[provenance].emplace_back(store, sourceValue);
				}
				std::size_t classIndex{};
				for (const auto &[unusedKey, members] : classes) {
					auto selected = solver.boolean(
						"rf_class_" + std::to_string(load.id) + "_" +
						std::to_string(classIndex++));
					selectors.push_back(selected);
					boolDecisions.push_back(selected);
					graphDecisions.push_back(selected);
					/* This representative is diagnostic metadata only. The class
					 * selector remains an over-approximation until source refinement. */
					std::vector<skeleton::NodeID> memberIDs;
					memberIDs.reserve(members.size());
					for (const auto &[store, unusedValue] : members)
						memberIDs.push_back(store);
					rf.push_back(
						{load.id, members.front().first, selected, std::move(memberIDs)});
					solver.constrain(
						solver.implies(selected, blocks[load.block]));
					std::vector<Expr> available;
					for (const auto &[store, unusedValue] : members)
						available.push_back(
							store == skeleton::invalidNode
								? solver.allOf(std::span<const Expr>{})
								: blocks[program.events[store].block]);
					solver.constrain(solver.implies(
						selected, solver.anyOf(available)));
					if (load.kind == skeleton::EventKind::load)
						solver.constrain(solver.implies(
							selected,
							solver.equal(values[load.value],
								     members.front().second)));
				}
			} else {
				for (const auto &[store, sourceValue] : sources) {
					auto selected = solver.boolean(
						"rf_" + std::to_string(load.id) + "_" +
						std::to_string(store));
					selectors.push_back(selected);
					boolDecisions.push_back(selected);
					graphDecisions.push_back(selected);
					rf.push_back({load.id, store, selected, {store}});
					solver.constrain(
						solver.implies(selected, blocks[load.block]));
					if (store != skeleton::invalidNode)
						solver.constrain(solver.implies(
							selected, blocks[program.events[store].block]));
					if (load.kind == skeleton::EventKind::load)
						solver.constrain(solver.implies(
							selected,
							solver.equal(values[load.value], sourceValue)));
				}
			}
			solver.constrain(solver.implies(blocks[load.block], solver.anyOf(selectors)));
			if (options.rfCardinality == RfCardinalityEncoding::native) {
				solver.constrain(solver.atMostOne(selectors));
			} else {
				for (std::size_t i = 0; i < selectors.size(); ++i)
					for (std::size_t j = i + 1; j < selectors.size(); ++j) {
						const Expr pair[]{selectors[i], selectors[j]};
						solver.constrain(
							solver.logicalNot(solver.allOf(pair)));
					}
			}
		}
	}

	void encodeCO()
	{
		if (!options.encodeCo)
			return;
		std::unordered_map<std::string, std::vector<const skeleton::EventSite *>> stores;
		for (const auto &event : program.events)
			if (event.kind == skeleton::EventKind::store ||
			    event.kind == skeleton::EventKind::unlock ||
			    event.kind == skeleton::EventKind::lock)
				stores[event.address].push_back(&event);
		for (const auto &[address, events] : stores) {
			const auto width = std::max(
				1U, static_cast<unsigned>(std::bit_width(events.size() - 1)));
			std::vector<CoRank> ranks;
			for (const auto *event : events) {
				auto rank = solver.bitVector("co_" + std::to_string(event->id), width);
				ranks.push_back({event->id, rank});
				co.push_back({event->id, rank});
			}
			for (std::size_t i = 0; i < ranks.size(); ++i)
				for (std::size_t j = i + 1; j < ranks.size(); ++j) {
					const Expr active[]{blocks[program.events[ranks[i].store].block],
							    blocks[program.events[ranks[j].store].block]};
					auto bothActive = solver.allOf(active);
					solver.constrain(solver.implies(
						bothActive,
						solver.logicalNot(
							solver.equal(ranks[i].rank, ranks[j].rank))));
					const Expr beforeTerms[]{
						bothActive,
						solver.unsignedLess(ranks[i].rank, ranks[j].rank)};
					auto before = solver.boolean(
						"co_before_" + std::to_string(ranks[i].store) + "_" +
						std::to_string(ranks[j].store));
					solver.constrain(solver.equal(before, solver.allOf(beforeTerms)));
					boolDecisions.push_back(before);
					graphDecisions.push_back(before);
				}
		}
	}

	void encode()
	{
		encodeValues();
		encodeCFG();
		encodeDefinitions();
		encodeAssumes();
		encodeErrorObjective();
		encodeRF();
		encodeCO();
		std::ranges::sort(reasons);
	}

	auto next() -> FiniteStep
	{
		if (!reasons.empty())
			return {.status = CheckResult::unavailable};
		/* Keep the returned model available for exact CAT explanation replay. The
		 * full-assignment blocker is installed only when the caller requests the next
		 * model, unless a graph/core blocker supersedes it first. */
		if (pendingAssignmentClause.valid()) {
			solver.constrain(pendingAssignmentClause);
			pendingAssignmentClause = {};
			lastGraphClause = {};
		}
		const auto status = solver.check();
		if (status != CheckResult::sat)
			return {.status = status};
		FiniteAssignment assignment;
		assignment.abstractReadsFrom = false;
		assignment.values.resize(values.size());
		assignment.readsFrom.resize(program.events.size());
		if (options.rfAbstraction != RfAbstractionEncoding::concrete)
			assignment.readsFromClassMembers.resize(program.events.size());
		for (const auto &event : program.events)
			if (solver.boolValue(blocks[event.block]).value_or(false))
				assignment.activeEvents.push_back(event.id);
		for (std::size_t i = 0; i < values.size(); ++i)
			assignment.values[i] = solver.bitVectorValue(values[i]);
		for (const auto &choice : rf)
			if (solver.boolValue(choice.selected).value_or(false)) {
				if (options.rfAbstraction == RfAbstractionEncoding::concrete) {
					assignment.readsFrom[choice.load] = choice.store;
				} else if (choice.refinedMembers.empty()) {
					assignment.abstractReadsFrom = true;
					assignment.readsFrom[choice.load] = choice.store;
					auto &activeMembers =
						assignment.readsFromClassMembers[choice.load];
					for (const auto store : choice.members)
						if (store == skeleton::invalidNode ||
						    solver.boolValue(
							    blocks[program.events[store].block])
							    .value_or(false))
							activeMembers.push_back(store);
				} else {
					const auto selectedMember = std::ranges::find_if(
						choice.refinedMembers, [&](const auto &member) {
							return solver.boolValue(member.second)
								.value_or(false);
						});
					if (selectedMember == choice.refinedMembers.end()) {
						addUnique(reasons, "refined-rf-class-without-source");
						assignment.abstractReadsFrom = true;
						assignment.readsFrom[choice.load] = choice.store;
					} else {
						assignment.readsFrom[choice.load] =
							selectedMember->first;
					}
				}
			}
		if (!assignment.abstractReadsFrom)
			assignment.readsFromClassMembers.clear();
		std::vector<std::tuple<std::string, std::uint64_t, skeleton::NodeID>> ordered;
		for (const auto &rank : co)
			if (solver.boolValue(blocks[program.events[rank.store].block]).value_or(false))
				ordered.emplace_back(program.events[rank.store].address,
						     solver.bitVectorValue(rank.rank).value_or(0),
						     rank.store);
		std::ranges::sort(ordered);
		for (const auto &[unusedAddress, unusedRank, store] : ordered)
			assignment.coherenceOrder.push_back(store);

		std::vector<Expr> different;
		std::vector<Expr> graphDifferent;
		for (const auto block : blocks) {
			const auto value = solver.boolValue(block).value_or(false);
			graphDifferent.push_back(value ? solver.logicalNot(block) : block);
		}
		for (const auto decision : graphDecisions) {
			const auto value = solver.boolValue(decision).value_or(false);
			graphDifferent.push_back(value ? solver.logicalNot(decision) : decision);
		}
		lastGraphClause = solver.anyOf(graphDifferent);
		for (const auto decision : boolDecisions) {
			const auto value = solver.boolValue(decision).value_or(false);
			different.push_back(value ? solver.logicalNot(decision) : decision);
		}
		for (const auto &decision : bvDecisions) {
			const auto value = solver.bitVectorValue(decision.expression);
			if (!value) {
				addUnique(reasons, "wide-decision-model-value");
				continue;
			}
			different.push_back(solver.logicalNot(
				solver.equal(decision.expression,
					     solver.bitVectorConstant(*value, decision.width))));
		}
		pendingAssignmentClause = solver.anyOf(different);
		return {.status = status, .assignment = std::move(assignment)};
	}

	auto refineCurrentRfClasses() -> FiniteStep
	{
		if (options.rfAbstraction == RfAbstractionEncoding::concrete)
			return {.status = CheckResult::unavailable};
		for (;;) {
			if (!lastGraphClause.valid())
				throw std::logic_error("no abstract RF model is available to refine");
			bool added{};
			for (auto &choice : rf) {
			if (!solver.boolValue(choice.selected).value_or(false) ||
			    !choice.refinedMembers.empty())
				continue;
			added = true;
			std::vector<Expr> selectors;
			selectors.reserve(choice.members.size());
			for (const auto store : choice.members) {
				auto selected = solver.boolean(
					"rf_refined_" + std::to_string(choice.load) + "_" +
					std::to_string(store));
				selectors.push_back(selected);
				choice.refinedMembers.emplace_back(store, selected);
				boolDecisions.push_back(selected);
				graphDecisions.push_back(selected);
				solver.constrain(solver.implies(selected, choice.selected));
				if (store != skeleton::invalidNode)
					solver.constrain(solver.implies(
						selected, blocks[program.events[store].block]));
			}
			solver.constrain(
				solver.implies(choice.selected, solver.anyOf(selectors)));
			if (options.rfCardinality == RfCardinalityEncoding::native) {
				solver.constrain(solver.atMostOne(selectors));
			} else {
				for (std::size_t i = 0; i < selectors.size(); ++i)
					for (std::size_t j = i + 1; j < selectors.size(); ++j) {
						const Expr pair[]{selectors[i], selectors[j]};
						solver.constrain(solver.logicalNot(
							solver.allOf(pair)));
					}
			}
			}
			if (!added) {
				addUnique(reasons, "abstract-rf-model-cannot-be-refined");
				return {.status = CheckResult::unavailable};
			}
			lastGraphClause = {};
			pendingAssignmentClause = {};
			auto step = next();
			if (!step.assignment || !step.assignment->abstractReadsFrom)
				return step;
		}
	}

	void blockCurrentGraph()
	{
		if (!lastGraphClause.valid())
			throw std::logic_error("no finite graph model is available to block");
		solver.constrain(lastGraphClause);
		lastGraphClause = {};
		pendingAssignmentClause = {};
	}

	void blockCurrentRfCore(std::span<const skeleton::NodeID> coreLoads)
	{
		if (!lastGraphClause.valid())
			throw std::logic_error("no finite graph model is available to block");
		std::vector<Expr> different;
		for (const auto block : blocks) {
			const auto value = solver.boolValue(block).value_or(false);
			different.push_back(value ? solver.logicalNot(block) : block);
		}
		for (const auto &choice : rf) {
			if (std::ranges::find(coreLoads, choice.load) == coreLoads.end() ||
			    !solver.boolValue(choice.selected).value_or(false))
				continue;
			if (choice.refinedMembers.empty()) {
				different.push_back(solver.logicalNot(choice.selected));
				continue;
			}
			const auto selected = std::ranges::find_if(
				choice.refinedMembers, [&](const auto &member) {
					return solver.boolValue(member.second).value_or(false);
				});
			if (selected != choice.refinedMembers.end())
				different.push_back(solver.logicalNot(selected->second));
		}
		solver.constrain(solver.anyOf(different));
		lastGraphClause = {};
		pendingAssignmentClause = {};
	}

	auto active(const FiniteDenseEvent &event) -> Expr
	{
		return event.part == FiniteDensePart::initial
			       ? solver.allOf(std::span<const Expr>{})
			       : blocks.at(program.events.at(event.site).block);
	}

	auto coBefore(const FiniteDenseEvent &from, const FiniteDenseEvent &to)
		-> std::optional<Expr>
	{
		if (from.address.empty() || from.address != to.address ||
		    to.part == FiniteDensePart::initial)
			return solver.anyOf(std::span<const Expr>{});
		if (from.part == FiniteDensePart::initial)
			return active(to);
		const auto fromRank = std::ranges::find_if(
			co, [&](const auto &rank) { return rank.store == from.site; });
		const auto toRank = std::ranges::find_if(
			co, [&](const auto &rank) { return rank.store == to.site; });
		if (fromRank == co.end() || toRank == co.end())
			return std::nullopt;
		const Expr terms[]{active(from), active(to),
				   solver.unsignedLess(fromRank->rank, toRank->rank)};
		return solver.allOf(terms);
	}

	auto relation(std::string_view name, const FiniteDenseEvent &from,
		      const FiniteDenseEvent &to) -> std::optional<Expr>
	{
		const Expr endpoints[]{active(from), active(to)};
		if (name == "co")
			return coBefore(from, to);
		if (name == "rf") {
			if (to.part != FiniteDensePart::lockRead &&
			    (to.part != FiniteDensePart::ordinary ||
			     program.events[to.site].kind != skeleton::EventKind::load))
				return solver.anyOf(std::span<const Expr>{});
			const auto source = from.part == FiniteDensePart::initial
						    ? skeleton::invalidNode
						    : from.site;
			const auto found = std::ranges::find_if(rf, [&](const auto &choice) {
				return choice.load == to.site && choice.store == source;
			});
			return found == rf.end()
				       ? std::optional<Expr>(solver.anyOf(std::span<const Expr>{}))
				       : std::optional<Expr>(found->selected);
		}
		if (name == "fr") {
			std::vector<Expr> alternatives;
			for (const auto &choice : rf) {
				if (choice.load != from.site)
					continue;
				FiniteDenseEvent source;
				if (choice.store == skeleton::invalidNode) {
					source.part = FiniteDensePart::initial;
					source.address = from.address;
				} else {
					source.site = choice.store;
					source.part = program.events[choice.store].kind ==
							      skeleton::EventKind::lock
						      ? FiniteDensePart::lockWrite
						      : FiniteDensePart::ordinary;
					source.address = program.events[choice.store].address;
				}
				auto before = coBefore(source, to);
				if (!before)
					return std::nullopt;
				const Expr terms[]{choice.selected, *before};
				alternatives.push_back(solver.allOf(terms));
			}
			return solver.anyOf(alternatives);
		}
		if (name == "po") {
			if (from.part == FiniteDensePart::initial ||
			    to.part == FiniteDensePart::initial ||
			    program.events[from.site].function != program.events[to.site].function)
				return solver.anyOf(std::span<const Expr>{});
			const auto ordered = from.site < to.site ||
					     (from.site == to.site &&
					      from.part == FiniteDensePart::lockRead &&
					      to.part == FiniteDensePart::lockWrite);
			return ordered ? solver.allOf(endpoints)
				       : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "loc")
			return !from.address.empty() && from.address == to.address
				       ? solver.allOf(endpoints)
				       : solver.anyOf(std::span<const Expr>{});
		if (name == "int") {
			const auto same = (from.part == FiniteDensePart::initial &&
					   to.part == FiniteDensePart::initial) ||
					  (from.part != FiniteDensePart::initial &&
					   to.part != FiniteDensePart::initial &&
					   program.events[from.site].function ==
						   program.events[to.site].function);
			return same ? solver.allOf(endpoints)
				    : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "ext") {
			const auto same = (from.part == FiniteDensePart::initial &&
					   to.part == FiniteDensePart::initial) ||
					  (from.part != FiniteDensePart::initial &&
					   to.part != FiniteDensePart::initial &&
					   program.events[from.site].function ==
						   program.events[to.site].function);
			return !same ? solver.allOf(endpoints)
				     : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "rmw") {
			const auto pair = from.site == to.site &&
					  from.part == FiniteDensePart::lockRead &&
					  to.part == FiniteDensePart::lockWrite;
			return pair ? solver.allOf(endpoints)
				    : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "id") {
			const auto same = from.site == to.site && from.part == to.part &&
					  from.address == to.address;
			return same ? active(from) : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "0")
			return solver.anyOf(std::span<const Expr>{});
		/* Lifecycle edges require synthetic start/finish identities in the next adapter
		 * revision. Falling back to the full graph signature remains exact. */
		return std::nullopt;
	}

	auto set(std::string_view name, const FiniteDenseEvent &event) -> std::optional<Expr>
	{
		const auto enabled = active(event);
		if (name == "_")
			return enabled;
		if (name == "IW")
			return event.part == FiniteDensePart::initial
				       ? enabled
				       : solver.anyOf(std::span<const Expr>{});
		if (name == "R") {
			const auto read = event.part == FiniteDensePart::lockRead ||
					  (event.part == FiniteDensePart::ordinary &&
					   program.events[event.site].kind ==
						   skeleton::EventKind::load);
			return read ? enabled : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "W") {
			const auto write = event.part == FiniteDensePart::initial ||
					   event.part == FiniteDensePart::lockWrite ||
					   (event.part == FiniteDensePart::ordinary &&
					    (program.events[event.site].kind ==
						     skeleton::EventKind::store ||
					     program.events[event.site].kind ==
						     skeleton::EventKind::unlock));
			return write ? enabled : solver.anyOf(std::span<const Expr>{});
		}
		if (name == "F")
			return event.part == FiniteDensePart::ordinary &&
				       program.events[event.site].kind == skeleton::EventKind::fence
			       ? enabled
			       : solver.anyOf(std::span<const Expr>{});
		if (name == "SC") {
			return event.part != FiniteDensePart::initial &&
				       program.events[event.site].sequentiallyConsistent
			       ? enabled
			       : solver.anyOf(std::span<const Expr>{});
		}
		return std::nullopt;
	}

	auto blockCurrentExplanation(std::span<const cat::BaseLiteral> explanation,
				     std::span<const FiniteDenseEvent> denseEvents) -> bool
	{
		lastExplanationFailure.clear();
		if (!lastGraphClause.valid()) {
			lastExplanationFailure = "no-current-model";
			return false;
		}
		if (explanation.empty()) {
			lastExplanationFailure = "empty-explanation";
			return false;
		}
		std::vector<Expr> breakLiteral;
		for (const auto &literal : explanation) {
			if (literal.first >= denseEvents.size() ||
			    (literal.second && *literal.second >= denseEvents.size())) {
				lastExplanationFailure = "dense-event-out-of-range:" +
							 literal.predicateName;
				return false;
			}
			auto present = literal.second
					       ? relation(literal.predicateName,
							  denseEvents[literal.first],
							  denseEvents[*literal.second])
					       : set(literal.predicateName,
						     denseEvents[literal.first]);
			if (!present) {
				lastExplanationFailure = "unsupported-predicate:" +
							 literal.predicateName;
				return false;
			}
			const auto observed = solver.boolValue(*present);
			if (!observed) {
				lastExplanationFailure = "non-ground-predicate:" +
							 literal.predicateName;
				return false;
			}
			if (*observed != literal.positive) {
				lastExplanationFailure = "model-mismatch:" + literal.predicateName +
							 ":expected=" +
							 (literal.positive ? "1" : "0") +
							 ":observed=" + (*observed ? "1" : "0");
				return false;
			}
			breakLiteral.push_back(literal.positive ? solver.logicalNot(*present)
									: *present);
		}
		solver.constrain(solver.anyOf(breakLiteral));
		lastGraphClause = {};
		pendingAssignmentClause = {};
		return true;
	}
};

FiniteRfRefiner::FiniteRfRefiner(const FiniteAssignment &abstractAssignment)
	: base_(abstractAssignment)
{
	if (!base_.abstractReadsFrom) {
		error_ = "assignment is already concrete";
		return;
	}
	if (base_.readsFromClassMembers.size() != base_.readsFrom.size()) {
		error_ = "RF class-member table size mismatch";
		return;
	}
	for (std::size_t load = 0; load < base_.readsFrom.size(); ++load) {
		if (!base_.readsFrom[load])
			continue;
		if (base_.readsFromClassMembers[load].empty()) {
			error_ = "selected RF class has no active concrete source";
			return;
		}
		loads_.push_back(static_cast<skeleton::NodeID>(load));
	}
	indices_.resize(loads_.size());
	lastSources_.resize(loads_.size());
}

auto FiniteRfRefiner::valid() const -> bool { return error_.empty(); }
auto FiniteRfRefiner::error() const -> const std::string & { return error_; }

void FiniteRfRefiner::advance()
{
	currentAvailable_ = false;
	for (std::size_t position = indices_.size(); position-- > 0;) {
		const auto load = loads_[position];
		auto &index = indices_[position];
		if (++index < base_.readsFromClassMembers[load].size())
			return;
		index = 0;
	}
	exhausted_ = true;
}

auto FiniteRfRefiner::next() -> std::optional<FiniteAssignment>
{
	if (!valid() || exhausted_)
		return std::nullopt;
	if (currentAvailable_)
		advance();
	if (exhausted_)
		return std::nullopt;
	auto concrete = base_;
	concrete.abstractReadsFrom = false;
	concrete.readsFromClassMembers.clear();
	for (std::size_t position = 0; position < loads_.size(); ++position) {
		const auto load = loads_[position];
		const auto source = base_.readsFromClassMembers[load][indices_[position]];
		concrete.readsFrom[load] = source;
		lastSources_[position] = source;
	}
	currentAvailable_ = true;
	++generated_;
	return concrete;
}

auto FiniteRfRefiner::matchesCore(std::span<const skeleton::NodeID> coreLoads) const -> bool
{
	for (const auto load : coreLoads) {
		const auto found = std::ranges::find(loads_, load);
		if (found == loads_.end())
			continue;
		const auto position = static_cast<std::size_t>(found - loads_.begin());
		if (base_.readsFromClassMembers[load][indices_[position]] !=
		    lastSources_[position])
			return false;
	}
	return true;
}

void FiniteRfRefiner::blockCurrentRfCore(std::span<const skeleton::NodeID> coreLoads)
{
	if (!currentAvailable_)
		throw std::logic_error("no refined RF assignment is available to block");
	if (coreLoads.empty()) {
		exhausted_ = true;
		currentAvailable_ = false;
		return;
	}
	do {
		advance();
		if (!exhausted_ && matchesCore(coreLoads))
			++skipped_;
	} while (!exhausted_ && matchesCore(coreLoads));
}

auto FiniteRfRefiner::candidatesGenerated() const -> std::uint64_t { return generated_; }
auto FiniteRfRefiner::candidatesSkipped() const -> std::uint64_t { return skipped_; }

FiniteSkeletonEncoder::FiniteSkeletonEncoder(const skeleton::Program &program,
					     FiniteEncodingOptions options)
	: impl_(std::make_unique<Impl>(program, options))
{}
FiniteSkeletonEncoder::~FiniteSkeletonEncoder() = default;
FiniteSkeletonEncoder::FiniteSkeletonEncoder(FiniteSkeletonEncoder &&) noexcept = default;
auto FiniteSkeletonEncoder::operator=(FiniteSkeletonEncoder &&) noexcept
	-> FiniteSkeletonEncoder & = default;
auto FiniteSkeletonEncoder::supported() const -> bool { return impl_->reasons.empty(); }
auto FiniteSkeletonEncoder::blockers() const -> const std::vector<std::string> &
{
	return impl_->reasons;
}
auto FiniteSkeletonEncoder::next() -> FiniteStep { return impl_->next(); }
auto FiniteSkeletonEncoder::refineCurrentRfClasses() -> FiniteStep
{
	return impl_->refineCurrentRfClasses();
}
void FiniteSkeletonEncoder::blockCurrentGraph() { impl_->blockCurrentGraph(); }
void FiniteSkeletonEncoder::blockCurrentRfCore(
	std::span<const skeleton::NodeID> coreLoads)
{
	impl_->blockCurrentRfCore(coreLoads);
}
auto FiniteSkeletonEncoder::blockCurrentExplanation(
	std::span<const cat::BaseLiteral> explanation,
	std::span<const FiniteDenseEvent> denseEvents) -> bool
{
	return impl_->blockCurrentExplanation(explanation, denseEvents);
}
auto FiniteSkeletonEncoder::explanationFailure() const -> const std::string &
{
	return impl_->lastExplanationFailure;
}

} /* namespace genmc::symbolic */
