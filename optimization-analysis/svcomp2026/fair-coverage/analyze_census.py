#!/usr/bin/env python3
"""Validate and summarize sharded fair-coverage BenchExec results."""

from __future__ import annotations

import argparse
import bz2
from collections import Counter
import csv
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def canonical_task(name: str) -> str:
    marker = "/sv-benchmarks/c/"
    if marker in name:
        return "c/" + name.split(marker, 1)[1]
    return name.removeprefix("../../../../")


def read_rows(result_root: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for xml_path in sorted(result_root.glob("shard*/*.xml.bz2")):
        with bz2.open(xml_path, "rb") as source:
            tree = ET.parse(source)
        for run in tree.getroot().findall("run"):
            columns = {
                column.attrib["title"]: column.attrib.get("value", "")
                for column in run.findall("column")
            }
            status = columns.get("status", "")
            category = columns.get("category", "")
            if category == "wrong" and status == "true":
                interpretation = "conditional-input-miss"
            elif category == "wrong" and status.startswith("false"):
                interpretation = "fixed-seed-soundness-failure"
            elif category == "correct":
                interpretation = "benchmark-consistent"
            else:
                interpretation = "unknown"
            rows.append(
                {
                    "task": canonical_task(run.attrib["name"]),
                    "status": status,
                    "category": category,
                    "interpretation": interpretation,
                    "cputime": columns.get("cputime", ""),
                    "walltime": columns.get("walltime", ""),
                    "memory": columns.get("memory", ""),
                    "executions": columns.get("executions", ""),
                    "blocked": columns.get("blocked", ""),
                    "xml": str(xml_path.relative_to(result_root)),
                }
            )
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    rows = read_rows(args.result_root)
    duplicates = sorted(task for task, count in Counter(r["task"] for r in rows).items() if count > 1)
    summary = {
        "rows": len(rows),
        "unique_tasks": len({row["task"] for row in rows}),
        "duplicates": duplicates,
        "status": dict(sorted(Counter(row["status"] for row in rows).items())),
        "category": dict(sorted(Counter(row["category"] for row in rows).items())),
        "interpretation": dict(
            sorted(Counter(row["interpretation"] for row in rows).items())
        ),
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    fields = list(rows[0]) if rows else []
    with (args.output / "rows.tsv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fields, delimiter="\t")
        if fields:
            writer.writeheader()
            writer.writerows(rows)
    print(json.dumps(summary, sort_keys=True))
    return 1 if duplicates else 0


if __name__ == "__main__":
    raise SystemExit(main())
