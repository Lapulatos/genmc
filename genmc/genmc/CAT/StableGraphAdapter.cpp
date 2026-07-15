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
#include <ranges>

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
				result.insert(mapping[from], mapping[to]);
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
	Relation programOrder(needPo ? size : 0), readsFrom(needReadsFrom ? size : 0),
		coherence(needCoherence ? size : 0), rmw(needRmw ? size : 0),
		location(needLoc ? size : 0), internal(needInt ? size : 0),
		external(needExt ? size : 0), threadCreate(needTc ? size : 0),
		threadJoin(needTj ? size : 0);

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
					readsFrom.insert(source->second, stable);
			} else if (const auto source = eventId(read->getRf()->getPos())) {
				readsFrom.insert(*source, stable);
			}
			if (needRmw && read->isRMW()) {
				if (const auto *write = graph.po_imm_succ(read)) {
					if (const auto writeId = eventId(write->getPos()))
						rmw.insert(stable, *writeId);
				}
			}
		}
		if (const auto *start = genmc::dyn_cast<ThreadStartLabel>(label);
		    needTc && start && start->getCreate()) {
			if (const auto create = eventId(start->getCreate()->getPos()))
				threadCreate.insert(*create, stable);
		}
		if (const auto *finish = genmc::dyn_cast<ThreadFinishLabel>(label);
		    needTj && finish && finish->getParentJoin()) {
			if (const auto join = eventId(finish->getParentJoin()->getPos()))
				threadJoin.insert(stable, *join);
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

	const bool needPairPrimitives = needPo || needLoc || needInt || needExt;
	if (needPairPrimitives)
		for (const auto &[lhsId, lhs] : active) {
			for (const auto &[rhsId, rhs] : active) {
				if (lhs->getThread() == rhs->getThread()) {
					if (needInt)
						internal.insert(lhsId, rhsId);
					if (needPo && lhs->getIndex() < rhs->getIndex())
						programOrder.insert(lhsId, rhsId);
				} else if (needExt) {
					external.insert(lhsId, rhsId);
				}
				const auto *lhsMem = genmc::dyn_cast<MemAccessLabel>(lhs);
				const auto *rhsMem = genmc::dyn_cast<MemAccessLabel>(rhs);
				if (needLoc && lhsMem && rhsMem &&
				    lhsMem->getAddr() == rhsMem->getAddr())
					location.insert(lhsId, rhsId);
			}
			for (const auto &[address, initId] : initialIds) {
				if (needExt) {
					external.insert(lhsId, initId);
					external.insert(initId, lhsId);
				}
				if (const auto *memory = genmc::dyn_cast<MemAccessLabel>(lhs);
				    needLoc && memory && memory->getAddr() == address) {
					location.insert(lhsId, initId);
					location.insert(initId, lhsId);
				}
			}
		}
	if (needInt || needLoc)
		for (const auto &[lhsAddress, lhsId] : initialIds) {
			for (const auto &[rhsAddress, rhsId] : initialIds) {
				if (needInt)
					internal.insert(lhsId, rhsId);
				if (needLoc && lhsAddress == rhsAddress)
					location.insert(lhsId, rhsId);
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
					coherence.insert(initial->second, stores[current]);
				for (std::size_t later = current + 1; later < stores.size();
				     ++later)
					coherence.insert(stores[current], stores[later]);
			}
		}

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
		addValue(values, "fr", compose(inverse(readsFrom), coherence));
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
