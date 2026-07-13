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

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace cat {
namespace {

enum class TokenKind : std::uint8_t {
	Identifier,
	String,
	Zero,
	Let,
	Include,
	Acyclic,
	Irreflexive,
	Empty,
	As,
	Equal,
	Pipe,
	Semicolon,
	Backslash,
	Ampersand,
	Star,
	Plus,
	Question,
	Inverse,
	LeftParen,
	RightParen,
	LeftBracket,
	RightBracket,
	Unsupported,
	End
};

struct Token {
	TokenKind kind{TokenKind::End};
	SourceSpan span;
	std::string text{};
};

/** Root-only host-profile declaration extracted from a herd-compatible comment. */
struct HostProfileDeclaration {
	HostProfile profile{HostProfile::SC};
	SourceSpan span;
};

static auto categoryName(DiagnosticKind kind) -> std::string_view
{
	switch (kind) {
	case DiagnosticKind::Io:
		return "io";
	case DiagnosticKind::Lex:
		return "lex";
	case DiagnosticKind::Parse:
		return "parse";
	case DiagnosticKind::Include:
		return "include";
	case DiagnosticKind::Name:
		return "name";
	case DiagnosticKind::Type:
		return "type";
	case DiagnosticKind::Unsupported:
		return "unsupported";
	case DiagnosticKind::Note:
		return "note";
	}
	return "parse";
}

static auto sourceLineAt(std::string_view source, std::size_t offset) -> std::string
{
	const auto begin = source.rfind('\n', offset == 0 ? 0 : offset - 1);
	const auto lineBegin = begin == std::string_view::npos ? 0 : begin + 1;
	const auto end = source.find('\n', offset);
	return std::string(source.substr(lineBegin, end == std::string_view::npos
							    ? source.size() - lineBegin
							    : end - lineBegin));
}

/** Return the first byte that cannot participate in a canonical UTF-8 encoding. */
static auto invalidUtf8Offset(std::string_view source) -> std::optional<std::size_t>
{
	for (std::size_t i = 0; i < source.size();) {
		const auto lead = static_cast<unsigned char>(source[i]);
		if (lead < 0x80) {
			++i;
			continue;
		}
		std::size_t length{};
		std::uint32_t value{};
		if (lead >= 0xC2 && lead <= 0xDF) {
			length = 2;
			value = lead & 0x1F;
		} else if (lead >= 0xE0 && lead <= 0xEF) {
			length = 3;
			value = lead & 0x0F;
		} else if (lead >= 0xF0 && lead <= 0xF4) {
			length = 4;
			value = lead & 0x07;
		} else {
			return i;
		}
		if (i + length > source.size())
			return i;
		for (std::size_t j = 1; j < length; ++j) {
			const auto continuation = static_cast<unsigned char>(source[i + j]);
			if ((continuation & 0xC0) != 0x80)
				return i + j;
			value = (value << 6) | (continuation & 0x3F);
		}
		if ((length == 3 && value < 0x800) || (length == 4 && value < 0x10000) ||
		    (value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)
			return i;
		i += length;
	}
	return std::nullopt;
}

/** Translate a validated byte prefix to the line and column used in diagnostics. */
static auto locationAt(const std::filesystem::path &path, std::string_view source,
		       std::size_t offset) -> SourceLocation
{
	SourceLocation location{path, offset, 1, 1};
	for (std::size_t i = 0; i < offset; ++i) {
		if (source[i] == '\n') {
			++location.line;
			location.column = 1;
		} else {
			++location.column;
		}
	}
	return location;
}

/** Stateful linear lexer; one instance owns no source text and never outlives its input. */
class Lexer {
public:
	Lexer(const std::filesystem::path &path, std::string_view source,
	      std::vector<Diagnostic> &diagnostics,
	      std::optional<HostProfileDeclaration> *hostProfile = nullptr)
		: path_(path), source_(source), diagnostics_(diagnostics), hostProfile_(hostProfile)
	{}

	/** Tokenize the complete source, returning an explicit end token. */
	auto lex() -> std::vector<Token>
	{
		std::vector<Token> tokens;
		while (skipTrivia()) {
			if (atEnd())
				break;
			const auto begin = location();
			const auto ch = peek();
			if (isIdentifierStart(ch)) {
				tokens.push_back(lexIdentifier(begin));
			} else if (ch == '"') {
				tokens.push_back(lexString(begin));
			} else if (ch == '0') {
				advance();
				tokens.push_back({TokenKind::Zero, {begin, location()}, "0"});
			} else {
				tokens.push_back(lexOperator(begin));
			}
			hasTokens_ = true;
		}
		const auto end = location();
		tokens.push_back({TokenKind::End, {end, end}, {}});
		return tokens;
	}

private:
	/** Return whether an ASCII byte may start a Phase 1 identifier. */
	static auto isIdentifierStart(char ch) -> bool
	{
		return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
	}

	/** Return whether an ASCII byte may continue a Phase 1 identifier. */
	static auto isIdentifierContinue(char ch) -> bool
	{
		return isIdentifierStart(ch) || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
	}

	[[nodiscard]] auto atEnd(std::size_t lookahead = 0) const -> bool
	{
		return offset_ + lookahead >= source_.size();
	}

	[[nodiscard]] auto peek(std::size_t lookahead = 0) const -> char
	{
		return atEnd(lookahead) ? '\0' : source_[offset_ + lookahead];
	}

	[[nodiscard]] auto location() const -> SourceLocation
	{
		return {path_, offset_, line_, column_};
	}

	void advance()
	{
		if (atEnd())
			return;
		if (source_[offset_++] == '\n') {
			++line_;
			column_ = 1;
		} else {
			++column_;
		}
	}

	void diagnose(DiagnosticKind kind, SourceLocation begin, std::string message)
	{
		diagnostics_.push_back({kind,
					{std::move(begin), location()},
					std::move(message),
					sourceLineAt(source_, begin.offset)});
	}

	/**
	 * Decode an exact `@genmc host-profile NAME` block-comment payload.
	 *
	 * The declaration must be a leading root-file comment. Herd ignores it as
	 * ordinary CAT trivia, while GenMC obtains an explicit, filename-independent
	 * exploration profile. Other comments remain semantically inert.
	 */
	void parseHostProfile(std::size_t contentBegin, std::size_t contentEnd,
			      const SourceLocation &begin)
	{
		auto payload = source_.substr(contentBegin, contentEnd - contentBegin);
		const auto first = payload.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos || !payload.substr(first).starts_with("@genmc"))
			return;
		const auto last = payload.find_last_not_of(" \t\r\n");
		std::istringstream words(std::string(payload.substr(first, last - first + 1)));
		std::string marker;
		std::string key;
		std::string value;
		std::string extra;
		words >> marker >> key >> value >> extra;
		if (!hostProfile_) {
			diagnose(
				DiagnosticKind::Unsupported, begin,
				"GenMC host-profile metadata is only allowed in the root CAT file");
			return;
		}
		if (hasTokens_) {
			diagnose(DiagnosticKind::Parse, begin,
				 "GenMC host-profile metadata must precede the model header");
			return;
		}
		if (marker != "@genmc" || key != "host-profile" || value.empty() ||
		    !extra.empty()) {
			diagnose(DiagnosticKind::Unsupported, begin,
				 "expected '@genmc host-profile sc' or '@genmc host-profile tso'");
			return;
		}
		HostProfile profile{HostProfile::SC};
		if (value == "sc")
			profile = HostProfile::SC;
		else if (value == "tso")
			profile = HostProfile::TSO;
		else {
			diagnose(DiagnosticKind::Unsupported, begin,
				 "unsupported GenMC host profile '" + value + "'");
			return;
		}
		if (hostProfile_->has_value()) {
			diagnose(DiagnosticKind::Parse, begin,
				 "GenMC host-profile metadata may only be declared once");
			return;
		}
		*hostProfile_ = HostProfileDeclaration{profile, {begin, location()}};
	}

	/** Consume whitespace and the three comment forms, including nested block comments. */
	auto skipTrivia() -> bool
	{
		for (;;) {
			while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r' ||
					    peek() == '\n'))
				advance();
			if (peek() == '#' || (peek() == '/' && peek(1) == '/')) {
				while (!atEnd() && peek() != '\n')
					advance();
				continue;
			}
			if (peek() != '(' || peek(1) != '*')
				return true;

			const auto begin = location();
			advance();
			advance();
			const auto contentBegin = offset_;
			std::size_t depth = 1;
			std::size_t contentEnd = contentBegin;
			while (!atEnd() && depth != 0) {
				if (peek() == '(' && peek(1) == '*') {
					advance();
					advance();
					++depth;
				} else if (peek() == '*' && peek(1) == ')') {
					if (depth == 1)
						contentEnd = offset_;
					advance();
					advance();
					--depth;
				} else {
					advance();
				}
			}
			if (depth != 0) {
				diagnose(DiagnosticKind::Lex, begin, "unterminated block comment");
				return false;
			}
			parseHostProfile(contentBegin, contentEnd, begin);
		}
	}

	/** Consume one identifier or keyword beginning at @p begin. */
	auto lexIdentifier(SourceLocation begin) -> Token
	{
		const auto start = offset_;
		while (isIdentifierContinue(peek()))
			advance();
		if (peek() == '\'')
			advance();
		auto text = std::string(source_.substr(start, offset_ - start));
		static const std::unordered_set<std::string_view> unsupported{
			"assert",	"call",
			"classes",	"enum",
			"flag",		"forall",
			"from",		"fun",
			"instructions", "linearisations",
			"match",	"procedure",
			"scope",	"scopes",
			"show",		"tag",
			"tags",		"undefined_unless",
			"unshow",	"variant",
			"variants",	"with"};
		TokenKind kind = TokenKind::Identifier;
		if (text == "let")
			kind = TokenKind::Let;
		else if (text == "include")
			kind = TokenKind::Include;
		else if (text == "acyclic")
			kind = TokenKind::Acyclic;
		else if (text == "irreflexive")
			kind = TokenKind::Irreflexive;
		else if (text == "empty")
			kind = TokenKind::Empty;
		else if (text == "as")
			kind = TokenKind::As;
		else if (unsupported.contains(text))
			kind = TokenKind::Unsupported;
		return {kind, {begin, location()}, std::move(text)};
	}

	/** Decode one quoted string and diagnose invalid escapes at @p begin. */
	auto lexString(SourceLocation begin) -> Token
	{
		advance();
		std::string value;
		bool valid = true;
		while (!atEnd() && peek() != '"') {
			if (peek() == '\n' || peek() == '\r') {
				valid = false;
				break;
			}
			if (peek() != '\\') {
				value.push_back(peek());
				advance();
				continue;
			}
			advance();
			const auto escaped = peek();
			switch (escaped) {
			case '"':
			case '\\':
				value.push_back(escaped);
				break;
			case 'n':
				value.push_back('\n');
				break;
			case 'r':
				value.push_back('\r');
				break;
			case 't':
				value.push_back('\t');
				break;
			default:
				valid = false;
				break;
			}
			if (!atEnd())
				advance();
			if (!valid)
				break;
		}
		if (!valid || atEnd()) {
			diagnose(DiagnosticKind::Lex, begin,
				 valid ? "unterminated string"
				       : "invalid string escape or newline");
			while (!atEnd() && peek() != '\n' && peek() != '"')
				advance();
		} else {
			advance();
		}
		return {TokenKind::String, {begin, location()}, std::move(value)};
	}

	/** Consume one operator/delimiter, recognizing `^-1` atomically. */
	auto lexOperator(SourceLocation begin) -> Token
	{
		const auto ch = peek();
		advance();
		TokenKind kind{TokenKind::Unsupported};
		switch (ch) {
		case '=':
			kind = TokenKind::Equal;
			break;
		case '|':
			kind = TokenKind::Pipe;
			break;
		case ';':
			kind = TokenKind::Semicolon;
			break;
		case '\\':
			kind = TokenKind::Backslash;
			break;
		case '&':
			kind = TokenKind::Ampersand;
			break;
		case '*':
			kind = TokenKind::Star;
			break;
		case '+':
			if (peek() == '+') {
				advance();
				kind = TokenKind::Unsupported;
			} else {
				kind = TokenKind::Plus;
			}
			break;
		case '?':
			kind = TokenKind::Question;
			break;
		case '(':
			kind = TokenKind::LeftParen;
			break;
		case ')':
			kind = TokenKind::RightParen;
			break;
		case '[':
			kind = TokenKind::LeftBracket;
			break;
		case ']':
			kind = TokenKind::RightBracket;
			break;
		case '^':
			if (peek() == '-' && peek(1) == '1') {
				advance();
				advance();
				kind = TokenKind::Inverse;
				break;
			}
			diagnose(DiagnosticKind::Lex, begin, "expected '^-1' inverse operator");
			kind = TokenKind::Unsupported;
			break;
		case '~':
		case '{':
		case '}':
		case ',':
			kind = TokenKind::Unsupported;
			break;
		default:
			diagnose(DiagnosticKind::Lex, begin,
				 "unexpected character '" + std::string(1, ch) + "'");
			kind = TokenKind::Unsupported;
			break;
		}
		return {kind, {begin, location()}, std::string(1, ch)};
	}

	std::filesystem::path path_;
	std::string_view source_;
	std::vector<Diagnostic> &diagnostics_;
	std::optional<HostProfileDeclaration> *hostProfile_{};
	std::size_t offset_{};
	std::size_t line_{1};
	std::size_t column_{1};
	bool hasTokens_{};
};

class Loader;

/** Precedence parser for one root model or one included statement fragment. */
class Parser {
public:
	Parser(Loader &loader, std::filesystem::path path, std::string source,
	       std::vector<Token> tokens, std::vector<Diagnostic> &diagnostics)
		: loader_(loader), path_(std::move(path)), source_(std::move(source)),
		  tokens_(std::move(tokens)), diagnostics_(diagnostics)
	{}

	auto parseRoot() -> std::optional<Model>;
	auto parseFragment(std::vector<Statement> &statements) -> bool;

private:
	/** Return a token without advancing, clamping lookahead to the end token. */
	[[nodiscard]] auto current(std::size_t lookahead = 0) const -> const Token &
	{
		return tokens_[std::min(position_ + lookahead, tokens_.size() - 1)];
	}

	auto match(TokenKind kind) -> bool
	{
		if (current().kind != kind)
			return false;
		++position_;
		return true;
	}

	auto expect(TokenKind kind, std::string message) -> const Token *
	{
		if (current().kind == kind)
			return &tokens_[position_++];
		diagnose(DiagnosticKind::Parse, current().span, std::move(message));
		return nullptr;
	}

	void diagnose(DiagnosticKind kind, const SourceSpan &span, std::string message)
	{
		diagnostics_.push_back(
			{kind, span, std::move(message), sourceLineAt(source_, span.begin.offset)});
	}

	/** Parse and append statements until the current file's end token. */
	auto parseStatements(std::vector<Statement> &statements) -> bool;
	/** Parse one include, binding, or check and append its expanded result. */
	auto parseStatement(std::vector<Statement> &statements) -> bool;
	/** Enter the expression precedence hierarchy at its lowest, union level. */
	auto parseExpression() -> std::unique_ptr<Expression>;
	/** Parse each precedence level, returning null after a diagnosed error. */
	auto parseUnion() -> std::unique_ptr<Expression>;
	auto parseComposition() -> std::unique_ptr<Expression>;
	auto parseDifference() -> std::unique_ptr<Expression>;
	auto parseIntersection() -> std::unique_ptr<Expression>;
	auto parseProduct() -> std::unique_ptr<Expression>;
	auto parsePostfix() -> std::unique_ptr<Expression>;
	auto parsePrimary() -> std::unique_ptr<Expression>;
	/** Fold one left-associative binary precedence level into syntax nodes. */
	auto parseBinary(std::unique_ptr<Expression> lhs, Expression::Kind kind, TokenKind token,
			 auto &&parseRhs) -> std::unique_ptr<Expression>;
	/** Wrap @p operand in one postfix node whose range ends at @p end. */
	auto wrapUnary(std::unique_ptr<Expression> operand, Expression::Kind kind,
		       const SourceSpan &end) -> std::unique_ptr<Expression>;
	/** Distinguish Cartesian `*` from postfix closure using the following token. */
	static auto startsPrimary(TokenKind kind) -> bool;
	/** Return whether a token can restart parsing after a top-level error. */
	static auto startsStatement(TokenKind kind) -> bool;

	Loader &loader_;
	std::filesystem::path path_;
	std::string source_;
	std::vector<Token> tokens_;
	std::vector<Diagnostic> &diagnostics_;
	std::size_t position_{};
};

/** Per-call include loader owning the active stack and all accumulated diagnostics. */
class Loader {
public:
	/** Load the canonical root, returning no model when any diagnostic occurs. */
	auto load(const std::filesystem::path &path) -> ParseResult
	{
		ParseResult result;
		auto canonical = canonicalize(path, SourceSpan{{path, 0, 1, 1}, {path, 0, 1, 1}},
					      DiagnosticKind::Io);
		if (!canonical)
			return {std::nullopt, std::move(diagnostics_)};
		auto model = parseRoot(*canonical);
		if (diagnostics_.empty() && model)
			result.model = std::move(*model);
		result.diagnostics = std::move(diagnostics_);
		return result;
	}

	/** Resolve and expand one include while preserving its insertion position. */
	auto include(const std::filesystem::path &includingFile, const Token &pathToken,
		     std::vector<Statement> &statements) -> bool
	{
		auto requested = std::filesystem::path(pathToken.text);
		if (requested.is_relative())
			requested = includingFile.parent_path() / requested;
		auto canonical = canonicalize(requested, pathToken.span, DiagnosticKind::Io);
		if (!canonical)
			return false;
		const auto matchesPath = [&](const auto &frame) {
			return frame.path == *canonical;
		};
		if (std::ranges::find_if(active_, matchesPath) != active_.end()) {
			std::ostringstream message;
			message << "include cycle:";
			for (const auto &frame : active_) {
				message << " " << frame.path.string();
				if (frame.includeSite)
					message << ":" << frame.includeSite->begin.line << ":"
						<< frame.includeSite->begin.column;
				message << " ->";
			}
			message << " " << canonical->string() << ":" << pathToken.span.begin.line
				<< ":" << pathToken.span.begin.column;
			diagnostics_.push_back(
				{DiagnosticKind::Include, pathToken.span, message.str(), {}});
			return false;
		}
		return parseFragment(*canonical, pathToken.span, statements);
	}

private:
	/** One active file and the source site by which its parent included it. */
	struct IncludeFrame {
		std::filesystem::path path;
		std::optional<SourceSpan> includeSite;
	};

	/** Canonicalize a regular file or emit an I/O diagnostic at @p span. */
	auto canonicalize(const std::filesystem::path &path, const SourceSpan &span,
			  DiagnosticKind kind) -> std::optional<std::filesystem::path>
	{
		std::error_code ec;
		const auto canonical = std::filesystem::canonical(path, ec);
		if (ec || !std::filesystem::is_regular_file(canonical, ec)) {
			diagnostics_.push_back(
				{kind, span, "cannot read CAT file '" + path.string() + "'", {}});
			return std::nullopt;
		}
		return canonical;
	}

	/** Read a complete UTF-8 file or diagnose I/O/encoding failure. */
	auto read(const std::filesystem::path &path) -> std::optional<std::string>
	{
		std::ifstream input(path, std::ios::binary);
		if (!input.good()) {
			diagnostics_.push_back({DiagnosticKind::Io,
						{SourceLocation{path}, SourceLocation{path}},
						"cannot read CAT file '" + path.string() + "'",
						{}});
			return std::nullopt;
		}
		auto source = std::string(std::istreambuf_iterator<char>(input), {});
		if (const auto invalid = invalidUtf8Offset(source)) {
			auto begin = locationAt(path, source, *invalid);
			auto end = begin;
			++end.offset;
			++end.column;
			diagnostics_.push_back({DiagnosticKind::Lex,
						{begin, end},
						"invalid UTF-8 byte",
						sourceLineAt(source, *invalid)});
			return std::nullopt;
		}
		return source;
	}

	/** Parse a root file with a required model header under an active-stack frame. */
	auto parseRoot(const std::filesystem::path &path) -> std::optional<Model>
	{
		auto source = read(path);
		if (!source)
			return std::nullopt;
		active_.push_back({path, std::nullopt});
		std::optional<HostProfileDeclaration> hostProfile;
		Lexer lexer(path, *source, diagnostics_, &hostProfile);
		/* Tokenize before moving the backing string into Parser. Lexer holds a
		 * string_view, and function-argument evaluation order must not decide
		 * whether that view observes a moved-from string. */
		auto tokens = lexer.lex();
		Parser parser(*this, path, std::move(*source), std::move(tokens), diagnostics_);
		auto model = parser.parseRoot();
		if (model && hostProfile) {
			model->hostProfile = hostProfile->profile;
			model->hostProfileSpan = hostProfile->span;
		}
		active_.pop_back();
		return model;
	}

	/** Parse an included statement fragment and append it at the include site. */
	auto parseFragment(const std::filesystem::path &path, const SourceSpan &includeSite,
			   std::vector<Statement> &statements) -> bool
	{
		auto source = read(path);
		if (!source)
			return false;
		active_.push_back({path, includeSite});
		Lexer lexer(path, *source, diagnostics_);
		auto tokens = lexer.lex();
		Parser parser(*this, path, std::move(*source), std::move(tokens), diagnostics_);
		const auto success = parser.parseFragment(statements);
		active_.pop_back();
		return success;
	}

	std::vector<IncludeFrame> active_;
	std::vector<Diagnostic> diagnostics_;
};

auto Parser::parseRoot() -> std::optional<Model>
{
	if (current().kind != TokenKind::Identifier && current().kind != TokenKind::String) {
		diagnose(DiagnosticKind::Parse, current().span,
			 "expected model name as the first non-comment token");
		return std::nullopt;
	}
	Model model;
	model.name = current().text;
	model.nameSpan = current().span;
	++position_;
	parseStatements(model.statements);
	return model;
}

auto Parser::parseFragment(std::vector<Statement> &statements) -> bool
{
	return parseStatements(statements);
}

auto Parser::parseStatements(std::vector<Statement> &statements) -> bool
{
	bool success = true;
	while (current().kind != TokenKind::End) {
		if (parseStatement(statements))
			continue;
		success = false;
		/* Statements have explicit leading keywords, so skipping to the next
		 * such keyword cannot reinterpret the failed expression as valid input. */
		while (current().kind != TokenKind::End && !startsStatement(current().kind))
			++position_;
	}
	return success;
}

auto Parser::parseStatement(std::vector<Statement> &statements) -> bool
{
	if (match(TokenKind::Include)) {
		const auto *path =
			expect(TokenKind::String, "expected quoted path after 'include'");
		return path && loader_.include(path_, *path, statements);
	}
	if (match(TokenKind::Unsupported)) {
		diagnose(DiagnosticKind::Unsupported, tokens_[position_ - 1].span,
			 "CAT construct '" + tokens_[position_ - 1].text + "' is not supported");
		return false;
	}
	if (match(TokenKind::Let)) {
		if (current().kind == TokenKind::Identifier && current().text == "rec") {
			diagnose(DiagnosticKind::Unsupported, current().span,
				 "recursive CAT bindings are not supported in Phase 1");
			return false;
		}
		const auto begin = tokens_[position_ - 1].span.begin;
		const auto *name =
			expect(TokenKind::Identifier, "expected binding name after 'let'");
		if (!name || !expect(TokenKind::Equal, "expected '=' after binding name"))
			return false;
		auto expression = parseExpression();
		if (!expression)
			return false;
		statements.push_back({Statement::Kind::Let,
				      Statement::CheckKind::Acyclic,
				      {begin, expression->span.end},
				      name->text,
				      std::move(expression)});
		return true;
	}

	Statement::CheckKind checkKind;
	const auto begin = current().span.begin;
	if (match(TokenKind::Acyclic))
		checkKind = Statement::CheckKind::Acyclic;
	else if (match(TokenKind::Irreflexive))
		checkKind = Statement::CheckKind::Irreflexive;
	else if (match(TokenKind::Empty))
		checkKind = Statement::CheckKind::Empty;
	else {
		diagnose(DiagnosticKind::Parse, current().span,
			 "expected 'include', 'let', or a consistency check");
		return false;
	}
	auto expression = parseExpression();
	if (!expression)
		return false;
	std::string name;
	auto end = expression->span.end;
	if (match(TokenKind::As)) {
		const auto *nameToken =
			expect(TokenKind::Identifier, "expected check name after 'as'");
		if (!nameToken)
			return false;
		name = nameToken->text;
		end = nameToken->span.end;
	}
	statements.push_back({Statement::Kind::Check,
			      checkKind,
			      {begin, end},
			      std::move(name),
			      std::move(expression)});
	return true;
}

auto Parser::parseExpression() -> std::unique_ptr<Expression> { return parseUnion(); }

auto Parser::parseBinary(std::unique_ptr<Expression> lhs, Expression::Kind kind, TokenKind token,
			 auto &&parseRhs) -> std::unique_ptr<Expression>
{
	if (!lhs)
		return nullptr;
	while (match(token)) {
		auto rhs = parseRhs();
		if (!rhs) {
			diagnose(DiagnosticKind::Parse, tokens_[position_ - 1].span,
				 "expected expression after binary operator");
			return nullptr;
		}
		auto span = SourceSpan{lhs->span.begin, rhs->span.end};
		auto node = std::make_unique<Expression>();
		node->kind = kind;
		node->span = std::move(span);
		node->operands.push_back(std::move(lhs));
		node->operands.push_back(std::move(rhs));
		lhs = std::move(node);
	}
	return lhs;
}

auto Parser::parseUnion() -> std::unique_ptr<Expression>
{
	return parseBinary(parseComposition(), Expression::Kind::Union, TokenKind::Pipe,
			   [&] { return parseComposition(); });
}

auto Parser::parseComposition() -> std::unique_ptr<Expression>
{
	return parseBinary(parseDifference(), Expression::Kind::Composition, TokenKind::Semicolon,
			   [&] { return parseDifference(); });
}

auto Parser::parseDifference() -> std::unique_ptr<Expression>
{
	return parseBinary(parseIntersection(), Expression::Kind::Difference, TokenKind::Backslash,
			   [&] { return parseIntersection(); });
}

auto Parser::parseIntersection() -> std::unique_ptr<Expression>
{
	return parseBinary(parseProduct(), Expression::Kind::Intersection, TokenKind::Ampersand,
			   [&] { return parseProduct(); });
}

auto Parser::parseProduct() -> std::unique_ptr<Expression>
{
	return parseBinary(parsePostfix(), Expression::Kind::Product, TokenKind::Star,
			   [&] { return parsePostfix(); });
}

auto Parser::wrapUnary(std::unique_ptr<Expression> operand, Expression::Kind kind,
		       const SourceSpan &end) -> std::unique_ptr<Expression>
{
	auto node = std::make_unique<Expression>();
	node->kind = kind;
	node->span = {operand->span.begin, end.end};
	node->operands.push_back(std::move(operand));
	return node;
}

auto Parser::parsePostfix() -> std::unique_ptr<Expression>
{
	auto expression = parsePrimary();
	if (!expression)
		return nullptr;
	for (;;) {
		if (match(TokenKind::Inverse))
			expression = wrapUnary(std::move(expression), Expression::Kind::Inverse,
					       tokens_[position_ - 1].span);
		else if (match(TokenKind::Question))
			expression = wrapUnary(std::move(expression), Expression::Kind::Optional,
					       tokens_[position_ - 1].span);
		else if (match(TokenKind::Plus))
			expression = wrapUnary(std::move(expression),
					       Expression::Kind::TransitiveClosure,
					       tokens_[position_ - 1].span);
		else if (current().kind == TokenKind::Star && !startsPrimary(current(1).kind)) {
			++position_;
			expression = wrapUnary(std::move(expression),
					       Expression::Kind::ReflexiveTransitiveClosure,
					       tokens_[position_ - 1].span);
		} else {
			break;
		}
	}
	return expression;
}

auto Parser::startsPrimary(TokenKind kind) -> bool
{
	return kind == TokenKind::Identifier || kind == TokenKind::Zero ||
	       kind == TokenKind::LeftParen || kind == TokenKind::LeftBracket;
}

auto Parser::startsStatement(TokenKind kind) -> bool
{
	return kind == TokenKind::Include || kind == TokenKind::Let || kind == TokenKind::Acyclic ||
	       kind == TokenKind::Irreflexive || kind == TokenKind::Empty ||
	       kind == TokenKind::Unsupported;
}

auto Parser::parsePrimary() -> std::unique_ptr<Expression>
{
	const auto token = current();
	if (match(TokenKind::Identifier)) {
		auto node = std::make_unique<Expression>();
		node->kind = token.text == "_" ? Expression::Kind::Universe
					       : Expression::Kind::Identifier;
		node->span = token.span;
		node->name = token.text;
		return node;
	}
	if (match(TokenKind::Zero)) {
		auto node = std::make_unique<Expression>();
		node->kind = Expression::Kind::EmptyRelation;
		node->span = token.span;
		return node;
	}
	if (match(TokenKind::LeftParen)) {
		auto expression = parseExpression();
		const auto *end = expect(TokenKind::RightParen, "expected ')' after expression");
		if (!expression || !end)
			return nullptr;
		expression->span = {token.span.begin, end->span.end};
		return expression;
	}
	if (match(TokenKind::LeftBracket)) {
		auto expression = parseExpression();
		const auto *end = expect(TokenKind::RightBracket, "expected ']' after expression");
		if (!expression || !end)
			return nullptr;
		auto node = std::make_unique<Expression>();
		node->kind = Expression::Kind::Identity;
		node->span = {token.span.begin, end->span.end};
		node->operands.push_back(std::move(expression));
		return node;
	}
	diagnose(current().kind == TokenKind::Unsupported ? DiagnosticKind::Unsupported
							  : DiagnosticKind::Parse,
		 current().span, "expected CAT expression");
	return nullptr;
}

} /* namespace */

auto Diagnostic::format() const -> std::string
{
	std::ostringstream output;
	output << span.begin.file.string() << ":" << span.begin.line << ":" << span.begin.column
	       << ": " << categoryName(kind) << ": " << message;
	if (!sourceLine.empty())
		output << "\n" << sourceLine;
	return output.str();
}

auto Frontend::parseFile(const std::filesystem::path &path) const -> ParseResult
{
	return Loader{}.load(path);
}

} /* namespace cat */
