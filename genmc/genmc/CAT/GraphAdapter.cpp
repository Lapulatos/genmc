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

#include "genmc/CAT/GraphAdapter.hpp"

#include "genmc/CAT/Value.hpp"
#include "genmc/Execution/Event.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"
#include "genmc/Support/SAddr.hpp"

#include <algorithm>
#include <format>
#include <string_view>

namespace cat {
namespace {

/** Return whether a label maps one-to-one to a CAT event. */
auto isRealEvent(const EventLabel &label) -> bool
{
	/* Init is expanded into one virtual event per location below. */
	return !genmc::isa<EmptyLabel>(&label) && !genmc::isa<InitLabel>(&label);
}

/** Insert one set/relation value without relying on map default construction. */
template <typename T> void addValue(BaseValues &values, std::string_view name, T value)
{
	values.emplace(std::string(name), std::move(value));
}

/** Look up one graph position without mutating the dense index. */
auto lookup(const std::unordered_map<Event, GraphAdapter::EventId> &ids, Event event)
	-> std::optional<GraphAdapter::EventId>
{
	const auto found = ids.find(event);
	return found == ids.end() ? std::nullopt
				  : std::optional<GraphAdapter::EventId>(found->second);
}

/** Classify labels and translate direct graph-pointer edges. */
void buildLabelPrimitives(const ExecutionGraph &graph,
			  const std::vector<const EventLabel *> &labels,
			  const std::vector<std::optional<SAddr>> &initialLocations,
			  const std::unordered_map<Event, GraphAdapter::EventId> &ids,
			  const std::unordered_map<SAddr, GraphAdapter::EventId> &initialIds,
			  EventSet &universe, EventSet &reads, EventSet &writes, EventSet &fences,
			  EventSet &initialWrites, EventSet &sequentiallyConsistent,
			  Relation &readsFrom, Relation &readModifyWrite, Relation &threadCreate,
			  Relation &threadJoin)
{
	for (GraphAdapter::EventId event = 0; event < labels.size(); ++event) {
		const auto *label = labels[event];
		universe.insert(event);
		if (initialLocations[event]) {
			writes.insert(event);
			initialWrites.insert(event);
			continue;
		}
		if (genmc::isa<ReadLabel>(label))
			reads.insert(event);
		if (genmc::isa<WriteLabel>(label))
			writes.insert(event);
		if (genmc::isa<FenceLabel>(label))
			fences.insert(event);
		if (label->isSC())
			sequentiallyConsistent.insert(event);

		if (const auto *read = genmc::dyn_cast<ReadLabel>(label); read && read->getRf()) {
			if (genmc::isa<InitLabel>(read->getRf())) {
				if (const auto source = initialIds.find(read->getAddr());
				    source != initialIds.end())
					readsFrom.insertDense(source->second, event);
			} else if (const auto source = lookup(ids, read->getRf()->getPos())) {
				readsFrom.insertDense(*source, event);
			}
			/* GenMC represents a successful RMW as adjacent read/write labels. */
			if (read->isRMW()) {
				const auto *write = graph.po_imm_succ(read);
				if (write) {
					if (const auto writeId = lookup(ids, write->getPos()))
						readModifyWrite.insertDense(event, *writeId);
				}
			}
		}
		if (const auto *start = genmc::dyn_cast<ThreadStartLabel>(label);
		    start && start->getCreate()) {
			if (const auto create = lookup(ids, start->getCreate()->getPos()))
				threadCreate.insertDense(*create, event);
		}
		if (const auto *finish = genmc::dyn_cast<ThreadFinishLabel>(label);
		    finish && finish->getParentJoin()) {
			if (const auto join = lookup(ids, finish->getParentJoin()->getPos()))
				threadJoin.insertDense(event, *join);
		}
	}
}

/** Build strict program order, thread partitions, and the same-location relation. */
void buildPairPrimitives(const std::vector<const EventLabel *> &labels,
			 const std::vector<std::optional<SAddr>> &initialLocations,
			 Relation &programOrder, Relation &location, Relation &internal,
			 Relation &external)
{
	for (GraphAdapter::EventId from = 0; from < labels.size(); ++from) {
		for (GraphAdapter::EventId to = 0; to < labels.size(); ++to) {
			const auto *lhs = labels[from];
			const auto *rhs = labels[to];
			const bool lhsInit = initialLocations[from].has_value();
			const bool rhsInit = initialLocations[to].has_value();
			/* Virtual initial writes share one synthetic initialization thread. */
			if ((lhsInit && rhsInit) ||
			    (!lhsInit && !rhsInit && lhs->getThread() == rhs->getThread())) {
				internal.insertDense(from, to);
				if (!lhsInit && lhs->getIndex() < rhs->getIndex())
					programOrder.insertDense(from, to);
			} else {
				external.insertDense(from, to);
			}

			const auto lhsLocation =
				lhsInit ? initialLocations[from]
					: (genmc::isa<MemAccessLabel>(lhs)
						   ? std::optional<SAddr>(
							     genmc::cast<MemAccessLabel>(lhs)
								     ->getAddr())
						   : std::nullopt);
			const auto rhsLocation =
				rhsInit ? initialLocations[to]
					: (genmc::isa<MemAccessLabel>(rhs)
						   ? std::optional<SAddr>(
							     genmc::cast<MemAccessLabel>(rhs)
								     ->getAddr())
						   : std::nullopt);
			if (lhsLocation && rhsLocation && *lhsLocation == *rhsLocation)
				location.insertDense(from, to);
		}
	}
}

/** Add strict per-location coherence, including GenMC's implicit initial write. */
void buildCoherence(const ExecutionGraph &graph,
		    const std::unordered_map<Event, GraphAdapter::EventId> &ids,
		    const std::unordered_map<SAddr, GraphAdapter::EventId> &initialIds,
		    Relation &coherence)
{
	for (auto location = graph.loc_begin(); location != graph.loc_end(); ++location) {
		std::vector<GraphAdapter::EventId> stores;
		for (const auto &write : location->second) {
			if (const auto writeId = lookup(ids, write.getPos()))
				stores.push_back(*writeId);
		}
		const auto initial = initialIds.find(location->first);
		for (std::size_t current = 0; current < stores.size(); ++current) {
			if (initial != initialIds.end())
				coherence.insertDense(initial->second, stores[current]);
			for (std::size_t later = current + 1; later < stores.size(); ++later)
				coherence.insertDense(stores[current], stores[later]);
		}
	}
}

} /* namespace */

GraphAdapter::GraphAdapter(const ExecutionGraph &graph)
	: graph_(&graph), labels_(), initialLocations_(), ids_(), initialIds_(), values_()
{
	/* Freeze real-label insertion order first so every later relation uses identical IDs. */
	for (const auto &label : graph.labels()) {
		if (!isRealEvent(label))
			continue;
		const auto dense = labels_.size();
		labels_.push_back(&label);
		initialLocations_.push_back(std::nullopt);
		ids_.emplace(label.getPos(), dense);
	}
	/* Expand GenMC's address-polymorphic InitLabel into stable per-location CAT writes. */
	std::vector<SAddr> locations;
	for (auto location = graph.loc_begin(); location != graph.loc_end(); ++location)
		locations.push_back(location->first);
	std::ranges::sort(locations);
	for (const auto location : locations) {
		const auto dense = labels_.size();
		labels_.push_back(graph.getInitLabel());
		initialLocations_.push_back(location);
		initialIds_.emplace(location, dense);
	}

	const auto size = eventCount();
	EventSet universe(size);
	EventSet reads(size);
	EventSet writes(size);
	EventSet fences(size);
	EventSet initialWrites(size);
	EventSet sequentiallyConsistent(size);
	Relation programOrder(size);
	Relation readsFrom(size);
	Relation coherence(size);
	Relation rmw(size);
	Relation loc(size);
	Relation internal(size);
	Relation external(size);
	Relation threadCreate(size);
	Relation threadJoin(size);

	buildLabelPrimitives(graph, labels_, initialLocations_, ids_, initialIds_, universe, reads,
			     writes, fences, initialWrites, sequentiallyConsistent, readsFrom, rmw,
			     threadCreate, threadJoin);
	buildPairPrimitives(labels_, initialLocations_, programOrder, loc, internal, external);
	buildCoherence(graph, ids_, initialIds_, coherence);

	/* Per-location virtual IW events make ordinary relational fr composition exact. */
	const auto fromRead = compose(inverse(readsFrom), coherence);
	addValue(values_, "_", std::move(universe));
	addValue(values_, "R", std::move(reads));
	addValue(values_, "W", std::move(writes));
	addValue(values_, "F", std::move(fences));
	addValue(values_, "IW", std::move(initialWrites));
	addValue(values_, "SC", std::move(sequentiallyConsistent));
	addValue(values_, "0", Relation(size));
	addValue(values_, "id", identity(std::get<EventSet>(values_.at("_"))));
	addValue(values_, "po", std::move(programOrder));
	addValue(values_, "rf", std::move(readsFrom));
	addValue(values_, "co", std::move(coherence));
	addValue(values_, "fr", fromRead);
	addValue(values_, "rmw", std::move(rmw));
	addValue(values_, "loc", std::move(loc));
	addValue(values_, "int", std::move(internal));
	addValue(values_, "ext", std::move(external));
	addValue(values_, "tc", std::move(threadCreate));
	addValue(values_, "tj", std::move(threadJoin));
}

auto GraphAdapter::id(Event event) const -> std::optional<EventId>
{
	const auto found = ids_.find(event);
	return found == ids_.end() ? std::nullopt : std::optional<EventId>(found->second);
}

auto GraphAdapter::initialId(SAddr location) const -> std::optional<EventId>
{
	const auto found = initialIds_.find(location);
	return found == initialIds_.end() ? std::nullopt : std::optional<EventId>(found->second);
}

auto GraphAdapter::structurallyCurrent() const -> bool
{
	std::size_t dense = 0;
	for (const auto &label : graph_->labels()) {
		if (!isRealEvent(label))
			continue;
		if (dense == labels_.size() || labels_[dense] != &label)
			return false;
		++dense;
	}
	if (dense + initialIds_.size() != labels_.size())
		return false;
	std::vector<SAddr> locations;
	for (auto location = graph_->loc_begin(); location != graph_->loc_end(); ++location)
		locations.push_back(location->first);
	std::ranges::sort(locations);
	if (locations.size() != initialIds_.size())
		return false;
	for (const auto location : locations) {
		if (!initialIds_.contains(location))
			return false;
	}
	return true;
}

auto GraphAdapter::validate() const -> std::vector<std::string>
{
	std::vector<std::string> errors;
	if (ids_.size() + initialIds_.size() != labels_.size())
		errors.emplace_back("event positions are not unique");
	for (EventId event = 0; event < labels_.size(); ++event) {
		const auto roundTrip = initialLocations_[event]
					       ? initialId(*initialLocations_[event])
					       : id(labels_[event]->getPos());
		if (roundTrip != event)
			errors.push_back(std::format("dense event {} does not round-trip", event));
	}
	for (const auto &[name, value] : values_) {
		const auto valueSize =
			std::visit([](const auto &primitive) { return primitive.size(); }, value);
		if (valueSize != labels_.size())
			errors.push_back(std::format("primitive '{}' has universe {}, expected {}",
						     name, valueSize, labels_.size()));
	}

	const auto &rf = std::get<Relation>(values_.at("rf"));
	const auto &coherence = std::get<Relation>(values_.at("co"));
	const auto &fr = std::get<Relation>(values_.at("fr"));
	const auto &readSet = std::get<EventSet>(values_.at("R"));
	const auto &writeSet = std::get<EventSet>(values_.at("W"));
	const auto &readModifyWrite = std::get<Relation>(values_.at("rmw"));
	for (EventId read = 0; read < labels_.size(); ++read) {
		std::size_t predecessors = 0;
		for (EventId write = 0; write < labels_.size(); ++write) {
			predecessors += rf.contains(write, read) ? 1U : 0U;
			if (rf.contains(write, read) &&
			    (!writeSet.contains(write) || !readSet.contains(read)))
				errors.push_back(std::format(
					"rf edge {} -> {} has invalid endpoint", write, read));
			if (readModifyWrite.contains(write, read) &&
			    (!readSet.contains(write) || !writeSet.contains(read)))
				errors.push_back(std::format(
					"rmw edge {} -> {} has invalid endpoint", write, read));
		}
		if (predecessors > 1)
			errors.push_back(std::format("rf is not functional at event {}", read));
	}
	if (fr != compose(inverse(rf), coherence))
		errors.emplace_back("fr differs from rf^-1 ; co");

	/* Strict per-location co must not contain a self edge or relate different addresses. */
	for (EventId from = 0; from < labels_.size(); ++from) {
		for (EventId to = 0; to < labels_.size(); ++to) {
			if (!coherence.contains(from, to))
				continue;
			if (!writeSet.contains(from) || !writeSet.contains(to))
				errors.push_back(std::format(
					"co edge {} -> {} has invalid endpoint", from, to));
			if (from == to)
				errors.push_back(
					std::format("co contains self edge at event {}", from));
			const auto lhsLocation =
				initialLocations_[from]
					? initialLocations_[from]
					: (genmc::isa<MemAccessLabel>(labels_[from])
						   ? std::optional<SAddr>(
							     genmc::cast<MemAccessLabel>(
								     labels_[from])
								     ->getAddr())
						   : std::nullopt);
			const auto rhsLocation =
				initialLocations_[to]
					? initialLocations_[to]
					: (genmc::isa<MemAccessLabel>(labels_[to])
						   ? std::optional<SAddr>(
							     genmc::cast<MemAccessLabel>(
								     labels_[to])
								     ->getAddr())
						   : std::nullopt);
			if (lhsLocation && rhsLocation && *lhsLocation != *rhsLocation)
				errors.push_back(std::format("co edge {} -> {} crosses locations",
							     from, to));
		}
	}
	return errors;
}

} /* namespace cat */
