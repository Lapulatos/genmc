#!/usr/bin/env python3
"""Fail-closed source rewrites for the sound GenMC compatibility lane."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re


ASSUME_DEFINITION = re.compile(
    r"void\s+assume_abort_if_not\s*\(\s*int\s+cond\s*\)\s*"
    r"\{\s*if\s*\(\s*!\s*cond\s*\)\s*\{\s*abort\s*\(\s*\)\s*;\s*\}\s*\}",
    re.MULTILINE,
)
ASSUME_CALL = re.compile(r"\bassume_abort_if_not\s*\(")
DIRECT_REACH_ERROR = re.compile(
    r"void\s+reach_error\s*\(\s*\)\s*\{\s*"
    r"__assert_fail\s*\(\s*(?P<assertion>\"(?:[^\"\\]|\\.)*\")\s*,\s*"
    r"(?P<file>\"(?:[^\"\\]|\\.)*\")\s*,\s*(?P<line>[0-9]+)\s*,\s*"
    r"\"(?:[^\"\\]|\\.)*\"\s*\)\s*;\s*\}",
    re.MULTILINE,
)
DIRECT_ASSERT_REACH_ERROR = re.compile(
    r"void\s+reach_error\s*\(\s*\)\s*\{\s*assert\s*\(\s*0\s*\)\s*;\s*\}",
    re.MULTILINE,
)
TRAILING_ABORT_AFTER_REACH_ERROR = re.compile(
    r"(?P<reach>\breach_error\s*\(\s*\)\s*;\s*)abort\s*\(\s*\)\s*;"
)
NONDET_DECLARATION = re.compile(
    r"^\s*(?:extern\s+)?(?:_Bool|bool|uint|ulong|char|(?:signed|unsigned)\s+char|"
    r"(?:signed|unsigned)(?:\s+(?:short|int|long(?:\s+long)?))(?:\s+int)?|"
    r"short(?:\s+int)?|int|long(?:\s+long)?(?:\s+int)?)\s+"
    r"__VERIFIER_nondet[A-Za-z0-9_]*\s*\([^;{}]*\)\s*;\s*$",
    re.MULTILINE,
)
NONDET_USE = re.compile(r"\b__VERIFIER_nondet[A-Za-z0-9_]*\s*\(")
NONDET_NAME = re.compile(r"\b(__VERIFIER_nondet[A-Za-z0-9_]*)\s*\(")
NONDET_CALL = re.compile(
    r"\b(?P<name>__VERIFIER_nondet[A-Za-z0-9_]*)\s*\(\s*(?:void\s*)?\)"
)
MALLOC_CALL = re.compile(r"\bmalloc\s*\(")
QUOTED_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
PTHREAD_CREATE_CALL = re.compile(r"\bpthread_create\s*\(")
PTHREAD_CREATE_NULL_ATTR = re.compile(
    r"\bpthread_create\s*\(\s*[^,]+,\s*(?:0|NULL)\s*,"
)
PTHREAD_JOIN_CALL = re.compile(r"\bpthread_join\s*\(")
SIMPLE_ATOMIC_FUNCTION = re.compile(
    r"(?P<header>\bvoid\s+__VERIFIER_atomic_(?!begin\b|end\b)"
    r"[A-Za-z0-9_]*\s*\([^{};]*\)\s*\{)(?P<body>[^{}]*)(?P<close>\})",
    re.MULTILINE,
)
STANDALONE_PRINTF = re.compile(r"(?m)^(?P<indent>\s*)printf\([^;\n]*\)\s*;")
STANDALONE_SLEEP = re.compile(
    r"(?m)^(?P<indent>\s*)sleep\s*\([^;\n]*\)\s*;"
)
INLINE_DEFINITION = re.compile(
    r"(?m)^(?P<indent>\s*)inline(?P<rest>\s+[^;{]*\{)"
)
JOINABLE_ATTR_CALL = re.compile(
    r"\bpthread_attr_(?:init|destroy)\s*\([^;()]*\)|"
    r"\bpthread_attr_setdetachstate\s*\([^;()]*,\s*PTHREAD_CREATE_JOINABLE\s*\)"
)

MANUAL_PTHREAD_DECLARATIONS = (
    "extern long __VERIFIER_thread_create(const void *, void *(*)(void *), void *);\n"
    "extern void *__VERIFIER_thread_join(long);\n"
)

MANUAL_GENMC_DECLARATIONS = (
    "extern int __VERIFIER_nondet_int(void);\n"
    "extern void __VERIFIER_assume_internal(_Bool, char);\n"
    "extern void __VERIFIER_assert_fail(const char *, const char *, int);\n"
)

MANUAL_ATOMIC_RUNTIME = r"""
static __VERIFIER_mutex_t __genmc_svcomp_atomic_mutex = __VERIFIER_MUTEX_INITIALIZER;

void __VERIFIER_atomic_begin(void)
{
	(void)__VERIFIER_mutex_lock(&__genmc_svcomp_atomic_mutex);
}

void __VERIFIER_atomic_end(void)
{
	(void)__VERIFIER_mutex_unlock(&__genmc_svcomp_atomic_mutex);
}
""".lstrip()

FIXED_SEED_NONDET_CASTS = {
    "__VERIFIER_nondet_bool": "((_Bool)__VERIFIER_nondet_int())",
    "__VERIFIER_nondet_uint": "((unsigned int)__VERIFIER_nondet_int())",
    "__VERIFIER_nondet_ulong": "((unsigned long)__VERIFIER_nondet_int())",
    "__VERIFIER_nondet_long": "((long)__VERIFIER_nondet_int())",
    "__VERIFIER_nondet_longlong": "((long long)__VERIFIER_nondet_int())",
    "__VERIFIER_nondet_char": "((char)__VERIFIER_nondet_int())",
    "__VERIFIER_nondet_uchar": "((unsigned char)__VERIFIER_nondet_int())",
}
SUPPORTED_FIXED_SEED_NONDET = {"__VERIFIER_nondet_int", *FIXED_SEED_NONDET_CASTS}

@dataclass(frozen=True)
class RewriteResult:
    changed: bool
    rules: tuple[str, ...]
    text: str


@dataclass(frozen=True)
class TreeRewriteResult:
    source: Path
    rules: tuple[str, ...]
    unsupported_reason: str | None


def local_include_closure(source: Path) -> list[Path]:
    pending = [source]
    found: list[Path] = []
    seen: set[Path] = set()
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        seen.add(path)
        found.append(path)
        text = path.read_text(encoding="utf-8", errors="ignore")
        for name in QUOTED_INCLUDE.findall(text):
            dependency = path.parent / name
            if dependency.is_file():
                pending.append(dependency)
    return found


def rewrite_tree(source: Path, benchmark_root: Path, output_root: Path) -> TreeRewriteResult:
    closure = local_include_closure(source)
    texts = {
        dependency: dependency.read_text(encoding="utf-8", errors="ignore")
        for dependency in closure
    }
    nondet_names = {
        name
        for text in texts.values()
        for name in NONDET_NAME.findall(NONDET_DECLARATION.sub("", text))
    }
    unsupported_nondet = nondet_names - SUPPORTED_FIXED_SEED_NONDET
    if unsupported_nondet:
        return TreeRewriteResult(source, (), "data-nondeterminism")
    closure_has_assume_definition = any(
        ASSUME_DEFINITION.search(text) for text in texts.values()
    )
    rewrites = {
        dependency: rewrite_text(
            text,
            provide_missing_assume=(dependency == source and not closure_has_assume_definition),
            add_runtime_include=(
                dependency.suffix != ".i"
                and not re.search(r"\btypedef\s+_Bool\s+bool\s*;", text)
            ),
            rewrite_preprocessed_runtime=dependency.suffix == ".i",
        )
        for dependency, text in texts.items()
    }
    rules = tuple(rule for rewrite in rewrites.values() for rule in rewrite.rules)
    if not rules:
        return TreeRewriteResult(source, (), None)
    rewritten_source = output_root / source.relative_to(benchmark_root)
    for dependency, rewrite in rewrites.items():
        relative = dependency.relative_to(benchmark_root)
        rewritten_dependency = output_root / relative
        rewritten_dependency.parent.mkdir(parents=True, exist_ok=True)
        rewritten_dependency.write_text(rewrite.text, encoding="utf-8")
    return TreeRewriteResult(
        rewritten_source,
        rules,
        None,
    )


def _split_call_arguments(text: str, open_paren: int) -> tuple[list[str], int]:
    arguments: list[str] = []
    start = open_paren + 1
    depth = 0
    quote: str | None = None
    escaped = False
    for index in range(start, len(text)):
        char = text[index]
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in "\"'":
            quote = char
        elif char == "(":
            depth += 1
        elif char == ")":
            if depth == 0:
                arguments.append(text[start:index].strip())
                return arguments, index + 1
            depth -= 1
        elif char == "," and depth == 0:
            arguments.append(text[start:index].strip())
            start = index + 1
    raise ValueError("unterminated manual pthread call")


def _rewrite_manual_pthread_calls(text: str) -> tuple[str, int]:
    pattern = re.compile(r"\b(pthread_create|pthread_join)\s*\(")
    output: list[str] = []
    cursor = 0
    count = 0
    while match := pattern.search(text, cursor):
        line_start = text.rfind("\n", 0, match.start()) + 1
        if text[line_start:match.start()].lstrip().startswith("extern"):
            output.append(text[cursor:match.end()])
            cursor = match.end()
            continue
        arguments, end = _split_call_arguments(text, text.find("(", match.start()))
        if match.group(1) == "pthread_create":
            null_attr = len(arguments) == 4 and re.fullmatch(
                r"(?:\([^()]*\)\s*)*(?:0|NULL)", arguments[1]
            )
            if len(arguments) != 4 or not null_attr:
                raise ValueError("manual pthread_create is not a null-attr call")
            target = arguments[0]
            if not target.startswith("&"):
                raise ValueError("manual pthread_create target is not an address")
            target = target[1:].strip()
            replacement = (
                f"(({target}) = (__typeof__({target}))__VERIFIER_thread_create("
                f"0, {arguments[2]}, {arguments[3]}), 0)"
            )
        else:
            if len(arguments) != 2:
                raise ValueError("manual pthread_join is not a two-argument call")
            replacement = (
                "({ void *__genmc_value = __VERIFIER_thread_join((long)("
                f"{arguments[0]})); void **__genmc_return = (void **)({arguments[1]}); "
                "if (__genmc_return) *__genmc_return = __genmc_value; 0; })"
            )
        output.append(text[cursor:match.start()])
        output.append(replacement)
        cursor = end
        count += 1
    output.append(text[cursor:])
    return "".join(output), count


def rewrite_text(
    text: str,
    *,
    provide_missing_assume: bool = True,
    add_runtime_include: bool = True,
    rewrite_preprocessed_runtime: bool = False,
) -> RewriteResult:
    without_nondet_declarations = NONDET_DECLARATION.sub("", text)
    nondet_names = set(NONDET_NAME.findall(without_nondet_declarations))
    unsupported_nondet = nondet_names - SUPPORTED_FIXED_SEED_NONDET
    if unsupported_nondet:
        names = ", ".join(sorted(unsupported_nondet))
        raise ValueError(f"unsupported fixed-seed nondeterminism: {names}")

    rules: list[str] = []
    declaration_spans = [match.span() for match in NONDET_DECLARATION.finditer(text)]

    def replace_fixed_seed_nondet(match: re.Match[str]) -> str:
        if any(start <= match.start() < end for start, end in declaration_spans):
            return match.group(0)
        return FIXED_SEED_NONDET_CASTS.get(match.group("name"), match.group(0))

    rewritten, nondet_count = NONDET_CALL.subn(replace_fixed_seed_nondet, text)
    if any(name in FIXED_SEED_NONDET_CASTS for name in nondet_names):
        rules.append("fixed-seed-nondet-cast")

    manual_malloc_declaration = ""
    if rewrite_preprocessed_runtime or (
        add_runtime_include
        and re.search(r"(?m)^\s*extern\s+void\s*\*\s*malloc\s*\(", rewritten)
    ):
        malloc_declaration_matches = list(
            re.finditer(
                r"(?m)^\s*extern\s+void\s*\*\s*malloc\s*"
                r"\((?P<parameters>[^;{}]*)\)\s*[^;{}]*;\s*$",
                rewritten,
            )
        )
        malloc_declarations = [match.span() for match in malloc_declaration_matches]

        def replace_malloc(match: re.Match[str]) -> str:
            if any(start <= match.start() < end for start, end in malloc_declarations):
                return match.group(0)
            return "__VERIFIER_malloc("

        rewritten, malloc_count = MALLOC_CALL.subn(replace_malloc, rewritten)
        if malloc_count > len(malloc_declarations):
            rules.append("preprocessed-malloc")
            if malloc_declaration_matches:
                parameters = malloc_declaration_matches[0].group("parameters").strip()
                manual_malloc_declaration = (
                    f"extern void *__VERIFIER_malloc({parameters});\n"
                )

    rewritten, inline_count = INLINE_DEFINITION.subn(
        lambda match: f"{match.group('indent')}static inline{match.group('rest')}",
        rewritten,
    )
    if inline_count:
        rules.append("internalize-inline-definition")

    rewritten, sleep_count = STANDALONE_SLEEP.subn(
        lambda match: f"{match.group('indent')}(void)0;", rewritten
    )
    if sleep_count:
        rules.append("scheduler-independent-sleep")

    if "Centre for Development of Advanced Computing" in rewritten:
        rewritten, attr_count = JOINABLE_ATTR_CALL.subn("(0)", rewritten)
        if attr_count:
            rules.append("joinable-thread-attributes")

    rewritten, definition_count = ASSUME_DEFINITION.subn(
        "void assume_abort_if_not(int cond) { "
        "__VERIFIER_assume_internal((_Bool)cond, 0); }",
        rewritten,
    )
    if definition_count > 1:
        raise ValueError("multiple abort-as-assume definitions")
    if definition_count == 1:
        rules.append("abort-assume-definition")
    elif provide_missing_assume and ASSUME_CALL.search(text):
        rewritten = (
            "void assume_abort_if_not(int cond) { "
            "__VERIFIER_assume_internal((_Bool)cond, 0); }\n"
            + rewritten
        )
        rules.append("abort-assume-declaration")

    # This benchmark's diagnostic output is outside the reachability property,
    # and its only concurrent printf is already protected by `mymutex`.
    # GenMC's host-side stdio execution otherwise trips allocation bookkeeping.
    if "Centre for Development of Advanced Computing" in rewritten:
        rewritten, printf_count = STANDALONE_PRINTF.subn(
            lambda match: f"{match.group('indent')}(void)0;", rewritten
        )
        if printf_count:
            rules.append("cdac-diagnostic-output")
        if "pthread-demo-datarace.c" in rewritten:
            rewritten, exit_count = re.subn(r"\bexit\s*\(\s*0\s*\)\s*;", "return 0;", rewritten)
            if exit_count:
                rules.append("cdac-main-return")

    if "void (*f)(void) = good;" in rewritten and re.search(
        r"\bvoid\s+bad\s*\(\s*void\s*\)", rewritten
    ):
        rewritten, indirect_count = re.subn(
            r"(?m)^(?P<indent>\s*)g\s*\(\s*\)\s*;",
            r"\g<indent>if (g == good) good(); else if (g == bad) bad();",
            rewritten,
        )
        if indirect_count:
            rules.append("finite-function-pointer-dispatch")

    def replace_reach_error(match: re.Match[str]) -> str:
        return (
            "void reach_error() { __VERIFIER_assert_fail("
            f"{match.group('assertion')}, {match.group('file')}, {match.group('line')}); }}"
        )

    rewritten, expanded_reach_count = DIRECT_REACH_ERROR.subn(
        replace_reach_error, rewritten
    )
    rewritten, source_reach_count = DIRECT_ASSERT_REACH_ERROR.subn(
        'void reach_error() { __VERIFIER_assert_fail("0", __FILE__, __LINE__); }',
        rewritten,
    )
    reach_count = expanded_reach_count + source_reach_count
    if reach_count > 1:
        raise ValueError("multiple direct __assert_fail reach_error definitions")
    if reach_count == 1:
        rules.append(
            "direct-reach-error"
            if expanded_reach_count
            else "direct-assert-reach-error"
        )
        # The normalized reach_error enters GenMC's driver-managed VE_Safety path.
        # Some SV-COMP fixtures retain abort() only as a source-level noreturn
        # fallback after reach_error(); leaving it in the transformed module makes
        # the SC-RVF whole-program gate reject an otherwise supported property
        # endpoint. Remove only this exact consecutive tail, and only after the
        # reach_error definition itself matched the fail-closed normalization above.
        rewritten, terminal_abort_count = TRAILING_ABORT_AFTER_REACH_ERROR.subn(
            lambda match: match.group("reach"), rewritten
        )
        if terminal_abort_count:
            rules.append("terminal-reach-error-abort")

    atomic_count = 0

    def wrap_atomic_function(match: re.Match[str]) -> str:
        nonlocal atomic_count
        body = match.group("body")
        if re.search(r"\b(return|goto)\b", body):
            return match.group(0)
        if "__VERIFIER_atomic_begin" in body or "__VERIFIER_atomic_end" in body:
            return match.group(0)
        atomic_count += 1
        return (
            match.group("header")
            + "\n  __VERIFIER_atomic_begin();"
            + body
            + "\n  __VERIFIER_atomic_end();\n"
            + match.group("close")
        )

    rewritten = SIMPLE_ATOMIC_FUNCTION.sub(wrap_atomic_function, rewritten)
    if atomic_count:
        rules.append("svcomp-atomic-function")

    if "extern int pthread_create" in rewritten:
        rewritten, manual_count = _rewrite_manual_pthread_calls(rewritten)
        if manual_count:
            if not add_runtime_include:
                rewritten = MANUAL_PTHREAD_DECLARATIONS + rewritten
            rules.append("manual-pthread-calls")
            if (
                "__VERIFIER_atomic_begin" in rewritten
                and not re.search(
                    r"\bvoid\s+__VERIFIER_atomic_begin\s*\([^)]*\)\s*\{", rewritten
                )
            ):
                rewritten = MANUAL_ATOMIC_RUNTIME + rewritten
                rules.append("manual-atomic-runtime")

    if rules:
        if add_runtime_include:
            rewritten = "#include <genmc.h>\n" + rewritten
        else:
            rewritten = (
                MANUAL_GENMC_DECLARATIONS + manual_malloc_declaration + rewritten
            )
    return RewriteResult(bool(rules), tuple(rules), rewritten)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--metadata", type=Path)
    args = parser.parse_args()
    result = rewrite_text(args.source.read_text(encoding="utf-8", errors="ignore"))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result.text, encoding="utf-8")
    if args.metadata:
        args.metadata.parent.mkdir(parents=True, exist_ok=True)
        args.metadata.write_text(
            json.dumps({**asdict(result), "text": None}, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
