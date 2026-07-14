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

#include "genmc/CAT/Value.hpp"

#include "genmc/Support/Error.hpp"

#include <algorithm>
#include <bit>

namespace cat {
namespace {

constexpr std::size_t bitsPerWord = 64;

static void requireSameSize(std::size_t lhs, std::size_t rhs)
{
	VERIFY(lhs == rhs, "CAT values use different event universes");
}

} /* namespace */

auto EventSet::wordCount(std::size_t size) -> std::size_t
{
	return (size + bitsPerWord - 1) / bitsPerWord;
}

EventSet::EventSet(std::size_t size) : size_(size), words_(wordCount(size)) {}

auto EventSet::count() const -> std::size_t
{
	std::size_t result{};
	for (const auto word : words_)
		result += std::popcount(word);
	return result;
}

auto EventSet::empty() const -> bool
{
	return std::ranges::all_of(words_, [](auto word) { return word == 0; });
}

auto EventSet::contains(std::size_t event) const -> bool
{
	VERIFY(event < size_, "CAT event-set index out of range");
	return (words_[event / bitsPerWord] & (std::uint64_t{1} << (event % bitsPerWord))) != 0;
}

void EventSet::insert(std::size_t event)
{
	VERIFY(event < size_, "CAT event-set index out of range");
	words_[event / bitsPerWord] |= std::uint64_t{1} << (event % bitsPerWord);
}

auto EventSet::first() const -> std::size_t
{
	for (std::size_t word = 0; word < words_.size(); ++word) {
		if (words_[word] != 0)
			return word * bitsPerWord + std::countr_zero(words_[word]);
	}
	return size_;
}

Relation::Relation(std::size_t size)
	: size_(size), rowWords_(EventSet::wordCount(size)), words_(size * rowWords_)
{}

auto Relation::rowOffset(std::size_t row) const -> std::size_t
{
	VERIFY(row < size_, "CAT relation row out of range");
	return row * rowWords_;
}

auto Relation::count() const -> std::size_t
{
	std::size_t result{};
	for (const auto word : words_)
		result += std::popcount(word);
	return result;
}

auto Relation::empty() const -> bool
{
	return std::ranges::all_of(words_, [](auto word) { return word == 0; });
}

auto Relation::contains(std::size_t from, std::size_t target) const -> bool
{
	VERIFY(target < size_, "CAT relation column out of range");
	const auto offset = rowOffset(from) + target / bitsPerWord;
	return (words_[offset] & (std::uint64_t{1} << (target % bitsPerWord))) != 0;
}

void Relation::insert(std::size_t from, std::size_t target)
{
	VERIFY(target < size_, "CAT relation column out of range");
	const auto offset = rowOffset(from) + target / bitsPerWord;
	words_[offset] |= std::uint64_t{1} << (target % bitsPerWord);
}

auto Relation::successors(std::size_t from) const -> EventSet
{
	EventSet result(size_);
	const auto offset = rowOffset(from);
	std::ranges::copy_n(words_.begin() + static_cast<std::ptrdiff_t>(offset), rowWords_,
			    result.words_.begin());
	return result;
}

void Relation::unionRow(std::size_t target, std::size_t source)
{
	const auto targetOffset = rowOffset(target);
	const auto sourceOffset = rowOffset(source);
	for (std::size_t word = 0; word < rowWords_; ++word)
		words_[targetOffset + word] |= words_[sourceOffset + word];
}

auto setUnion(const EventSet &lhs, const EventSet &rhs) -> EventSet
{
	requireSameSize(lhs.size_, rhs.size_);
	EventSet result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] | rhs.words_[word];
	return result;
}

auto setIntersection(const EventSet &lhs, const EventSet &rhs) -> EventSet
{
	requireSameSize(lhs.size_, rhs.size_);
	EventSet result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] & rhs.words_[word];
	return result;
}

auto setDifference(const EventSet &lhs, const EventSet &rhs) -> EventSet
{
	requireSameSize(lhs.size_, rhs.size_);
	EventSet result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] & ~rhs.words_[word];
	return result;
}

auto relationUnion(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	Relation result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] | rhs.words_[word];
	return result;
}

auto relationIntersection(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	Relation result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] & rhs.words_[word];
	return result;
}

auto relationDifference(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	Relation result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] & ~rhs.words_[word];
	return result;
}

auto product(const EventSet &lhs, const EventSet &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	Relation result(lhs.size_);
	for (std::size_t from = 0; from < lhs.size_; ++from) {
		if (!lhs.contains(from))
			continue;
		const auto offset = result.rowOffset(from);
		std::ranges::copy(rhs.words_,
				  result.words_.begin() + static_cast<std::ptrdiff_t>(offset));
	}
	return result;
}

auto identity(const EventSet &events) -> Relation
{
	Relation result(events.size_);
	for (std::size_t event = 0; event < events.size_; ++event) {
		if (events.contains(event))
			result.insert(event, event);
	}
	return result;
}

auto domain(const Relation &relation) -> EventSet
{
	EventSet result(relation.size());
	for (std::size_t from = 0; from < relation.size(); ++from) {
		if (!relation.successors(from).empty())
			result.insert(from);
	}
	return result;
}

auto range(const Relation &relation) -> EventSet
{
	EventSet result(relation.size());
	for (std::size_t from = 0; from < relation.size(); ++from) {
		for (std::size_t target = 0; target < relation.size(); ++target) {
			if (relation.contains(from, target))
				result.insert(target);
		}
	}
	return result;
}

auto inverse(const Relation &relation) -> Relation
{
	Relation result(relation.size_);
	for (std::size_t from = 0; from < relation.size_; ++from) {
		for (std::size_t to = 0; to < relation.size_; ++to) {
			if (relation.contains(from, to))
				result.insert(to, from);
		}
	}
	return result;
}

auto compose(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	Relation result(lhs.size_);
	/* For every `from --lhs--> middle`, union the complete rhs row for
	 * `middle`; row packing makes the inner union word-parallel. */
	for (std::size_t from = 0; from < lhs.size_; ++from) {
		for (std::size_t middle = 0; middle < lhs.size_; ++middle) {
			if (lhs.contains(from, middle)) {
				const auto targetOffset = result.rowOffset(from);
				const auto sourceOffset = rhs.rowOffset(middle);
				for (std::size_t word = 0; word < result.rowWords_; ++word)
					result.words_[targetOffset + word] |=
						rhs.words_[sourceOffset + word];
			}
		}
	}
	return result;
}

auto optional(const Relation &relation) -> Relation
{
	EventSet universe(relation.size_);
	for (std::size_t event = 0; event < relation.size_; ++event)
		universe.insert(event);
	return relationUnion(relation, identity(universe));
}

auto transitiveClosure(const Relation &relation) -> Relation
{
	auto result = relation;
	/* Bitset Warshall: when row `from` reaches pivot `k`, all successors of
	 * `k` are reachable from `from`. In-place updates compute the least closure. */
	for (std::size_t pivot = 0; pivot < result.size_; ++pivot) {
		for (std::size_t from = 0; from < result.size_; ++from) {
			if (result.contains(from, pivot))
				result.unionRow(from, pivot);
		}
	}
	return result;
}

auto reflexiveTransitiveClosure(const Relation &relation) -> Relation
{
	return optional(transitiveClosure(relation));
}

} /* namespace cat */
