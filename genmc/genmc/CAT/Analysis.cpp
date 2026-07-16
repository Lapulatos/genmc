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

#include "genmc/CAT/Analysis.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace cat {
namespace {

/** Per-call Tarjan, polarity, and admissibility state. */
class AnalysisBuilder {
public:
	explicit AnalysisBuilder(const NormalizedModel &model)
		: model_(model), adjacency_(model.predicates().size()),
		  index_(model.predicates().size(), unvisited), lowlink_(model.predicates().size()),
		  onStack_(model.predicates().size()), domainIndependent_(model.predicates().size())
	{}

	auto run() -> AnalysisResult
	{
		buildDependencies();
		computeComponents();
		orderComponents();
		validateDeclaredRecursion();
		validateStratification();
		validateSemiPositivity();
		computeDomainIndependence();
		validateCheckDomains();
		if (!diagnostics_.empty())
			return {nullptr, std::move(diagnostics_)};
		buildLazyCyclePlans();
		std::shared_ptr<const ModelAnalysis> analysis = std::make_shared<ModelAnalysis>(
			std::move(dependencies_), std::move(strata_), std::move(componentOf_),
			std::move(domainIndependent_), std::move(lazyCycleRoots_),
			std::move(lazyCycleElided_));
		return {std::move(analysis), {}};
	}

private:
	static constexpr std::uint32_t unvisited = std::numeric_limits<std::uint32_t>::max();

	void diagnose(const Predicate &predicate, std::string message)
	{
		diagnostics_.push_back(
			{DiagnosticKind::Unsupported, predicate.span, std::move(message), {}});
	}

	void buildDependencies()
	{
		for (const auto &predicate : model_.predicates()) {
			for (std::size_t operandIndex = 0; operandIndex < predicate.operands.size();
			     ++operandIndex) {
				const auto operand = predicate.operands[operandIndex];
				const bool negative =
					predicate.kind == Predicate::Kind::Difference &&
					operandIndex == 1;
				dependencies_.push_back({operand, predicate.id, negative});
				adjacency_[operand].push_back(predicate.id);
			}
		}
	}

	void strongConnect(PredicateId predicate)
	{
		index_[predicate] = nextIndex_;
		lowlink_[predicate] = nextIndex_++;
		stack_.push_back(predicate);
		onStack_[predicate] = true;
		for (const auto target : adjacency_[predicate]) {
			if (index_[target] == unvisited) {
				strongConnect(target);
				lowlink_[predicate] =
					std::min(lowlink_[predicate], lowlink_[target]);
			} else if (onStack_[target]) {
				lowlink_[predicate] = std::min(lowlink_[predicate], index_[target]);
			}
		}
		if (lowlink_[predicate] != index_[predicate])
			return;
		std::vector<PredicateId> component;
		for (;;) {
			const auto member = stack_.back();
			stack_.pop_back();
			onStack_[member] = false;
			component.push_back(member);
			if (member == predicate)
				break;
		}
		std::ranges::sort(component);
		rawComponents_.push_back(std::move(component));
	}

	void computeComponents()
	{
		for (PredicateId predicate = 0; predicate < model_.predicates().size();
		     ++predicate) {
			if (index_[predicate] == unvisited)
				strongConnect(predicate);
		}
	}

	void orderComponents()
	{
		/* Tarjan emits reverse topological SCCs for source-to-user dependency
		 * edges. Reverse them to evaluate dependencies before their users. */
		strata_.assign(rawComponents_.rbegin(), rawComponents_.rend());
		componentOf_.resize(model_.predicates().size());
		for (std::uint32_t component = 0; component < strata_.size(); ++component) {
			for (const auto predicate : strata_[component])
				componentOf_[predicate] = component;
		}
	}

	[[nodiscard]] auto componentIsCyclic(std::uint32_t component) const -> bool
	{
		if (strata_[component].size() > 1)
			return true;
		const auto only = strata_[component].front();
		return std::ranges::any_of(dependencies_, [&](const Dependency &dependency) {
			return dependency.source == only && dependency.target == only;
		});
	}

	void validateDeclaredRecursion()
	{
		for (std::uint32_t component = 0; component < strata_.size(); ++component) {
			if (!componentIsCyclic(component))
				continue;
			std::uint32_t group{};
			for (const auto id : strata_[component]) {
				const auto &predicate = model_.predicates()[id];
				if (predicate.generated || predicate.kind == Predicate::Kind::Base)
					continue;
				if (predicate.declaredRecursiveGroup == 0) {
					diagnose(predicate,
						 "recursive predicate '" + predicate.name +
							 "' must be declared with 'let rec'");
					continue;
				}
				if (group == 0)
					group = predicate.declaredRecursiveGroup;
				else if (group != predicate.declaredRecursiveGroup)
					diagnose(predicate, "mutually recursive predicates must "
							    "share one declaration group");
			}
		}
	}

	void validateStratification()
	{
		for (const auto &dependency : dependencies_) {
			if (dependency.negative &&
			    componentOf_[dependency.source] == componentOf_[dependency.target]) {
				const auto &target = model_.predicates()[dependency.target];
				diagnose(target,
					 "negative dependency inside recursive stratum for '" +
						 target.name + "'");
			}
		}
	}

	void validateSemiPositivity()
	{
		for (const auto &predicate : model_.predicates()) {
			if (predicate.kind != Predicate::Kind::Difference)
				continue;
			const auto &rhs = model_.predicates()[predicate.operands[1]];
			if (rhs.kind != Predicate::Kind::Base)
				diagnose(predicate,
					 "non-semi-positive difference '" + predicate.name +
						 "' requires cutting derived predicate '" +
						 rhs.name + "'");
		}
	}

	[[nodiscard]] auto domainRule(const Predicate &predicate) const -> bool
	{
		const auto operand = [&](std::size_t index) {
			return domainIndependent_[predicate.operands[index]];
		};
		switch (predicate.kind) {
		case Predicate::Kind::Base:
			return predicate.name != "_" && predicate.name != "id";
		case Predicate::Kind::Intersection:
			return operand(0) || operand(1);
		case Predicate::Kind::Difference:
			return operand(0);
		case Predicate::Kind::Union:
		case Predicate::Kind::Composition:
		case Predicate::Kind::Product:
			return operand(0) && operand(1);
		case Predicate::Kind::Alias:
		case Predicate::Kind::Identity:
		case Predicate::Kind::Domain:
		case Predicate::Kind::Range:
		case Predicate::Kind::Inverse:
		case Predicate::Kind::Optional:
		case Predicate::Kind::TransitiveClosure:
		case Predicate::Kind::ReflexiveTransitiveClosure:
			return operand(0);
		}
		return false;
	}

	void computeDomainIndependence()
	{
		/* Recursive definitions such as `ob = base | ob ; ob` are guarded and
		 * domain-independent. Start from the greatest Boolean solution and remove
		 * predicates that violate a rule; a least solution would incorrectly reject
		 * every productive recursive equation merely because it references itself. */
		std::ranges::fill(domainIndependent_, true);
		bool changed = true;
		while (changed) {
			changed = false;
			for (const auto &predicate : model_.predicates()) {
				const auto value = domainRule(predicate);
				if (domainIndependent_[predicate.id] != value) {
					domainIndependent_[predicate.id] = value;
					changed = true;
				}
			}
		}
	}

	void validateCheckDomains()
	{
		for (const auto &check : model_.checks()) {
			if (!domainIndependent_[check.predicate]) {
				const auto &predicate = model_.predicates()[check.predicate];
				diagnostics_.push_back(
					{DiagnosticKind::Unsupported,
					 check.span,
					 "axiom '" + check.name +
						 "' is not domain-independent at predicate '" +
						 predicate.name + "'",
					 {}});
			}
		}
	}

	[[nodiscard]] auto componentIsRecursive(PredicateId id) const -> bool
	{
		const auto component = componentOf_[id];
		if (strata_[component].size() != 1)
			return true;
		return std::ranges::any_of(dependencies_, [id](const auto &dependency) {
			return dependency.source == id && dependency.target == id;
		});
	}

	/** Validate the exact positive expression grammar used by lazy cycle DFS. */
	[[nodiscard]] auto collectLazyCone(PredicateId id, std::vector<bool> &cone,
					   std::vector<bool> &visiting) const -> bool
	{
		if (cone[id])
			return true;
		if (visiting[id] || componentIsRecursive(id))
			return false;
		const auto &predicate = model_.predicates()[id];
		if (predicate.kind == Predicate::Kind::Base)
			return true;
		visiting[id] = true;
		const auto relationOperand = [&](std::size_t index) {
			const auto operand = predicate.operands[index];
			return model_.predicates()[operand].type == ValueType::Relation &&
			       collectLazyCone(operand, cone, visiting);
		};
		const auto setOperand = [&](std::size_t index) {
			const auto operand = predicate.operands[index];
			return model_.predicates()[operand].type == ValueType::Set &&
			       collectLazyCone(operand, cone, visiting);
		};
		bool supported{};
		if (predicate.type == ValueType::Set) {
			supported = (predicate.kind == Predicate::Kind::Alias && setOperand(0)) ||
				    ((predicate.kind == Predicate::Kind::Union ||
				      predicate.kind == Predicate::Kind::Intersection ||
				      predicate.kind == Predicate::Kind::Difference) &&
				     setOperand(0) && setOperand(1));
		} else {
			switch (predicate.kind) {
			case Predicate::Kind::Alias:
			case Predicate::Kind::Optional:
				supported = relationOperand(0);
				break;
			case Predicate::Kind::Union:
			case Predicate::Kind::Composition:
				supported = relationOperand(0) && relationOperand(1);
				break;
			case Predicate::Kind::Intersection: {
				const auto lhs = predicate.operands[0];
				const auto rhs = predicate.operands[1];
				const auto cheapFilter = [&](PredicateId operand) {
					const auto kind = model_.predicates()[operand].kind;
					return kind == Predicate::Kind::Base ||
					       kind == Predicate::Kind::Identity;
				};
				supported = (cheapFilter(lhs) || cheapFilter(rhs)) &&
					    relationOperand(0) && relationOperand(1);
				break;
			}
			case Predicate::Kind::Identity:
				supported = setOperand(0);
				break;
			default:
				supported = false;
				break;
			}
		}
		visiting[id] = false;
		if (supported)
			cone[id] = true;
		return supported;
	}

	void buildLazyCyclePlans()
	{
		lazyCycleRoots_.resize(model_.checks().size());
		lazyCycleElided_.assign(model_.predicates().size(), false);
		for (std::size_t checkIndex = 0; checkIndex < model_.checks().size(); ++checkIndex) {
			const auto &check = model_.checks()[checkIndex];
			if (check.kind != Statement::CheckKind::Acyclic ||
			    model_.predicates()[check.predicate].type != ValueType::Relation ||
			    model_.predicates()[check.predicate].kind == Predicate::Kind::Base)
				continue;
			std::vector<bool> cone(model_.predicates().size());
			std::vector<bool> visiting(model_.predicates().size());
			if (!collectLazyCone(check.predicate, cone, visiting))
				continue;
			bool exclusive = true;
			for (PredicateId id = 0; id < cone.size() && exclusive; ++id) {
				if (!cone[id] || model_.predicates()[id].kind == Predicate::Kind::Base)
					continue;
				for (const auto &dependency : dependencies_) {
					if (dependency.source == id && !cone[dependency.target]) {
						exclusive = false;
						break;
					}
				}
				for (std::size_t other = 0; other < model_.checks().size(); ++other) {
					if (other != checkIndex && model_.checks()[other].predicate == id)
						exclusive = false;
				}
			}
			if (!exclusive)
				continue;
			lazyCycleRoots_[checkIndex] = check.predicate;
			for (PredicateId id = 0; id < cone.size(); ++id) {
				if (cone[id] && model_.predicates()[id].kind != Predicate::Kind::Base)
					lazyCycleElided_[id] = true;
			}
		}
		/* Lazy enumeration amortizes its per-edge dispatch only when it removes at
		 * least one expensive composition cone. Union-only orders are faster and
		 * smaller in the packed generic evaluator, so select by normalized structure
		 * rather than a model/profile name or a runtime verdict. */
		const auto removesComposition = std::ranges::any_of(
			model_.predicates(), [&](const auto &predicate) {
				return lazyCycleElided_[predicate.id] &&
				       predicate.kind == Predicate::Kind::Composition;
			});
		if (!removesComposition) {
			std::ranges::fill(lazyCycleRoots_, std::nullopt);
			std::ranges::fill(lazyCycleElided_, false);
		}
	}

	const NormalizedModel &model_;
	std::vector<Dependency> dependencies_;
	std::vector<std::vector<PredicateId>> adjacency_;
	std::vector<std::uint32_t> index_;
	std::vector<std::uint32_t> lowlink_;
	std::vector<bool> onStack_;
	std::vector<PredicateId> stack_;
	std::uint32_t nextIndex_{};
	std::vector<std::vector<PredicateId>> rawComponents_;
	std::vector<std::vector<PredicateId>> strata_;
	std::vector<std::uint32_t> componentOf_;
	std::vector<bool> domainIndependent_;
	std::vector<std::optional<PredicateId>> lazyCycleRoots_;
	std::vector<bool> lazyCycleElided_;
	std::vector<Diagnostic> diagnostics_;
};

} /* namespace */

auto Analyzer::analyze(const NormalizedModel &model) const -> AnalysisResult
{
	return AnalysisBuilder(model).run();
}

} /* namespace cat */
