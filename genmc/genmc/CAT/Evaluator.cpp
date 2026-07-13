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

#include "genmc/CAT/Evaluator.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace cat {
namespace {

/** Per-call memo table and witness/error construction state. */
class Evaluation {
public:
	Evaluation(const ModelIR &model, std::size_t eventCount, const BaseValues &base)
		: model_(model), eventCount_(eventCount), base_(base), memo_(model.nodes().size()),
		  attempted_(model.nodes().size()), counts_(model.nodes().size()), violations_(),
		  errors_()
	{}

	auto run() -> EvaluationResult
	{
		/* Evaluate named bindings even when no check references them. Shared DAG
		 * nodes remain cached when checks are evaluated afterwards. */
		for (const auto &binding : model_.bindings())
			evaluateNode(binding.value);
		for (const auto &check : model_.checks()) {
			const auto *value = evaluateNode(check.value);
			if (value)
				evaluateCheck(check, *value);
		}
		return {std::move(violations_), std::move(errors_), std::move(counts_)};
	}

private:
	/** Construct the full event universe used by `_`, `id`, and optional closure. */
	[[nodiscard]] auto universe() const -> EventSet
	{
		EventSet result(eventCount_);
		for (std::size_t event = 0; event < eventCount_; ++event)
			result.insert(event);
		return result;
	}

	/** Return whether a supplied value matches the node's static type and universe. */
	[[nodiscard]] auto validBaseValue(const Node &node, const Value &value) const -> bool
	{
		if (node.type == ValueType::Set) {
			const auto *set = std::get_if<EventSet>(&value);
			return set && set->size() == eventCount_;
		}
		const auto *relation = std::get_if<Relation>(&value);
		return relation && relation->size() == eventCount_;
	}

	/** Evaluate one node recursively and memoize its owned value exactly once. */
	auto evaluateNode(NodeId id) -> const Value *
	{
		if (attempted_.at(id))
			return memo_[id] ? &*memo_[id] : nullptr;
		attempted_[id] = true;
		const auto &node = model_.nodes().at(id);
		++counts_[id];
		if (node.kind == Node::Kind::Builtin)
			return evaluateBuiltin(node);

		std::vector<const Value *> operands;
		operands.reserve(node.operands.size());
		for (const auto operand : node.operands) {
			const auto *value = evaluateNode(operand);
			if (!value)
				return nullptr;
			operands.push_back(value);
		}
		memo_[id] = evaluateOperation(node, operands);
		return &*memo_[id];
	}

	/** Materialize intrinsic constants or copy one caller-supplied primitive. */
	auto evaluateBuiltin(const Node &node) -> const Value *
	{
		if (node.name == "0")
			memo_[node.id] = Relation(eventCount_);
		else if (node.name == "_")
			memo_[node.id] = universe();
		else if (node.name == "id")
			memo_[node.id] = identity(universe());
		else if (const auto found = base_.find(node.name); found != base_.end()) {
			if (!validBaseValue(node, found->second)) {
				errors_.push_back({node.id, node.span,
						   "primitive '" + node.name +
							   "' has wrong type or event universe"});
				return nullptr;
			}
			memo_[node.id] = found->second;
		} else {
			errors_.push_back(
				{node.id, node.span, "missing CAT primitive '" + node.name + "'"});
			return nullptr;
		}
		return &*memo_[node.id];
	}

	/** Dispatch a statically typed IR operation to the matching pure value algebra. */
	[[nodiscard]] auto evaluateOperation(const Node &node,
					     const std::vector<const Value *> &operands) const
		-> Value
	{
		if (node.kind == Node::Kind::Identity)
			return identity(std::get<EventSet>(*operands[0]));
		if (node.kind == Node::Kind::Inverse)
			return inverse(std::get<Relation>(*operands[0]));
		if (node.kind == Node::Kind::Optional)
			return optional(std::get<Relation>(*operands[0]));
		if (node.kind == Node::Kind::TransitiveClosure)
			return transitiveClosure(std::get<Relation>(*operands[0]));
		if (node.kind == Node::Kind::ReflexiveTransitiveClosure)
			return reflexiveTransitiveClosure(std::get<Relation>(*operands[0]));
		if (node.kind == Node::Kind::Product)
			return product(std::get<EventSet>(*operands[0]),
				       std::get<EventSet>(*operands[1]));
		if (node.kind == Node::Kind::Composition)
			return compose(std::get<Relation>(*operands[0]),
				       std::get<Relation>(*operands[1]));

		if (node.type == ValueType::Set) {
			const auto &lhs = std::get<EventSet>(*operands[0]);
			const auto &rhs = std::get<EventSet>(*operands[1]);
			if (node.kind == Node::Kind::Union)
				return setUnion(lhs, rhs);
			if (node.kind == Node::Kind::Intersection)
				return setIntersection(lhs, rhs);
			return setDifference(lhs, rhs);
		}
		const auto &lhs = std::get<Relation>(*operands[0]);
		const auto &rhs = std::get<Relation>(*operands[1]);
		if (node.kind == Node::Kind::Union)
			return relationUnion(lhs, rhs);
		if (node.kind == Node::Kind::Intersection)
			return relationIntersection(lhs, rhs);
		return relationDifference(lhs, rhs);
	}

	/** Return one closed DFS cycle, or an empty vector when relation is acyclic. */
	[[nodiscard]] auto findCycle(const Relation &relation) const -> std::vector<std::size_t>
	{
		std::vector<std::uint8_t> color(eventCount_);
		std::vector<std::size_t> parent(eventCount_, eventCount_);
		std::vector<std::size_t> cycle;
		auto visit = [&](auto &self, std::size_t event) -> bool {
			color[event] = 1;
			for (std::size_t target = 0; target < eventCount_; ++target) {
				if (!relation.contains(event, target))
					continue;
				if (color[target] == 0) {
					parent[target] = event;
					if (self(self, target))
						return true;
				} else if (color[target] == 1) {
					cycle.push_back(event);
					while (cycle.back() != target)
						cycle.push_back(parent[cycle.back()]);
					std::ranges::reverse(cycle);
					cycle.push_back(target);
					return true;
				}
			}
			color[event] = 2;
			return false;
		};
		for (std::size_t event = 0; event < eventCount_; ++event) {
			if (color[event] == 0 && visit(visit, event))
				break;
		}
		return cycle;
	}

	/** Evaluate one named check and append a compact witness on failure. */
	void evaluateCheck(const Check &check, const Value &value)
	{
		std::vector<std::size_t> witness;
		if (check.kind == Statement::CheckKind::Empty) {
			if (const auto *set = std::get_if<EventSet>(&value)) {
				if (!set->empty())
					witness.push_back(set->first());
			} else {
				const auto &relation = std::get<Relation>(value);
				for (std::size_t from = 0; from < eventCount_ && witness.empty();
				     ++from) {
					const auto target = relation.successors(from).first();
					if (target != eventCount_)
						witness = {from, target};
				}
			}
		} else if (check.kind == Statement::CheckKind::Irreflexive) {
			const auto &relation = std::get<Relation>(value);
			for (std::size_t event = 0; event < eventCount_; ++event) {
				if (relation.contains(event, event)) {
					witness.push_back(event);
					break;
				}
			}
		} else {
			witness = findCycle(std::get<Relation>(value));
		}
		if (!witness.empty())
			violations_.push_back(
				{check.name, check.kind, check.span, std::move(witness)});
	}

	const ModelIR &model_;
	std::size_t eventCount_{};
	const BaseValues &base_;
	std::vector<std::optional<Value>> memo_;
	std::vector<bool> attempted_;
	std::vector<std::size_t> counts_;
	std::vector<Violation> violations_;
	std::vector<EvaluationError> errors_;
};

} /* namespace */

auto Evaluator::evaluate(const ModelIR &model, std::size_t eventCount, const BaseValues &base) const
	-> EvaluationResult
{
	return Evaluation(model, eventCount, base).run();
}

} /* namespace cat */
