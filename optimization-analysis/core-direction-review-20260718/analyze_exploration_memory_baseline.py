#!/usr/bin/env python3
"""Relate resource failures to pre-CAT progress and retained exploration work."""

from __future__ import annotations

import argparse
import bz2
import json
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter, defaultdict
from pathlib import Path, PurePosixPath


def task_key(path: str) -> str:
    marker = "/c/"
    normalized = path.replace(".i", ".c")
    return normalized.split(marker, 1)[1].rsplit(".", 1)[0] if marker in normalized else normalized


def percentile(values: list[int], fraction: float) -> int | None:
    if not values:
        return None
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * fraction)]


def summarize(values: list[int]) -> dict[str, int | float | None]:
    return {
        "n": len(values),
        "min": min(values) if values else None,
        "median": statistics.median(values) if values else None,
        "p90": percentile(values, 0.9),
        "max": max(values) if values else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("xml", type=Path)
    parser.add_argument("logs", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    root = ET.fromstring(bz2.open(args.xml, "rb").read())
    tasks: dict[str, dict[str, object]] = {}
    for run in root.findall("run"):
        columns = {c.attrib["title"]: c.attrib.get("value", "") for c in run.findall("column")}
        key = task_key(run.attrib["files"])
        tasks[key] = {
            "status": columns.get("status", "missing"),
            "category": columns.get("category", "missing"),
            "memory_bytes": int(columns.get("memory", "0B").removesuffix("B") or 0),
        }

    with zipfile.ZipFile(args.logs) as archive:
        for member in archive.namelist():
            if member.endswith("/"):
                continue
            text = archive.read(member).decode("utf-8", "replace")
            command = text.split("\n", 1)[0]
            sources = re.findall(r"(/workspace/[^ ]+\.(?:c|i))(?=\s|$)", command)
            if not sources:
                continue
            key = task_key(sources[-1])
            if key not in tasks:
                continue
            stats_lines = re.findall(r"^Exploration statistics: (.+)$", text, re.MULTILINE)
            counters = dict(re.findall(r"([a-z][a-z0-9-]*)=([0-9]+)", stats_lines[-1])) if stats_lines else {}
            tasks[key].update(
                {
                    "compilation_complete": "*** Compilation complete." in text,
                    "transformation_complete": "*** Transformation complete." in text,
                    "verification_complete": "*** Verification complete." in text,
                    "has_exploration_stats": bool(stats_lines),
                    "has_cat_query": "CAT incremental statistics:" in text,
                    "max_retained_work": int(counters.get("max-retained-work", "0")),
                    "max_current_graph_labels": int(
                        counters.get("max-current-graph-labels", "0")
                    ),
                    "max_stack_graph_labels": int(counters.get("max-stack-graph-labels", "0")),
                    "max_scheduler_cached_labels": int(
                        counters.get("max-scheduler-cached-labels", "0")
                    ),
                    "work_added": int(counters.get("work-added", "0")),
                    "work_popped": int(counters.get("work-popped", "0")),
                }
            )

    groups: dict[str, list[dict[str, object]]] = defaultdict(list)
    for row in tasks.values():
        groups[str(row["status"])].append(row)

    result: dict[str, object] = {
        "tasks": len(tasks),
        "status_counts": dict(Counter(str(row["status"]) for row in tasks.values())),
        "groups": {},
    }
    for status, rows in sorted(groups.items()):
        complete = [row for row in rows if "has_exploration_stats" in row]
        result["groups"][status] = {
            "tasks": len(rows),
            "compilation_complete": sum(bool(row.get("compilation_complete")) for row in rows),
            "transformation_complete": sum(bool(row.get("transformation_complete")) for row in rows),
            "verification_complete": sum(bool(row.get("verification_complete")) for row in rows),
            "logs_with_exploration_stats": sum(bool(row.get("has_exploration_stats")) for row in rows),
            "logs_reaching_cat_query": sum(bool(row.get("has_cat_query")) for row in rows),
            "memory_bytes": summarize([int(row["memory_bytes"]) for row in rows]),
            "max_retained_work": summarize([int(row["max_retained_work"]) for row in complete]),
            "max_current_graph_labels": summarize(
                [int(row["max_current_graph_labels"]) for row in complete]
            ),
            "max_stack_graph_labels": summarize(
                [int(row["max_stack_graph_labels"]) for row in complete]
            ),
            "max_scheduler_cached_labels": summarize(
                [int(row["max_scheduler_cached_labels"]) for row in complete]
            ),
            "work_added": summarize([int(row["work_added"]) for row in complete]),
            "work_popped": summarize([int(row["work_popped"]) for row in complete]),
            "task_keys": sorted(key for key, row in tasks.items() if row in rows),
        }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
