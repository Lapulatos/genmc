/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/FiniteSymbolicSolver.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace genmc::symbolic;

TEST(FiniteSymbolicSolverTest, UnavailableBackendFailsOpen)
{
	if (Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is available in this build";
	Solver solver;
	auto value = solver.boolean("value");
	solver.constrain(value);
	EXPECT_EQ(solver.check(), CheckResult::unavailable);
	EXPECT_EQ(solver.boolValue(value), std::nullopt);
}

TEST(FiniteSymbolicSolverTest, SolvesBooleanAndBitVectorConstraints)
{
	if (!Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	Solver solver;
	auto enabled = solver.boolean("enabled");
	auto x = solver.bitVector("x", 4);
	auto three = solver.bitVectorConstant(3, 4);
	auto two = solver.bitVectorConstant(2, 4);
	auto five = solver.bitVectorConstant(5, 4);
	solver.constrain(enabled);
	solver.constrain(solver.equal(x, three));
	solver.constrain(solver.equal(solver.bitVectorAdd(x, two), five));
	EXPECT_EQ(solver.check(), CheckResult::sat);
	EXPECT_EQ(solver.boolValue(enabled), true);
	EXPECT_EQ(solver.bitVectorValue(x), 3U);
}

TEST(FiniteSymbolicSolverTest, ProvesContradictoryGuardUnsatisfiable)
{
	if (!Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	Solver solver;
	auto guard = solver.boolean("guard");
	solver.constrain(guard);
	solver.constrain(solver.logicalNot(guard));
	EXPECT_EQ(solver.check(), CheckResult::unsat);
}

TEST(FiniteSymbolicSolverTest, EnforcesNativeAtMostOne)
{
	if (!Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	Solver solver;
	auto first = solver.boolean("first");
	auto second = solver.boolean("second");
	auto third = solver.boolean("third");
	const Expr choices[]{first, second, third};
	solver.constrain(solver.atMostOne(choices));
	solver.constrain(first);
	solver.constrain(second);
	EXPECT_EQ(solver.check(), CheckResult::unsat);
}

TEST(FiniteSymbolicSolverTest, ReturnsIndexedAssumptionUnsatCore)
{
	using namespace genmc::symbolic;
	if (!Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	Solver solver;
	const auto x = solver.boolean("assumption_core_x");
	const auto y = solver.boolean("assumption_core_y");
	const auto z = solver.boolean("assumption_core_z");
	const Expr forbidden[]{x, y};
	solver.constrain(solver.logicalNot(solver.allOf(forbidden)));
	const Expr assumptions[]{x, y, z};
	const auto result = solver.checkAssuming(assumptions);
	EXPECT_EQ(result.status, CheckResult::unsat);
	EXPECT_EQ(result.core, (std::vector<std::size_t>{0, 1}));
	EXPECT_EQ(solver.checkAssuming(std::span<const Expr>{assumptions}.first(1)).status,
		  CheckResult::sat);
}

TEST(FiniteSymbolicSolverTest, FourBitOperationsMatchExhaustiveOracle)
{
	if (!Solver::backendAvailable())
		GTEST_SKIP() << "Z3 is unavailable in this build";
	for (std::uint64_t lhs = 0; lhs < 16; ++lhs) {
		for (std::uint64_t rhs = 0; rhs < 16; ++rhs) {
			Solver solver;
			auto l = solver.bitVectorConstant(lhs, 4);
			auto r = solver.bitVectorConstant(rhs, 4);
			auto add = solver.bitVectorAdd(l, r);
			auto sub = solver.bitVectorSub(l, r);
			auto land = solver.bitVectorAnd(l, r);
			auto lor = solver.bitVectorOr(l, r);
			auto lxor = solver.bitVectorXor(l, r);
			auto ult = solver.unsignedLess(l, r);
			auto trunc = solver.bitVectorTruncate(add, 3);
			auto zext = solver.bitVectorZeroExtend(trunc, 6);
			auto sext = solver.bitVectorSignExtend(trunc, 6);
			EXPECT_EQ(solver.check(), CheckResult::sat);
			EXPECT_EQ(solver.bitVectorValue(add), (lhs + rhs) & 15U);
			EXPECT_EQ(solver.bitVectorValue(sub), (lhs - rhs) & 15U);
			EXPECT_EQ(solver.bitVectorValue(land), lhs & rhs);
			EXPECT_EQ(solver.bitVectorValue(lor), lhs | rhs);
			EXPECT_EQ(solver.bitVectorValue(lxor), lhs ^ rhs);
			EXPECT_EQ(solver.boolValue(ult), lhs < rhs);
			EXPECT_EQ(solver.bitVectorValue(trunc), (lhs + rhs) & 7U);
			EXPECT_EQ(solver.bitVectorValue(zext), (lhs + rhs) & 7U);
			const auto low = (lhs + rhs) & 7U;
			EXPECT_EQ(solver.bitVectorValue(sext), low < 4 ? low : low | 56U);
		}
	}
}

TEST(FiniteSymbolicSolverTest, RejectsCrossSolverAndSortMixing)
{
	Solver first;
	Solver second;
	auto firstBool = first.boolean("first");
	Expr secondBool;
	for (auto i = 0; i < 32; ++i)
		secondBool = second.boolean("second");
	auto bits = first.bitVector("bits", 8);
	EXPECT_THROW((void)first.implies(firstBool, secondBool), std::invalid_argument);
	EXPECT_THROW((void)first.equal(firstBool, bits), std::invalid_argument);
	EXPECT_THROW((void)first.select(firstBool, firstBool, secondBool), std::invalid_argument);
	EXPECT_THROW((void)first.bitVectorAdd(bits, secondBool), std::invalid_argument);
	EXPECT_THROW((void)first.unsignedLess(bits, secondBool), std::invalid_argument);
	EXPECT_THROW((void)first.equal(firstBool, Expr{}), std::invalid_argument);
}
