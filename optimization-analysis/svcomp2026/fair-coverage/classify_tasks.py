#!/usr/bin/env python3
"""Classify fair-census tasks by source semantics and applied rewrite rule."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import yaml

from rewrite_sources import (
    ASSUME_DEFINITION,
    NONDET_DECLARATION,
    NONDET_USE,
    local_include_closure,
    rewrite_text,
)


def source_for(task_path: Path, task: dict) -> Path | None:
    inputs = task.get("input_files", [])
    if isinstance(inputs, str):
        inputs = [inputs]
    if len(inputs) != 1:
        return None
    source = task_path.parent / inputs[0]
    original = source.with_suffix(".c")
    if (
        source.suffix == ".i"
        and original.is_file()
        and "#include <svcomp.h>" in original.read_text(encoding="utf-8", errors="ignore")
    ):
        return source
    return original if source.suffix == ".i" and original.is_file() else source


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("benchmark_root", type=Path)
    parser.add_argument("task_set", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    marker = "/sv-benchmarks/"
    rows = []
    for raw in args.task_set.read_text(encoding="utf-8").splitlines():
        relative = raw.split(marker, 1)[1]
        task_path = args.benchmark_root / relative
        task = yaml.safe_load(task_path.read_text(encoding="utf-8"))
        source = source_for(task_path, task)
        if source is None:
            rows.append({"task": relative, "source": "", "multi_input": True})
            continue
        closure = local_include_closure(source)
        texts = [path.read_text(encoding="utf-8", errors="ignore") for path in closure]
        nondet = any(
            NONDET_USE.search(NONDET_DECLARATION.sub("", text)) for text in texts
        )
        has_definition = any(ASSUME_DEFINITION.search(text) for text in texts)
        rules = []
        if not nondet:
            for path, text in zip(closure, texts):
                rewrite = rewrite_text(
                    text,
                    provide_missing_assume=(path == source and not has_definition),
                )
                rules.extend(rewrite.rules)
        combined = "\n".join(texts)
        rows.append({
            "task": relative,
            "source": str(source.relative_to(args.benchmark_root)),
            "multi_input": False,
            "data_nondeterminism": nondet,
            "rules": ",".join(rules),
            "has_abort_assume": "assume_abort_if_not" in combined,
            "has_atomic_compat": (
                "__VERIFIER_atomic_begin" in combined
                or "__VERIFIER_atomic_end" in combined
            ),
            "has_rwlock_initializer": "PTHREAD_RWLOCK_INITIALIZER" in combined,
        })
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "source-classification.json").write_text(
        json.dumps(rows, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    fields = list(rows[0])
    with (args.output / "source-classification.tsv").open("w", encoding="utf-8", newline="") as out:
        writer = csv.DictWriter(out, fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
    print(json.dumps({
        "tasks": len(rows),
        "multi_input": sum(bool(row.get("multi_input")) for row in rows),
        "data_nondeterminism": sum(bool(row.get("data_nondeterminism")) for row in rows),
        "rewritten": sum(bool(row.get("rules")) for row in rows),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
