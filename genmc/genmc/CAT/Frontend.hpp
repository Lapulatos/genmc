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

#ifndef GENMC_CAT_FRONTEND_HPP
#define GENMC_CAT_FRONTEND_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cat {

/** A byte position and human-readable line/column in one CAT source file. */
struct SourceLocation {
	std::filesystem::path file;
	std::size_t offset{};
	std::size_t line{1};
	std::size_t column{1};

	auto operator==(const SourceLocation &) const -> bool = default;
};

/** Half-open source range `[begin, end)` retained by every syntax node. */
struct SourceSpan {
	SourceLocation begin;
	SourceLocation end;

	auto operator==(const SourceSpan &) const -> bool = default;
};

/** Stable frontend diagnostic category used by CLI and focused tests. */
enum class DiagnosticKind { Io, Lex, Parse, Include, Name, Type, Unsupported, Note };

/**
 * GenMC exploration profile used to maintain causal views around CAT evaluation.
 *
 * This is explicit execution metadata, not a CAT consistency axiom. The generic
 * evaluator still decides consistency exclusively from the model's relational
 * checks. SC is the compatibility default for files without a declaration.
 */
enum class HostProfile : std::uint8_t { SC, TSO };

/**
 * One CAT frontend diagnostic with exact source provenance.
 *
 * Diagnostics own their strings and are independent of frontend lifetime.
 * The structure is immutable by convention after publication and safe to
 * read concurrently.
 */
struct Diagnostic {
	DiagnosticKind kind{DiagnosticKind::Parse};
	SourceSpan span;
	std::string message;
	std::string sourceLine;

	/** Return `path:line:column: category: message` and an optional source line. */
	[[nodiscard]] auto format() const -> std::string;
};

/** Syntax-only CAT expression; typing and name resolution are Phase 1.3 work. */
struct Expression {
	/** Operators correspond directly to the frozen CAT grammar. */
	enum class Kind {
		Identifier,
		EmptyRelation,
		Universe,
		Identity,
		Union,
		Composition,
		Difference,
		Intersection,
		Product,
		Inverse,
		Optional,
		TransitiveClosure,
		ReflexiveTransitiveClosure
	};

	Kind kind{Kind::Identifier};
	SourceSpan span;
	std::string name;
	std::vector<std::unique_ptr<Expression>> operands;
};

/** One top-level binding or consistency check after include expansion. */
struct Statement {
	/** Statement forms accepted by the Phase 1 grammar. */
	enum class Kind { Let, Check };

	/** Consistency predicates accepted by the syntax frontend. */
	enum class CheckKind { Acyclic, Irreflexive, Empty };

	Kind kind{Kind::Let};
	CheckKind checkKind{CheckKind::Acyclic};
	SourceSpan span;
	std::string name;
	std::unique_ptr<Expression> expression;
};

/**
 * Parsed CAT model with includes expanded at their source positions.
 *
 * The model exclusively owns its syntax tree. Moving the model preserves all
 * node addresses; copying is intentionally disabled by unique ownership.
 */
struct Model {
	std::string name;
	SourceSpan nameSpan;
	HostProfile hostProfile{HostProfile::SC};
	std::optional<SourceSpan> hostProfileSpan;
	std::vector<Statement> statements;
};

/** Result of loading a root CAT model; a model is present only on success. */
struct ParseResult {
	std::optional<Model> model;
	std::vector<Diagnostic> diagnostics;

	/** Return true exactly when parsing and include expansion succeeded. */
	[[nodiscard]] auto ok() const -> bool { return model.has_value() && diagnostics.empty(); }
};

/**
 * Dependency-free loader for the frozen Phase 1 CAT syntax.
 *
 * A Frontend object has no mutable shared state: each parse owns its file
 * buffers and include stack, so separate calls and separate objects are safe
 * to execute concurrently. Complexity is linear in source bytes plus AST
 * allocation for valid input.
 */
class Frontend {
public:
	/**
	 * Parse one root model and recursively expand relative or absolute includes.
	 *
	 * @param path Root CAT file. It must contain the model-name header.
	 * @return Owned syntax tree, or diagnostics with exact source locations.
	 * @errors Reports I/O, lexical, parse, unsupported-feature, and include-cycle errors.
	 */
	[[nodiscard]] auto parseFile(const std::filesystem::path &path) const -> ParseResult;
};

} /* namespace cat */

#endif /* GENMC_CAT_FRONTEND_HPP */
