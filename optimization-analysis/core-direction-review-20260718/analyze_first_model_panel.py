#!/usr/bin/env python3
"""Summarize the fixed-panel finite first-model experiment."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import math
import re
import xml.etree.ElementTree as ET
from pathlib import Path


FIRST_MODEL = re.compile(
    r"Finite skeleton first model: mode=([^ ]+) supported=(true|false) "
    r"status=([0-9]+) assignment=(true|false) build-us=([0-9]+) check-us=([0-9]+)"
)


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(fraction * len(ordered)) - 1)]


def read_panel(path: Path) -> dict[str, str]:
    with path.open(newline="") as source:
        return {row["task"]: row["class"] for row in csv.DictReader(source, delimiter="\t")}


def read_results(root: Path) -> dict[str, dict[str, dict[str, object]]]:
    result: dict[str, dict[str, dict[str, object]]] = {}
    for path in root.glob("*.xml.bz2"):
        mode = path.name.split(".results.", 1)[1].split(".panel", 1)[0]
        with bz2.open(path, "rb") as source:
            xml = ET.parse(source).getroot()
        rows: dict[str, dict[str, object]] = {}
        for run in xml.findall("run"):
            columns = {column.get("title"): column.get("value", "") for column in run}
            task = Path(run.get("name", "")).name
            rows[task] = {
                "bench-status": columns.get("status", ""),
                "cpu-s": float(columns["cputime"][:-1]),
                "wall-s": float(columns["walltime"][:-1]),
                "memory-b": int(columns["memory"][:-1]),
            }
        result[mode] = rows
    return result


def read_models(root: Path) -> dict[tuple[str, str], dict[str, object]]:
    models: dict[tuple[str, str], dict[str, object]] = {}
    for path in root.rglob("*.log"):
        mode, task = path.name.split(".", 1)
        task = task[:-4]
        match = FIRST_MODEL.search(path.read_text(errors="replace"))
        if match:
            models[(mode, task)] = {
                "solver-status": int(match.group(3)),
                "assignment": match.group(4) == "true",
                "build-us": int(match.group(5)),
                "check-us": int(match.group(6)),
            }
    return models


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("panel", type=Path)
    parser.add_argument("result_root", type=Path)
    parser.add_argument("log_root", type=Path)
    args = parser.parse_args()

    classes = read_panel(args.panel)
    results = read_results(args.result_root)
    models = read_models(args.log_root)
    output: dict[str, object] = {"modes": {}, "tasks": {}}
    for mode, rows in sorted(results.items()):
        completed = [task for task in classes if (mode, task) in models]
        build = [float(models[(mode, task)]["build-us"]) for task in completed]
        check = [float(models[(mode, task)]["check-us"]) for task in completed]
        output["modes"][mode] = {
            "models": len(completed),
            "models-by-class": {
                group: sum(classes[task] == group for task in completed)
                for group in ("easy", "medium", "hard")
            },
            "cpu-s-sum": sum(float(row["cpu-s"]) for row in rows.values()),
            "peak-memory-b": max(int(row["memory-b"]) for row in rows.values()),
            "build-us-p50": percentile(build, 0.5),
            "build-us-max": max(build),
            "check-us-p50": percentile(check, 0.5),
            "check-us-p90": percentile(check, 0.9),
            "check-us-max": max(check),
        }
    for task, group in classes.items():
        output["tasks"][task] = {
            "class": group,
            **{
                mode: {**rows[task], **models.get((mode, task), {"model": False})}
                for mode, rows in sorted(results.items())
            },
        }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
