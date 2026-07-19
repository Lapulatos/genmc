#!/usr/bin/env python3
"""Select tasks completed with one common verdict across requested backends."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path


SOLVED = {"true", "false(unreach-call)"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--backends", nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    with args.input.open(encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    grouped: dict[str, dict[str, list[str]]] = defaultdict(lambda: defaultdict(list))
    for row in rows:
        if row["backend"] in args.backends:
            grouped[row["task_yaml"]][row["backend"]].append(row["status"])

    selected = []
    reasons = Counter()
    for task, by_backend in sorted(grouped.items()):
        if set(by_backend) != set(args.backends):
            reasons["missing_backend"] += 1
            continue
        statuses = [status for backend in args.backends for status in by_backend[backend]]
        if not all(status in SOLVED for status in statuses):
            reasons["not_completed"] += 1
            continue
        if len(set(statuses)) != 1:
            reasons["verdict_mismatch"] += 1
            continue
        selected.append(task)
        reasons["selected"] += 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("".join(f"{task}\n" for task in selected), encoding="utf-8")
    summary = args.summary or args.output.with_suffix(".summary.json")
    summary.write_text(
        json.dumps(
            {
                "backends": args.backends,
                "input_tasks": len(grouped),
                "selected_tasks": len(selected),
                "classification": dict(sorted(reasons.items())),
            },
            indent=2,
            sort_keys=True,
        ) + "\n",
        encoding="utf-8",
    )
    print(f"selected {len(selected)} of {len(grouped)} tasks")
    print(args.output)
    print(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
