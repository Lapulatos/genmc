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

#include "genmc/CAT/Normalized.hpp"

#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace cat {
namespace {

static auto kindName(Predicate::Kind kind) -> std::string_view
{
	switch (kind) {
	case Predicate::Kind::Base:
		return "base";
	case Predicate::Kind::Alias:
		return "alias";
	case Predicate::Kind::Union:
		return "union";
	case Predicate::Kind::Composition:
		return "composition";
	case Predicate::Kind::Difference:
		return "difference";
	case Predicate::Kind::Intersection:
		return "intersection";
	case Predicate::Kind::Product:
		return "product";
	case Predicate::Kind::Identity:
		return "identity";
	case Predicate::Kind::Domain:
		return "domain";
	case Predicate::Kind::Range:
		return "range";
	case Predicate::Kind::Inverse:
		return "inverse";
	case Predicate::Kind::Optional:
		return "optional";
	case Predicate::Kind::TransitiveClosure:
		return "transitive-closure";
	case Predicate::Kind::ReflexiveTransitiveClosure:
		return "reflexive-transitive-closure";
	}
	return "base";
}

static auto typeName(ValueType type) -> std::string_view
{
	return type == ValueType::Set ? "set" : "rel";
}

/** Small union-find whose roots optionally carry a resolved CAAT value type. */
class TypeVariables {
public:
	auto create() -> std::size_t
	{
		const auto id = parent_.size();
		parent_.push_back(id);
		rank_.push_back(0);
		type_.push_back(std::nullopt);
		return id;
	}

	auto merge(std::size_t lhs, std::size_t rhs) -> bool
	{
		lhs = find(lhs);
		rhs = find(rhs);
		if (lhs == rhs)
			return true;
		if (type_[lhs] && type_[rhs] && type_[lhs] != type_[rhs])
			return false;
		if (rank_[lhs] < rank_[rhs])
			std::swap(lhs, rhs);
		parent_[rhs] = lhs;
		if (rank_[lhs] == rank_[rhs])
			++rank_[lhs];
		if (!type_[lhs])
			type_[lhs] = type_[rhs];
		return true;
	}

	auto constrain(std::size_t variable, ValueType type) -> bool
	{
		const auto root = find(variable);
		if (type_[root] && *type_[root] != type)
			return false;
		type_[root] = type;
		return true;
	}

	[[nodiscard]] auto get(std::size_t variable) -> std::optional<ValueType>
	{
		return type_[find(variable)];
	}

private:
	auto find(std::size_t variable) -> std::size_t
	{
		if (parent_[variable] != variable)
			parent_[variable] = find(parent_[variable]);
		return parent_[variable];
	}

	std::vector<std::size_t> parent_;
	std::vector<std::uint8_t> rank_;
	std::vector<std::optional<ValueType>> type_;
};

/** Mutable construction state for one normalized model. */
class Builder {
public:
	auto build(const Model &syntax) -> NormalizeResult
	{
		collectDeclarations(syntax);
		if (!diagnostics_.empty())
			return {nullptr, std::move(diagnostics_)};
		for (const auto &statement : syntax.statements)
			collectConstraints(statement);
		resolveDeclaredTypes(syntax);
		if (!diagnostics_.empty())
			return {nullptr, std::move(diagnostics_)};
		for (const auto &statement : syntax.statements)
			lowerStatement(statement);
		if (!diagnostics_.empty())
			return {nullptr, std::move(diagnostics_)};
		std::shared_ptr<const NormalizedModel> result = std::make_shared<NormalizedModel>(
			syntax.name, syntax.hostProfile, std::move(predicates_),
			std::move(checks_));
		return {std::move(result), {}};
	}

private:
	struct Symbol {
		PredicateId predicate{};
		std::size_t typeVariable{};
		const Statement *declaration{};
	};

	static auto builtinType(std::string_view name) -> std::optional<ValueType>
	{
		static const std::unordered_set<std::string_view> sets{"_", "M",  "R", "W",
								       "F", "IW", "SC"};
		static const std::unordered_set<std::string_view> relations{
			"0",   "id", "po", "rf",  "co",	 "mo",	"fr",  "rmw", "loc", "int",
			"ext", "tc", "tj", "rfi", "rfe", "coi", "coe", "fri", "fre", "po-loc"};
		if (sets.contains(name))
			return ValueType::Set;
		if (relations.contains(name))
			return ValueType::Relation;
		return std::nullopt;
	}

	void diagnose(DiagnosticKind kind, const SourceSpan &span, std::string message)
	{
		diagnostics_.push_back({kind, span, std::move(message), {}});
	}

	auto addPredicate(Predicate predicate) -> PredicateId
	{
		predicate.id = static_cast<PredicateId>(predicates_.size());
		predicates_.push_back(std::move(predicate));
		return predicates_.back().id;
	}

	/** Reserve named predicate IDs before resolving any right-hand side. */
	void collectDeclarations(const Model &syntax)
	{
		for (const auto &statement : syntax.statements) {
			if (statement.kind != Statement::Kind::Let)
				continue;
			if (builtinType(statement.name) || symbols_.contains(statement.name)) {
				diagnose(DiagnosticKind::Name, statement.span,
					 "duplicate or reserved CAT name '" + statement.name + "'");
				continue;
			}
			const auto variable = types_.create();
			const auto id = addPredicate({0,
						      Predicate::Kind::Alias,
						      ValueType::Relation,
						      statement.span,
						      statement.name,
						      {},
						      statement.recursiveGroup,
						      false});
			symbols_.emplace(statement.name, Symbol{id, variable, &statement});
			declaredNames_.insert(statement.name);
		}
	}

	auto builtin(std::string_view name, const SourceSpan &span) -> std::optional<Symbol>
	{
		if (const auto found = symbols_.find(std::string(name)); found != symbols_.end())
			return found->second;
		const auto type = builtinType(name);
		if (!type)
			return std::nullopt;
		const auto variable = types_.create();
		types_.constrain(variable, *type);
		Predicate::Kind kind{Predicate::Kind::Base};
		std::vector<PredicateId> operands;
		if (name == "M") {
			kind = Predicate::Kind::Union;
			operands = {builtin("R", span)->predicate, builtin("W", span)->predicate};
		} else if (name == "mo") {
			kind = Predicate::Kind::Alias;
			operands = {builtin("co", span)->predicate};
		} else {
			static const std::unordered_map<
				std::string_view, std::pair<std::string_view, std::string_view>>
				aliases{{"rfi", {"rf", "int"}},	  {"rfe", {"rf", "ext"}},
					{"coi", {"co", "int"}},	  {"coe", {"co", "ext"}},
					{"fri", {"fr", "int"}},	  {"fre", {"fr", "ext"}},
					{"po-loc", {"po", "loc"}}};
			if (const auto alias = aliases.find(name); alias != aliases.end()) {
				kind = Predicate::Kind::Intersection;
				operands = {builtin(alias->second.first, span)->predicate,
					    builtin(alias->second.second, span)->predicate};
			}
		}
		const auto id = addPredicate(
			{0, kind, *type, span, std::string(name), std::move(operands), 0, false});
		Symbol symbol{id, variable, nullptr};
		symbols_.emplace(std::string(name), symbol);
		return symbol;
	}

	auto expressionVariable(const Expression &expression) -> std::size_t
	{
		if (const auto found = expressionTypes_.find(&expression);
		    found != expressionTypes_.end())
			return found->second;
		const auto variable = types_.create();
		expressionTypes_.emplace(&expression, variable);

		if (expression.kind == Expression::Kind::Identifier) {
			auto symbol = builtin(expression.name, expression.span);
			if (!symbol) {
				diagnose(DiagnosticKind::Name, expression.span,
					 "undefined CAT name '" + expression.name + "'");
				return variable;
			}
			if (!types_.merge(variable, symbol->typeVariable))
				diagnose(DiagnosticKind::Type, expression.span,
					 "identifier has conflicting set/relation uses");
			return variable;
		}
		if (expression.kind == Expression::Kind::EmptyRelation) {
			types_.constrain(variable, ValueType::Relation);
			builtin("0", expression.span);
			return variable;
		}
		if (expression.kind == Expression::Kind::Universe) {
			types_.constrain(variable, ValueType::Set);
			builtin("_", expression.span);
			return variable;
		}

		std::vector<std::size_t> operands;
		for (const auto &operand : expression.operands)
			operands.push_back(expressionVariable(*operand));
		auto require = [&](std::size_t operand, ValueType type) {
			if (!types_.constrain(operand, type))
				diagnose(DiagnosticKind::Type, expression.span,
					 "operator has conflicting set/relation operands");
		};
		switch (expression.kind) {
		case Expression::Kind::Union:
		case Expression::Kind::Difference:
		case Expression::Kind::Intersection:
			if (!types_.merge(variable, operands[0]) ||
			    !types_.merge(variable, operands[1]))
				diagnose(DiagnosticKind::Type, expression.span,
					 "set/relation operands must have the same type");
			break;
		case Expression::Kind::Product:
			require(operands[0], ValueType::Set);
			require(operands[1], ValueType::Set);
			require(variable, ValueType::Relation);
			break;
		case Expression::Kind::Identity:
			require(operands[0], ValueType::Set);
			require(variable, ValueType::Relation);
			break;
		case Expression::Kind::Domain:
		case Expression::Kind::Range:
			require(operands[0], ValueType::Relation);
			require(variable, ValueType::Set);
			break;
		case Expression::Kind::Composition:
			require(operands[0], ValueType::Relation);
			require(operands[1], ValueType::Relation);
			require(variable, ValueType::Relation);
			break;
		case Expression::Kind::Inverse:
		case Expression::Kind::Optional:
		case Expression::Kind::TransitiveClosure:
		case Expression::Kind::ReflexiveTransitiveClosure:
			require(operands[0], ValueType::Relation);
			require(variable, ValueType::Relation);
			break;
		default:
			break;
		}
		return variable;
	}

	void collectConstraints(const Statement &statement)
	{
		const auto variable = expressionVariable(*statement.expression);
		if (statement.kind == Statement::Kind::Let) {
			const auto found = symbols_.find(statement.name);
			if (found != symbols_.end() &&
			    !types_.merge(found->second.typeVariable, variable))
				diagnose(DiagnosticKind::Type, statement.span,
					 "binding has conflicting set/relation uses");
		} else if (statement.checkKind != Statement::CheckKind::Empty &&
			   !types_.constrain(variable, ValueType::Relation)) {
			diagnose(DiagnosticKind::Type, statement.span,
				 "acyclic/irreflexive check requires rel, found set");
		}
	}

	void resolveDeclaredTypes(const Model &syntax)
	{
		for (const auto &statement : syntax.statements) {
			if (statement.kind != Statement::Kind::Let)
				continue;
			auto &symbol = symbols_.at(statement.name);
			const auto type = types_.get(symbol.typeVariable);
			if (!type) {
				diagnose(DiagnosticKind::Type, statement.span,
					 "cannot infer whether recursive predicate '" +
						 statement.name + "' is a set or relation");
				continue;
			}
			predicates_[symbol.predicate].type = *type;
		}
	}

	static auto predicateKind(Expression::Kind kind) -> Predicate::Kind
	{
		switch (kind) {
		case Expression::Kind::Union:
			return Predicate::Kind::Union;
		case Expression::Kind::Composition:
			return Predicate::Kind::Composition;
		case Expression::Kind::Difference:
			return Predicate::Kind::Difference;
		case Expression::Kind::Intersection:
			return Predicate::Kind::Intersection;
		case Expression::Kind::Product:
			return Predicate::Kind::Product;
		case Expression::Kind::Identity:
			return Predicate::Kind::Identity;
		case Expression::Kind::Domain:
			return Predicate::Kind::Domain;
		case Expression::Kind::Range:
			return Predicate::Kind::Range;
		case Expression::Kind::Inverse:
			return Predicate::Kind::Inverse;
		case Expression::Kind::Optional:
			return Predicate::Kind::Optional;
		case Expression::Kind::TransitiveClosure:
			return Predicate::Kind::TransitiveClosure;
		case Expression::Kind::ReflexiveTransitiveClosure:
			return Predicate::Kind::ReflexiveTransitiveClosure;
		default:
			return Predicate::Kind::Alias;
		}
	}

	auto lowerExpression(const Expression &expression,
			     std::optional<PredicateId> target = std::nullopt) -> PredicateId
	{
		if (expression.kind == Expression::Kind::Identifier)
			return symbols_.at(expression.name).predicate;
		if (expression.kind == Expression::Kind::EmptyRelation)
			return symbols_.at("0").predicate;
		if (expression.kind == Expression::Kind::Universe)
			return symbols_.at("_").predicate;

		std::vector<PredicateId> operands;
		for (const auto &operand : expression.operands)
			operands.push_back(lowerExpression(*operand));
		const auto type = *types_.get(expressionTypes_.at(&expression));
		if (target) {
			auto &predicate = predicates_.at(*target);
			predicate.kind = predicateKind(expression.kind);
			predicate.type = type;
			predicate.operands = std::move(operands);
			return *target;
		}
		const auto name = "$n" + std::to_string(generatedCount_++);
		return addPredicate({0, predicateKind(expression.kind), type, expression.span, name,
				     std::move(operands), 0, true});
	}

	void lowerStatement(const Statement &statement)
	{
		if (statement.kind == Statement::Kind::Let) {
			const auto target = symbols_.at(statement.name).predicate;
			if (statement.expression->kind == Expression::Kind::Identifier ||
			    statement.expression->kind == Expression::Kind::EmptyRelation ||
			    statement.expression->kind == Expression::Kind::Universe) {
				predicates_[target].kind = Predicate::Kind::Alias;
				predicates_[target].operands = {
					lowerExpression(*statement.expression)};
			} else {
				lowerExpression(*statement.expression, target);
			}
			return;
		}
		auto predicate = lowerExpression(*statement.expression);
		auto name = statement.name;
		if (name.empty())
			name = "check@" + statement.span.begin.file.filename().string() + ":" +
			       std::to_string(statement.span.begin.line) + ":" +
			       std::to_string(statement.span.begin.column) + "#" +
			       std::to_string(checks_.size());
		if (builtinType(name) || declaredNames_.contains(name)) {
			diagnose(DiagnosticKind::Name, statement.span,
				 "duplicate or reserved CAT name '" + name + "'");
			return;
		}
		declaredNames_.insert(name);
		checks_.push_back(
			{statement.checkKind, std::move(name), predicate, statement.span});
	}

	TypeVariables types_;
	std::unordered_map<const Expression *, std::size_t> expressionTypes_;
	std::unordered_map<std::string, Symbol> symbols_;
	std::unordered_set<std::string> declaredNames_;
	std::vector<Predicate> predicates_;
	std::vector<NormalizedCheck> checks_;
	std::vector<Diagnostic> diagnostics_;
	std::size_t generatedCount_{};
};

} /* namespace */

auto NormalizedModel::summary() const -> std::string
{
	std::ostringstream output;
	output << "model " << name_ << "\n";
	output << "host-profile " << (hostProfile_ == HostProfile::SC ? "sc" : "tso") << "\n";
	for (const auto &predicate : predicates_) {
		output << "predicate " << predicate.id << " " << typeName(predicate.type) << " "
		       << kindName(predicate.kind) << " " << predicate.name;
		if (predicate.declaredRecursiveGroup != 0)
			output << " rec=" << predicate.declaredRecursiveGroup;
		if (!predicate.operands.empty()) {
			output << " (";
			for (std::size_t i = 0; i < predicate.operands.size(); ++i) {
				if (i != 0)
					output << ",";
				output << predicate.operands[i];
			}
			output << ")";
		}
		output << "\n";
	}
	for (const auto &check : checks_)
		output << "check " << check.name << " = " << check.predicate << "\n";
	return output.str();
}

auto Normalizer::normalize(const Model &syntax) const -> NormalizeResult
{
	return Builder{}.build(syntax);
}

} /* namespace cat */
