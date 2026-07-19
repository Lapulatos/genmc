/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_FINITE_SYMBOLIC_SOLVER_HPP
#define GENMC_FINITE_SYMBOLIC_SOLVER_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace genmc::symbolic {

/** Opaque, solver-owned expression handle. Handles from different solvers must not mix. */
class Expr {
public:
	Expr() = default;
	[[nodiscard]] auto valid() const -> bool { return owner_ != nullptr; }

private:
	friend class Solver;
	Expr(const void *owner, std::uint32_t id) : owner_(owner), id_(id) {}
	const void *owner_{};
	std::uint32_t id_{};
};

enum class CheckResult : std::uint8_t { sat, unsat, unknown, unavailable };

struct AssumptionCheck {
	CheckResult status{CheckResult::unavailable};
	/** Indices into the supplied assumption span. Populated only for UNSAT. */
	std::vector<std::size_t> core{};
};

/** Typed construction layer for the finite event-skeleton constraints. The implementation
 * is optional: without Z3, check() returns unavailable before any search-space change. */
class Solver {
public:
	Solver();
	~Solver();
	Solver(const Solver &) = delete;
	auto operator=(const Solver &) -> Solver & = delete;
	Solver(Solver &&) noexcept;
	auto operator=(Solver &&) noexcept -> Solver &;

	[[nodiscard]] static auto backendAvailable() -> bool;
	[[nodiscard]] auto boolean(std::string_view name) -> Expr;
	[[nodiscard]] auto bitVector(std::string_view name, std::uint32_t width) -> Expr;
	[[nodiscard]] auto bitVectorConstant(std::uint64_t value, std::uint32_t width) -> Expr;
	[[nodiscard]] auto logicalNot(Expr value) -> Expr;
	[[nodiscard]] auto allOf(std::span<const Expr> values) -> Expr;
	[[nodiscard]] auto anyOf(std::span<const Expr> values) -> Expr;
	[[nodiscard]] auto atMostOne(std::span<const Expr> values) -> Expr;
	[[nodiscard]] auto implies(Expr premise, Expr consequence) -> Expr;
	[[nodiscard]] auto equal(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto select(Expr condition, Expr whenTrue, Expr whenFalse) -> Expr;
	[[nodiscard]] auto bitVectorAdd(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto bitVectorSub(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto bitVectorAnd(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto bitVectorOr(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto bitVectorXor(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto bitVectorTruncate(Expr value, std::uint32_t width) -> Expr;
	[[nodiscard]] auto bitVectorZeroExtend(Expr value, std::uint32_t width) -> Expr;
	[[nodiscard]] auto bitVectorSignExtend(Expr value, std::uint32_t width) -> Expr;
	[[nodiscard]] auto unsignedLess(Expr lhs, Expr rhs) -> Expr;
	[[nodiscard]] auto signedLess(Expr lhs, Expr rhs) -> Expr;

	void constrain(Expr formula);
	[[nodiscard]] auto check() -> CheckResult;
	/** Check under temporary Boolean assumptions and return Z3's UNSAT assumption core. */
	[[nodiscard]] auto checkAssuming(std::span<const Expr> assumptions) -> AssumptionCheck;
	[[nodiscard]] auto boolValue(Expr expression) const -> std::optional<bool>;
	[[nodiscard]] auto bitVectorValue(Expr expression) const -> std::optional<std::uint64_t>;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} /* namespace genmc::symbolic */

#endif
