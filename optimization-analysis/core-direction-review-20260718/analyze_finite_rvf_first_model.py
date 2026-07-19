#!/usr/bin/env python3
"""Strict analysis of concrete-RF versus RVF-class finite first-model runs."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import math
import re
import statistics
import xml.etree.ElementTree as ET
from pathlib import Path


FIRST_MODEL = re.compile(
    r"Finite skeleton first model: mode=([^ ]+) supported=(true|false) "
    r"status=([0-9]+) assignment=(true|false) build-us=([0-9]+) check-us=([0-9]+)"
)
CONTROL = "abstract-cardinality"


def number(value: str) -> float:
    text = re.sub(r"[^0-9.eE+-]", "", value)
    return float(text) if text else 0.0


def read_xmls(root: Path) -> dict[str, dict[str, dict[str, float | str]]]:
    modes: dict[str, dict[str, dict[str, float | str]]] = {}
    for path in root.glob("*.xml.bz2"):
        mode = path.name.split(".results.", 1)[1].split(".pthread-wmm", 1)[0]
        xml = ET.fromstring(bz2.open(path, "rb").read())
        rows: dict[str, dict[str, float | str]] = {}
        for run in xml.findall("run"):
            columns = {column.get("title"): column.get("value", "") for column in run}
            rows[Path(run.get("name", "")).name] = {
                "bench-status": columns.get("status", ""),
                "cpu-s": number(columns.get("cputime", "0")),
                "wall-s": number(columns.get("walltime", "0")),
                "memory-b": number(columns.get("memory", "0")),
            }
        modes[mode] = rows
    return modes


def read_models(root: Path) -> dict[tuple[str, str], dict[str, int | bool]]:
    models: dict[tuple[str, str], dict[str, int | bool]] = {}
    for path in root.rglob("*.log"):
        mode, task = path.name.split(".", 1)
        match = FIRST_MODEL.search(path.read_text(errors="replace"))
        if match:
            models[(mode, task[:-4])] = {
                "solver-status": int(match.group(3)),
                "assignment": match.group(4) == "true",
                "build-us": int(match.group(5)),
                "check-us": int(match.group(6)),
            }
    return models


def read_baseline(path: Path) -> dict[str, str]:
    xml = ET.fromstring(bz2.open(path, "rb").read())
    result = {}
    for run in xml.findall("run"):
        columns = {column.get("title"): column.get("value", "") for column in run}
        result[Path(run.get("name", "")).name] = columns.get("status", "")
    return result


def geometric_mean(values: list[float]) -> float | None:
    return math.exp(statistics.fmean(map(math.log, values))) if values else None


def distribution(values: list[float]) -> dict[str, float | int | None]:
    ordered = sorted(values)
    return {
        "count": len(ordered),
        "median": statistics.median(ordered) if ordered else None,
        "p90": ordered[max(0, math.ceil(0.90 * len(ordered)) - 1)] if ordered else None,
        "max": max(ordered) if ordered else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_root", type=Path)
    parser.add_argument("log_root", type=Path)
    parser.add_argument("baseline_xml", type=Path)
    parser.add_argument("--expected-tasks", type=int, default=283)
    args = parser.parse_args()

    results = read_xmls(args.result_root)
    models = read_models(args.log_root)
    baseline = read_baseline(args.baseline_xml)
    if CONTROL not in results or len(results) != 3:
        raise SystemExit("expected concrete control and exactly two RVF modes")
    task_sets = {mode: set(rows) for mode, rows in results.items()}
    if any(len(tasks) != args.expected_tasks for tasks in task_sets.values()) or len(
        {frozenset(tasks) for tasks in task_sets.values()}
    ) != 1:
        raise SystemExit("mode task sets are incomplete or differ")
    tasks = sorted(next(iter(task_sets.values())))
    missing_baseline = sorted(set(tasks) - set(baseline))
    if missing_baseline:
        raise SystemExit(f"baseline join missing {missing_baseline[0]}")
    timeout_tasks = {task for task in tasks if baseline[task] == "TIMEOUT"}

    output: dict[str, object] = {
        "tasks": len(tasks),
        "baseline-timeout-tasks": len(timeout_tasks),
        "modes": {},
        "comparisons": {},
    }
    for mode, rows in sorted(results.items()):
        completed = [task for task in tasks if (mode, task) in models]
        timeout_completed = [task for task in completed if task in timeout_tasks]
        output["modes"][mode] = {
            "first-models": len(completed),
            "first-models-baseline-timeout": len(timeout_completed),
            "solver-status-histogram": {
                str(status): sum(models[(mode, task)]["solver-status"] == status for task in completed)
                for status in sorted({int(models[(mode, task)]["solver-status"]) for task in completed})
            },
            "assignments": sum(bool(models[(mode, task)]["assignment"]) for task in completed),
            "cpu-s-all": sum(float(row["cpu-s"]) for row in rows.values()),
            "memory-b-sum-all": sum(float(row["memory-b"]) for row in rows.values()),
            "memory-b-peak": max(float(row["memory-b"]) for row in rows.values()),
            "build-us": distribution([float(models[(mode, task)]["build-us"]) for task in completed]),
            "check-us": distribution([float(models[(mode, task)]["check-us"]) for task in completed]),
        }

    control_models = {task for task in tasks if (CONTROL, task) in models}
    for mode in sorted(set(results) - {CONTROL}):
        candidate_models = {task for task in tasks if (mode, task) in models}
        common = sorted(control_models & candidate_models)
        comparison: dict[str, object] = {
            "common-first-models": len(common),
            "control-only-models": sorted(control_models - candidate_models),
            "candidate-only-models": sorted(candidate_models - control_models),
            "solver-status-differences": [
                task for task in common
                if models[(CONTROL, task)]["solver-status"] != models[(mode, task)]["solver-status"]
            ],
            "assignment-differences": [
                task for task in common
                if models[(CONTROL, task)]["assignment"] != models[(mode, task)]["assignment"]
            ],
        }
        for metric in ("cpu-s", "wall-s", "memory-b"):
            ratios = [float(results[mode][task][metric]) / float(results[CONTROL][task][metric])
                      for task in common if float(results[CONTROL][task][metric]) > 0]
            comparison[metric] = {
                "geometric-mean-ratio": geometric_mean(ratios),
                "median-ratio": statistics.median(ratios),
                "sum-ratio": sum(float(results[mode][task][metric]) for task in common) /
                             sum(float(results[CONTROL][task][metric]) for task in common),
                "regressed-tasks": sum(ratio > 1 for ratio in ratios),
            }
        for metric in ("build-us", "check-us"):
            ratios = [float(models[(mode, task)][metric]) /
                      float(models[(CONTROL, task)][metric]) for task in common
                      if float(models[(CONTROL, task)][metric]) > 0]
            comparison[metric] = {
                "geometric-mean-ratio": geometric_mean(ratios),
                "median-ratio": statistics.median(ratios),
                "regressed-tasks": sum(ratio > 1 for ratio in ratios),
            }
        output["comparisons"][mode] = comparison

    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
