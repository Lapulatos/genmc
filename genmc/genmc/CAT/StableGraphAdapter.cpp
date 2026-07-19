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
#include <chrono>
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
using CoherenceRows =
	std::vector<std::pair<std::size_t, std::vector<std::size_t>>>;

/** Select exact CSR only when its owned payload is at most half the dense matrix. */
auto sizeAdaptiveRelation(std::size_t size, RelationEdges edges, bool fastDenseBuild) -> Relation
{
	/* Dense bit insertion is idempotent, so sorting and uniquing only adds
	 * O(edges log edges) work on the certified small-graph path. */
	if (fastDenseBuild && size <= 512) {
		Relation result(size);
		for (const auto [from, target] : edges)
			result.insertDense(from, target);
		return result;
	}
	std::ranges::sort(edges);
	edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	const auto denseBytes = size * ((size + 63) / 64) * sizeof(std::uint64_t);
	const auto sparseBytes = (size + 1 + edges.size()) * sizeof(std::uint32_t);
	if (size <= std::numeric_limits<std::uint32_t>::max() &&
	    edges.size() <= std::numeric_limits<std::uint32_t>::max() &&
	    sparseBytes <= denseBytes / 2)
		return Relation::sparse(size, std::move(edges));
	Relation result(size);
	for (const auto [from, target] : edges)
		result.insertDense(from, target);
	return result;
}

/** Reference derivation used by the experiment baseline. */
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

/** Derive exact `fr = rf^-1 ; co` directly from each location's store order. */
auto fromReadEdgesOrdered(const RelationEdges &readsFrom, const CoherenceRows &coherence)
	-> RelationEdges
{
	std::unordered_map<std::size_t, std::vector<std::size_t>> readsBySource;
	for (const auto [source, read] : readsFrom)
		readsBySource[source].push_back(read);
	RelationEdges result;
	for (const auto &[initial, stores] : coherence) {
		const auto append = [&](std::size_t source, std::size_t firstLater) {
			const auto found = readsBySource.find(source);
			if (found == readsBySource.end())
				return;
			for (const auto read : found->second)
				for (std::size_t later = firstLater; later < stores.size(); ++later)
					result.emplace_back(read, stores[later]);
		};
		append(initial, 0);
		for (std::size_t current = 0; current < stores.size(); ++current)
			append(stores[current], current + 1);
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

StableGraphAdapter::StableGraphAdapter(std::vector<std::string> requiredPrimitives,
				       bool fastPrimitiveBuild, bool fastCoherenceBuild,
				       bool fastDescriptorBuild, bool fastDescriptorReuse)
	: requiredPrimitives_(requiredPrimitives.begin(), requiredPrimitives.end()),
	  buildAllPrimitives_(requiredPrimitives.empty()), fastPrimitiveBuild_(fastPrimitiveBuild),
	  fastCoherenceBuild_(fastCoherenceBuild), fastDescriptorBuild_(fastDescriptorBuild),
	  fastDescriptorReuse_(fastDescriptorReuse)
{}

auto StableGraphAdapter::required(std::string_view name) const -> bool
{
	return buildAllPrimitives_ || requiredPrimitives_.contains(name);
}

auto StableGraphAdapter::describe(const ExecutionGraph &graph) const -> GraphDescriptor
{
	GraphDescriptor result;
	describeInto(graph, result);
	return result;
}

void StableGraphAdapter::describeInto(const ExecutionGraph &graph, GraphDescriptor &result) const
{
	constexpr std::uint8_t readClass = 1U << 0;
	constexpr std::uint8_t writeClass = 1U << 1;
	constexpr std::uint8_t fenceClass = 1U << 2;
	constexpr std::uint8_t scClass = 1U << 3;
	result.events.clear();
	for (const auto &label : graph.labels()) {
		if (!isRealEvent(label))
			continue;
		EventDescriptor event{&label, label.getPos(), label.getThread(), label.getIndex()};
		if (genmc::isa<ReadLabel>(&label))
			event.classes |= readClass;
		if (genmc::isa<WriteLabel>(&label))
			event.classes |= writeClass;
		if (genmc::isa<FenceLabel>(&label))
			event.classes |= fenceClass;
		if (label.isSC())
			event.classes |= scClass;
		if (const auto *memory = genmc::dyn_cast<MemAccessLabel>(&label))
			event.address = memory->getAddr();
		if (const auto *read = genmc::dyn_cast<ReadLabel>(&label); read && read->getRf()) {
			event.readsFrom = genmc::isa<InitLabel>(read->getRf())
					      ? StableEventKey(read->getAddr())
					      : StableEventKey(read->getRf()->getPos());
			if (read->isRMW()) {
				if (const auto *write = graph.po_imm_succ(read))
					event.rmwTarget = write->getPos();
			}
		}
		if (const auto *start = genmc::dyn_cast<ThreadStartLabel>(&label);
		    start && start->getCreate())
			event.createSource = start->getCreate()->getPos();
		if (const auto *finish = genmc::dyn_cast<ThreadFinishLabel>(&label);
		    finish && finish->getParentJoin())
			event.joinTarget = finish->getParentJoin()->getPos();
		result.events.push_back(std::move(event));
	}
	std::size_t locationIndex = 0;
	for (auto location = graph.loc_begin(); location != graph.loc_end(); ++location) {
		if (locationIndex == result.coherence.size())
			result.coherence.emplace_back();
		auto &[address, writes] = result.coherence[locationIndex++];
		address = location->first;
		writes.clear();
		for (const auto &write : location->second)
			writes.push_back(write.getPos());
	}
	result.coherence.resize(locationIndex);
	std::ranges::sort(result.coherence, [](const auto &left, const auto &right) {
		return left.first < right.first;
	});
}

auto StableGraphAdapter::materializeCached(const ExecutionGraph &graph, bool verifyHit)
	-> StableGraphSnapshot
{
	GraphDescriptor ownedDescriptor;
	GraphDescriptor *descriptor;
	if (fastDescriptorReuse_) {
		describeInto(graph, scratchDescriptor_);
		descriptor = &scratchDescriptor_;
	} else {
		ownedDescriptor = describe(graph);
		descriptor = &ownedDescriptor;
	}
	if (descriptor->events.size() + descriptor->coherence.size() > 512) {
		cachedDescriptor_.reset();
		cachedSnapshot_.reset();
		return materialize(graph);
	}
	if (cachedDescriptor_ && *cachedDescriptor_ == *descriptor) {
		VERIFY(cachedSnapshot_.has_value(), "CAT primitive cache lacks its exact snapshot");
		++cacheHits_;
		if (verifyHit) {
			++cacheOracleChecks_;
			const auto oracle = materialize(graph);
			VERIFY(oracle.eventCount == cachedSnapshot_->eventCount &&
			       oracle.activeEventCount == cachedSnapshot_->activeEventCount &&
			       oracle.base == cachedSnapshot_->base &&
			       oracle.denseToStable == cachedSnapshot_->denseToStable,
			       "CAT unchanged primitive cache diverged from full materialization");
		}
		return *cachedSnapshot_;
	}
	++cacheMisses_;
	const auto commitDescriptor = [&] {
		if (fastDescriptorReuse_) {
			if (!cachedDescriptor_)
				cachedDescriptor_.emplace();
			std::swap(*cachedDescriptor_, scratchDescriptor_);
		} else {
			cachedDescriptor_ = std::move(ownedDescriptor);
		}
	};
	if (!cachedDescriptor_ || !cachedSnapshot_) {
		cachedSnapshot_ = fastDescriptorBuild_ ? materializeImpl(graph, descriptor)
						       : materialize(graph);
		commitDescriptor();
		return *cachedSnapshot_;
	}
	/* The per-relation changed-query delta prototype was rejected by the
	 * mutation oracle and by its construction-cost gate. Keep the exact
	 * unchanged-query cache, but rebuild every changed snapshot from the graph.
	 * This is the last independently validated cache boundary. */
	cachedSnapshot_ = fastDescriptorBuild_ ? materializeImpl(graph, descriptor)
					       : materialize(graph);
	commitDescriptor();
	return *cachedSnapshot_;

}

auto StableGraphAdapter::materialize(const ExecutionGraph &graph) -> StableGraphSnapshot
{
	return materializeImpl(graph, nullptr);
}

auto StableGraphAdapter::materializeImpl(const ExecutionGraph &graph,
					 const GraphDescriptor *prepared) -> StableGraphSnapshot
{
	const auto started = std::chrono::steady_clock::now();
	std::vector<std::pair<std::size_t, const EventLabel *>> active;
	std::vector<GraphAdapter::EventId> denseToStable;
	const auto appendActive = [&](const EventLabel &label) {
		StableEventKey key = label.getPos();
		auto [found, inserted] = ids_.try_emplace(key, keys_.size());
		if (inserted)
			keys_.push_back(std::move(key));
		active.emplace_back(found->second, &label);
		denseToStable.push_back(found->second);
	};
	if (prepared) {
		for (const auto &event : prepared->events) {
			VERIFY(event.label, "CAT prepared descriptor lacks its current label");
			appendActive(*event.label);
		}
	} else {
		for (const auto &label : graph.labels()) {
			if (!isRealEvent(label))
				continue;
			appendActive(label);
		}
	}

	std::vector<SAddr> locations;
	if (prepared) {
		locations.reserve(prepared->coherence.size());
		for (const auto &[address, stores] : prepared->coherence) {
			(void)stores;
			locations.push_back(address);
		}
	} else {
		for (auto location = graph.loc_begin(); location != graph.loc_end(); ++location)
			locations.push_back(location->first);
		std::ranges::sort(locations);
	}
	std::map<SAddr, std::size_t> initialIds;
	for (const auto location : locations) {
		StableEventKey key = location;
		auto [found, inserted] = ids_.try_emplace(key, keys_.size());
		if (inserted)
			keys_.push_back(std::move(key));
		initialIds.emplace(location, found->second);
		denseToStable.push_back(found->second);
	}
	const auto scanned = std::chrono::steady_clock::now();

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
	const bool directOrderedCoherence = fastCoherenceBuild_ && size <= 512;
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
	const auto labelsBuilt = std::chrono::steady_clock::now();

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
	const auto structuralBuilt = std::chrono::steady_clock::now();

	CoherenceRows coherenceRows;
	Relation directCoherence(directOrderedCoherence && needCoherence ? size : 0);
	if (needCoherence) {
		coherenceRows.reserve(initialIds.size());
		const auto appendCoherence = [&](SAddr address, const std::vector<Event> &positions) {
			std::vector<std::size_t> stores;
			for (const auto position : positions) {
				if (const auto writeId = eventId(position))
					stores.push_back(*writeId);
			}
			const auto initial = initialIds.find(address);
			VERIFY(initial != initialIds.end(), "CAT coherence location lacks initial write");
			coherenceRows.emplace_back(initial->second, stores);
			if (directOrderedCoherence) {
				EventSet later(size);
				for (auto current = stores.rbegin(); current != stores.rend(); ++current) {
					directCoherence.insertSuccessors(*current, later);
					later.insert(*current);
				}
				directCoherence.insertSuccessors(initial->second, later);
			} else {
				for (std::size_t current = 0; current < stores.size(); ++current) {
					coherenceEdges.emplace_back(initial->second, stores[current]);
					for (std::size_t later = current + 1; later < stores.size();
					     ++later)
						coherenceEdges.emplace_back(stores[current], stores[later]);
				}
			}
		};
		if (prepared) {
			for (const auto &[address, positions] : prepared->coherence)
				appendCoherence(address, positions);
		} else {
			for (auto currentLocation = graph.loc_begin();
			     currentLocation != graph.loc_end(); ++currentLocation) {
				std::vector<Event> positions;
				positions.reserve(currentLocation->second.size());
				for (const auto &write : currentLocation->second)
					positions.push_back(write.getPos());
				appendCoherence(currentLocation->first, positions);
			}
		}
	}
	const auto coherenceEdgesBuilt = std::chrono::steady_clock::now();
	Relation directFromRead(directOrderedCoherence && needFr ? size : 0);
	RelationEdges frEdges;
	if (needFr && directOrderedCoherence) {
		std::unordered_map<std::size_t, std::vector<std::size_t>> readsBySource;
		for (const auto [source, read] : readsFromEdges)
			readsBySource[source].push_back(read);
		for (const auto &[initial, stores] : coherenceRows) {
			EventSet later(size);
			for (auto current = stores.rbegin(); current != stores.rend(); ++current) {
				if (const auto reads = readsBySource.find(*current);
				    reads != readsBySource.end())
					for (const auto read : reads->second)
						directFromRead.insertSuccessors(read, later);
				later.insert(*current);
			}
			if (const auto reads = readsBySource.find(initial);
			    reads != readsBySource.end())
				for (const auto read : reads->second)
					directFromRead.insertSuccessors(read, later);
		}
	} else if (needFr) {
		frEdges = fastPrimitiveBuild_ ? fromReadEdgesOrdered(readsFromEdges, coherenceRows)
					      : fromReadEdges(readsFromEdges, coherenceEdges);
	}
	const auto fromReadBuilt = std::chrono::steady_clock::now();
	Relation readsFrom = needReadsFrom
				     ? sizeAdaptiveRelation(size, std::move(readsFromEdges),
							    fastPrimitiveBuild_)
				     : Relation{};
	Relation coherence = directOrderedCoherence && needCoherence
				     ? std::move(directCoherence)
				     : needCoherence
				     ? sizeAdaptiveRelation(size, std::move(coherenceEdges),
							    fastPrimitiveBuild_)
				     : Relation{};
	Relation fromRead = directOrderedCoherence && needFr
				? std::move(directFromRead)
				: needFr ? sizeAdaptiveRelation(size, std::move(frEdges),
							 fastPrimitiveBuild_)
					 : Relation{};
	Relation rmw = needRmw ? sizeAdaptiveRelation(size, std::move(rmwEdges),
							 fastPrimitiveBuild_)
				     : Relation{};
	Relation threadCreate =
		needTc ? sizeAdaptiveRelation(size, std::move(threadCreateEdges),
					      fastPrimitiveBuild_)
		       : Relation{};
	Relation threadJoin =
		needTj ? sizeAdaptiveRelation(size, std::move(threadJoinEdges),
					      fastPrimitiveBuild_)
		       : Relation{};
	const auto coherenceBuilt = std::chrono::steady_clock::now();

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
	const auto assembled = std::chrono::steady_clock::now();
	const auto ns = [](auto duration) {
		return static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
	};
	scanNanoseconds_ += ns(scanned - started);
	labelNanoseconds_ += ns(labelsBuilt - scanned);
	structuralNanoseconds_ += ns(structuralBuilt - labelsBuilt);
	coherenceNanoseconds_ += ns(coherenceBuilt - structuralBuilt);
	coherenceEdgeNanoseconds_ += ns(coherenceEdgesBuilt - structuralBuilt);
	fromReadNanoseconds_ += ns(fromReadBuilt - coherenceEdgesBuilt);
	relationPackNanoseconds_ += ns(coherenceBuilt - fromReadBuilt);
	assemblyNanoseconds_ += ns(assembled - coherenceBuilt);
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
