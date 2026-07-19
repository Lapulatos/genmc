#!/usr/bin/env python3
"""Template for a verifier-specific SV-COMP preprocessing runner.

Bundle one specialized copy of this file in each tool archive.  BenchExec
measures this process and all children, so preprocessing cost is included.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Callable


ADAPTER_VERSION = "0.1-template"
RESULT_PREFIX = "SVCOMP_RESULT="
STATUS_PREFIX = "SVCOMP_STATUS="
STAT_PREFIX = "SVCOMP_STAT "
MAX_DIAGNOSTIC_BYTES = 128 * 1024


class UnsupportedTask(Exception):
    """The adapter cannot preserve the requested task semantics."""


@dataclass(frozen=True)
class TaskContext:
    source: Path
    property_file: Path
    property_kind: str
    data_model: str
    cpu_cores: int | None
    cpu_limit: int | None
    memory_limit: int | None


@dataclass(frozen=True)
class RewriteResult:
    text: str
    applied_rules: tuple[str, ...]


@dataclass(frozen=True)
class Transform:
    name: str
    precondition: Callable[[str, TaskContext], bool]
    apply: Callable[[str, TaskContext], str]


def parse_property(path: Path) -> str:
    text = path.read_text(encoding="utf-8", errors="strict")
    known = {
        "unreach-call": "call(reach_error())",
        "no-data-race": "data-race",
        "no-overflow": "overflow",
        "valid-memsafety": "valid-deref",
    }
    matches = [kind for kind, token in known.items() if token in text]
    if len(matches) != 1:
        raise UnsupportedTask("unsupported-or-ambiguous-property")
    return matches[0]


def exact_transform_example(text: str, context: TaskContext) -> str:
    """Example for the standard SV-COMP ``__VERIFIER_assume`` convention.

    Replace this textual example with a parser-backed rewrite before submission.
    Ordinary program identifiers must never be used to identify benchmark groups.
    """
    del context
    old = "void __VERIFIER_assume(int cond) { if (!cond) { abort(); } }"
    new = (
        "void __VERIFIER_assume(int cond) "
        "{ __VERIFIER_assume_internal((_Bool)cond, 0); }"
    )
    if text.count(old) != 1:
        raise UnsupportedTask("transform-precondition-changed")
    return text.replace(old, new)


TRANSFORMS: tuple[Transform, ...] = (
    Transform(
        name="exact-abort-assume",
        precondition=lambda text, context: (
            context.property_kind == "unreach-call"
            and "void __VERIFIER_assume(int cond) { if (!cond) { abort(); } }"
            in text
        ),
        apply=exact_transform_example,
    ),
)


def rewrite_source(text: str, context: TaskContext) -> RewriteResult:
    rules = []
    current = text
    for transform in TRANSFORMS:
        if transform.precondition(current, context):
            updated = transform.apply(current, context)
            if updated == current:
                raise UnsupportedTask(f"no-op-transform:{transform.name}")
            current = updated
            rules.append(transform.name)
    return RewriteResult(current, tuple(rules))


def build_tool_command(
    tool_root: Path,
    context: TaskContext,
    rewritten_source: Path,
    forwarded_options: list[str],
) -> list[str]:
    """Specialize this function for GenMC, TruSt, Deagle, CBMC, etc."""
    tool = tool_root / "libexec" / "REPLACE_WITH_TOOL_BINARY"
    if not tool.is_file():
        raise UnsupportedTask("missing-bundled-tool")
    architecture = "-m32" if context.data_model == "ILP32" else "-m64"
    return [str(tool), *forwarded_options, "--", architecture, str(rewritten_source)]


def classify_tool_output(output: str, returncode: int, property_kind: str) -> str:
    """Replace markers with exact, version-pinned output contracts.

    Never infer TRUE from exit code alone.  TRUE requires the verifier's explicit
    completion marker and a configuration that is complete for the task semantics.
    """
    lines = set(output.splitlines())
    if "TOOL_UNSUPPORTED" in lines:
        return "unknown"
    if returncode == 0 and "TOOL_RESULT: TRUE" in lines:
        return "true"
    if "TOOL_RESULT: FALSE" in lines:
        false_results = {
            "unreach-call": "false(unreach-call)",
            "no-data-race": "false(no-data-race)",
            "no-overflow": "false(no-overflow)",
        }
        return false_results.get(property_kind, "unknown")
    if "TOOL_RESULT: UNKNOWN" in lines:
        return "unknown"
    raise UnsupportedTask("unclassified-tool-output")


def extract_statistics(output: str) -> dict[str, str]:
    """Use anchored patterns for stable, version-pinned tool statistics."""
    stats: dict[str, str] = {}
    patterns = {
        "tool.executions": re.compile(
            r"^Number of complete executions explored:\s*([0-9]+)$"
        ),
        "tool.blocked": re.compile(
            r"^Number of blocked executions seen:\s*([0-9]+)$"
        ),
    }
    for line in output.splitlines():
        for key, pattern in patterns.items():
            match = pattern.fullmatch(line)
            if match:
                stats[key] = match.group(1)
    return stats


def emit_bounded_diagnostics(path: Path) -> None:
    size = path.stat().st_size
    with path.open("rb") as handle:
        if size > MAX_DIAGNOSTIC_BYTES:
            handle.seek(size - MAX_DIAGNOSTIC_BYTES)
            handle.readline()
            print("SVCOMP_ADAPTER_DIAGNOSTIC_TRUNCATED=1")
        data = handle.read()
        sys.stdout.flush()
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
    if data and not data.endswith(b"\n"):
        print()


def emit_status(status: str, audit: dict[str, object]) -> None:
    print(f"{STATUS_PREFIX}{status}")
    print("SVCOMP_AUDIT=" + json.dumps(audit, sort_keys=True, separators=(",", ":")))


def run(args: argparse.Namespace) -> int:
    source = Path(args.input).resolve()
    property_file = Path(args.property).resolve()
    context = TaskContext(
        source=source,
        property_file=property_file,
        property_kind=parse_property(property_file),
        data_model=args.data_model,
        cpu_cores=args.cpu_cores,
        cpu_limit=args.cpu_limit,
        memory_limit=args.memory_limit,
    )
    audit: dict[str, object] = {
        "adapter_version": ADAPTER_VERSION,
        "data_model": context.data_model,
        "property": context.property_kind,
        "source_suffix": source.suffix,
    }

    original = source.read_text(encoding="utf-8", errors="strict")
    rewritten = rewrite_source(original, context)
    audit["transforms"] = list(rewritten.applied_rules)

    # Use only paths permitted by the SV-COMP archive contract.  Preserve the
    # original source location for witnesses and diagnostics with a line marker.
    temp_parent = os.environ.get("TMPDIR", "/tmp")
    with tempfile.TemporaryDirectory(prefix="svcomp-adapter-", dir=temp_parent) as tmp:
        tmp_path = Path(tmp)
        rewritten_source = tmp_path / (source.stem + ".adapter.c")
        line_path = str(source).replace("\\", "\\\\").replace('"', '\\"')
        rewritten_source.write_text(
            f'#line 1 "{line_path}"\n{rewritten.text}', encoding="utf-8"
        )
        tool_root = Path(__file__).resolve().parent.parent
        command = build_tool_command(
            tool_root, context, rewritten_source, args.tool_option
        )
        audit["command_role"] = Path(command[0]).name
        raw_log = tmp_path / "tool.log"
        with raw_log.open("wb") as output:
            completed = subprocess.run(
                command,
                stdin=subprocess.DEVNULL,
                stdout=output,
                stderr=subprocess.STDOUT,
                cwd=Path.cwd(),
                check=False,
            )
        raw = raw_log.read_text(encoding="utf-8", errors="replace")
        verdict = classify_tool_output(raw, completed.returncode, context.property_kind)
        emit_bounded_diagnostics(raw_log)
        print(f"{RESULT_PREFIX}{verdict}")
        print(f"{STAT_PREFIX}adapter.transforms={len(rewritten.applied_rules)}")
        for key, value in extract_statistics(raw).items():
            print(f"{STAT_PREFIX}{key}={value}")
        print("SVCOMP_AUDIT=" + json.dumps(audit, sort_keys=True, separators=(",", ":")))
        return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", action="store_true")
    parser.add_argument("--property")
    parser.add_argument("--data-model", choices=("ILP32", "LP64"))
    parser.add_argument("--input")
    parser.add_argument("--cpu-cores", type=int)
    parser.add_argument("--cpu-limit", type=int)
    parser.add_argument("--memory-limit", type=int)
    parser.add_argument("--tool-option", action="append", default=[])
    args = parser.parse_args(argv)
    if args.version:
        print(f"adapter {ADAPTER_VERSION}; tool REPLACE_WITH_PINNED_VERSION")
        raise SystemExit(0)
    if not args.property or not args.data_model or not args.input:
        parser.error("--property, --data-model, and --input are required")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return run(args)
    except UnsupportedTask as error:
        emit_status(str(error), {"adapter_version": ADAPTER_VERSION})
        print(f"{RESULT_PREFIX}unknown")
        return 0
    except (OSError, UnicodeError, ValueError) as error:
        emit_status(type(error).__name__, {"adapter_version": ADAPTER_VERSION})
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
