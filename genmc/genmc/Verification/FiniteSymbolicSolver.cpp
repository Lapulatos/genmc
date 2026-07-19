/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSymbolicSolver.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if GENMC_HAVE_Z3
#include <z3++.h>
#endif

namespace genmc::symbolic {

class Solver::Impl {
public:
	enum class Kind : std::uint8_t { boolean, bitVector };
	struct Sort {
		Kind kind{};
		std::uint32_t width{};
	};

#if GENMC_HAVE_Z3
	Impl() : solver(context) {}
	z3::context context;
	z3::solver solver;
	std::vector<z3::expr> expressions;
	std::optional<z3::model> model;
#endif
	std::vector<Sort> sorts;

	[[nodiscard]] auto sortOf(Expr expression) const -> Sort
	{
		if (expression.owner_ != this || expression.id_ >= sorts.size())
			throw std::invalid_argument("symbolic expression belongs to another solver");
		return sorts[expression.id_];
	}

	void require(Expr expression, Kind kind, std::optional<std::uint32_t> width = {}) const
	{
		const auto sort = sortOf(expression);
		if (sort.kind != kind || (width && sort.width != *width))
			throw std::invalid_argument("symbolic expression sort mismatch");
	}

	[[nodiscard]] auto append(Sort sort
#if GENMC_HAVE_Z3
				 , z3::expr expression
#endif
	) -> Expr
	{
		const auto id = static_cast<std::uint32_t>(sorts.size());
		sorts.push_back(sort);
#if GENMC_HAVE_Z3
		expressions.push_back(std::move(expression));
#endif
		return Expr(this, id);
	}
};

Solver::Solver() : impl_(std::make_unique<Impl>()) {}
Solver::~Solver() = default;
Solver::Solver(Solver &&) noexcept = default;
auto Solver::operator=(Solver &&) noexcept -> Solver & = default;

auto Solver::backendAvailable() -> bool
{
#if GENMC_HAVE_Z3
	return true;
#else
	return false;
#endif
}

auto Solver::boolean(std::string_view name) -> Expr
{
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1},
			     impl_->context.bool_const(std::string(name).c_str()));
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::bitVector(std::string_view name, std::uint32_t width) -> Expr
{
	if (width == 0)
		throw std::invalid_argument("bit-vector width must be positive");
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::bitVector, width},
			     impl_->context.bv_const(std::string(name).c_str(), width));
#else
	return impl_->append({Impl::Kind::bitVector, width});
#endif
}

auto Solver::bitVectorConstant(std::uint64_t value, std::uint32_t width) -> Expr
{
	if (width == 0 || width > 64)
		throw std::invalid_argument("constant bit-vector width must be in [1,64]");
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::bitVector, width}, impl_->context.bv_val(value, width));
#else
	return impl_->append({Impl::Kind::bitVector, width});
#endif
}

auto Solver::logicalNot(Expr value) -> Expr
{
	impl_->require(value, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1}, !impl_->expressions[value.id_]);
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::allOf(std::span<const Expr> values) -> Expr
{
#if GENMC_HAVE_Z3
	auto result = impl_->context.bool_val(true);
#endif
	for (const auto value : values) {
		impl_->require(value, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
		result = result && impl_->expressions[value.id_];
#endif
	}
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1}, std::move(result));
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::anyOf(std::span<const Expr> values) -> Expr
{
#if GENMC_HAVE_Z3
	auto result = impl_->context.bool_val(false);
#endif
	for (const auto value : values) {
		impl_->require(value, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
		result = result || impl_->expressions[value.id_];
#endif
	}
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1}, std::move(result));
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::atMostOne(std::span<const Expr> values) -> Expr
{
	if (values.size() <= 1)
		return allOf(std::span<const Expr>{});
#if GENMC_HAVE_Z3
	z3::expr_vector expressions(impl_->context);
#endif
	for (const auto value : values) {
		impl_->require(value, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
		expressions.push_back(impl_->expressions[value.id_]);
#endif
	}
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1}, z3::atmost(expressions, 1));
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::implies(Expr premise, Expr consequence) -> Expr
{
	impl_->require(premise, Impl::Kind::boolean);
	impl_->require(consequence, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1},
			     z3::implies(impl_->expressions[premise.id_],
					 impl_->expressions[consequence.id_]));
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::equal(Expr lhs, Expr rhs) -> Expr
{
	const auto lhsSort = impl_->sortOf(lhs);
	const auto rhsSort = impl_->sortOf(rhs);
	if (lhsSort.kind != rhsSort.kind || lhsSort.width != rhsSort.width)
		throw std::invalid_argument("equality requires identical solver sorts");
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1},
			     impl_->expressions[lhs.id_] == impl_->expressions[rhs.id_]);
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::select(Expr condition, Expr whenTrue, Expr whenFalse) -> Expr
{
	impl_->require(condition, Impl::Kind::boolean);
	const auto trueSort = impl_->sortOf(whenTrue);
	const auto falseSort = impl_->sortOf(whenFalse);
	if (trueSort.kind != falseSort.kind || trueSort.width != falseSort.width)
		throw std::invalid_argument("select branches require identical solver sorts");
	const auto sort = trueSort;
#if GENMC_HAVE_Z3
	return impl_->append(sort, z3::ite(impl_->expressions[condition.id_],
					 impl_->expressions[whenTrue.id_],
					 impl_->expressions[whenFalse.id_]));
#else
	return impl_->append(sort);
#endif
}

#define GENMC_BV_BINARY(method, operation)                                                        \
	auto Solver::method(Expr lhs, Expr rhs) -> Expr                                            \
	{                                                                                          \
		const auto sort = impl_->sortOf(lhs);                                                 \
		impl_->require(lhs, Impl::Kind::bitVector);                                          \
		impl_->require(rhs, Impl::Kind::bitVector, sort.width);                               \
		/* NOLINTNEXTLINE */                                                                  \
		return impl_->append(sort                                                            \
			GENMC_Z3_ARGUMENT(operation));                                                 \
	}

#if GENMC_HAVE_Z3
#define GENMC_Z3_ARGUMENT(operation) , (impl_->expressions[lhs.id_] operation impl_->expressions[rhs.id_])
#else
#define GENMC_Z3_ARGUMENT(operation)
#endif
GENMC_BV_BINARY(bitVectorAdd, +)
GENMC_BV_BINARY(bitVectorSub, -)
GENMC_BV_BINARY(bitVectorAnd, &)
GENMC_BV_BINARY(bitVectorOr, |)
GENMC_BV_BINARY(bitVectorXor, ^)
#undef GENMC_BV_BINARY
#undef GENMC_Z3_ARGUMENT

auto Solver::bitVectorTruncate(Expr value, std::uint32_t width) -> Expr
{
	const auto source = impl_->sortOf(value);
	impl_->require(value, Impl::Kind::bitVector);
	if (width == 0 || width >= source.width)
		throw std::invalid_argument("truncate width must be smaller and positive");
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::bitVector, width},
			     impl_->expressions[value.id_].extract(width - 1, 0));
#else
	return impl_->append({Impl::Kind::bitVector, width});
#endif
}

auto Solver::bitVectorZeroExtend(Expr value, std::uint32_t width) -> Expr
{
	const auto source = impl_->sortOf(value);
	impl_->require(value, Impl::Kind::bitVector);
	if (width <= source.width)
		throw std::invalid_argument("zero-extend width must be larger");
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::bitVector, width},
			     z3::zext(impl_->expressions[value.id_], width - source.width));
#else
	return impl_->append({Impl::Kind::bitVector, width});
#endif
}

auto Solver::bitVectorSignExtend(Expr value, std::uint32_t width) -> Expr
{
	const auto source = impl_->sortOf(value);
	impl_->require(value, Impl::Kind::bitVector);
	if (width <= source.width)
		throw std::invalid_argument("sign-extend width must be larger");
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::bitVector, width},
			     z3::sext(impl_->expressions[value.id_], width - source.width));
#else
	return impl_->append({Impl::Kind::bitVector, width});
#endif
}

auto Solver::unsignedLess(Expr lhs, Expr rhs) -> Expr
{
	const auto width = impl_->sortOf(lhs).width;
	impl_->require(lhs, Impl::Kind::bitVector);
	impl_->require(rhs, Impl::Kind::bitVector, width);
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1},
			     z3::ult(impl_->expressions[lhs.id_], impl_->expressions[rhs.id_]));
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

auto Solver::signedLess(Expr lhs, Expr rhs) -> Expr
{
	const auto width = impl_->sortOf(lhs).width;
	impl_->require(lhs, Impl::Kind::bitVector);
	impl_->require(rhs, Impl::Kind::bitVector, width);
#if GENMC_HAVE_Z3
	return impl_->append({Impl::Kind::boolean, 1},
			     impl_->expressions[lhs.id_] < impl_->expressions[rhs.id_]);
#else
	return impl_->append({Impl::Kind::boolean, 1});
#endif
}

void Solver::constrain(Expr formula)
{
	impl_->require(formula, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
	impl_->solver.add(impl_->expressions[formula.id_]);
	impl_->model.reset();
#endif
}

auto Solver::check() -> CheckResult
{
#if GENMC_HAVE_Z3
	switch (impl_->solver.check()) {
	case z3::sat:
		impl_->model = impl_->solver.get_model();
		return CheckResult::sat;
	case z3::unsat:
		impl_->model.reset();
		return CheckResult::unsat;
	case z3::unknown:
		impl_->model.reset();
		return CheckResult::unknown;
	}
	return CheckResult::unknown;
#else
	return CheckResult::unavailable;
#endif
}

auto Solver::checkAssuming(std::span<const Expr> assumptions) -> AssumptionCheck
{
#if GENMC_HAVE_Z3
	z3::expr_vector expressions(impl_->context);
	for (const auto assumption : assumptions) {
		impl_->require(assumption, Impl::Kind::boolean);
		expressions.push_back(impl_->expressions[assumption.id_]);
	}
	switch (impl_->solver.check(expressions)) {
	case z3::sat:
		impl_->model = impl_->solver.get_model();
		return {.status = CheckResult::sat};
	case z3::unsat: {
		impl_->model.reset();
		AssumptionCheck result{.status = CheckResult::unsat};
		const auto core = impl_->solver.unsat_core();
		for (unsigned coreIndex = 0; coreIndex < core.size(); ++coreIndex) {
			for (std::size_t assumption = 0; assumption < assumptions.size(); ++assumption) {
				if (!z3::eq(core[coreIndex],
					    impl_->expressions[assumptions[assumption].id_]))
					continue;
				result.core.push_back(assumption);
				break;
			}
		}
		return result;
	}
	case z3::unknown:
		impl_->model.reset();
		return {.status = CheckResult::unknown};
	}
	return {.status = CheckResult::unknown};
#else
	for (const auto assumption : assumptions)
		impl_->require(assumption, Impl::Kind::boolean);
	return {.status = CheckResult::unavailable};
#endif
}

auto Solver::boolValue(Expr expression) const -> std::optional<bool>
{
	impl_->require(expression, Impl::Kind::boolean);
#if GENMC_HAVE_Z3
	if (!impl_->model)
		return std::nullopt;
	const auto value = impl_->model->eval(impl_->expressions[expression.id_], true);
	if (value.is_true())
		return true;
	if (value.is_false())
		return false;
#endif
	return std::nullopt;
}

auto Solver::bitVectorValue(Expr expression) const -> std::optional<std::uint64_t>
{
	impl_->require(expression, Impl::Kind::bitVector);
#if GENMC_HAVE_Z3
	if (!impl_->model || impl_->sorts[expression.id_].width > 64)
		return std::nullopt;
	std::uint64_t value{};
	if (impl_->model->eval(impl_->expressions[expression.id_], true).is_numeral_u64(value))
		return value;
#endif
	return std::nullopt;
}

} /* namespace genmc::symbolic */
