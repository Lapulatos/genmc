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
		: object_(&function), invoke_([](void *object, std::size_t target) -> bool {
			return (*static_cast<Function *>(object))(target);
		})
	{}

	[[nodiscard]] auto operator()(std::size_t target) const -> bool
	{
		return invoke_(object_, target);
	}

private:
	void *object_{};
	bool (*invoke_)(void *, std::size_t){};
};

/** Exact extensional interpreter for one analyzer-certified relation expression. */
class LazyRelation {
public:
	static constexpr std::size_t maxStreamingDepth = 2048;

	LazyRelation(const NormalizedModel &model,
		     const std::vector<std::optional<Value>> &values, std::size_t eventCount,
		     LazyCycleStatistics *statistics)
		: model_(model), values_(values), eventCount_(eventCount), statistics_(statistics)
	{}

	[[nodiscard]] auto cycle(PredicateId root) -> std::vector<std::size_t>
	{
		if (statistics_)
			++statistics_->checks;
		std::vector<std::uint8_t> color(eventCount_);
		std::vector<std::size_t> parent(eventCount_, eventCount_);
		std::vector<std::size_t> witness;
		bool depthExceeded{};
		auto visit = [&](auto &self, std::size_t event, std::size_t depth) -> bool {
			color[event] = 1;
			auto consume = [&](std::size_t target) -> bool {
				VERIFY(target < eventCount_, "lazy CAT successor is out of range");
				if (statistics_)
					++statistics_->emittedCandidates;
				if (color[target] == 0) {
					if (depth >= maxStreamingDepth) {
						depthExceeded = true;
						return true;
					}
					parent[target] = event;
					if (self(self, target, depth + 1))
						return true;
				} else if (color[target] == 1) {
					witness.push_back(event);
					while (witness.back() != target)
						witness.push_back(parent[witness.back()]);
					std::ranges::reverse(witness);
					witness.push_back(target);
					return true;
				}
				return false;
			};
			if (emit(root, event, consume))
				return true;
			color[event] = 2;
			return false;
		};
		for (std::size_t event = 0; event < eventCount_; ++event) {
			if (color[event] == 0 && visit(visit, event, 1))
				break;
		}
		if (depthExceeded) {
			if (statistics_)
				++statistics_->depthFallbacks;
			return bufferedCycle(root);
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

	[[nodiscard]] auto emit(PredicateId id, std::size_t from,
				SuccessorConsumer consumer) const -> bool
	{
		const auto &predicate = model_.predicates().at(id);
		switch (predicate.kind) {
		case Predicate::Kind::Base: {
			const auto &relation = std::get<Relation>(value(id));
			for (auto target = relation.nextSuccessor(from, 0); target < eventCount_;
			     target = relation.nextSuccessor(from, target + 1)) {
				if (consumer(target))
					return true;
			}
			return false;
		}
		case Predicate::Kind::Alias:
			return emit(predicate.operands[0], from, consumer);
		case Predicate::Kind::Union:
			return emit(predicate.operands[0], from, consumer) ||
			       emit(predicate.operands[1], from, consumer);
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
			auto accept = [&](std::size_t target) -> bool {
				return contains(filter, from, target) && consumer(target);
			};
			return emit(candidates, from, accept);
		}
		case Predicate::Kind::Composition: {
			auto follow = [&](std::size_t middle) -> bool {
				return emit(predicate.operands[1], middle, consumer);
			};
			return emit(predicate.operands[0], from, follow);
		}
		case Predicate::Kind::Identity:
			return setContains(predicate.operands[0], from) && consumer(from);
		case Predicate::Kind::Optional:
			return consumer(from) || emit(predicate.operands[0], from, consumer);
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
			auto follow = [&](std::size_t middle) -> bool {
				return contains(predicate.operands[1], middle, target);
			};
			return emit(predicate.operands[0], from, follow);
		}
		case Predicate::Kind::Identity:
			return from == target && setContains(predicate.operands[0], from);
		case Predicate::Kind::Optional:
			return from == target || contains(predicate.operands[0], from, target);
		default:
			UNREACHABLE("unsupported relation membership reached lazy CAT cycle plan");
		}
	}

	/** Exact heap-frame fallback for event paths too deep for recursive streaming. */
	[[nodiscard]] auto bufferedCycle(PredicateId root) const -> std::vector<std::size_t>
	{
		std::vector<std::uint32_t> seen(eventCount_);
		std::uint32_t generation{};
		auto successors = [&](std::size_t from) {
			if (++generation == 0) {
				std::ranges::fill(seen, 0);
				generation = 1;
			}
			std::vector<std::size_t> result;
			auto collect = [&](std::size_t target) -> bool {
				VERIFY(target < eventCount_, "lazy CAT successor is out of range");
				if (statistics_)
					++statistics_->emittedCandidates;
				if (seen[target] != generation) {
					seen[target] = generation;
					result.push_back(target);
				}
				return false;
			};
			const auto interrupted = emit(root, from, collect);
			VERIFY(!interrupted, "buffered lazy CAT successor collection interrupted");
			std::ranges::sort(result);
			return result;
		};

		struct Frame {
			std::size_t event{};
			std::vector<std::size_t> successors;
			std::size_t next{};
		};
		std::vector<std::uint8_t> color(eventCount_);
		std::vector<std::size_t> parent(eventCount_, eventCount_);
		std::vector<Frame> stack;
		stack.reserve(std::min(eventCount_, maxStreamingDepth));
		for (std::size_t start = 0; start < eventCount_; ++start) {
			if (color[start] != 0)
				continue;
			color[start] = 1;
			stack.push_back({start, successors(start), 0});
			while (!stack.empty()) {
				auto &frame = stack.back();
				if (frame.next == frame.successors.size()) {
					color[frame.event] = 2;
					stack.pop_back();
					continue;
				}
				const auto from = frame.event;
				const auto target = frame.successors[frame.next++];
				if (color[target] == 0) {
					parent[target] = from;
					color[target] = 1;
					stack.push_back({target, successors(target), 0});
					continue;
				}
				if (color[target] != 1)
					continue;
				std::vector<std::size_t> witness{from};
				while (witness.back() != target)
					witness.push_back(parent[witness.back()]);
				std::ranges::reverse(witness);
				witness.push_back(target);
				return witness;
			}
		}
		return {};
	}

	const NormalizedModel &model_;
	const std::vector<std::optional<Value>> &values_;
	std::size_t eventCount_{};
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
