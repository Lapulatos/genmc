/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSkeletonSCCompletion.hpp"

#include "genmc/Verification/FiniteSkeletonCAT.hpp"
#include "genmc/Verification/FiniteSymbolicSolver.hpp"

#include <algorithm>
#include <bit>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <tuple>
#include <variant>
#include <vector>

namespace genmc::symbolic {
namespace {

[[nodiscard]] auto eventSet(const cat::BaseValues &base, std::string_view name)
	-> const cat::EventSet *
{
	const auto found = base.find(std::string(name));
	return found == base.end() ? nullptr : std::get_if<cat::EventSet>(&found->second);
}

[[nodiscard]] auto relation(const cat::BaseValues &base, std::string_view name)
	-> const cat::Relation *
{
	const auto found = base.find(std::string(name));
	return found == base.end() ? nullptr : std::get_if<cat::Relation>(&found->second);
}

void appendUnique(std::vector<rvf::EventId> &values, rvf::EventId value)
{
	if (std::ranges::find(values, value) == values.end())
		values.push_back(value);
}

[[nodiscard]] auto hasDiagonal(const cat::Relation &relation) -> bool
{
	for (std::size_t event = 0; event < relation.size(); ++event)
		if (relation.contains(event, event))
			return true;
	return false;
}

[[nodiscard]] auto recursiveSCBaseAccepts(const cat::BaseValues &base) -> bool
{
	const auto *po = relation(base, "po");
	const auto *tc = relation(base, "tc");
	const auto *tj = relation(base, "tj");
	const auto *rf = relation(base, "rf");
	const auto *fr = relation(base, "fr");
	const auto *co = relation(base, "co");
	const auto *rmw = relation(base, "rmw");
	const auto *internal = relation(base, "int");
	const auto *external = relation(base, "ext");
	if (!po || !tc || !tj || !rf || !fr || !co || !rmw || !internal || !external)
		return false;

	auto order = cat::relationUnion(*po, *tc);
	order = cat::relationUnion(order, *tj);
	order = cat::relationUnion(order, *rf);
	order = cat::relationUnion(order, *fr);
	order = cat::relationUnion(order, *co);
	if (hasDiagonal(cat::transitiveClosure(order)))
		return false;

	const auto fre = cat::relationIntersection(*fr, *external);
	const auto coe = cat::relationIntersection(*co, *external);
	return cat::relationIntersection(*rmw, cat::compose(fre, coe)).empty();
}

} /* namespace */

auto completeFiniteSC(const skeleton::Program &program, const FiniteAssignment &assignment)
	-> FiniteSCCompletionResult
{
	auto [snapshot, base] = materializeFiniteAssignment(program, assignment);
	if (!snapshot.errors.empty())
		return {.status = FiniteSCCompletionStatus::invalidInput,
			.error = snapshot.errors.front()};
	const auto *reads = eventSet(base, "R");
	const auto *writes = eventSet(base, "W");
	const auto *initials = eventSet(base, "IW");
	const auto *po = relation(base, "po");
	const auto *tc = relation(base, "tc");
	const auto *tj = relation(base, "tj");
	const auto *rf = relation(base, "rf");
	const auto *rmw = relation(base, "rmw");
	if (!reads || !writes || !initials || !po || !tc || !tj || !rf || !rmw)
		return {.status = FiniteSCCompletionStatus::invalidInput,
			.error = "finite CAT snapshot lacks an SC primitive"};

	std::map<std::string, rvf::VariableId> variables;
	for (const auto &event : snapshot.denseEvents)
		if (!event.address.empty() && !variables.contains(event.address))
			variables.emplace(event.address,
					  static_cast<rvf::VariableId>(variables.size()));

	rvf::Problem problem;
	problem.events.resize(snapshot.eventCount);
	problem.goodWrites.resize(snapshot.eventCount);
	std::optional<rvf::EventId> lastInitial;
	for (std::size_t dense = 0; dense < snapshot.eventCount; ++dense) {
		auto &event = problem.events[dense];
		event.id = static_cast<rvf::EventId>(dense);
		event.thread = static_cast<rvf::ThreadId>(dense);
		event.threadIndex = 0U;
		if (reads->contains(dense))
			event.kind = rvf::EventKind::read;
		else if (writes->contains(dense))
			event.kind = rvf::EventKind::write;
		else
			event.kind = rvf::EventKind::other;
		if (event.kind != rvf::EventKind::other) {
			const auto found = variables.find(snapshot.denseEvents[dense].address);
			if (found == variables.end())
				return {.status = FiniteSCCompletionStatus::invalidInput,
					.error = "finite memory event lacks an address"};
			event.variable = found->second;
		}
		if (initials->contains(dense)) {
			if (lastInitial)
				event.extraPredecessors.push_back(*lastInitial);
			lastInitial = event.id;
		}
	}
	if (lastInitial)
		for (auto &event : problem.events)
			if (!initials->contains(event.id))
				appendUnique(event.extraPredecessors, *lastInitial);

	for (std::size_t from = 0; from < snapshot.eventCount; ++from) {
		for (const auto *edge : {po, tc, tj}) {
			for (auto to = edge->nextSuccessor(from, 0U); to < snapshot.eventCount;
			     to = edge->nextSuccessor(from, to + 1U))
				appendUnique(problem.events[to].extraPredecessors,
					     static_cast<rvf::EventId>(from));
		}
	}

	for (std::size_t read = 0; read < snapshot.eventCount; ++read) {
		if (!reads->contains(read))
			continue;
		for (auto write = rf->nextPredecessor(read, 0U); write < snapshot.eventCount;
		     write = rf->nextPredecessor(read, write + 1U))
			problem.goodWrites[read].push_back(static_cast<rvf::EventId>(write));
		if (problem.goodWrites[read].size() != 1U)
			return {.status = FiniteSCCompletionStatus::invalidInput,
				.error = "fixed finite read does not have exactly one RF source"};
	}

	const auto witness = rvf::verifySC(problem);
	if (witness.status == rvf::Status::invalidInput)
		return {.status = FiniteSCCompletionStatus::invalidInput,
			.metrics = witness.metrics,
			.error = witness.error};
	if (witness.status == rvf::Status::noWitness) {
		FiniteSCCompletionResult result{
			.status = FiniteSCCompletionStatus::noWitness,
			.metrics = witness.metrics,
			.refinementSafe = true,
		};
		std::vector<std::vector<rvf::EventId>> writesByVariable(variables.size());
		for (const auto &event : problem.events)
			if (event.kind == rvf::EventKind::write)
				writesByVariable[event.variable].push_back(event.id);
		auto relaxedProblem = problem;
		std::vector<rvf::EventId> candidates;
		for (auto &event : relaxedProblem.events) {
			if (event.kind != rvf::EventKind::read)
				continue;
			const auto &relaxed = writesByVariable[event.variable];
			if (relaxedProblem.goodWrites[event.id] == relaxed)
				continue;
			candidates.push_back(event.id);
			relaxedProblem.goodWrites[event.id] = relaxed;
		}
		auto inconsistent = [&](std::span<const rvf::EventId> fixed) {
			auto query = relaxedProblem;
			for (const auto read : fixed)
				query.goodWrites[read] = problem.goodWrites[read];
			const auto reduced = rvf::verifySC(query);
			++result.coreChecks;
			if (reduced.status == rvf::Status::invalidInput) {
				result.status = FiniteSCCompletionStatus::invalidInput;
				result.error = reduced.error;
				return false;
			}
			return reduced.status == rvf::Status::noWitness;
		};
		std::function<std::vector<rvf::EventId>(std::vector<rvf::EventId>,
						      std::vector<rvf::EventId>)>
			quickXplain;
		quickXplain = [&](std::vector<rvf::EventId> background,
				    std::vector<rvf::EventId> choices) {
			if (choices.empty() || inconsistent(background))
				return std::vector<rvf::EventId>{};
			if (choices.size() == 1U)
				return choices;
			const auto middle = choices.begin() + choices.size() / 2U;
			std::vector<rvf::EventId> left(choices.begin(), middle);
			std::vector<rvf::EventId> right(middle, choices.end());
			auto withRight = background;
			withRight.insert(withRight.end(), right.begin(), right.end());
			auto leftCore = quickXplain(std::move(withRight), std::move(left));
			auto withLeftCore = std::move(background);
			withLeftCore.insert(withLeftCore.end(), leftCore.begin(), leftCore.end());
			auto rightCore = quickXplain(std::move(withLeftCore), std::move(right));
			leftCore.insert(leftCore.end(), rightCore.begin(), rightCore.end());
			return leftCore;
		};
		std::vector<rvf::EventId> core;
		if (!inconsistent(std::span<const rvf::EventId>{}))
			core = quickXplain({}, std::move(candidates));
		if (result.status == FiniteSCCompletionStatus::invalidInput)
			return result;
		for (const auto read : core) {
			const auto site = snapshot.denseEvents[read].site;
			if (site == skeleton::invalidNode) {
				result.status = FiniteSCCompletionStatus::invalidInput;
				result.error = "finite read RF core has no skeleton site";
				return result;
			}
			result.rfCoreLoads.push_back(site);
		}
		return result;
	}

	auto completed = assignment;
	completed.coherenceOrder.clear();
	for (const auto dense : witness.witness) {
		if (!writes->contains(dense) || initials->contains(dense))
			continue;
		const auto site = snapshot.denseEvents[dense].site;
		if (site == skeleton::invalidNode)
			return {.status = FiniteSCCompletionStatus::invalidInput,
				.metrics = witness.metrics,
				.error = "finite non-initial write has no skeleton site"};
		if (std::ranges::find(completed.coherenceOrder, site) ==
		    completed.coherenceOrder.end())
			completed.coherenceOrder.push_back(site);
	}

	auto [completedSnapshot, completedBase] =
		materializeFiniteAssignment(program, completed);
	if (!completedSnapshot.errors.empty() || !recursiveSCBaseAccepts(completedBase))
		return {.status = FiniteSCCompletionStatus::witnessRejected,
			.metrics = witness.metrics,
			.error = completedSnapshot.errors.empty()
					 ? "SC witness failed exact recursive-SC base checks"
					 : completedSnapshot.errors.front()};
	return {.status = FiniteSCCompletionStatus::completed,
		.assignment = std::move(completed),
		.metrics = witness.metrics};
}

auto completeFiniteSCWithOrdering(const skeleton::Program &program,
				  const FiniteAssignment &assignment)
	-> FiniteSCCompletionResult
{
	auto [snapshot, base] = materializeFiniteAssignment(program, assignment);
	if (!snapshot.errors.empty())
		return {.status = FiniteSCCompletionStatus::invalidInput,
			.error = snapshot.errors.front()};
	const auto *reads = eventSet(base, "R");
	const auto *writes = eventSet(base, "W");
	const auto *initials = eventSet(base, "IW");
	const auto *po = relation(base, "po");
	const auto *tc = relation(base, "tc");
	const auto *tj = relation(base, "tj");
	const auto *rf = relation(base, "rf");
	const auto *rmw = relation(base, "rmw");
	const auto *external = relation(base, "ext");
	if (!reads || !writes || !initials || !po || !tc || !tj || !rf || !rmw ||
	    !external)
		return {.status = FiniteSCCompletionStatus::invalidInput,
			.error = "finite CAT snapshot lacks an SC ordering primitive"};
	if (snapshot.eventCount == 0U) {
		auto completed = assignment;
		completed.coherenceOrder.clear();
		return {.status = FiniteSCCompletionStatus::completed,
			.assignment = std::move(completed),
			.refinementSafe = true};
	}

	Solver solver;
	const auto width = std::max(
		1U, static_cast<std::uint32_t>(std::bit_width(snapshot.eventCount - 1U)));
	std::vector<Expr> ranks;
	ranks.reserve(snapshot.eventCount);
	for (std::size_t event = 0; event < snapshot.eventCount; ++event)
		ranks.push_back(solver.bitVector("sc_rank_" + std::to_string(event), width));
	if (width < 64U && snapshot.eventCount < (std::uint64_t{1} << width)) {
		const auto bound = solver.bitVectorConstant(snapshot.eventCount, width);
		for (const auto rank : ranks)
			solver.constrain(solver.unsignedLess(rank, bound));
	}
	for (std::size_t lhs = 0; lhs < ranks.size(); ++lhs)
		for (std::size_t rhs = lhs + 1U; rhs < ranks.size(); ++rhs)
			solver.constrain(solver.logicalNot(solver.equal(ranks[lhs], ranks[rhs])));
	const auto constrainEdges = [&](const cat::Relation &edges) {
		for (std::size_t from = 0; from < snapshot.eventCount; ++from)
			for (auto to = edges.nextSuccessor(from, 0U); to < snapshot.eventCount;
			     to = edges.nextSuccessor(from, to + 1U))
				solver.constrain(solver.unsignedLess(ranks[from], ranks[to]));
	};
	constrainEdges(*po);
	constrainEdges(*tc);
	constrainEdges(*tj);
	for (std::size_t initial = 0; initial < snapshot.eventCount; ++initial) {
		if (!initials->contains(initial))
			continue;
		for (std::size_t write = 0; write < snapshot.eventCount; ++write)
			if (writes->contains(write) && !initials->contains(write) &&
			    snapshot.denseEvents[initial].address ==
				    snapshot.denseEvents[write].address)
				solver.constrain(solver.unsignedLess(ranks[initial], ranks[write]));
	}

	std::vector<Expr> assumptions;
	std::vector<std::size_t> assumptionReads;
	for (std::size_t read = 0; read < snapshot.eventCount; ++read) {
		if (!reads->contains(read))
			continue;
		const auto source = rf->nextPredecessor(read, 0U);
		if (source >= snapshot.eventCount ||
		    rf->nextPredecessor(read, source + 1U) < snapshot.eventCount)
			return {.status = FiniteSCCompletionStatus::invalidInput,
				.error = "fixed finite read does not have exactly one RF source"};
		std::vector<Expr> conditions;
		conditions.push_back(solver.unsignedLess(ranks[source], ranks[read]));
		for (std::size_t write = 0; write < snapshot.eventCount; ++write) {
			if (!writes->contains(write) || write == source ||
			    snapshot.denseEvents[write].address !=
				    snapshot.denseEvents[read].address)
				continue;
			const Expr outside[]{solver.unsignedLess(ranks[write], ranks[source]),
					     solver.unsignedLess(ranks[read], ranks[write])};
			conditions.push_back(solver.anyOf(outside));
		}
		const auto rmwWrite = rmw->nextSuccessor(read, 0U);
		if (rmwWrite < snapshot.eventCount) {
			for (std::size_t write = 0; write < snapshot.eventCount; ++write) {
				if (!writes->contains(write) || write == source || write == rmwWrite ||
				    snapshot.denseEvents[write].address !=
					    snapshot.denseEvents[read].address ||
				    !external->contains(read, write) ||
				    !external->contains(write, rmwWrite))
					continue;
				const Expr outside[]{
					solver.unsignedLess(ranks[write], ranks[source]),
					solver.unsignedLess(ranks[rmwWrite], ranks[write])};
				conditions.push_back(solver.anyOf(outside));
			}
		}
		const auto assumption =
			solver.boolean("sc_rf_assumption_" + std::to_string(read));
		solver.constrain(solver.implies(assumption, solver.allOf(conditions)));
		assumptions.push_back(assumption);
		assumptionReads.push_back(read);
	}

	const auto checked = solver.checkAssuming(assumptions);
	if (checked.status == CheckResult::unsat) {
		FiniteSCCompletionResult result{
			.status = FiniteSCCompletionStatus::noWitness,
			.coreChecks = 1U,
			.refinementSafe = true,
		};
		for (const auto index : checked.core) {
			if (index >= assumptionReads.size())
				return {.status = FiniteSCCompletionStatus::invalidInput,
					.error = "ordering UNSAT core index is out of range"};
			const auto site = snapshot.denseEvents[assumptionReads[index]].site;
			if (site == skeleton::invalidNode)
				return {.status = FiniteSCCompletionStatus::invalidInput,
					.error = "ordering UNSAT core read has no skeleton site"};
			result.rfCoreLoads.push_back(site);
		}
		return result;
	}
	if (checked.status != CheckResult::sat)
		return {.status = FiniteSCCompletionStatus::invalidInput,
			.error = "SC ordering solver is unavailable or returned unknown"};

	auto completed = assignment;
	completed.coherenceOrder.clear();
	std::vector<std::tuple<std::string, std::uint64_t, skeleton::NodeID>> ordered;
	for (std::size_t write = 0; write < snapshot.eventCount; ++write) {
		if (!writes->contains(write) || initials->contains(write))
			continue;
		const auto rank = solver.bitVectorValue(ranks[write]);
		const auto site = snapshot.denseEvents[write].site;
		if (!rank || site == skeleton::invalidNode)
			return {.status = FiniteSCCompletionStatus::invalidInput,
				.error = "SC ordering SAT model lacks a write rank"};
		ordered.emplace_back(snapshot.denseEvents[write].address, *rank, site);
	}
	std::ranges::sort(ordered);
	for (const auto &[unusedAddress, unusedRank, site] : ordered)
		if (std::ranges::find(completed.coherenceOrder, site) ==
		    completed.coherenceOrder.end())
			completed.coherenceOrder.push_back(site);
	auto [completedSnapshot, completedBase] =
		materializeFiniteAssignment(program, completed);
	if (!completedSnapshot.errors.empty() || !recursiveSCBaseAccepts(completedBase))
		return {.status = FiniteSCCompletionStatus::witnessRejected,
			.error = completedSnapshot.errors.empty()
					 ? "ordering SAT witness failed recursive-SC base checks"
					 : completedSnapshot.errors.front()};
	return {.status = FiniteSCCompletionStatus::completed,
		.assignment = std::move(completed),
		.refinementSafe = true};
}

} /* namespace genmc::symbolic */
