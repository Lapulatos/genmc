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

#include "genmc/CAT/Frontend.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

/** Return the repository root using this source file's stable in-tree location. */
static auto repositoryRoot() -> std::filesystem::path
{
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

/** Create an isolated directory for include and diagnostic fixtures. */
static auto createFixtureDirectory(std::string_view name) -> std::filesystem::path
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	auto directory =
		std::filesystem::path(testing::TempDir()) /
		(std::string("genmc-cat-") + std::string(name) + "-" + std::to_string(nonce));
	std::filesystem::create_directories(directory);
	return directory;
}

/** Write one exact CAT fixture and return its path. */
static auto writeFixture(const std::filesystem::path &directory, std::string_view name,
			 std::string_view source) -> std::filesystem::path
{
	auto path = directory / name;
	std::ofstream output(path, std::ios::binary);
	output << source;
	output.close();
	return path;
}

/** Find the first diagnostic of a requested stable category. */
static auto findDiagnostic(const cat::ParseResult &result, cat::DiagnosticKind kind)
	-> const cat::Diagnostic *
{
	for (const auto &diagnostic : result.diagnostics) {
		if (diagnostic.kind == kind)
			return &diagnostic;
	}
	return nullptr;
}

} /* namespace */

/* The three frozen acceptance models exercise real multiline statements and comments. */
TEST(CatFrontendTest, ParsesBundledModels)
{
	for (const auto *name : {"sc.cat", "tso.cat", "pso.cat"}) {
		auto result = cat::Frontend().parseFile(repositoryRoot() / "models/cat" / name);
		ASSERT_TRUE(result.ok())
			<< name << ": "
			<< (result.diagnostics.empty() ? "" : result.diagnostics[0].format());
		EXPECT_FALSE(result.model->statements.empty());
	}
}

/* Every comment form and supported postfix/binary token reaches the syntax AST. */
TEST(CatFrontendTest, ParsesCompleteOperatorSurface)
{
	auto directory = createFixtureDirectory("operators");
	auto path = writeFixture(directory, "operators.cat", R"CAT(
# line comment
"Header" // second line comment
(* outer (* nested *) comment *)
let a = (R | W) & M \ IW
let b = [a] ; (po^-1)? | (rf+ ; co*)
let c = R * W
empty 0 as zero
irreflexive b as irrefl
acyclic b as acyclic-name
)CAT");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_TRUE(result.ok()) << (result.diagnostics.empty() ? ""
								: result.diagnostics[0].format());
	EXPECT_EQ(result.model->name, "Header");
	EXPECT_EQ(result.model->statements.size(), 6U);
	std::filesystem::remove_all(directory);
}

/* Precedence produces union(composition(identifier, intersection(...))) as frozen. */
TEST(CatFrontendTest, AppliesFrozenPrecedence)
{
	auto directory = createFixtureDirectory("precedence");
	auto path = writeFixture(directory, "precedence.cat",
				 "P\nlet x = R | po ; rf \\ co & (W * M)\nempty x\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_TRUE(result.ok());
	const auto &root = *result.model->statements[0].expression;
	EXPECT_EQ(root.kind, cat::Expression::Kind::Union);
	ASSERT_EQ(root.operands.size(), 2U);
	EXPECT_EQ(root.operands[1]->kind, cat::Expression::Kind::Composition);
	EXPECT_EQ(root.operands[1]->operands[1]->kind, cat::Expression::Kind::Difference);
	std::filesystem::remove_all(directory);
}

/* Relative includes expand in place using the including file, not process cwd. */
TEST(CatFrontendTest, ExpandsRelativeInclude)
{
	auto directory = createFixtureDirectory("include");
	writeFixture(directory, "fragment.cat", "let imported = po | rf\n");
	auto root = writeFixture(directory, "root.cat",
				 "IncludeModel\ninclude \"fragment.cat\"\nacyclic imported\n");

	auto result = cat::Frontend().parseFile(root);

	ASSERT_TRUE(result.ok());
	ASSERT_EQ(result.model->statements.size(), 2U);
	EXPECT_EQ(result.model->statements[0].name, "imported");
	std::filesystem::remove_all(directory);
}

/* Absolute includes name exactly one file and do not consult implicit search paths. */
TEST(CatFrontendTest, ExpandsAbsoluteInclude)
{
	auto directory = createFixtureDirectory("absolute-include");
	auto fragment = writeFixture(directory, "fragment.cat", "let imported = po\n");
	auto root =
		writeFixture(directory, "root.cat",
			     "Absolute\ninclude \"" + fragment.string() + "\"\nempty imported\n");

	auto result = cat::Frontend().parseFile(root);

	ASSERT_TRUE(result.ok());
	EXPECT_EQ(result.model->statements.size(), 2U);
	std::filesystem::remove_all(directory);
}

/* Active-stack detection reports a dedicated include error for indirect cycles. */
TEST(CatFrontendTest, RejectsIncludeCycle)
{
	auto directory = createFixtureDirectory("cycle");
	writeFixture(directory, "a.cat", "include \"b.cat\"\n");
	writeFixture(directory, "b.cat", "include \"a.cat\"\n");
	auto root = writeFixture(directory, "root.cat", "Cycle\ninclude \"a.cat\"\n");

	auto result = cat::Frontend().parseFile(root);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Include);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_NE(diagnostic->message.find("include cycle"), std::string::npos);
	EXPECT_NE(diagnostic->message.find("a.cat"), std::string::npos);
	EXPECT_NE(diagnostic->message.find("b.cat"), std::string::npos);
	EXPECT_NE(diagnostic->message.find(":1:9"), std::string::npos);
	std::filesystem::remove_all(directory);
}

/* A missing relative include is diagnosed before any program compilation can begin. */
TEST(CatFrontendTest, RejectsMissingInclude)
{
	auto directory = createFixtureDirectory("missing-include");
	auto root = writeFixture(directory, "root.cat", "Missing\ninclude \"absent.cat\"\n");

	auto result = cat::Frontend().parseFile(root);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Io);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_EQ(diagnostic->span.begin.line, 2U);
	EXPECT_EQ(diagnostic->span.begin.column, 9U);
	std::filesystem::remove_all(directory);
}

/* Malformed binary expressions retain the exact operator source location. */
TEST(CatFrontendTest, ReportsExactParseLocation)
{
	auto directory = createFixtureDirectory("location");
	auto path = writeFixture(directory, "bad.cat", "Bad\nacyclic po |\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Parse);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_EQ(diagnostic->span.begin.file, std::filesystem::canonical(path));
	EXPECT_EQ(diagnostic->span.begin.line, 3U);
	EXPECT_EQ(diagnostic->span.begin.column, 1U);
	EXPECT_NE(diagnostic->format().find(":3:1: parse:"), std::string::npos);
	std::filesystem::remove_all(directory);
}

/* A missing left operand is diagnosed without dereferencing a partial syntax node. */
TEST(CatFrontendTest, RejectsMissingLeftOperand)
{
	auto directory = createFixtureDirectory("left-operand");
	auto path = writeFixture(directory, "left.cat", "Left\nacyclic | po\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Parse);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_EQ(diagnostic->span.begin.line, 2U);
	EXPECT_EQ(diagnostic->span.begin.column, 9U);
	std::filesystem::remove_all(directory);
}

/* Recovery at statement keywords reports independent top-level errors in one pass. */
TEST(CatFrontendTest, RecoversAtStatementBoundary)
{
	auto directory = createFixtureDirectory("recovery");
	auto path = writeFixture(directory, "recovery.cat", "Recovery\nacyclic | po\nempty & rf\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	std::size_t parseErrors{};
	for (const auto &diagnostic : result.diagnostics)
		parseErrors += diagnostic.kind == cat::DiagnosticKind::Parse;
	EXPECT_GE(parseErrors, 2U);
	std::filesystem::remove_all(directory);
}

/* Unterminated nested comments are lexical errors at their opening delimiter. */
TEST(CatFrontendTest, ReportsUnterminatedComment)
{
	auto directory = createFixtureDirectory("comment");
	auto path = writeFixture(directory, "comment.cat", "Comment\n(* missing end\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Lex);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_EQ(diagnostic->span.begin.line, 2U);
	EXPECT_EQ(diagnostic->span.begin.column, 1U);
	std::filesystem::remove_all(directory);
}

/* Escaped headers, universe constants, and trailing-apostrophe identifiers are lexical. */
TEST(CatFrontendTest, ParsesEscapesUniverseAndApostrophe)
{
	auto directory = createFixtureDirectory("lexical-details");
	auto path = writeFixture(directory, "details.cat",
				 "\"Escaped\\nHeader\"\nlet x' = _\nempty x' as universe-empty\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_TRUE(result.ok());
	EXPECT_EQ(result.model->name, "Escaped\nHeader");
	EXPECT_EQ(result.model->statements[0].name, "x'");
	EXPECT_EQ(result.model->statements[0].expression->kind, cat::Expression::Kind::Universe);
	std::filesystem::remove_all(directory);
}

/* Unknown string escapes are rejected instead of receiving platform-dependent meaning. */
TEST(CatFrontendTest, RejectsInvalidStringEscape)
{
	auto directory = createFixtureDirectory("escape");
	auto path = writeFixture(directory, "escape.cat", "\"bad\\q\"\nempty 0\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Lex);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_EQ(diagnostic->span.begin.line, 1U);
	EXPECT_EQ(diagnostic->span.begin.column, 1U);
	std::filesystem::remove_all(directory);
}

/* Invalid UTF-8 receives a lexical diagnostic at the offending byte. */
TEST(CatFrontendTest, RejectsInvalidUtf8)
{
	auto directory = createFixtureDirectory("utf8");
	std::string source = "Utf8\nlet x = po\n";
	source.push_back(static_cast<char>(0xFF));
	auto path = writeFixture(directory, "utf8.cat", source);

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	const auto *diagnostic = findDiagnostic(result, cat::DiagnosticKind::Lex);
	ASSERT_NE(diagnostic, nullptr);
	EXPECT_EQ(diagnostic->span.begin.line, 3U);
	EXPECT_EQ(diagnostic->span.begin.column, 1U);
	std::filesystem::remove_all(directory);
}

/* Recognizable full-CAT recursion is rejected as unsupported, not malformed syntax. */
TEST(CatFrontendTest, ClassifiesRecursiveBindingAsUnsupported)
{
	auto directory = createFixtureDirectory("unsupported");
	auto path = writeFixture(directory, "unsupported.cat", "Unsupported\nlet rec x = po\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	EXPECT_NE(findDiagnostic(result, cat::DiagnosticKind::Unsupported), nullptr);
	std::filesystem::remove_all(directory);
}

/* Known full-CAT complement syntax is unsupported rather than an unknown byte. */
TEST(CatFrontendTest, ClassifiesComplementAsUnsupported)
{
	auto directory = createFixtureDirectory("unsupported-complement");
	auto path = writeFixture(directory, "complement.cat", "Unsupported\nacyclic ~po\n");

	auto result = cat::Frontend().parseFile(path);

	ASSERT_FALSE(result.ok());
	EXPECT_NE(findDiagnostic(result, cat::DiagnosticKind::Unsupported), nullptr);
	EXPECT_EQ(findDiagnostic(result, cat::DiagnosticKind::Lex), nullptr);
	std::filesystem::remove_all(directory);
}

/* Repeated model loading supplies a reproducible parser-throughput smoke measurement. */
TEST(CatFrontendTest, BenchmarkBundledModelParsing)
{
	constexpr std::size_t iterations = 300;
	const auto path = repositoryRoot() / "models/cat/pso.cat";
	const auto start = std::chrono::steady_clock::now();
	for (std::size_t i = 0; i < iterations; ++i)
		ASSERT_TRUE(cat::Frontend().parseFile(path).ok());
	const auto elapsed = std::chrono::steady_clock::now() - start;
	const auto seconds = std::chrono::duration<double>(elapsed).count();
	RecordProperty("files_per_second", static_cast<double>(iterations) / seconds);
	RecordProperty("iterations", iterations);
}
