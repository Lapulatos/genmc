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

	/** Compare expression algebra while deliberately ignoring source locations. */
	static auto sameExpression(const Expression &lhs, const Expression &rhs) -> bool
	{
		if (lhs.kind != rhs.kind || lhs.name != rhs.name ||
		    lhs.operands.size() != rhs.operands.size())
			return false;
		for (std::size_t index = 0; index < lhs.operands.size(); ++index) {
			if (!sameExpression(*lhs.operands[index], *rhs.operands[index]))
				return false;
		}
		return true;
	}

	/** Return whether an expression references any declaration in @p group. */
	[[nodiscard]] auto referencesRecursiveGroup(const Expression &expression,
						    std::uint32_t group) const -> bool
	{
		if (expression.kind == Expression::Kind::Identifier) {
			const auto found = symbols_.find(expression.name);
			return found != symbols_.end() && found->second.declaration &&
			       found->second.declaration->recursiveGroup == group;
		}
		for (const auto &operand : expression.operands) {
			if (referencesRecursiveGroup(*operand, group))
				return true;
		}
		return false;
	}

	/**
	 * Recognize a finite-relation closure equation without inspecting model identity.
	 *
	 * The least fixed point of `X = R | X;R` or `X = R | R;X` is exactly `R+`.
	 * Requiring the same recursion-free seed on both occurrences keeps the rewrite
	 * fail-closed for mutual recursion and superficially similar equations.
	 */
	[[nodiscard]] auto linearClosureSeed(const Statement &statement) const
		-> const Expression *
	{
		if (statement.recursiveGroup == 0 ||
		    statement.expression->kind != Expression::Kind::Union)
			return nullptr;
		const auto match = [&](const Expression &seed,
				       const Expression &recursive) -> const Expression * {
			if (referencesRecursiveGroup(seed, statement.recursiveGroup) ||
			    recursive.kind != Expression::Kind::Composition)
				return nullptr;
			const auto isSelf = [&](const Expression &expression) {
				return expression.kind == Expression::Kind::Identifier &&
				       expression.name == statement.name;
			};
			if ((isSelf(*recursive.operands[0]) &&
			     sameExpression(seed, *recursive.operands[1])) ||
			    (isSelf(*recursive.operands[1]) &&
			     sameExpression(seed, *recursive.operands[0])))
				return &seed;
			return nullptr;
		};
		const auto &lhs = *statement.expression->operands[0];
		const auto &rhs = *statement.expression->operands[1];
		if (const auto *seed = match(lhs, rhs))
			return seed;
		return match(rhs, lhs);
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
			if (const auto *seed = linearClosureSeed(statement)) {
				auto &predicate = predicates_[target];
				predicate.kind = Predicate::Kind::TransitiveClosure;
				predicate.type = ValueType::Relation;
				predicate.operands = {lowerExpression(*seed)};
				return;
			}
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

auto NormalizedModel::certifiedCandidateProfile() const -> std::optional<HostProfile>
{
	/* These are semantic fingerprints, rather than filenames or host-profile
	 * assertions. Keeping the full normalized form makes the certificate fail
	 * closed when any checked relation or axiom changes. */
	static constexpr std::string_view recursiveSC = R"CAT(model RecursiveSC
host-profile sc
predicate 0 rel union com (13,5)
predicate 1 rel union order (15,0)
predicate 2 rel transitive-closure reach rec=1 (1)
predicate 3 rel base rf
predicate 4 rel base fr
predicate 5 rel base co
predicate 6 rel base po
predicate 7 rel base tc
predicate 8 rel base tj
predicate 9 rel base rmw
predicate 10 rel base ext
predicate 11 rel intersection fre (4,10)
predicate 12 rel intersection coe (5,10)
predicate 13 rel union $n0 (3,4)
predicate 14 rel union $n1 (6,7)
predicate 15 rel union $n2 (14,8)
predicate 16 rel composition $n3 (11,12)
predicate 17 rel intersection $n4 (9,16)
check atomicity = 17
check sc = 2
)CAT";
	static constexpr std::string_view recursiveTSO = R"CAT(model RecursiveTSO
host-profile tso
predicate 0 rel union com (21,9)
predicate 1 rel union ppo (30,36)
predicate 2 rel union lifecycle (40,44)
predicate 3 rel union order (45,0)
predicate 4 rel transitive-closure reach rec=1 (3)
predicate 5 rel base rf
predicate 6 rel base ext
predicate 7 rel intersection rfe (5,6)
predicate 8 rel base fr
predicate 9 rel base co
predicate 10 set base R
predicate 11 rel base po
predicate 12 set base W
predicate 13 set base F
predicate 14 set base SC
predicate 15 rel base tc
predicate 16 rel base tj
predicate 17 rel base rmw
predicate 18 rel intersection fre (8,6)
predicate 19 rel intersection coe (9,6)
predicate 20 rel base loc
predicate 21 rel union $n0 (7,8)
predicate 22 rel identity $n1 (10)
predicate 23 rel composition $n2 (22,11)
predicate 24 rel identity $n3 (12)
predicate 25 rel composition $n4 (11,24)
predicate 26 rel union $n5 (23,25)
predicate 27 rel identity $n6 (13)
predicate 28 rel composition $n7 (11,27)
predicate 29 rel composition $n8 (28,11)
predicate 30 rel union $n9 (26,29)
predicate 31 set intersection $n10 (12,14)
predicate 32 rel identity $n11 (31)
predicate 33 rel composition $n12 (32,11)
predicate 34 set intersection $n13 (10,14)
predicate 35 rel identity $n14 (34)
predicate 36 rel composition $n15 (33,35)
predicate 37 rel optional $n16 (11)
predicate 38 rel composition $n17 (37,15)
predicate 39 rel optional $n18 (11)
predicate 40 rel composition $n19 (38,39)
predicate 41 rel optional $n20 (11)
predicate 42 rel composition $n21 (41,16)
predicate 43 rel optional $n22 (11)
predicate 44 rel composition $n23 (42,43)
predicate 45 rel union $n24 (1,2)
predicate 46 rel composition $n25 (18,19)
predicate 47 rel intersection $n26 (17,46)
predicate 48 rel intersection $n27 (11,20)
predicate 49 rel union $n28 (48,5)
predicate 50 rel union $n29 (49,8)
predicate 51 rel union $n30 (50,9)
check atomicity = 47
check coherence = 51
check tso = 4
)CAT";

	const auto fingerprint = summary();
	if (fingerprint == recursiveSC)
		return HostProfile::SC;
	if (fingerprint == recursiveTSO)
		return HostProfile::TSO;
	return std::nullopt;
}

auto NormalizedModel::certifiedAdaptiveOffline() const -> bool
{
	/* Backend selection changes only how the same normalized fixed point is
	 * evaluated. Keep a separate PSO fingerprint here: PSO uses the TSO causal
	 * host, but TSO's candidate-pruning theorem must not be applied to PSO. */
	if (certifiedCandidateProfile())
		return true;
	static constexpr std::string_view recursivePSO = R"CAT(model RecursivePSO
host-profile tso
predicate 0 rel union com (22,10)
predicate 1 rel intersection ww_loc (26,13)
predicate 2 rel union ppo (33,38)
predicate 3 rel union lifecycle (42,46)
predicate 4 rel union order (47,0)
predicate 5 rel transitive-closure reach rec=1 (4)
predicate 6 rel base rf
predicate 7 rel base ext
predicate 8 rel intersection rfe (6,7)
predicate 9 rel base fr
predicate 10 rel base co
predicate 11 set base W
predicate 12 rel base po
predicate 13 rel base loc
predicate 14 set base R
predicate 15 set base F
predicate 16 set base SC
predicate 17 rel base tc
predicate 18 rel base tj
predicate 19 rel base rmw
predicate 20 rel intersection fre (9,7)
predicate 21 rel intersection coe (10,7)
predicate 22 rel union $n0 (8,9)
predicate 23 rel identity $n1 (11)
predicate 24 rel composition $n2 (23,12)
predicate 25 rel identity $n3 (11)
predicate 26 rel composition $n4 (24,25)
predicate 27 rel identity $n5 (14)
predicate 28 rel composition $n6 (27,12)
predicate 29 rel union $n7 (28,1)
predicate 30 rel identity $n8 (15)
predicate 31 rel composition $n9 (12,30)
predicate 32 rel composition $n10 (31,12)
predicate 33 rel union $n11 (29,32)
predicate 34 set intersection $n12 (11,16)
predicate 35 rel identity $n13 (34)
predicate 36 rel composition $n14 (35,12)
predicate 37 rel identity $n15 (16)
predicate 38 rel composition $n16 (36,37)
predicate 39 rel optional $n17 (12)
predicate 40 rel composition $n18 (39,17)
predicate 41 rel optional $n19 (12)
predicate 42 rel composition $n20 (40,41)
predicate 43 rel optional $n21 (12)
predicate 44 rel composition $n22 (43,18)
predicate 45 rel optional $n23 (12)
predicate 46 rel composition $n24 (44,45)
predicate 47 rel union $n25 (2,3)
predicate 48 rel composition $n26 (20,21)
predicate 49 rel intersection $n27 (19,48)
predicate 50 rel intersection $n28 (12,13)
predicate 51 rel union $n29 (50,6)
predicate 52 rel union $n30 (51,9)
predicate 53 rel union $n31 (52,10)
check atomicity = 49
check coherence = 53
check pso = 5
)CAT";
	return summary() == recursivePSO;
}

auto Normalizer::normalize(const Model &syntax) const -> NormalizeResult
{
	return Builder{}.build(syntax);
}

} /* namespace cat */
