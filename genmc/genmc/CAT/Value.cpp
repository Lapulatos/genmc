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
#include <limits>
#include <numeric>
#include <unordered_map>

namespace cat {

struct Relation::StructuralData {
	StructuralRelationKind kind;
	std::vector<std::uint64_t> keys;
	std::vector<std::uint32_t> offsets;
	std::vector<std::uint32_t> targets;
};

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

void EventSet::erase(std::size_t event)
{
	VERIFY(event < size_, "CAT event-set index out of range");
	words_[event / bitsPerWord] &= ~(std::uint64_t{1} << (event % bitsPerWord));
}

void EventSet::grow(std::size_t size)
{
	VERIFY(size >= size_, "CAT event-set universe cannot shrink");
	if (size == size_)
		return;
	words_.resize(wordCount(size));
	size_ = size;
}

void EventSet::shrink(std::size_t size)
{
	VERIFY(size <= size_, "CAT event-set shrink cannot grow universe");
	words_.resize(wordCount(size));
	if (size % bitsPerWord != 0 && !words_.empty())
		words_.back() &= (std::uint64_t{1} << (size % bitsPerWord)) - 1;
	size_ = size;
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

auto Relation::structural(StructuralRelationKind kind, std::vector<std::uint64_t> keys) -> Relation
{
	Relation result;
	result.size_ = keys.size();
	result.rowWords_ = EventSet::wordCount(result.size_);
	result.structure_ = std::make_shared<StructuralData>(kind, std::move(keys));
	return result;
}

auto Relation::sparse(std::size_t size,
			      std::vector<std::pair<std::size_t, std::size_t>> edges) -> Relation
{
	VERIFY(size <= std::numeric_limits<std::uint32_t>::max(),
	       "CAT sparse relation universe exceeds 32-bit IDs");
	std::ranges::sort(edges);
	edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	VERIFY(edges.size() <= std::numeric_limits<std::uint32_t>::max(),
	       "CAT sparse relation edge count exceeds 32-bit offsets");

	auto data = std::make_shared<StructuralData>();
	data->kind = StructuralRelationKind::ExplicitEdges;
	data->offsets.assign(size + 1, 0);
	data->targets.reserve(edges.size());
	for (const auto [from, target] : edges) {
		VERIFY(from < size && target < size, "CAT sparse relation edge out of range");
		++data->offsets[from + 1];
		data->targets.push_back(static_cast<std::uint32_t>(target));
	}
	std::inclusive_scan(data->offsets.begin(), data->offsets.end(), data->offsets.begin());

	Relation result;
	result.size_ = size;
	result.rowWords_ = EventSet::wordCount(size);
	result.structure_ = std::move(data);
	return result;
}

auto Relation::rowOffset(std::size_t row) const -> std::size_t
{
	VERIFY(row < size_, "CAT relation row out of range");
	return row * rowWords_;
}

auto Relation::denseContains(std::size_t from, std::size_t target) const -> bool
{
	const auto offset = rowOffset(from) + target / bitsPerWord;
	return (words_[offset] & (std::uint64_t{1} << (target % bitsPerWord))) != 0;
}

auto Relation::storageBytes() const -> std::size_t
{
	return isStructural()
		       ? structure_->keys.size() * sizeof(std::uint64_t) +
				 structure_->offsets.size() * sizeof(std::uint32_t) +
				 structure_->targets.size() * sizeof(std::uint32_t)
		       : words_.size() * sizeof(std::uint64_t);
}

auto Relation::structuralContains(std::size_t from, std::size_t target) const -> bool
{
	if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
		const auto begin = structure_->targets.begin() + structure_->offsets[from];
		const auto end = structure_->targets.begin() + structure_->offsets[from + 1];
		return std::binary_search(begin, end, static_cast<std::uint32_t>(target));
	}
	const auto lhs = structure_->keys[from];
	const auto rhs = structure_->keys[target];
	if (lhs == 0 || rhs == 0)
		return false;
	if (structure_->kind == StructuralRelationKind::Location)
		return lhs == rhs;

	const auto lhsGroup = lhs >> 32;
	const auto rhsGroup = rhs >> 32;
	if (structure_->kind == StructuralRelationKind::ProgramOrder)
		return lhsGroup >= 2 && lhsGroup == rhsGroup &&
		       static_cast<std::uint32_t>(lhs) < static_cast<std::uint32_t>(rhs);
	if (structure_->kind == StructuralRelationKind::Internal)
		return (lhsGroup == 1 && rhsGroup == 1) || (lhsGroup >= 2 && lhsGroup == rhsGroup);
	return (lhsGroup == 1 && rhsGroup >= 2) || (lhsGroup >= 2 && rhsGroup == 1) ||
	       (lhsGroup >= 2 && rhsGroup >= 2 && lhsGroup != rhsGroup);
}

void Relation::insertStructureInto(Relation &target) const
{
	VERIFY(isStructural(), "CAT dense relation has no structural rows");
	requireSameSize(size_, target.size_);
	VERIFY(!target.isStructural(), "CAT structural target cannot receive packed rows");
	if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
		for (std::size_t from = 0; from < size_; ++from) {
			for (auto edge = structure_->offsets[from];
			     edge < structure_->offsets[from + 1]; ++edge)
				target.insertDense(from, structure_->targets[edge]);
		}
		return;
	}
	if (structure_->kind == StructuralRelationKind::Location) {
		std::unordered_map<std::uint64_t, std::vector<std::size_t>> groups;
		for (std::size_t event = 0; event < size_; ++event) {
			if (structure_->keys[event] != 0)
				groups[structure_->keys[event]].push_back(event);
		}
		for (const auto &[group, members] : groups) {
			(void)group;
			EventSet memberSet(size_);
			for (const auto event : members)
				memberSet.insert(event);
			for (const auto event : members)
				target.insertSuccessors(event, memberSet);
		}
		return;
	}

	std::unordered_map<std::uint64_t, std::vector<std::pair<std::uint32_t, std::size_t>>>
		groups;
	EventSet active(size_), initial(size_);
	for (std::size_t event = 0; event < size_; ++event) {
		const auto key = structure_->keys[event];
		const auto group = key >> 32;
		if (group == 0)
			continue;
		groups[group].emplace_back(static_cast<std::uint32_t>(key), event);
		if (group == 1)
			initial.insert(event);
		else
			active.insert(event);
	}
	if (structure_->kind == StructuralRelationKind::ProgramOrder) {
		for (auto &[group, members] : groups) {
			if (group < 2)
				continue;
			std::ranges::sort(members);
			EventSet later(size_);
			for (auto member = members.rbegin(); member != members.rend(); ++member) {
				target.insertSuccessors(member->second, later);
				later.insert(member->second);
			}
		}
		return;
	}
	if (structure_->kind == StructuralRelationKind::Internal) {
		for (const auto &[group, members] : groups) {
			(void)group;
			EventSet memberSet(size_);
			for (const auto &[index, event] : members) {
				(void)index;
				memberSet.insert(event);
			}
			for (const auto &[index, event] : members) {
				(void)index;
				target.insertSuccessors(event, memberSet);
			}
		}
		return;
	}
	for (const auto &[group, members] : groups) {
		if (group < 2)
			continue;
		EventSet memberSet(size_);
		for (const auto &[index, event] : members) {
			(void)index;
			memberSet.insert(event);
		}
		auto successors = setUnion(setDifference(active, memberSet), initial);
		for (const auto &[index, event] : members) {
			(void)index;
			target.insertSuccessors(event, successors);
		}
	}
	for (const auto &[index, event] : groups[1]) {
		(void)index;
		target.insertSuccessors(event, active);
	}
}

void Relation::ensureDense()
{
	if (!isStructural())
		return;
	Relation dense(size_);
	insertStructureInto(dense);
	words_ = std::move(dense.words_);
	structure_.reset();
}

auto Relation::count() const -> std::size_t
{
	if (isStructural()) {
		if (structure_->kind == StructuralRelationKind::ExplicitEdges)
			return structure_->targets.size();
		std::unordered_map<std::uint64_t, std::size_t> groups;
		std::size_t active{}, initial{};
		for (const auto key : structure_->keys) {
			if (key == 0)
				continue;
			const auto group = structure_->kind == StructuralRelationKind::Location
						   ? key
						   : key >> 32;
			++groups[group];
			if (group == 1)
				++initial;
			else
				++active;
		}
		if (structure_->kind == StructuralRelationKind::ProgramOrder) {
			std::size_t result{};
			for (const auto &[group, members] : groups) {
				if (group >= 2)
					result += members * (members - 1) / 2;
			}
			return result;
		}
		if (structure_->kind == StructuralRelationKind::Internal) {
			std::size_t result{};
			for (const auto &[group, members] : groups)
				result += members * members;
			return result;
		}
		if (structure_->kind == StructuralRelationKind::External) {
			std::size_t sameThread{};
			for (const auto &[group, members] : groups) {
				if (group >= 2)
					sameThread += members * members;
			}
			return active * active - sameThread + 2 * active * initial;
		}
		std::size_t result{};
		for (const auto &[group, members] : groups) {
			(void)group;
			result += members * members;
		}
		return result;
	}
	std::size_t result{};
	for (const auto word : words_)
		result += std::popcount(word);
	return result;
}

auto Relation::empty() const -> bool
{
	if (isStructural())
		return count() == 0;
	return std::ranges::all_of(words_, [](auto word) { return word == 0; });
}

auto Relation::contains(std::size_t from, std::size_t target) const -> bool
{
	VERIFY(from < size_, "CAT relation row out of range");
	VERIFY(target < size_, "CAT relation column out of range");
	if (isStructural())
		return structuralContains(from, target);
	return denseContains(from, target);
}

void Relation::insert(std::size_t from, std::size_t target)
{
	ensureDense();
	insertDense(from, target);
}

void Relation::insertDense(std::size_t from, std::size_t target)
{
	VERIFY(target < size_, "CAT relation column out of range");
	const auto offset = rowOffset(from) + target / bitsPerWord;
	words_[offset] |= std::uint64_t{1} << (target % bitsPerWord);
}

void Relation::insertSuccessors(std::size_t from, const EventSet &targets)
{
	requireSameSize(size_, targets.size_);
	VERIFY(!isStructural(), "CAT packed row insertion requires a dense relation");
	const auto offset = rowOffset(from);
	for (std::size_t word = 0; word < rowWords_; ++word)
		words_[offset + word] |= targets.words_[word];
}

void Relation::erase(std::size_t from, std::size_t target)
{
	ensureDense();
	VERIFY(target < size_, "CAT relation column out of range");
	const auto offset = rowOffset(from) + target / bitsPerWord;
	words_[offset] &= ~(std::uint64_t{1} << (target % bitsPerWord));
}

void Relation::grow(std::size_t size)
{
	VERIFY(size >= size_, "CAT relation universe cannot shrink");
	if (size == size_)
		return;
	if (isStructural()) {
		if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
			auto offsets = structure_->offsets;
			offsets.resize(size + 1, offsets.back());
			structure_ = std::make_shared<StructuralData>(
				StructuralData{structure_->kind, {}, std::move(offsets),
					       structure_->targets});
			size_ = size;
			rowWords_ = EventSet::wordCount(size);
			return;
		}
		auto keys = structure_->keys;
		keys.resize(size);
		structure_ = std::make_shared<StructuralData>(structure_->kind, std::move(keys));
		size_ = size;
		rowWords_ = EventSet::wordCount(size);
		return;
	}

	const auto newRowWords = EventSet::wordCount(size);
	if (newRowWords == rowWords_) {
		/* Existing rows keep their offsets until a word boundary is crossed. */
		words_.resize(size * newRowWords);
		size_ = size;
		return;
	}

	/* A wider row changes every following row offset. Copy the old packed
	 * prefix of each row into a zero-initialized matrix with the new stride. */
	std::vector<std::uint64_t> grown(size * newRowWords);
	for (std::size_t row = 0; row < size_; ++row) {
		const auto oldOffset = row * rowWords_;
		const auto newOffset = row * newRowWords;
		std::ranges::copy_n(words_.begin() + static_cast<std::ptrdiff_t>(oldOffset),
				    rowWords_,
				    grown.begin() + static_cast<std::ptrdiff_t>(newOffset));
	}
	words_ = std::move(grown);
	size_ = size;
	rowWords_ = newRowWords;
}

void Relation::shrink(std::size_t size)
{
	VERIFY(size <= size_, "CAT relation shrink cannot grow universe");
	if (size == size_)
		return;
	if (isStructural()) {
		if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
			std::vector<std::pair<std::size_t, std::size_t>> edges;
			for (std::size_t from = 0; from < size; ++from) {
				for (auto edge = structure_->offsets[from];
				     edge < structure_->offsets[from + 1]; ++edge) {
					if (structure_->targets[edge] < size)
						edges.emplace_back(from, structure_->targets[edge]);
				}
			}
			*this = Relation::sparse(size, std::move(edges));
			return;
		}
		auto keys = structure_->keys;
		keys.resize(size);
		structure_ = std::make_shared<StructuralData>(structure_->kind, std::move(keys));
		size_ = size;
		rowWords_ = EventSet::wordCount(size);
		return;
	}
	Relation smaller(size);
	for (std::size_t from = 0; from < size; ++from) {
		for (std::size_t to = 0; to < size; ++to) {
			if (denseContains(from, to))
				smaller.insertDense(from, to);
		}
	}
	*this = std::move(smaller);
}

auto Relation::successors(std::size_t from) const -> EventSet
{
	EventSet result(size_);
	if (isStructural()) {
		VERIFY(from < size_, "CAT relation row out of range");
		if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
			for (auto edge = structure_->offsets[from];
			     edge < structure_->offsets[from + 1]; ++edge)
				result.insert(structure_->targets[edge]);
			return result;
		}
		for (std::size_t target = 0; target < size_; ++target) {
			if (structuralContains(from, target))
				result.insert(target);
		}
		return result;
	}
	const auto offset = rowOffset(from);
	std::ranges::copy_n(words_.begin() + static_cast<std::ptrdiff_t>(offset), rowWords_,
			    result.words_.begin());
	return result;
}

auto Relation::nextSuccessor(std::size_t from, std::size_t lowerBound) const -> std::size_t
{
	VERIFY(from < size_, "CAT relation row out of range");
	if (lowerBound >= size_)
		return size_;
	if (isStructural()) {
		if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
			const auto begin = structure_->targets.begin() + structure_->offsets[from];
			const auto end = structure_->targets.begin() + structure_->offsets[from + 1];
			const auto found = std::lower_bound(begin, end,
						    static_cast<std::uint32_t>(lowerBound));
			return found == end ? size_ : *found;
		}
		for (auto target = lowerBound; target < size_; ++target) {
			if (structuralContains(from, target))
				return target;
		}
		return size_;
	}

	const auto offset = rowOffset(from);
	auto word = lowerBound / bitsPerWord;
	auto facts = words_[offset + word] & (~std::uint64_t{} << (lowerBound % bitsPerWord));
	for (;;) {
		if (facts != 0) {
			const auto target = word * bitsPerWord + std::countr_zero(facts);
			return target < size_ ? target : size_;
		}
		if (++word == rowWords_)
			return size_;
		facts = words_[offset + word];
	}
}

void Relation::unionRow(std::size_t target, std::size_t source)
{
	const auto targetOffset = rowOffset(target);
	const auto sourceOffset = rowOffset(source);
	for (std::size_t word = 0; word < rowWords_; ++word)
		words_[targetOffset + word] |= words_[sourceOffset + word];
}

auto Relation::operator==(const Relation &other) const -> bool
{
	if (size_ != other.size_)
		return false;
	if (!isStructural() && !other.isStructural())
		return words_ == other.words_;
	if (isStructural() && other.isStructural() && structure_->kind == other.structure_->kind)
		return structure_->keys == other.structure_->keys &&
		       structure_->offsets == other.structure_->offsets &&
		       structure_->targets == other.structure_->targets;
	for (std::size_t from = 0; from < size_; ++from) {
		for (std::size_t target = 0; target < size_; ++target) {
			if (contains(from, target) != other.contains(from, target))
				return false;
		}
	}
	return true;
}

auto Relation::isSubsetOf(const Relation &other) const -> bool
{
	if (size_ != other.size_)
		return false;
	if (!isStructural() && !other.isStructural() && words_ == other.words_)
		return true;
	if (isStructural() && other.isStructural() && structure_->kind == other.structure_->kind) {
		if (structure_->keys == other.structure_->keys &&
		    structure_->offsets == other.structure_->offsets &&
		    structure_->targets == other.structure_->targets)
			return true;
		if (structure_->kind == StructuralRelationKind::ExplicitEdges) {
			for (std::size_t from = 0; from < size_; ++from) {
				auto lhs = structure_->offsets[from];
				auto rhs = other.structure_->offsets[from];
				const auto lhsEnd = structure_->offsets[from + 1];
				const auto rhsEnd = other.structure_->offsets[from + 1];
				while (lhs < lhsEnd) {
					while (rhs < rhsEnd && other.structure_->targets[rhs] <
								 structure_->targets[lhs])
						++rhs;
					if (rhs == rhsEnd || other.structure_->targets[rhs] !=
							     structure_->targets[lhs])
						return false;
					++lhs;
				}
			}
			return true;
		}
		if (structure_->kind != StructuralRelationKind::Location) {
			for (std::size_t event = 0; event < size_; ++event) {
				const auto oldKey = structure_->keys[event];
				if (oldKey == 0)
					continue;
				const auto newKey = other.structure_->keys[event];
				const bool same =
					structure_->kind == StructuralRelationKind::ProgramOrder
						? oldKey == newKey
						: (oldKey >> 32) == (newKey >> 32);
				if (!same)
					return false;
			}
			return true;
		}
		std::unordered_map<std::uint64_t, std::uint64_t> forward, reverse;
		for (std::size_t event = 0; event < size_; ++event) {
			const auto oldKey = structure_->keys[event];
			if (oldKey == 0)
				continue;
			const auto newKey = other.structure_->keys[event];
			if (newKey == 0)
				return false;
			const auto [oldMapping, oldInserted] = forward.emplace(oldKey, newKey);
			if (!oldInserted && oldMapping->second != newKey)
				return false;
			const auto [newMapping, newInserted] = reverse.emplace(newKey, oldKey);
			if (!newInserted && newMapping->second != oldKey)
				return false;
		}
		return true;
	}
	if (!isStructural()) {
		for (std::size_t from = 0; from < size_; ++from) {
			const auto offset = rowOffset(from);
			for (std::size_t word = 0; word < rowWords_; ++word) {
				auto facts = words_[offset + word];
				while (facts != 0) {
					const auto bit = std::countr_zero(facts);
					const auto target = word * bitsPerWord + bit;
					if (target < size_ && !other.contains(from, target))
						return false;
					facts &= facts - 1;
				}
			}
		}
		return true;
	}
	for (std::size_t from = 0; from < size_; ++from) {
		for (std::size_t target = 0; target < size_; ++target) {
			if (structuralContains(from, target) && !other.contains(from, target))
				return false;
		}
	}
	return true;
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
	if (lhs.isStructural() || rhs.isStructural()) {
		Relation result(lhs.size_);
		if (!lhs.isStructural())
			result = lhs;
		else
			lhs.insertStructureInto(result);
		if (!rhs.isStructural()) {
			for (std::size_t word = 0; word < result.words_.size(); ++word)
				result.words_[word] |= rhs.words_[word];
		} else {
			rhs.insertStructureInto(result);
		}
		return result;
	}
	Relation result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] | rhs.words_[word];
	return result;
}

auto relationIntersection(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	if (lhs.isStructural() || rhs.isStructural()) {
		const Relation *source = &lhs;
		const Relation *filter = &rhs;
		Relation result(lhs.size_);
		if (lhs.isStructural() && rhs.isStructural()) {
			if (rhs.count() < lhs.count())
				std::swap(source, filter);
			source->insertStructureInto(result);
		} else if (lhs.isStructural()) {
			result = rhs;
			filter = &lhs;
		} else {
			result = lhs;
			filter = &rhs;
		}
		for (std::size_t from = 0; from < result.size_; ++from) {
			const auto offset = result.rowOffset(from);
			for (std::size_t word = 0; word < result.rowWords_; ++word) {
				auto facts = result.words_[offset + word];
				while (facts != 0) {
					const auto bit = std::countr_zero(facts);
					const auto target = word * bitsPerWord + bit;
					if (target < result.size_ &&
					    !filter->structuralContains(from, target))
						result.words_[offset + word] &=
							~(std::uint64_t{1} << bit);
					facts &= facts - 1;
				}
			}
		}
		return result;
	}
	Relation result(lhs.size_);
	for (std::size_t word = 0; word < lhs.words_.size(); ++word)
		result.words_[word] = lhs.words_[word] & rhs.words_[word];
	return result;
}

auto relationDifference(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	if (lhs.isStructural()) {
		Relation result(lhs.size_);
		lhs.insertStructureInto(result);
		if (!rhs.isStructural()) {
			for (std::size_t word = 0; word < result.words_.size(); ++word)
				result.words_[word] &= ~rhs.words_[word];
			return result;
		}
		for (std::size_t from = 0; from < result.size_; ++from) {
			const auto offset = result.rowOffset(from);
			for (std::size_t word = 0; word < result.rowWords_; ++word) {
				auto facts = result.words_[offset + word];
				while (facts != 0) {
					const auto bit = std::countr_zero(facts);
					const auto target = word * bitsPerWord + bit;
					if (target < result.size_ &&
					    rhs.structuralContains(from, target))
						result.words_[offset + word] &=
							~(std::uint64_t{1} << bit);
					facts &= facts - 1;
				}
			}
		}
		return result;
	}
	if (rhs.isStructural()) {
		auto result = lhs;
		for (std::size_t from = 0; from < result.size_; ++from) {
			const auto offset = result.rowOffset(from);
			for (std::size_t word = 0; word < result.rowWords_; ++word) {
				auto facts = result.words_[offset + word];
				while (facts != 0) {
					const auto bit = std::countr_zero(facts);
					const auto target = word * bitsPerWord + bit;
					if (target < result.size_ &&
					    rhs.structuralContains(from, target))
						result.words_[offset + word] &=
							~(std::uint64_t{1} << bit);
					facts &= facts - 1;
				}
			}
		}
		return result;
	}
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
			result.insertDense(event, event);
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
	if (relation.isStructural() &&
	    relation.structure_->kind == StructuralRelationKind::ExplicitEdges) {
		for (const auto target : relation.structure_->targets)
			result.insert(target);
		return result;
	}
	if (!relation.isStructural()) {
		for (std::size_t from = 0; from < relation.size(); ++from) {
			for (std::size_t target = 0; target < relation.size(); ++target) {
				if (relation.denseContains(from, target))
					result.insert(target);
			}
		}
		return result;
	}
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
	if (relation.isStructural() &&
	    relation.structure_->kind == StructuralRelationKind::ExplicitEdges) {
		std::vector<std::pair<std::size_t, std::size_t>> edges;
		edges.reserve(relation.structure_->targets.size());
		for (std::size_t from = 0; from < relation.size_; ++from) {
			for (auto edge = relation.structure_->offsets[from];
			     edge < relation.structure_->offsets[from + 1]; ++edge)
				edges.emplace_back(relation.structure_->targets[edge], from);
		}
		return Relation::sparse(relation.size_, std::move(edges));
	}
	if (!relation.isStructural()) {
		for (std::size_t from = 0; from < relation.size_; ++from) {
			for (std::size_t to = 0; to < relation.size_; ++to) {
				if (relation.denseContains(from, to))
					result.insertDense(to, from);
			}
		}
		return result;
	}
	for (std::size_t from = 0; from < relation.size_; ++from) {
		for (std::size_t to = 0; to < relation.size_; ++to) {
			if (relation.contains(from, to))
				result.insertDense(to, from);
		}
	}
	return result;
}

auto compose(const Relation &lhs, const Relation &rhs) -> Relation
{
	requireSameSize(lhs.size_, rhs.size_);
	Relation result(lhs.size_);
	if (lhs.isStructural() &&
	    lhs.structure_->kind == StructuralRelationKind::ExplicitEdges) {
		for (std::size_t from = 0; from < lhs.size_; ++from) {
			for (auto edge = lhs.structure_->offsets[from];
			     edge < lhs.structure_->offsets[from + 1]; ++edge) {
				const auto middle = lhs.structure_->targets[edge];
				if (rhs.isStructural() &&
				    rhs.structure_->kind == StructuralRelationKind::ExplicitEdges) {
					for (auto target = rhs.structure_->offsets[middle];
					     target < rhs.structure_->offsets[middle + 1]; ++target)
						result.insertDense(from,
								   rhs.structure_->targets[target]);
				} else if (rhs.isStructural()) {
					result.insertSuccessors(from, rhs.successors(middle));
				} else {
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
	if (!lhs.isStructural() && !rhs.isStructural()) {
		for (std::size_t from = 0; from < lhs.size_; ++from) {
			for (std::size_t middle = 0; middle < lhs.size_; ++middle) {
				if (!lhs.denseContains(from, middle))
					continue;
				const auto targetOffset = result.rowOffset(from);
				const auto sourceOffset = rhs.rowOffset(middle);
				for (std::size_t word = 0; word < result.rowWords_; ++word)
					result.words_[targetOffset + word] |=
						rhs.words_[sourceOffset + word];
			}
		}
		return result;
	}
	/* For every `from --lhs--> middle`, union the complete rhs row for
	 * `middle`; row packing makes the inner union word-parallel. */
	for (std::size_t from = 0; from < lhs.size_; ++from) {
		for (std::size_t middle = 0; middle < lhs.size_; ++middle) {
			if (lhs.contains(from, middle)) {
				if (rhs.isStructural() &&
				    rhs.structure_->kind == StructuralRelationKind::ExplicitEdges) {
					for (auto target = rhs.structure_->offsets[middle];
					     target < rhs.structure_->offsets[middle + 1]; ++target)
						result.insertDense(from,
								   rhs.structure_->targets[target]);
				} else if (rhs.isStructural()) {
					result.insertSuccessors(from, rhs.successors(middle));
				} else {
					const auto targetOffset = result.rowOffset(from);
					const auto sourceOffset = rhs.rowOffset(middle);
					for (std::size_t word = 0; word < result.rowWords_; ++word)
						result.words_[targetOffset + word] |=
							rhs.words_[sourceOffset + word];
				}
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
	result.ensureDense();
	/* Bitset Warshall: when row `from` reaches pivot `k`, all successors of
	 * `k` are reachable from `from`. In-place updates compute the least closure. */
	for (std::size_t pivot = 0; pivot < result.size_; ++pivot) {
		for (std::size_t from = 0; from < result.size_; ++from) {
			if (result.denseContains(from, pivot))
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
