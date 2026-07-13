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

#include "genmc/Verification/Config.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

/**
 * Return whether validation produced an error containing @p fragment.
 *
 * @param status Result returned by `Config::validate`.
 * @param fragment Stable diagnostic fragment expected by the test.
 * @return True when any reported error contains the requested fragment.
 */
static auto hasError(const ValidationStatus &status, std::string_view fragment) -> bool
{
	const auto *errors = std::get_if<ConfigErrorList>(&status);
	return errors && std::ranges::any_of(*errors, [&](const auto &error) {
		       return error.find(fragment) != std::string::npos;
	       });
}

/**
 * Create a small readable CAT file in GoogleTest's temporary directory.
 *
 * Each caller supplies a unique filename so independently discovered tests do
 * not race when CTest runs them in parallel.
 *
 * @param filename Unique basename for the temporary model.
 * @return Path to the created model file.
 */
static auto createModelFile(std::string_view filename) -> std::filesystem::path
{
	auto path = std::filesystem::path(testing::TempDir()) / filename;
	std::ofstream output(path);
	output << "SC\nacyclic po | rf | fr | co as sc\n";
	output.close();
	return path;
}

} /* namespace */

/* A readable path is canonicalized before the deliberate Phase-1.1 boundary. */
TEST(ConfigModelFileTest, CanonicalizesReadableFile)
{
	auto path = createModelFile("genmc-config-readable.cat");
	Config config;
	config.modelFile = path.parent_path() / "." / path.filename();
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "CAT model parsed"));
	ASSERT_TRUE(config.modelFile.has_value());
	EXPECT_EQ(*config.modelFile, std::filesystem::canonical(path));
	std::filesystem::remove(path);
}

/* A CAT file and an explicitly selected built-in model are unambiguously conflicting. */
TEST(ConfigModelFileTest, RejectsExplicitBuiltInModel)
{
	auto path = createModelFile("genmc-config-conflict.cat");
	Config config;
	config.modelFile = path;
	config.modelExplicit = true;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "cannot be combined with an explicit built-in"));
	EXPECT_FALSE(hasError(status, "CAT model parsed"));
	std::filesystem::remove(path);
}

/* Repeated scalar options are rejected instead of silently accepting the final path. */
TEST(ConfigModelFileTest, RejectsDuplicateOption)
{
	auto path = createModelFile("genmc-config-duplicate.cat");
	Config config;
	config.modelFile = path;
	config.modelFileOccurrences = 2;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "--model-file may only be specified once"));
	EXPECT_FALSE(hasError(status, "CAT model parsed"));
	std::filesystem::remove(path);
}

/* Missing input is rejected before the parser or program compiler can run. */
TEST(ConfigModelFileTest, RejectsMissingFile)
{
	Config config;
	config.modelFile =
		std::filesystem::path(testing::TempDir()) / "genmc-config-definitely-missing.cat";
	std::filesystem::remove(*config.modelFile);
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "CAT model file does not exist"));
	EXPECT_FALSE(hasError(status, "CAT model parsed"));
}

/* Directories are not accepted as model inputs even when they are readable. */
TEST(ConfigModelFileTest, RejectsDirectory)
{
	Config config;
	config.modelFile = std::filesystem::path(testing::TempDir());
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, "CAT model file is not a regular file"));
	EXPECT_FALSE(hasError(status, "CAT model parsed"));
}

/* Syntax errors from the CAT frontend retain their file/line/column through Config. */
TEST(ConfigModelFileTest, ReportsParserDiagnosticBeforeExecution)
{
	auto path = std::filesystem::path(testing::TempDir()) / "genmc-config-invalid.cat";
	std::ofstream output(path);
	output << "Broken\nacyclic po |\n";
	output.close();
	Config config;
	config.modelFile = path;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(hasError(status, path.string() + ":3:1: parse:"));
	EXPECT_FALSE(hasError(status, "CAT model parsed"));
	std::filesystem::remove(path);
}

/* Existing invocations without a CAT file retain the legacy validation path. */
TEST(ConfigModelFileTest, PreservesLegacyConfiguration)
{
	Config config;
	std::vector<std::string> warnings;

	auto status = config.validate(warnings);

	EXPECT_TRUE(std::holds_alternative<std::monostate>(status));
	EXPECT_FALSE(config.modelFile.has_value());
}
