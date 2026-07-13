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

#ifndef GENMC_CAT_MODEL_HPP
#define GENMC_CAT_MODEL_HPP

#include "genmc/CAT/Frontend.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cat {

/** Stable dense index into one ModelIR's immutable node array. */
using NodeId = std::uint32_t;

/** The two finite value domains defined by the Phase 1 CAT contract. */
enum class ValueType : std::uint8_t { Set, Relation };

/**
 * One immutable typed relational operation.
 *
 * Operand IDs always refer to earlier nodes in the same ModelIR, making the
 * array a topologically ordered DAG. Builtin nodes carry their CAT spelling;
 * all other semantics are encoded by kind and operand IDs.
 */
struct Node {
	/** Typed operations retained by the generic evaluator in Phase 1.4. */
	enum class Kind : std::uint8_t {
		Builtin,
		Union,
		Composition,
		Difference,
		Intersection,
		Product,
		Identity,
		Inverse,
		Optional,
		TransitiveClosure,
		ReflexiveTransitiveClosure
	};

	NodeId id{};
	Kind kind{Kind::Builtin};
	ValueType type{ValueType::Relation};
	SourceSpan span;
	std::string name;
	std::vector<NodeId> operands;
};

/** A user binding points to its already-resolved typed DAG node. */
struct Binding {
	std::string name;
	NodeId value{};
	SourceSpan span;
};

/** One typed consistency predicate and its stable explicit/generated name. */
struct Check {
	Statement::CheckKind kind{Statement::CheckKind::Acyclic};
	std::string name;
	NodeId value{};
	SourceSpan span;
};

class Compiler;

/**
 * Immutable, thread-safe relational model shared by every GenMC worker.
 *
 * ModelIR exclusively owns a topologically ordered DAG plus binding/check
 * metadata. Construction is restricted to Compiler, after which access is
 * read-only. Node IDs and summaries are deterministic for identical source.
 */
class ModelIR {
public:
	/**
	 * Construct storage after Compiler has validated topological/type invariants.
	 *
	 * @param name Root CAT model header retained for display only.
	 * @param hostProfile Explicit/default causal-view profile for exploration.
	 * @param nodes Topologically ordered typed expression DAG.
	 * @param bindings Source-ordered resolved user bindings.
	 * @param checks Source-ordered typed consistency checks.
	 */
	ModelIR(std::string name, HostProfile hostProfile, std::vector<Node> nodes,
		std::vector<Binding> bindings, std::vector<Check> checks)
		: name_(std::move(name)), hostProfile_(hostProfile), nodes_(std::move(nodes)),
		  bindings_(std::move(bindings)), checks_(std::move(checks))
	{}

	ModelIR(const ModelIR &) = delete;
	ModelIR(ModelIR &&) = delete;
	auto operator=(const ModelIR &) -> ModelIR & = delete;
	auto operator=(ModelIR &&) -> ModelIR & = delete;
	~ModelIR() = default;

	/** Return the root model header exactly as decoded by the frontend. */
	[[nodiscard]] auto name() const -> std::string_view { return name_; }
	/** Return the explicit/default GenMC profile used for exploration views. */
	[[nodiscard]] auto hostProfile() const -> HostProfile { return hostProfile_; }
	/** Return the complete topologically ordered typed node array. */
	[[nodiscard]] auto nodes() const -> const std::vector<Node> & { return nodes_; }
	/** Return user bindings in expanded source order. */
	[[nodiscard]] auto bindings() const -> const std::vector<Binding> & { return bindings_; }
	/** Return consistency checks in expanded source order. */
	[[nodiscard]] auto checks() const -> const std::vector<Check> & { return checks_; }
	/**
	 * Find a reachable operation that prevents sound prefix-time rejection.
	 *
	 * Phase 1's online checker requires each check value to grow monotonically as
	 * a graph prefix grows. Difference is non-monotone in its right operand, so a
	 * reachable difference is conservatively rejected at the CLI boundary while
	 * remaining available to the parser and from-scratch evaluator.
	 *
	 * @return First reachable inadmissible node in stable ID order, or no value.
	 * @complexity Linear in the reachable typed DAG.
	 */
	[[nodiscard]] auto firstOnlineInadmissibleNode() const -> std::optional<NodeId>;
	/** Return a platform-independent textual summary for golden tests/review. */
	[[nodiscard]] auto summary() const -> std::string;

private:
	std::string name_;
	HostProfile hostProfile_{HostProfile::SC};
	std::vector<Node> nodes_;
	std::vector<Binding> bindings_;
	std::vector<Check> checks_;
};

/** Result of resolving and typing one syntax model. */
struct CompileResult {
	std::shared_ptr<const ModelIR> model;
	std::vector<Diagnostic> diagnostics;
	std::vector<Diagnostic> notes;

	/** Return true exactly when compilation produced an error-free immutable model. */
	[[nodiscard]] auto ok() const -> bool { return model && diagnostics.empty(); }
};

/**
 * Resolve and type-check the frozen CAT subset into an immutable ModelIR.
 *
 * Compiler instances have no shared state. A compile call uses sequential
 * definition visibility, expands conventional aliases from primitive nodes,
 * and returns all diagnostics accumulated at statement boundaries.
 */
class Compiler {
public:
	/**
	 * Compile one already parsed model.
	 *
	 * @param syntax Syntax tree whose source spans remain valid values after return.
	 * @return Shared immutable IR, or name/type diagnostics and no model.
	 * @errors Undefined/duplicate/reserved names and invalid operator/check types.
	 */
	[[nodiscard]] auto compile(const Model &syntax) const -> CompileResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_MODEL_HPP */
