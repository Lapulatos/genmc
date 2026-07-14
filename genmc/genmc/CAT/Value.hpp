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

#ifndef GENMC_CAT_VALUE_HPP
#define GENMC_CAT_VALUE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cat {

class Relation;

/**
 * Dense finite event set backed by 64-bit words.
 *
 * Event IDs are `[0, size())`. Tail bits outside that universe are always
 * zero, making equality and word-parallel algebra deterministic. Values own
 * their storage and are safe to read concurrently after construction.
 */
class EventSet {
public:
	explicit EventSet(std::size_t size = 0);

	/** Return the fixed event universe size represented by this value. */
	[[nodiscard]] auto size() const -> std::size_t { return size_; }
	/** Return the number of set event bits in O(number-of-words). */
	[[nodiscard]] auto count() const -> std::size_t;
	/** Return owned packed-word bytes, excluding vector object overhead. */
	[[nodiscard]] auto storageBytes() const -> std::size_t
	{
		return words_.size() * sizeof(std::uint64_t);
	}
	/** Return true when no event is present. */
	[[nodiscard]] auto empty() const -> bool;
	/** Test one in-range event ID. */
	[[nodiscard]] auto contains(std::size_t event) const -> bool;
	/** Insert one in-range event ID. */
	void insert(std::size_t event);
	/**
	 * Grow the represented universe while preserving every existing member.
	 *
	 * @param size New universe size; shrinking is a contract violation.
	 * @complexity O(new packed word count) only when storage must grow.
	 */
	void grow(std::size_t size);
	/** Return one present event, or `size()` as the no-event sentinel. */
	[[nodiscard]] auto first() const -> std::size_t;

	auto operator==(const EventSet &) const -> bool = default;

private:
	friend auto setUnion(const EventSet &, const EventSet &) -> EventSet;
	friend auto setIntersection(const EventSet &, const EventSet &) -> EventSet;
	friend auto setDifference(const EventSet &, const EventSet &) -> EventSet;
	friend auto product(const EventSet &, const EventSet &) -> Relation;
	friend auto identity(const EventSet &) -> Relation;
	friend class Relation;

	static auto wordCount(std::size_t size) -> std::size_t;

	std::size_t size_{};
	std::vector<std::uint64_t> words_;
};

/**
 * Dense square binary relation represented as contiguous word-packed rows.
 *
 * Row `i` stores all successors of event `i`. The layout uses
 * `size() * ceil(size()/64)` words, enabling row-wise composition and closure.
 * Values own storage and are safe to read concurrently after construction.
 */
class Relation {
public:
	explicit Relation(std::size_t size = 0);

	/** Return the common source/target event universe size. */
	[[nodiscard]] auto size() const -> std::size_t { return size_; }
	/** Return the number of relation pairs. */
	[[nodiscard]] auto count() const -> std::size_t;
	/** Return owned packed-row bytes, excluding vector object overhead. */
	[[nodiscard]] auto storageBytes() const -> std::size_t
	{
		return words_.size() * sizeof(std::uint64_t);
	}
	/** Return true when no pair is present. */
	[[nodiscard]] auto empty() const -> bool;
	/** Test one in-range `(from,to)` pair. */
	[[nodiscard]] auto contains(std::size_t from, std::size_t target) const -> bool;
	/** Insert one in-range `(from,to)` pair. */
	void insert(std::size_t from, std::size_t target);
	/**
	 * Grow both relation dimensions while preserving every existing pair.
	 *
	 * Row packing changes whenever the target universe crosses a 64-event
	 * boundary, so those transitions repack all live rows.
	 *
	 * @param size New common source/target size; shrinking is invalid.
	 * @complexity O(old pairs' packed rows + newly allocated storage).
	 */
	void grow(std::size_t size);
	/** Return all targets of @p from as a value copy. */
	[[nodiscard]] auto successors(std::size_t from) const -> EventSet;

	auto operator==(const Relation &) const -> bool = default;

private:
	friend auto relationUnion(const Relation &, const Relation &) -> Relation;
	friend auto relationIntersection(const Relation &, const Relation &) -> Relation;
	friend auto relationDifference(const Relation &, const Relation &) -> Relation;
	friend auto product(const EventSet &, const EventSet &) -> Relation;
	friend auto identity(const EventSet &) -> Relation;
	friend auto inverse(const Relation &) -> Relation;
	friend auto compose(const Relation &, const Relation &) -> Relation;
	friend auto optional(const Relation &) -> Relation;
	friend auto transitiveClosure(const Relation &) -> Relation;
	friend auto reflexiveTransitiveClosure(const Relation &) -> Relation;

	[[nodiscard]] auto rowOffset(std::size_t row) const -> std::size_t;
	void unionRow(std::size_t target, std::size_t source);

	std::size_t size_{};
	std::size_t rowWords_{};
	std::vector<std::uint64_t> words_;
};

/** Set union; operands must have identical universes. */
[[nodiscard]] auto setUnion(const EventSet &lhs, const EventSet &rhs) -> EventSet;
/** Set intersection; operands must have identical universes. */
[[nodiscard]] auto setIntersection(const EventSet &lhs, const EventSet &rhs) -> EventSet;
/** Set difference `lhs \ rhs`; operands must have identical universes. */
[[nodiscard]] auto setDifference(const EventSet &lhs, const EventSet &rhs) -> EventSet;
/** Relation union; operands must have identical universes. */
[[nodiscard]] auto relationUnion(const Relation &lhs, const Relation &rhs) -> Relation;
/** Relation intersection; operands must have identical universes. */
[[nodiscard]] auto relationIntersection(const Relation &lhs, const Relation &rhs) -> Relation;
/** Relation difference `lhs \ rhs`; operands must have identical universes. */
[[nodiscard]] auto relationDifference(const Relation &lhs, const Relation &rhs) -> Relation;
/** Cartesian product of two sets from the same universe. */
[[nodiscard]] auto product(const EventSet &lhs, const EventSet &rhs) -> Relation;
/** Identity restriction `[events]`. */
[[nodiscard]] auto identity(const EventSet &events) -> Relation;
/** Set of sources that have at least one outgoing relation edge. */
[[nodiscard]] auto domain(const Relation &relation) -> EventSet;
/** Set of targets that have at least one incoming relation edge. */
[[nodiscard]] auto range(const Relation &relation) -> EventSet;
/** Relational inverse. */
[[nodiscard]] auto inverse(const Relation &relation) -> Relation;
/** Relational composition `lhs ; rhs`. */
[[nodiscard]] auto compose(const Relation &lhs, const Relation &rhs) -> Relation;
/** Optional relation `id | relation` over the complete event universe. */
[[nodiscard]] auto optional(const Relation &relation) -> Relation;
/** Non-reflexive transitive closure. */
[[nodiscard]] auto transitiveClosure(const Relation &relation) -> Relation;
/** Reflexive-transitive closure over the complete event universe. */
[[nodiscard]] auto reflexiveTransitiveClosure(const Relation &relation) -> Relation;

} /* namespace cat */

#endif /* GENMC_CAT_VALUE_HPP */
