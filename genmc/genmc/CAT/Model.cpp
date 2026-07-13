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

#include "genmc/CAT/Model.hpp"

#include <array>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace cat {
namespace {

struct Symbol {
	ValueType type{ValueType::Relation};
	NodeId value{};
};

static auto typeName(ValueType type) -> std::string_view
{
	return type == ValueType::Set ? "set" : "rel";
}

static auto nodeKindName(Node::Kind kind) -> std::string_view
{
	switch (kind) {
	case Node::Kind::Builtin:
		return "builtin";
	case Node::Kind::Union:
		return "union";
	case Node::Kind::Composition:
		return "composition";
	case Node::Kind::Difference:
		return "difference";
	case Node::Kind::Intersection:
		return "intersection";
	case Node::Kind::Product:
		return "product";
	case Node::Kind::Identity:
		return "identity";
	case Node::Kind::Inverse:
		return "inverse";
	case Node::Kind::Optional:
		return "optional";
	case Node::Kind::TransitiveClosure:
		return "transitive-closure";
	case Node::Kind::ReflexiveTransitiveClosure:
		return "reflexive-transitive-closure";
	}
	return "builtin";
}

static auto checkKindName(Statement::CheckKind kind) -> std::string_view
{
	switch (kind) {
	case Statement::CheckKind::Acyclic:
		return "acyclic";
	case Statement::CheckKind::Irreflexive:
		return "irreflexive";
	case Statement::CheckKind::Empty:
		return "empty";
	}
	return "empty";
}

/** Per-compilation mutable state; the published ModelIR receives moved value storage. */
class Builder {
public:
	auto build(const Model &syntax) -> CompileResult
	{
		for (const auto &statement : syntax.statements)
			compileStatement(statement);
		if (!diagnostics_.empty())
			return {nullptr, std::move(diagnostics_), std::move(notes_)};
		std::shared_ptr<const ModelIR> model = std::make_shared<ModelIR>(
			syntax.name, std::move(nodes_), std::move(bindings_), std::move(checks_));
		return {std::move(model), {}, std::move(notes_)};
	}

private:
	/** Return the static type of one immutable node ID. */
	[[nodiscard]] auto typeOf(NodeId id) const -> ValueType { return nodes_.at(id).type; }

	/** Append a topologically ordered node and return its stable dense ID. */
	auto addNode(Node::Kind kind, ValueType type, const SourceSpan &span, std::string name,
		     std::vector<NodeId> operands = {}) -> NodeId
	{
		const auto id = static_cast<NodeId>(nodes_.size());
		nodes_.push_back({id, kind, type, span, std::move(name), std::move(operands)});
		return id;
	}

	/** Emit one source-located resolver/type diagnostic. */
	void diagnose(DiagnosticKind kind, const SourceSpan &span, std::string message)
	{
		diagnostics_.push_back({kind, span, std::move(message), {}});
	}

	/** Resolve a primitive or derived read-only built-in, creating it lazily once. */
	auto resolveBuiltin(std::string_view name, const SourceSpan &span) -> std::optional<Symbol>
	{
		if (const auto found = builtins_.find(std::string(name)); found != builtins_.end())
			return found->second;

		if (name == "mo") {
			auto symbol = resolveBuiltin("co", span);
			builtins_.emplace("mo", *symbol);
			notes_.push_back({DiagnosticKind::Note,
					  span,
					  "'mo' is deprecated in this profile; use 'co'",
					  {}});
			return symbol;
		}
		if (name == "M") {
			const auto read = resolveBuiltin("R", span)->value;
			const auto write = resolveBuiltin("W", span)->value;
			Symbol symbol{ValueType::Set, addNode(Node::Kind::Union, ValueType::Set,
							      span, "M", {read, write})};
			builtins_.emplace("M", symbol);
			return symbol;
		}

		static const std::unordered_map<std::string_view, std::array<std::string_view, 2>>
			aliases{{"rfi", {"rf", "int"}},	  {"rfe", {"rf", "ext"}},
				{"coi", {"co", "int"}},	  {"coe", {"co", "ext"}},
				{"fri", {"fr", "int"}},	  {"fre", {"fr", "ext"}},
				{"po-loc", {"po", "loc"}}};
		if (const auto alias = aliases.find(name); alias != aliases.end()) {
			const auto lhs = resolveBuiltin(alias->second[0], span)->value;
			const auto rhs = resolveBuiltin(alias->second[1], span)->value;
			Symbol symbol{ValueType::Relation,
				      addNode(Node::Kind::Intersection, ValueType::Relation, span,
					      std::string(name), {lhs, rhs})};
			builtins_.emplace(std::string(name), symbol);
			return symbol;
		}

		static const std::unordered_set<std::string_view> setBuiltins{"_", "R",	 "W",
									      "F", "IW", "SC"};
		static const std::unordered_set<std::string_view> relationBuiltins{
			"0", "id", "po", "rf", "co", "fr", "rmw", "loc", "int", "ext", "tc", "tj"};
		ValueType type{ValueType::Relation};
		if (setBuiltins.contains(name))
			type = ValueType::Set;
		else if (relationBuiltins.contains(name))
			type = ValueType::Relation;
		else
			return std::nullopt;
		Symbol symbol{type,
			      addNode(Node::Kind::Builtin, type, span, std::string(name), {})};
		builtins_.emplace(std::string(name), symbol);
		return symbol;
	}

	/** Resolve one identifier using definition-order visibility then the read-only prelude. */
	auto resolveName(std::string_view name, const SourceSpan &span) -> std::optional<Symbol>
	{
		if (const auto found = symbols_.find(std::string(name)); found != symbols_.end())
			return found->second;
		if (auto builtin = resolveBuiltin(name, span))
			return builtin;
		diagnose(DiagnosticKind::Name, span,
			 "undefined CAT name '" + std::string(name) + "'");
		return std::nullopt;
	}

	/** Return whether @p name is a frozen built-in/alias and therefore cannot be rebound. */
	static auto isReserved(std::string_view name) -> bool
	{
		static const std::unordered_set<std::string_view> names{
			"_",  "M",  "R",   "W",	  "F",	 "IW",	"SC",  "0",   "id",
			"po", "rf", "co",  "mo",  "fr",	 "rmw", "loc", "int", "ext",
			"tc", "tj", "rfi", "rfe", "coi", "coe", "fri", "fre", "po-loc"};
		return names.contains(name);
	}

	/** Compile one syntax expression, checking every operator's frozen operand contract. */
	auto compileExpression(const Expression &expression) -> std::optional<NodeId>
	{
		if (expression.kind == Expression::Kind::Identifier)
			return resolveName(expression.name, expression.span)
				.transform([](const Symbol &symbol) { return symbol.value; });
		if (expression.kind == Expression::Kind::EmptyRelation)
			return resolveBuiltin("0", expression.span)->value;
		if (expression.kind == Expression::Kind::Universe)
			return resolveBuiltin("_", expression.span)->value;

		if (expression.operands.size() == 1)
			return compileUnary(expression);
		if (expression.operands.size() == 2)
			return compileBinary(expression);
		diagnose(DiagnosticKind::Type, expression.span,
			 "malformed internal CAT expression");
		return std::nullopt;
	}

	/** Type-check and lower identity/inverse/closure postfix operations. */
	auto compileUnary(const Expression &expression) -> std::optional<NodeId>
	{
		auto operand = compileExpression(*expression.operands[0]);
		if (!operand)
			return std::nullopt;
		Node::Kind kind{Node::Kind::Identity};
		constexpr ValueType resultType{ValueType::Relation};
		if (expression.kind == Expression::Kind::Identity) {
			kind = Node::Kind::Identity;
			if (typeOf(*operand) != ValueType::Set) {
				diagnose(DiagnosticKind::Type, expression.span,
					 "identity restriction requires set, found " +
						 std::string(typeName(typeOf(*operand))));
				return std::nullopt;
			}
		} else {
			if (typeOf(*operand) != ValueType::Relation) {
				diagnose(DiagnosticKind::Type, expression.span,
					 "relational postfix operator requires rel, found " +
						 std::string(typeName(typeOf(*operand))));
				return std::nullopt;
			}
			switch (expression.kind) {
			case Expression::Kind::Inverse:
				kind = Node::Kind::Inverse;
				break;
			case Expression::Kind::Optional:
				kind = Node::Kind::Optional;
				break;
			case Expression::Kind::TransitiveClosure:
				kind = Node::Kind::TransitiveClosure;
				break;
			case Expression::Kind::ReflexiveTransitiveClosure:
				kind = Node::Kind::ReflexiveTransitiveClosure;
				break;
			default:
				diagnose(DiagnosticKind::Type, expression.span,
					 "malformed internal unary CAT expression");
				return std::nullopt;
			}
		}
		return addNode(kind, resultType, expression.span, {}, {*operand});
	}

	/** Type-check and lower one left-associated binary relation/set operation. */
	auto compileBinary(const Expression &expression) -> std::optional<NodeId>
	{
		auto lhs = compileExpression(*expression.operands[0]);
		auto rhs = compileExpression(*expression.operands[1]);
		if (!lhs || !rhs)
			return std::nullopt;
		const auto lhsType = typeOf(*lhs);
		const auto rhsType = typeOf(*rhs);
		Node::Kind kind{Node::Kind::Union};
		ValueType resultType{ValueType::Relation};
		switch (expression.kind) {
		case Expression::Kind::Union:
		case Expression::Kind::Difference:
		case Expression::Kind::Intersection:
			if (lhsType != rhsType) {
				diagnose(DiagnosticKind::Type, expression.span,
					 "set/relation operands must have the same type");
				return std::nullopt;
			}
			if (expression.kind == Expression::Kind::Union)
				kind = Node::Kind::Union;
			else if (expression.kind == Expression::Kind::Difference)
				kind = Node::Kind::Difference;
			else
				kind = Node::Kind::Intersection;
			resultType = lhsType;
			break;
		case Expression::Kind::Composition:
			if (lhsType != ValueType::Relation || rhsType != ValueType::Relation) {
				diagnose(DiagnosticKind::Type, expression.span,
					 "composition requires two rel operands");
				return std::nullopt;
			}
			kind = Node::Kind::Composition;
			resultType = ValueType::Relation;
			break;
		case Expression::Kind::Product:
			if (lhsType != ValueType::Set || rhsType != ValueType::Set) {
				diagnose(DiagnosticKind::Type, expression.span,
					 "Cartesian product requires two set operands");
				return std::nullopt;
			}
			kind = Node::Kind::Product;
			resultType = ValueType::Relation;
			break;
		default:
			diagnose(DiagnosticKind::Type, expression.span,
				 "malformed internal binary CAT expression");
			return std::nullopt;
		}
		return addNode(kind, resultType, expression.span, {}, {*lhs, *rhs});
	}

	/** Compile one binding/check while retaining independent-statement diagnostics. */
	void compileStatement(const Statement &statement)
	{
		if (statement.kind == Statement::Kind::Let) {
			if (isReserved(statement.name) || declaredNames_.contains(statement.name)) {
				diagnose(DiagnosticKind::Name, statement.span,
					 "duplicate or reserved CAT name '" + statement.name + "'");
				return;
			}
			auto value = compileExpression(*statement.expression);
			if (!value)
				return;
			symbols_.emplace(statement.name, Symbol{typeOf(*value), *value});
			declaredNames_.insert(statement.name);
			bindings_.push_back({statement.name, *value, statement.span});
			return;
		}

		auto value = compileExpression(*statement.expression);
		if (!value)
			return;
		if (statement.checkKind != Statement::CheckKind::Empty &&
		    typeOf(*value) != ValueType::Relation) {
			diagnose(DiagnosticKind::Type, statement.span,
				 "acyclic/irreflexive check requires rel, found set");
			return;
		}
		auto name = statement.name;
		if (name.empty())
			name = "check@" + statement.span.begin.file.filename().string() + ":" +
			       std::to_string(statement.span.begin.line) + ":" +
			       std::to_string(statement.span.begin.column) + "#" +
			       std::to_string(checks_.size());
		if (isReserved(name) || declaredNames_.contains(name)) {
			diagnose(DiagnosticKind::Name, statement.span,
				 "duplicate or reserved CAT name '" + name + "'");
			return;
		}
		declaredNames_.insert(name);
		checks_.push_back({statement.checkKind, std::move(name), *value, statement.span});
	}

	std::vector<Node> nodes_;
	std::vector<Binding> bindings_;
	std::vector<Check> checks_;
	std::unordered_map<std::string, Symbol> symbols_;
	std::unordered_map<std::string, Symbol> builtins_;
	std::unordered_set<std::string> declaredNames_;
	std::vector<Diagnostic> diagnostics_;
	std::vector<Diagnostic> notes_;
};

} /* namespace */

auto ModelIR::summary() const -> std::string
{
	std::ostringstream output;
	output << "model " << name_ << "\n";
	for (const auto &node : nodes_) {
		output << "node " << node.id << " " << typeName(node.type) << " "
		       << nodeKindName(node.kind);
		if (!node.name.empty())
			output << " " << node.name;
		if (!node.operands.empty()) {
			output << " (";
			for (std::size_t i = 0; i < node.operands.size(); ++i) {
				if (i != 0)
					output << ",";
				output << node.operands[i];
			}
			output << ")";
		}
		output << "\n";
	}
	for (const auto &binding : bindings_)
		output << "let " << binding.name << " = " << binding.value << "\n";
	for (const auto &check : checks_)
		output << "check " << checkKindName(check.kind) << " " << check.name << " = "
		       << check.value << "\n";
	return output.str();
}

auto Compiler::compile(const Model &syntax) const -> CompileResult
{
	return Builder{}.build(syntax);
}

} /* namespace cat */
