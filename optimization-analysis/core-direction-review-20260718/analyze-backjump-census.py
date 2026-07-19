#!/usr/bin/env python3
"""Strict paired analysis for the P2 Gate-A1 observation census."""

from __future__ import annotations

import argparse
import bz2
import json
import re
import zipfile
from pathlib import Path
from xml.etree import ElementTree


KEY_VALUE = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")


def parse_number(value: str | None, suffix: str) -> float | None:
    if not value:
        return None
    return float(value.removesuffix(suffix))


def read_lane(root: Path, lane: str) -> dict[str, dict]:
    lane_root = root / lane
    xmls = list(lane_root.glob("*.xml.bz2"))
    zips = list(lane_root.glob("*.logfiles.zip"))
    if len(xmls) != 1 or len(zips) != 1:
        raise SystemExit(f"{lane}: expected exactly one XML and one log archive")
    with bz2.open(xmls[0]) as stream:
        result = ElementTree.parse(stream).getroot()
    runs: dict[str, dict] = {}
    for run in result.findall("run"):
        name = Path(run.attrib["name"]).name
        columns = {column.attrib["title"]: column.attrib.get("value") for column in run}
        runs[name] = {
            "status": columns.get("status"),
            "category": columns.get("category"),
            "cpu_seconds": parse_number(columns.get("cputime"), "s"),
            "wall_seconds": parse_number(columns.get("walltime"), "s"),
            "memory_bytes": int(columns["memory"].removesuffix("B"))
            if columns.get("memory")
            else None,
        }
    if len(runs) != 15:
        raise SystemExit(f"{lane}: XML has {len(runs)} runs, expected 15")
    with zipfile.ZipFile(zips[0]) as archive:
        logs = archive.namelist()
        if len(logs) != 15:
            raise SystemExit(f"{lane}: archive has {len(logs)} logs, expected 15")
        for member in logs:
            task = member.rsplit(".", 2)[-2] + ".yml"
            # Preserve compound YAML names: strip only mode prefix and .log suffix.
            task = Path(member).name.split(".", 1)[1].removesuffix(".log")
            text = archive.read(member).decode("utf-8", errors="replace")
            exploration = [line for line in text.splitlines() if "Exploration statistics:" in line]
            census = [line for line in text.splitlines() if "CAT backjump census:" in line]
            final = [line for line in text.splitlines() if "CAT incremental statistics:" in line]
            counters = {}
            if exploration:
                counters.update({key: int(value) for key, value in KEY_VALUE.findall(exploration[-1])})
            source = census[-1] if census else final[-1] if final else ""
            counters.update({key: int(value) for key, value in KEY_VALUE.findall(source)})
            for short, long_name in {
                "conflicts": "backjump-conflict-queries",
                "cores": "backjump-cores-derived",
                "unsupported": "backjump-cores-unsupported",
                "core-literals": "backjump-core-literals",
                "mapped": "backjump-mapped-choice-literals",
                "unresolved": "backjump-unresolved-choice-facts",
                "newest-only": "backjump-newest-only",
                "nonlocal": "backjump-nonlocal",
                "distance-sum": "backjump-distance-sum",
                "max-distance": "backjump-max-distance",
                "recurring": "backjump-recurring-signatures",
                "unique-signatures": "backjump-unique-signatures",
            }.items():
                if long_name in counters:
                    counters[short] = counters[long_name]
            if task not in runs:
                raise SystemExit(f"{lane}: log has unknown task {task}")
            runs[task]["counters"] = counters
            runs[task]["has_progress_census"] = bool(census)
            runs[task]["has_final_census"] = bool(final)
    return runs


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not (args.root / "complete.txt").is_file():
        raise SystemExit("strict run is incomplete")
    baseline = read_lane(args.root, "baseline")
    candidate = read_lane(args.root, "candidate")
    if set(baseline) != set(candidate):
        raise SystemExit("lane task sets differ")
    tasks = sorted(baseline)
    status_differences = [
        task
        for task in tasks
        if (baseline[task]["status"], baseline[task]["category"])
        != (candidate[task]["status"], candidate[task]["category"])
    ]
    terminal = [task for task in tasks if baseline[task]["status"] != "TIMEOUT"]
    search_names = [
        "rf-offered", "rf-queued", "co-offered", "co-queued", "backward-offered",
        "backward-queued", "work-added", "work-popped", "realized-revisit-prefixes",
        "inconsistent-revisit-prefixes", "validity-queries",
    ]
    search_counter_differences = {}
    for task in tasks:
        differences = {}
        for name in search_names:
            left = baseline[task]["counters"].get(name)
            right = candidate[task]["counters"].get(name)
            if left is not None and right is not None and left != right:
                differences[name] = [left, right]
        if differences:
            search_counter_differences[task] = differences
    counter_names = [
        "conflicts", "cores", "unsupported", "core-literals", "mapped", "unresolved",
        "newest-only", "nonlocal", "distance-sum", "max-distance", "recurring",
        "unique-signatures",
    ]
    totals = {
        name: sum(candidate[task]["counters"].get(name, 0) for task in tasks)
        for name in counter_names
    }
    totals["max-distance"] = max(
        (candidate[task]["counters"].get("max-distance", 0) for task in tasks), default=0
    )
    report = {
        "root": str(args.root),
        "tasks": len(tasks),
        "status_differences": status_differences,
        "terminal_tasks": len(terminal),
        "timeout_tasks": len(tasks) - len(terminal),
        "candidate_tasks_with_progress_census": sum(
            candidate[task]["has_progress_census"] for task in tasks
        ),
        "candidate_tasks_with_final_census": sum(
            candidate[task]["has_final_census"] for task in tasks
        ),
        "candidate_tasks_with_any_census": sum(
            candidate[task]["has_progress_census"] or candidate[task]["has_final_census"]
            for task in tasks
        ),
        "search_counter_differences_where_both_recorded": search_counter_differences,
        "cpu_seconds": {
            "baseline_all": sum(baseline[task]["cpu_seconds"] or 0 for task in tasks),
            "candidate_all": sum(candidate[task]["cpu_seconds"] or 0 for task in tasks),
            "baseline_common_terminal": sum(baseline[task]["cpu_seconds"] or 0 for task in terminal),
            "candidate_common_terminal": sum(candidate[task]["cpu_seconds"] or 0 for task in terminal),
        },
        "memory_bytes_sum": {
            "baseline": sum(baseline[task]["memory_bytes"] or 0 for task in tasks),
            "candidate": sum(candidate[task]["memory_bytes"] or 0 for task in tasks),
        },
        "candidate_backjump_totals_from_latest_records": totals,
        "per_task": {
            task: {"baseline": baseline[task], "candidate": candidate[task]} for task in tasks
        },
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    if args.output:
        args.output.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)


if __name__ == "__main__":
    main()
