/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 */

#include "genmc/CAT/LazyCycle.hpp"

#include "genmc/Support/Error.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace cat {
namespace {

/** Allocation-free, non-owning callback used by recursive successor enumeration. */
class SuccessorConsumer {
public:
	template <typename Function> SuccessorConsumer(Function &function)
		: object_(&function), invoke_([](void *object, std::size_t target) {
			(*static_cast<Function *>(object))(target);
		})
	{}

	void operator()(std::size_t target) const { invoke_(object_, target); }

private:
	void *object_{};
	void (*invoke_)(void *, std::size_t){};
};

/** Exact extensional interpreter for one analyzer-certified relation expression. */
class LazyRelation {
public:
	LazyRelation(const NormalizedModel &model,
		     const std::vector<std::optional<Value>> &values, std::size_t eventCount,
		     LazyCycleStatistics *statistics)
		: model_(model), values_(values), eventCount_(eventCount), seen_(eventCount),
		  statistics_(statistics)
	{}

	[[nodiscard]] auto cycle(PredicateId root) -> std::vector<std::size_t>
	{
		if (statistics_)
			++statistics_->checks;
		std::vector<std::uint8_t> color(eventCount_);
		std::vector<std::size_t> parent(eventCount_, eventCount_);
		std::vector<std::size_t> witness;
		auto visit = [&](auto &self, std::size_t event) -> bool {
			color[event] = 1;
			const auto targets = successors(root, event);
			for (const auto target : targets) {
				if (color[target] == 0) {
					parent[target] = event;
					if (self(self, target))
						return true;
				} else if (color[target] == 1) {
					witness.push_back(event);
					while (witness.back() != target)
						witness.push_back(parent[witness.back()]);
					std::ranges::reverse(witness);
					witness.push_back(target);
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
		return witness;
	}

private:
	[[nodiscard]] auto value(PredicateId id) const -> const Value &
	{
		VERIFY(values_.at(id).has_value(), "lazy CAT leaf has no materialized value");
		return *values_[id];
	}

	[[nodiscard]] auto setContains(PredicateId id, std::size_t event) const -> bool
	{
		const auto &predicate = model_.predicates().at(id);
		switch (predicate.kind) {
		case Predicate::Kind::Base:
			return std::get<EventSet>(value(id)).contains(event);
		case Predicate::Kind::Alias:
			return setContains(predicate.operands[0], event);
		case Predicate::Kind::Union:
			return setContains(predicate.operands[0], event) ||
			       setContains(predicate.operands[1], event);
		case Predicate::Kind::Intersection:
			return setContains(predicate.operands[0], event) &&
			       setContains(predicate.operands[1], event);
		case Predicate::Kind::Difference:
			return setContains(predicate.operands[0], event) &&
			       !setContains(predicate.operands[1], event);
		default:
			UNREACHABLE("unsupported set predicate reached lazy CAT cycle plan");
		}
	}

	void emit(PredicateId id, std::size_t from, SuccessorConsumer consumer) const
	{
		const auto &predicate = model_.predicates().at(id);
		switch (predicate.kind) {
		case Predicate::Kind::Base: {
			const auto &relation = std::get<Relation>(value(id));
			for (auto target = relation.nextSuccessor(from, 0); target < eventCount_;
			     target = relation.nextSuccessor(from, target + 1))
				consumer(target);
			return;
		}
		case Predicate::Kind::Alias:
			emit(predicate.operands[0], from, consumer);
			return;
		case Predicate::Kind::Union:
			emit(predicate.operands[0], from, consumer);
			emit(predicate.operands[1], from, consumer);
			return;
		case Predicate::Kind::Intersection: {
			const auto cheap = [&](PredicateId operand) {
				const auto kind = model_.predicates()[operand].kind;
				return kind == Predicate::Kind::Base ||
				       kind == Predicate::Kind::Identity;
			};
			const auto candidates = cheap(predicate.operands[1])
						? predicate.operands[0]
						: predicate.operands[1];
			const auto filter = candidates == predicate.operands[0]
					    ? predicate.operands[1]
					    : predicate.operands[0];
			auto accept = [&](std::size_t target) {
				if (contains(filter, from, target))
					consumer(target);
			};
			emit(candidates, from, accept);
			return;
		}
		case Predicate::Kind::Composition: {
			auto follow = [&](std::size_t middle) {
				emit(predicate.operands[1], middle, consumer);
			};
			emit(predicate.operands[0], from, follow);
			return;
		}
		case Predicate::Kind::Identity:
			if (setContains(predicate.operands[0], from))
				consumer(from);
			return;
		case Predicate::Kind::Optional:
			consumer(from);
			emit(predicate.operands[0], from, consumer);
			return;
		default:
			UNREACHABLE("unsupported relation predicate reached lazy CAT cycle plan");
		}
	}

	[[nodiscard]] auto contains(PredicateId id, std::size_t from, std::size_t target) const
		-> bool
	{
		const auto &predicate = model_.predicates().at(id);
		switch (predicate.kind) {
		case Predicate::Kind::Base:
			return std::get<Relation>(value(id)).contains(from, target);
		case Predicate::Kind::Alias:
			return contains(predicate.operands[0], from, target);
		case Predicate::Kind::Union:
			return contains(predicate.operands[0], from, target) ||
			       contains(predicate.operands[1], from, target);
		case Predicate::Kind::Intersection:
			return contains(predicate.operands[0], from, target) &&
			       contains(predicate.operands[1], from, target);
		case Predicate::Kind::Composition: {
			bool found{};
			auto follow = [&](std::size_t middle) {
				found = found || contains(predicate.operands[1], middle, target);
			};
			emit(predicate.operands[0], from, follow);
			return found;
		}
		case Predicate::Kind::Identity:
			return from == target && setContains(predicate.operands[0], from);
		case Predicate::Kind::Optional:
			return from == target || contains(predicate.operands[0], from, target);
		default:
			UNREACHABLE("unsupported relation membership reached lazy CAT cycle plan");
		}
	}

	[[nodiscard]] auto successors(PredicateId root, std::size_t from)
		-> std::vector<std::size_t>
	{
		if (++generation_ == 0) {
			std::ranges::fill(seen_, 0);
			generation_ = 1;
		}
		std::vector<std::size_t> result;
		auto collect = [&](std::size_t target) {
			VERIFY(target < eventCount_, "lazy CAT successor is out of range");
			if (statistics_)
				++statistics_->emittedCandidates;
			if (seen_[target] != generation_) {
				seen_[target] = generation_;
				result.push_back(target);
				if (statistics_)
					++statistics_->uniqueSuccessors;
			}
		};
		emit(root, from, collect);
		std::ranges::sort(result);
		return result;
	}

	const NormalizedModel &model_;
	const std::vector<std::optional<Value>> &values_;
	std::size_t eventCount_{};
	std::vector<std::uint32_t> seen_;
	std::uint32_t generation_{};
	LazyCycleStatistics *statistics_{};
};

} /* namespace */

auto findLazyCycle(const NormalizedModel &model, PredicateId root,
		   const std::vector<std::optional<Value>> &values, std::size_t eventCount,
		   LazyCycleStatistics *statistics)
	-> std::vector<std::size_t>
{
	return LazyRelation(model, values, eventCount, statistics).cycle(root);
}

} /* namespace cat */
