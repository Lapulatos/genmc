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

#ifndef GENMC_CAT_NORMALIZED_HPP
#define GENMC_CAT_NORMALIZED_HPP

#include "genmc/CAT/Model.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cat {

/** Stable dense predicate index in one normalized CAAT model. */
using PredicateId = std::uint32_t;

/**
 * One normalized CAAT predicate equation.
 *
 * Every derived predicate contains exactly one operator. Operands may point in
 * either direction, so mutually recursive equations form cycles without
 * violating vector-address stability. Base predicates have no operands.
 */
struct Predicate {
	/** One-operation forms from the CAAT consistency language. */
	enum class Kind : std::uint8_t {
		Base,
		Alias,
		Union,
		Composition,
		Difference,
		Intersection,
		Product,
		Identity,
		Domain,
		Range,
		Inverse,
		Optional,
		TransitiveClosure,
		ReflexiveTransitiveClosure
	};

	PredicateId id{};
	Kind kind{Kind::Base};
	ValueType type{ValueType::Relation};
	SourceSpan span;
	std::string name;
	std::vector<PredicateId> operands;
	std::uint32_t declaredRecursiveGroup{};
	bool generated{};
};

/** One normalized axiom referring directly to one named predicate. */
struct NormalizedCheck {
	Statement::CheckKind kind{Statement::CheckKind::Acyclic};
	std::string name;
	PredicateId predicate{};
	SourceSpan span;
};

/**
 * Immutable normalized input for dependency analysis and offline CAAT solving.
 *
 * The object owns all names, equations, and source spans. It is read-only after
 * construction and can therefore be shared by GenMC workers. Unlike ModelIR,
 * operand IDs are not required to precede their users because recursion is an
 * explicit part of this representation.
 */
class NormalizedModel {
public:
	/**
	 * Take ownership of a completely typed normalized model.
	 *
	 * @param name Root model header retained for diagnostics.
	 * @param hostProfile GenMC causal-view profile selected by model metadata.
	 * @param predicates Stable dense predicate equation array.
	 * @param checks Source-ordered normalized consistency axioms.
	 */
	NormalizedModel(std::string name, HostProfile hostProfile,
			std::vector<Predicate> predicates, std::vector<NormalizedCheck> checks)
		: name_(std::move(name)), hostProfile_(hostProfile),
		  predicates_(std::move(predicates)), checks_(std::move(checks))
	{}

	NormalizedModel(const NormalizedModel &) = delete;
	NormalizedModel(NormalizedModel &&) = delete;
	auto operator=(const NormalizedModel &) -> NormalizedModel & = delete;
	auto operator=(NormalizedModel &&) -> NormalizedModel & = delete;
	~NormalizedModel() = default;

	/** Return the root CAT model header. */
	[[nodiscard]] auto name() const -> std::string_view { return name_; }
	/** Return the explicit/default GenMC host profile. */
	[[nodiscard]] auto hostProfile() const -> HostProfile { return hostProfile_; }
	/** Return all base and derived predicates in stable ID order. */
	[[nodiscard]] auto predicates() const -> const std::vector<Predicate> &
	{
		return predicates_;
	}
	/** Return normalized axioms in source order. */
	[[nodiscard]] auto checks() const -> const std::vector<NormalizedCheck> &
	{
		return checks_;
	}
	/** Return a deterministic summary for golden tests and human review. */
	[[nodiscard]] auto summary() const -> std::string;

private:
	std::string name_;
	HostProfile hostProfile_{HostProfile::SC};
	std::vector<Predicate> predicates_;
	std::vector<NormalizedCheck> checks_;
};

/** Result of syntax-to-normalized-CAAT compilation. */
struct NormalizeResult {
	std::shared_ptr<const NormalizedModel> model;
	std::vector<Diagnostic> diagnostics;

	/** Return true exactly when normalization produced an error-free model. */
	[[nodiscard]] auto ok() const -> bool { return model && diagnostics.empty(); }
};

/**
 * Resolve forward references, infer predicate types, and normalize equations.
 *
 * This compiler is independent of Phase 1's topological ModelIR compiler. It
 * accepts cycles only when their declarations carry the same nonzero recursive
 * group ID; dependency/polarity admissibility is checked in Phase 2.2.
 */
class Normalizer {
public:
	/** Compile one parsed model without mutating its syntax tree. */
	[[nodiscard]] auto normalize(const Model &syntax) const -> NormalizeResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_NORMALIZED_HPP */
