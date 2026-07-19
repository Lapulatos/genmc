#!/usr/bin/env python3
"""Build a property-level manifest for SV-Benchmarks C.Concurrency.

The output has one row per (task YAML, property), which is the comparison unit
used by SV-COMP.  This script performs only source/metadata classification;
dynamic frontend compatibility is merged later from BenchExec results.
"""

from __future__ import annotations

import argparse
import csv
import glob
import hashlib
import json
from pathlib import Path
import re
import sys

import yaml


FEATURES = {
    "uses_pthread": r"\bpthread_",
    "uses_c11_atomic": r"\b(?:atomic_|_Atomic\b)",
    "uses_mutex": r"\bpthread_mutex_",
    "uses_condvar": r"\bpthread_cond_",
    "uses_dynamic_memory": r"\b(?:malloc|calloc|realloc|free)\s*\(",
    "uses_inline_asm": r"\b(?:asm|__asm__)\b",
    "uses_verifier_nondet": r"\b__VERIFIER_nondet_",
    "uses_verifier_assume": r"\b__VERIFIER_assume\s*\(",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("benchmark_root", type=Path, help="sv-benchmarks checkout")
    parser.add_argument("output_tsv", type=Path)
    parser.add_argument("--summary-json", type=Path)
    return parser.parse_args()


def load_set_patterns(set_file: Path) -> list[str]:
    patterns = []
    for raw in set_file.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            patterns.append(line)
    return patterns


def normalize_inputs(value: object) -> list[str]:
    if isinstance(value, str):
        return [value]
    if isinstance(value, list) and all(isinstance(item, str) for item in value):
        return value
    raise ValueError(f"unsupported input_files value: {value!r}")


def property_kind(path: str) -> str:
    return Path(path).stem


def read_sources(yaml_path: Path, inputs: list[str]) -> tuple[str, int, int, dict[str, int]]:
    chunks: list[bytes] = []
    line_count = 0
    total_bytes = 0
    for relative in inputs:
        source = (yaml_path.parent / relative).resolve()
        if not source.is_file():
            continue
        data = source.read_bytes()
        chunks.append(data)
        total_bytes += len(data)
        line_count += data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)
    payload = b"\0".join(chunks)
    text = payload.decode("utf-8", errors="ignore")
    flags = {name: int(bool(re.search(pattern, text))) for name, pattern in FEATURES.items()}
    return hashlib.sha256(payload).hexdigest(), total_bytes, line_count, flags


def adapter_sources(yaml_path: Path, inputs: list[str]) -> tuple[list[Path], list[Path]]:
    """Return GenMC entry sources and recursively included local quoted headers."""
    entries = []
    for relative in inputs:
        source = (yaml_path.parent / relative).resolve()
        original = source.with_suffix(".c") if source.suffix == ".i" else source
        entries.append(original if original.is_file() else source)
    pending = list(entries)
    transitive: list[Path] = []
    seen: set[Path] = set()
    include = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
    while pending:
        source = pending.pop()
        if source in seen or not source.is_file():
            continue
        seen.add(source)
        transitive.append(source)
        text = source.read_text(encoding="utf-8", errors="ignore")
        for name in include.findall(text):
            dependency = (source.parent / name).resolve()
            if dependency.is_file():
                pending.append(dependency)
    return entries, transitive


def aggregate_size(paths: list[Path]) -> tuple[int, int]:
    total_bytes = 0
    total_lines = 0
    for path in paths:
        if not path.is_file():
            continue
        data = path.read_bytes()
        total_bytes += len(data)
        total_lines += data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)
    return total_bytes, total_lines


def main() -> int:
    args = parse_args()
    root = args.benchmark_root.resolve()
    c_root = root / "c"
    set_file = c_root / "Concurrency.set"
    if not set_file.is_file():
        raise SystemExit(f"missing category definition: {set_file}")

    yaml_paths: set[Path] = set()
    for pattern in load_set_patterns(set_file):
        yaml_paths.update(Path(path) for path in glob.glob(str(c_root / pattern)))
    if not yaml_paths:
        raise SystemExit("C.Concurrency resolved to zero YAML files")

    rows: list[dict[str, object]] = []
    parse_errors: list[str] = []
    for yaml_path in sorted(yaml_paths):
        relative_yaml = yaml_path.relative_to(root).as_posix()
        try:
            task = yaml.safe_load(yaml_path.read_text(encoding="utf-8"))
            inputs = normalize_inputs(task["input_files"])
            options = task.get("options") or {}
            sha256, source_bytes, source_lines, flags = read_sources(yaml_path, inputs)
            entry_sources, transitive_sources = adapter_sources(yaml_path, inputs)
            adapter_source_bytes, adapter_source_lines = aggregate_size(entry_sources)
            adapter_transitive_bytes, adapter_transitive_lines = aggregate_size(
                transitive_sources
            )
            properties = task.get("properties") or []
            if not properties:
                raise ValueError("task has no properties")
            for prop in properties:
                prop_file = str(prop["property_file"])
                kind = property_kind(prop_file)
                row: dict[str, object] = {
                    "task_id": f"{relative_yaml}#{kind}",
                    "task_yaml": relative_yaml,
                    "family": yaml_path.parent.name,
                    "property": kind,
                    "property_file": (yaml_path.parent / prop_file).resolve().relative_to(root).as_posix(),
                    "expected_verdict": str(prop.get("expected_verdict", "")).lower(),
                    "language": options.get("language", ""),
                    "data_model": options.get("data_model", ""),
                    "input_count": len(inputs),
                    "input_files": ";".join(inputs),
                    "source_sha256": sha256,
                    "source_bytes": source_bytes,
                    "source_lines": source_lines,
                    "adapter_input_files": ";".join(
                        source.relative_to(root).as_posix()
                        for source in entry_sources
                    ),
                    "adapter_source_bytes": adapter_source_bytes,
                    "adapter_source_lines": adapter_source_lines,
                    "adapter_transitive_bytes": adapter_transitive_bytes,
                    "adapter_transitive_lines": adapter_transitive_lines,
                }
                row.update(flags)
                rows.append(row)
        except Exception as error:  # preserve a census row for malformed metadata
            parse_errors.append(f"{relative_yaml}: {type(error).__name__}: {error}")

    args.output_tsv.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0])
    with args.output_tsv.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    counts: dict[str, dict[str, int] | int | list[str]] = {
        "yaml_files": len(yaml_paths),
        "property_tasks": len(rows),
        "parse_error_count": len(parse_errors),
        "parse_errors": parse_errors,
        "by_property": {},
        "by_family": {},
        "by_data_model": {},
        "by_expected_verdict": {},
    }
    for row in rows:
        for source, key in (
            ("by_property", "property"),
            ("by_family", "family"),
            ("by_data_model", "data_model"),
            ("by_expected_verdict", "expected_verdict"),
        ):
            bucket = counts[source]
            assert isinstance(bucket, dict)
            value = str(row[key])
            bucket[value] = bucket.get(value, 0) + 1

    summary = args.summary_json or args.output_tsv.with_suffix(".summary.json")
    summary.write_text(json.dumps(counts, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {len(rows)} property tasks from {len(yaml_paths)} YAML files")
    print(f"manifest: {args.output_tsv}")
    print(f"summary: {summary}")
    if parse_errors:
        print(f"warning: {len(parse_errors)} metadata parse errors", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
