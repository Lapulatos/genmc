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

namespace cat {
namespace {

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
	return {keys_.size(), std::move(values), std::move(mapping)};
}

auto StableGraphAdapter::id(const StableEventKey &key) const -> std::optional<std::size_t>
{
	const auto found = ids_.find(key);
	return found == ids_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
}

} /* namespace cat */
