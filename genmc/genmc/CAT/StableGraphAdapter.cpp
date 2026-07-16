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

#include "genmc/CAT/StableGraphAdapter.hpp"

#include "genmc/CAT/Value.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"

#include <algorithm>
#include <limits>
#include <ranges>
#include <unordered_map>

namespace cat {
namespace {

auto isRealEvent(const EventLabel &label) -> bool
{
	return !genmc::isa<EmptyLabel>(&label) && !genmc::isa<InitLabel>(&label);
}

template <typename T> void addValue(BaseValues &values, std::string_view name, T value)
{
	values.emplace(std::string(name), std::move(value));
}

using RelationEdges = std::vector<std::pair<std::size_t, std::size_t>>;

/** Select exact CSR only when its owned payload is at most half the dense matrix. */
auto sizeAdaptiveRelation(std::size_t size, RelationEdges edges) -> Relation
{
	std::ranges::sort(edges);
	edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	const auto denseBytes = size * ((size + 63) / 64) * sizeof(std::uint64_t);
	const auto sparseBytes = (size + 1 + edges.size()) * sizeof(std::uint32_t);
	if (size > 512 && size <= std::numeric_limits<std::uint32_t>::max() &&
	    edges.size() <= std::numeric_limits<std::uint32_t>::max() &&
	    sparseBytes <= denseBytes / 2)
		return Relation::sparse(size, std::move(edges));
	Relation result(size);
	for (const auto [from, target] : edges)
		result.insertDense(from, target);
	return result;
}

/** Derive exact `fr = rf^-1 ; co` edges without constructing either dense operand. */
auto fromReadEdges(const RelationEdges &readsFrom, const RelationEdges &coherence)
	-> RelationEdges
{
	std::unordered_map<std::size_t, std::vector<std::size_t>> laterWrites;
	for (const auto [before, after] : coherence)
		laterWrites[before].push_back(after);
	RelationEdges result;
	for (const auto [write, read] : readsFrom) {
		if (const auto found = laterWrites.find(write); found != laterWrites.end()) {
			for (const auto later : found->second)
				result.emplace_back(read, later);
		}
	}
	return result;
}

/** Remap every live membership while preserving the source value type. */
auto remapValue(const Value &value, std::size_t size,
		const std::vector<GraphAdapter::EventId> &mapping) -> Value
{
	if (const auto *set = std::get_if<EventSet>(&value)) {
		EventSet result(size);
		for (std::size_t event = 0; event < mapping.size(); ++event) {
			if (set->contains(event))
				result.insert(mapping[event]);
		}
		return result;
	}
	const auto &relation = std::get<Relation>(value);
	Relation result(size);
	for (std::size_t from = 0; from < mapping.size(); ++from) {
		for (std::size_t to = 0; to < mapping.size(); ++to) {
			if (relation.contains(from, to))
				result.insertDense(mapping[from], mapping[to]);
		}
	}
	return result;
}

} /* namespace */

StableGraphAdapter::StableGraphAdapter(std::vector<std::string> requiredPrimitives)
	: requiredPrimitives_(requiredPrimitives.begin(), requiredPrimitives.end()),
	  buildAllPrimitives_(requiredPrimitives.empty())
{}

auto StableGraphAdapter::required(std::string_view name) const -> bool
{
	return buildAllPrimitives_ || requiredPrimitives_.contains(name);
}

auto StableGraphAdapter::materialize(const ExecutionGraph &graph) -> StableGraphSnapshot
{
	std::vector<std::pair<std::size_t, const EventLabel *>> active;
	std::vector<GraphAdapter::EventId> denseToStable;
	for (const auto &label : graph.labels()) {
		if (!isRealEvent(label))
			continue;
		StableEventKey key = label.getPos();
		auto [found, inserted] = ids_.try_emplace(key, keys_.size());
		if (inserted)
			keys_.push_back(std::move(key));
		active.emplace_back(found->second, &label);
		denseToStable.push_back(found->second);
	}

	std::vector<SAddr> locations;
	for (auto location = graph.loc_begin(); location != graph.loc_end(); ++location)
		locations.push_back(location->first);
	std::ranges::sort(locations);
	std::map<SAddr, std::size_t> initialIds;
	for (const auto location : locations) {
		StableEventKey key = location;
		auto [found, inserted] = ids_.try_emplace(key, keys_.size());
		if (inserted)
			keys_.push_back(std::move(key));
		initialIds.emplace(location, found->second);
		denseToStable.push_back(found->second);
	}

	const auto size = keys_.size();
	const bool needUnderscore = required("_");
	const bool needR = required("R"), needW = required("W"), needF = required("F");
	const bool needIW = required("IW"), needSC = required("SC"), needZero = required("0");
	const bool needId = required("id"), needPo = required("po"), needRf = required("rf");
	const bool needCo = required("co"), needFr = required("fr"), needRmw = required("rmw");
	const bool needLoc = required("loc"), needInt = required("int"), needExt = required("ext");
	const bool needTc = required("tc"), needTj = required("tj");
	const bool needUniverse = needUnderscore || needId;
	const bool needReadsFrom = needRf || needFr;
	const bool needCoherence = needCo || needFr;
	EventSet universe(needUniverse ? size : 0), reads(needR ? size : 0),
		writes(needW ? size : 0), fences(needF ? size : 0),
		initialWrites(needIW ? size : 0), sequentiallyConsistent(needSC ? size : 0);
	const auto denseStructuralSize = size <= 512 ? size : 0;
	Relation programOrder(needPo ? denseStructuralSize : 0),
		location(needLoc ? denseStructuralSize : 0),
		internal(needInt ? denseStructuralSize : 0),
		external(needExt ? denseStructuralSize : 0);
	RelationEdges readsFromEdges, coherenceEdges, rmwEdges, threadCreateEdges,
		threadJoinEdges;

	const auto eventId = [&](Event event) -> std::optional<std::size_t> {
		const auto found = ids_.find(StableEventKey(event));
		return found == ids_.end() ? std::nullopt
					   : std::optional<std::size_t>(found->second);
	};
	for (const auto &[stable, label] : active) {
		if (needUniverse)
			universe.insert(stable);
		if (needR && genmc::isa<ReadLabel>(label))
			reads.insert(stable);
		if (needW && genmc::isa<WriteLabel>(label))
			writes.insert(stable);
		if (needF && genmc::isa<FenceLabel>(label))
			fences.insert(stable);
		if (needSC && label->isSC())
			sequentiallyConsistent.insert(stable);

		if (const auto *read = genmc::dyn_cast<ReadLabel>(label);
		    needReadsFrom && read && read->getRf()) {
			if (genmc::isa<InitLabel>(read->getRf())) {
				if (const auto source = initialIds.find(read->getAddr());
				    source != initialIds.end())
					readsFromEdges.emplace_back(source->second, stable);
			} else if (const auto source = eventId(read->getRf()->getPos())) {
				readsFromEdges.emplace_back(*source, stable);
			}
			if (needRmw && read->isRMW()) {
				if (const auto *write = graph.po_imm_succ(read)) {
					if (const auto writeId = eventId(write->getPos()))
						rmwEdges.emplace_back(stable, *writeId);
				}
			}
		}
		if (const auto *start = genmc::dyn_cast<ThreadStartLabel>(label);
		    needTc && start && start->getCreate()) {
			if (const auto create = eventId(start->getCreate()->getPos()))
				threadCreateEdges.emplace_back(*create, stable);
		}
		if (const auto *finish = genmc::dyn_cast<ThreadFinishLabel>(label);
		    needTj && finish && finish->getParentJoin()) {
			if (const auto join = eventId(finish->getParentJoin()->getPos()))
				threadJoinEdges.emplace_back(stable, *join);
		}
	}
	for (const auto &[initialLocation, stable] : initialIds) {
		(void)initialLocation;
		if (needUniverse)
			universe.insert(stable);
		if (needW)
			writes.insert(stable);
		if (needIW)
			initialWrites.insert(stable);
	}

	if (needPo || needLoc || needInt || needExt) {
		if (size > 512) {
			std::vector<std::uint64_t> threadKeys((needPo || needInt || needExt) ? size
											     : 0);
			std::vector<std::uint64_t> locationKeys(needLoc ? size : 0);
			std::map<SAddr, std::uint64_t> locationGroups;
			if (needLoc) {
				std::uint64_t nextGroup = 1;
				for (const auto &[address, stable] : initialIds) {
					locationGroups.emplace(address, nextGroup);
					locationKeys[stable] = nextGroup++;
				}
			}
			for (const auto &[address, stable] : initialIds) {
				(void)address;
				if (needPo || needInt || needExt)
					threadKeys[stable] = std::uint64_t{1} << 32;
			}
			for (const auto &[stable, label] : active) {
				if (needPo || needInt || needExt) {
					const auto group =
						static_cast<std::uint64_t>(label->getThread()) + 2;
					threadKeys[stable] =
						(group << 32) |
						static_cast<std::uint32_t>(label->getIndex());
				}
				if (const auto *memory = genmc::dyn_cast<MemAccessLabel>(label);
				    needLoc && memory) {
					const auto found = locationGroups.find(memory->getAddr());
					VERIFY(found != locationGroups.end(),
					       "CAT memory event lacks an initial location group");
					locationKeys[stable] = found->second;
				}
			}
			if (needPo)
				programOrder = Relation::structural(
					StructuralRelationKind::ProgramOrder, threadKeys);
			if (needInt)
				internal = Relation::structural(StructuralRelationKind::Internal,
								threadKeys);
			if (needExt)
				external = Relation::structural(StructuralRelationKind::External,
								threadKeys);
			if (needLoc)
				location = Relation::structural(StructuralRelationKind::Location,
								locationKeys);
		} else {
			std::map<int, std::vector<std::pair<int, std::size_t>>> threadMembers;
			std::map<SAddr, std::vector<std::size_t>> locationMembers;
			EventSet activeEvents(needExt ? size : 0),
				initialEvents((needInt || needExt) ? size : 0);
			for (const auto &[stable, label] : active) {
				if (needExt)
					activeEvents.insert(stable);
				if (needPo || needInt || needExt)
					threadMembers[label->getThread()].emplace_back(
						label->getIndex(), stable);
				if (const auto *memory = genmc::dyn_cast<MemAccessLabel>(label);
				    needLoc && memory)
					locationMembers[memory->getAddr()].push_back(stable);
			}
			for (const auto &[address, stable] : initialIds) {
				if (needInt || needExt)
					initialEvents.insert(stable);
				if (needLoc)
					locationMembers[address].push_back(stable);
			}
			for (auto &[thread, members] : threadMembers) {
				(void)thread;
				std::ranges::sort(members);
				EventSet memberSet((needInt || needExt) ? size : 0),
					later(needPo ? size : 0);
				if (needInt || needExt)
					for (const auto &[index, stable] : members) {
						(void)index;
						memberSet.insert(stable);
					}
				if (needInt)
					for (const auto &[index, stable] : members) {
						(void)index;
						internal.insertSuccessors(stable, memberSet);
					}
				if (needPo)
					for (auto member = members.rbegin();
					     member != members.rend(); ++member) {
						programOrder.insertSuccessors(member->second,
									      later);
						later.insert(member->second);
					}
				if (needExt) {
					auto otherThreads = setDifference(activeEvents, memberSet);
					otherThreads = setUnion(otherThreads, initialEvents);
					for (const auto &[index, stable] : members) {
						(void)index;
						external.insertSuccessors(stable, otherThreads);
					}
				}
			}
			if (needInt)
				for (const auto &[address, stable] : initialIds) {
					(void)address;
					internal.insertSuccessors(stable, initialEvents);
				}
			if (needExt)
				for (const auto &[address, stable] : initialIds) {
					(void)address;
					external.insertSuccessors(stable, activeEvents);
				}
			if (needLoc)
				for (const auto &[address, members] : locationMembers) {
					(void)address;
					EventSet memberSet(size);
					for (const auto stable : members)
						memberSet.insert(stable);
					for (const auto stable : members)
						location.insertSuccessors(stable, memberSet);
				}
		}
	}

	if (needCoherence)
		for (auto currentLocation = graph.loc_begin(); currentLocation != graph.loc_end();
		     ++currentLocation) {
			std::vector<std::size_t> stores;
			for (const auto &write : currentLocation->second) {
				if (const auto writeId = eventId(write.getPos()))
					stores.push_back(*writeId);
			}
			const auto initial = initialIds.find(currentLocation->first);
			for (std::size_t current = 0; current < stores.size(); ++current) {
				if (initial != initialIds.end())
					coherenceEdges.emplace_back(initial->second, stores[current]);
				for (std::size_t later = current + 1; later < stores.size();
				     ++later)
					coherenceEdges.emplace_back(stores[current], stores[later]);
			}
		}
	auto frEdges = needFr ? fromReadEdges(readsFromEdges, coherenceEdges) : RelationEdges{};
	Relation readsFrom = needReadsFrom
				     ? sizeAdaptiveRelation(size, std::move(readsFromEdges))
				     : Relation{};
	Relation coherence = needCoherence
				     ? sizeAdaptiveRelation(size, std::move(coherenceEdges))
				     : Relation{};
	Relation fromRead = needFr ? sizeAdaptiveRelation(size, std::move(frEdges)) : Relation{};
	Relation rmw = needRmw ? sizeAdaptiveRelation(size, std::move(rmwEdges)) : Relation{};
	Relation threadCreate =
		needTc ? sizeAdaptiveRelation(size, std::move(threadCreateEdges)) : Relation{};
	Relation threadJoin =
		needTj ? sizeAdaptiveRelation(size, std::move(threadJoinEdges)) : Relation{};

	BaseValues values;
	if (needUnderscore)
		addValue(values, "_", universe);
	if (needR)
		addValue(values, "R", std::move(reads));
	if (needW)
		addValue(values, "W", std::move(writes));
	if (needF)
		addValue(values, "F", std::move(fences));
	if (needIW)
		addValue(values, "IW", std::move(initialWrites));
	if (needSC)
		addValue(values, "SC", std::move(sequentiallyConsistent));
	if (needZero)
		addValue(values, "0", Relation(size));
	if (needId)
		addValue(values, "id", identity(universe));
	if (needPo)
		addValue(values, "po", std::move(programOrder));
	if (needRf)
		addValue(values, "rf", readsFrom);
	if (needCo)
		addValue(values, "co", coherence);
	if (needFr)
		addValue(values, "fr", std::move(fromRead));
	if (needRmw)
		addValue(values, "rmw", std::move(rmw));
	if (needLoc)
		addValue(values, "loc", std::move(location));
	if (needInt)
		addValue(values, "int", std::move(internal));
	if (needExt)
		addValue(values, "ext", std::move(external));
	if (needTc)
		addValue(values, "tc", std::move(threadCreate));
	if (needTj)
		addValue(values, "tj", std::move(threadJoin));
	return {size, active.size() + initialIds.size(), std::move(values),
		std::move(denseToStable)};
}

auto StableGraphAdapter::materialize(const GraphAdapter &snapshot) -> StableGraphSnapshot
{
	std::vector<GraphAdapter::EventId> mapping;
	mapping.reserve(snapshot.eventCount());
	for (GraphAdapter::EventId dense = 0; dense < snapshot.eventCount(); ++dense) {
		StableEventKey key = snapshot.initialLocation(dense)
					     ? StableEventKey(*snapshot.initialLocation(dense))
					     : StableEventKey(snapshot.label(dense)->getPos());
		auto [found, inserted] = ids_.try_emplace(key, keys_.size());
		if (inserted)
			keys_.push_back(std::move(key));
		mapping.push_back(found->second);
	}

	BaseValues values;
	for (const auto &[name, value] : snapshot.baseValues())
		values.emplace(name, remapValue(value, keys_.size(), mapping));
	return {keys_.size(), snapshot.eventCount(), std::move(values), std::move(mapping)};
}

auto StableGraphAdapter::id(const StableEventKey &key) const -> std::optional<std::size_t>
{
	const auto found = ids_.find(key);
	return found == ids_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
}

} /* namespace cat */
