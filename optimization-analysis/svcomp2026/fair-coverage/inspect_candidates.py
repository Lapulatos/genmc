#!/usr/bin/env python3
"""Inspect selected SV-Benchmarks tasks without modifying the corpus."""

from __future__ import annotations

import argparse
from pathlib import Path

import yaml


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("benchmark_root", type=Path)
    parser.add_argument("tasks", type=Path)
    args = parser.parse_args()

    root = args.benchmark_root.resolve()
    for raw in args.tasks.read_text(encoding="utf-8").splitlines():
        relative = raw.strip()
        if not relative or relative.startswith("#"):
            continue
        task_path = root / relative
        print(f"@@ {relative} exists={task_path.is_file()}")
        if not task_path.is_file():
            continue
        task = yaml.safe_load(task_path.read_text(encoding="utf-8"))
        inputs = task.get("input_files", [])
        if isinstance(inputs, str):
            inputs = [inputs]
        print(f"inputs={inputs!r} options={task.get('options', {})!r}")
        print(f"properties={task.get('properties', [])!r}")
        for input_name in inputs:
            source_path = task_path.parent / input_name
            text = source_path.read_text(encoding="utf-8", errors="ignore")
            head = " | ".join(text.splitlines()[:40])
            print(
                f"source={source_path.name} lines={len(text.splitlines())} "
                f"nondet={'__VERIFIER_nondet' in text} "
                f"abort_assume={'assume_abort_if_not' in text}"
            )
            print(f"head={head}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
