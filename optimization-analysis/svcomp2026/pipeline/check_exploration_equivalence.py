#!/usr/bin/env python3
"""Check verdict and explored-execution equivalence on common solved cells."""

from __future__ import annotations

import argparse
from collections import defaultdict
import csv
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--backends", nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path)
    return parser.parse_args()


def main() -> int:
    config = parse_args()
    with config.input.open(encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    requested = set(config.backends)
    available = {row["backend"] for row in rows}
    if not requested <= available:
        raise SystemExit(f"missing backends: {sorted(requested - available)}")

    grouped: dict[tuple[str, str, str], dict[str, dict[str, str]]] = defaultdict(dict)
    for row in rows:
        if row["backend"] not in requested or row["category"] != "correct":
            continue
        key = (row["task_yaml"], row["property"], row["repetition"])
        if row["backend"] in grouped[key]:
            raise SystemExit(f"duplicate solved row for {key} / {row['backend']}")
        grouped[key][row["backend"]] = row

    common = {key: cell for key, cell in grouped.items() if set(cell) == requested}
    mismatches = []
    missing_counts = []
    for (task, prop, repetition), cell in sorted(common.items()):
        if any(not row["executions"] for row in cell.values()):
            missing_counts.append((task, prop, repetition))
            continue
        values = {
            backend: (
                row["status"],
                int(row["executions"]),
                int(row["blocked"] or 0),
            )
            for backend, row in cell.items()
        }
        if len(set(values.values())) != 1:
            mismatches.append(
                {
                    "task_yaml": task,
                    "property": prop,
                    "repetition": repetition,
                    **{
                        backend: "/".join(map(str, values[backend]))
                        for backend in config.backends
                    },
                }
            )

    config.output.parent.mkdir(parents=True, exist_ok=True)
    fields = ["task_yaml", "property", "repetition", *config.backends]
    with config.output.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(mismatches)
    summary_path = config.summary or config.output.with_suffix(".summary.json")
    summary_path.write_text(
        json.dumps(
            {
                "backends": config.backends,
                "common_solved_task_repetitions": len(common),
                "missing_execution_counts": len(missing_counts),
                "verdict_execution_blocked_mismatches": len(mismatches),
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(summary_path)
    if missing_counts:
        print(f"missing execution counts in {len(missing_counts)} common solved cells")
        return 2
    if mismatches:
        print(f"found {len(mismatches)} exploration mismatches")
        return 1
    print(f"verified {len(common)} common solved task-repetitions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
