/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSkeletonCAT.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

namespace genmc::symbolic {
namespace {

struct DenseEvent {
	skeleton::NodeID site{skeleton::invalidNode};
	FiniteDensePart part{FiniteDensePart::ordinary};
	skeleton::NodeID function{skeleton::invalidNode};
	std::string address{};
};

auto isCATSite(const skeleton::EventSite &event) -> bool
{
	using enum skeleton::EventKind;
	return event.kind == load || event.kind == store || event.kind == lock ||
	       event.kind == unlock || event.kind == fence || event.kind == threadCreate ||
	       event.kind == threadJoin;
}

} /* namespace */

auto materializeFiniteAssignment(const skeleton::Program &program,
				 const FiniteAssignment &assignment)
	-> std::pair<FiniteCATResult, cat::BaseValues>
{
	FiniteCATResult result;
	cat::BaseValues base;
	std::vector<bool> active(program.events.size(), false);
	for (const auto event : assignment.activeEvents) {
		if (event >= active.size()) {
			result.errors.emplace_back("active event is outside the skeleton");
			return {std::move(result), std::move(base)};
		}
		active[event] = true;
	}

	std::vector<DenseEvent> dense;
	std::unordered_map<skeleton::NodeID, std::size_t> ordinary;
	std::unordered_map<skeleton::NodeID, std::size_t> lockReads;
	std::unordered_map<skeleton::NodeID, std::size_t> lockWrites;
	std::set<std::string> locations;
	for (const auto &event : program.events) {
		if (!active[event.id] || !isCATSite(event))
			continue;
		if (!event.address.empty())
			locations.insert(event.address);
		if (event.kind == skeleton::EventKind::lock) {
			lockReads[event.id] = dense.size();
			dense.push_back(
				{event.id, FiniteDensePart::lockRead, event.function, event.address});
			lockWrites[event.id] = dense.size();
			dense.push_back(
				{event.id, FiniteDensePart::lockWrite, event.function, event.address});
		} else {
			ordinary[event.id] = dense.size();
			dense.push_back(
				{event.id, FiniteDensePart::ordinary, event.function, event.address});
		}
	}
	std::map<std::string, std::size_t> initials;
	for (const auto &location : locations) {
		initials.emplace(location, dense.size());
		dense.push_back(
			{skeleton::invalidNode, FiniteDensePart::initial, skeleton::invalidNode,
			 location});
	}
	result.eventCount = dense.size();
	for (const auto &event : dense)
		result.denseEvents.push_back({event.site, event.part, event.address});

	cat::EventSet universe(dense.size());
	cat::EventSet reads(dense.size());
	cat::EventSet writes(dense.size());
	cat::EventSet fences(dense.size());
	cat::EventSet initialWrites(dense.size());
	cat::EventSet sequentiallyConsistent(dense.size());
	cat::Relation po(dense.size());
	cat::Relation rf(dense.size());
	cat::Relation co(dense.size());
	cat::Relation rmw(dense.size());
	cat::Relation loc(dense.size());
	cat::Relation internal(dense.size());
	cat::Relation external(dense.size());
	cat::Relation tc(dense.size());
	cat::Relation tj(dense.size());

	for (std::size_t id = 0; id < dense.size(); ++id) {
		universe.insert(id);
		const auto &entry = dense[id];
		if (entry.part == FiniteDensePart::initial) {
			writes.insert(id);
			initialWrites.insert(id);
			continue;
		}
		const auto kind = program.events[entry.site].kind;
		if (kind == skeleton::EventKind::load ||
		    entry.part == FiniteDensePart::lockRead)
			reads.insert(id);
		if (kind == skeleton::EventKind::store || kind == skeleton::EventKind::unlock ||
		    entry.part == FiniteDensePart::lockWrite)
			writes.insert(id);
		if (kind == skeleton::EventKind::fence)
			fences.insert(id);
		if (program.events[entry.site].sequentiallyConsistent)
			sequentiallyConsistent.insert(id);
	}

	for (std::size_t from = 0; from < dense.size(); ++from) {
		for (std::size_t to = 0; to < dense.size(); ++to) {
			const auto &lhs = dense[from];
			const auto &rhs = dense[to];
			if (lhs.part == FiniteDensePart::initial &&
			    rhs.part == FiniteDensePart::initial)
				internal.insertDense(from, to);
			else if (lhs.part != FiniteDensePart::initial &&
				 rhs.part != FiniteDensePart::initial &&
				 lhs.function == rhs.function) {
				internal.insertDense(from, to);
				if (lhs.site < rhs.site ||
				    (lhs.site == rhs.site &&
				     lhs.part == FiniteDensePart::lockRead &&
				     rhs.part == FiniteDensePart::lockWrite))
					po.insertDense(from, to);
			} else
				external.insertDense(from, to);
			if (!lhs.address.empty() && lhs.address == rhs.address)
				loc.insertDense(from, to);
		}
	}

	for (const auto &[site, read] : lockReads)
		rmw.insertDense(read, lockWrites.at(site));
	for (const auto &event : program.events) {
		if (!active[event.id])
			continue;
		if (event.kind == skeleton::EventKind::threadCreate) {
			const auto create = ordinary.find(event.id);
			if (create == ordinary.end())
				continue;
			std::optional<std::size_t> first;
			for (std::size_t id = 0; id < dense.size(); ++id)
				if (dense[id].part != FiniteDensePart::initial &&
				    program.functions[dense[id].function].name == event.threadEntry &&
				    (!first || dense[id].site < dense[*first].site))
					first = id;
			if (first)
				tc.insertDense(create->second, *first);
		}
	}

	for (const auto &event : program.events) {
		if (!active[event.id] || (event.kind != skeleton::EventKind::load &&
					 event.kind != skeleton::EventKind::lock))
			continue;
		if (event.id >= assignment.readsFrom.size() ||
		    !assignment.readsFrom[event.id]) {
			result.errors.push_back("active read lacks an RF source");
			continue;
		}
		const auto source = *assignment.readsFrom[event.id];
		const auto read = event.kind == skeleton::EventKind::lock
					  ? lockReads.at(event.id)
					  : ordinary.at(event.id);
		std::optional<std::size_t> write;
		if (source == skeleton::invalidNode)
			write = initials.at(event.address);
		else if (program.events[source].kind == skeleton::EventKind::lock)
			write = lockWrites.at(source);
		else {
			const auto found = ordinary.find(source);
			if (found != ordinary.end())
				write = found->second;
		}
		if (!write)
			result.errors.push_back("RF source is not an active write");
		else
			rf.insertDense(*write, read);
	}

	std::map<std::string, std::vector<std::size_t>> orderedWrites;
	for (const auto site : assignment.coherenceOrder) {
		if (site >= program.events.size() || !active[site]) {
			result.errors.emplace_back("CO contains an inactive or invalid store");
			continue;
		}
		const auto &event = program.events[site];
		const auto write = event.kind == skeleton::EventKind::lock
					   ? std::optional<std::size_t>(lockWrites.at(site))
					   : (ordinary.contains(site)
						      ? std::optional<std::size_t>(ordinary.at(site))
						      : std::nullopt);
		if (!write)
			result.errors.emplace_back("CO site is not an active write");
		else
			orderedWrites[event.address].push_back(*write);
	}
	for (const auto &[address, order] : orderedWrites) {
		const auto initial = initials.at(address);
		for (std::size_t i = 0; i < order.size(); ++i) {
			co.insertDense(initial, order[i]);
			for (std::size_t j = i + 1; j < order.size(); ++j)
				co.insertDense(order[i], order[j]);
		}
	}

	base.emplace("_", universe);
	base.emplace("R", reads);
	base.emplace("W", writes);
	base.emplace("F", fences);
	base.emplace("IW", initialWrites);
	base.emplace("SC", sequentiallyConsistent);
	base.emplace("0", cat::Relation(dense.size()));
	base.emplace("id", cat::identity(universe));
	base.emplace("po", po);
	base.emplace("rf", rf);
	base.emplace("co", co);
	base.emplace("fr", cat::compose(cat::inverse(rf), co));
	base.emplace("rmw", rmw);
	base.emplace("loc", loc);
	base.emplace("int", internal);
	base.emplace("ext", external);
	base.emplace("tc", tc);
	base.emplace("tj", tj);
	return {std::move(result), std::move(base)};
}

auto evaluateFiniteAssignment(const skeleton::Program &program,
			      const FiniteAssignment &assignment, const cat::ModelIR &model)
	-> FiniteCATResult
{
	auto [result, base] = materializeFiniteAssignment(program, assignment);
	if (result.errors.empty())
		result.evaluation = cat::Evaluator{}.evaluate(model, result.eventCount, base);
	return result;
}

auto evaluateFiniteAssignment(const skeleton::Program &program,
			      const FiniteAssignment &assignment,
			      const cat::NormalizedModel &model,
			      const cat::ModelAnalysis &analysis) -> FiniteCATResult
{
	auto [result, base] = materializeFiniteAssignment(program, assignment);
	if (result.errors.empty()) {
		auto evaluated =
			cat::CaatEvaluator{}.evaluate(model, analysis, result.eventCount, base);
		if (evaluated.errors.empty() && !evaluated.violations.empty()) {
			auto explained = cat::Reasoner{}.explain(
				model, analysis, result.eventCount, evaluated.values,
				evaluated.violations);
			if (explained.ok())
				for (auto &violation : explained.violations)
					result.explanations.push_back(
						std::move(violation.explanation));
		}
		result.evaluation.violations = std::move(evaluated.violations);
		result.evaluation.errors = std::move(evaluated.errors);
		result.evaluation.evaluationCounts = std::move(evaluated.evaluationCounts);
	}
	return result;
}

} /* namespace genmc::symbolic */
