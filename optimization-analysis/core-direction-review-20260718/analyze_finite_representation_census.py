#!/usr/bin/env python3
"""Summarize solver-free finite-representation logs and join baseline terminals."""

from __future__ import annotations

import argparse
import bz2
import json
import math
import re
import xml.etree.ElementTree as ET
from pathlib import Path


REPRESENTATION_PREFIX = "Finite skeleton representation: "
INTEGER_FIELD = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")


def percentile(values: list[int], fraction: float) -> int:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(fraction * len(ordered)) - 1)]


def task_from_log(path: Path) -> str:
    marker = ".pthread-wmm."
    if marker not in path.name or not path.name.endswith(".log"):
        raise ValueError(f"unexpected logfile name: {path.name}")
    return path.name.split(marker, 1)[1][:-4]


def read_census(root: Path) -> dict[str, dict[str, int]]:
    rows: dict[str, dict[str, int]] = {}
    for path in root.rglob("*.log"):
        for line in path.read_text(errors="replace").splitlines():
            if line.startswith(REPRESENTATION_PREFIX):
                rows[task_from_log(path)] = {
                    key: int(value) for key, value in INTEGER_FIELD.findall(line)
                }
                break
    return rows


def read_terminals(path: Path) -> dict[str, dict[str, object]]:
    with bz2.open(path, "rb") as source:
        root = ET.parse(source).getroot()
    terminals: dict[str, dict[str, object]] = {}
    for run in root.findall("run"):
        columns = {column.get("title"): column.get("value", "") for column in run}
        cpu = columns.get("cputime", "0s")
        terminals[Path(run.get("name", "")).name] = {
            "status": columns.get("status", ""),
            "cputime": float(cpu[:-1]),
        }
    return terminals


def summarize(rows: dict[str, dict[str, int]], tasks: list[str]) -> dict[str, object]:
    selected = [rows[task] for task in tasks]
    fields = sorted(selected[0])
    metrics: dict[str, object] = {}
    for field in fields:
        values = [row[field] for row in selected]
        metrics[field] = {
            "sum": sum(values),
            "p50": percentile(values, 0.50),
            "p90": percentile(values, 0.90),
            "p95": percentile(values, 0.95),
            "p99": percentile(values, 0.99),
            "max": max(values),
            "max-task": tasks[values.index(max(values))],
        }
    return {"tasks": len(tasks), "metrics": metrics}


def summarize_rvf_opportunity(
    rows: dict[str, dict[str, int]], tasks: list[str]
) -> dict[str, object]:
    selectors = sum(rows[task]["rf-selectors"] for task in tasks)
    pairs = sum(rows[task]["rf-pairs"] for task in tasks)

    def layer(prefix: str) -> dict[str, object]:
        classes = sum(rows[task][f"rf-{prefix}-classes"] for task in tasks)
        class_pairs = sum(rows[task][f"rf-{prefix}-class-pairs"] for task in tasks)
        mergeable = sum(rows[task][f"rf-{prefix}-mergeable-sources"] for task in tasks)
        return {
            "tasks-with-opportunity": sum(
                rows[task][f"rf-{prefix}-mergeable-sources"] > 0 for task in tasks
            ),
            "classes": classes,
            "class-pairs": class_pairs,
            "mergeable-sources": mergeable,
            "selector-reduction-fraction": mergeable / selectors if selectors else 0.0,
            "pair-reduction-fraction": 1 - class_pairs / pairs if pairs else 0.0,
        }

    return {
        "tasks": len(tasks),
        "concrete-selectors": selectors,
        "concrete-pairs": pairs,
        "value": layer("value"),
        "value-provenance": layer("value-provenance"),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log_root", type=Path)
    parser.add_argument("baseline_xml", type=Path)
    args = parser.parse_args()

    rows = read_census(args.log_root)
    terminals = read_terminals(args.baseline_xml)
    missing = sorted(set(rows) - set(terminals))
    if missing:
        raise SystemExit(f"baseline join missing {len(missing)} tasks; first={missing[0]}")
    tasks = sorted(rows)
    timeout = [task for task in tasks if terminals[task]["status"] == "TIMEOUT"]
    non_timeout = [task for task in tasks if terminals[task]["status"] != "TIMEOUT"]

    def describe(task: str) -> dict[str, object]:
        return {
            "task": task,
            **terminals[task],
            "rf-pairs": rows[task]["rf-pairs"],
            "co-pairs": rows[task]["co-pairs"],
            "value-bits": rows[task]["value-bits"],
            "max-rf-sources": rows[task]["max-rf-sources"],
            "max-writes-address": rows[task]["max-writes-address"],
        }

    completed = sorted(non_timeout, key=lambda task: terminals[task]["cputime"])
    hard = sorted(
        timeout,
        key=lambda task: (rows[task]["rf-pairs"] + rows[task]["co-pairs"], task),
        reverse=True,
    )
    output = {
        "built-tasks": len(rows),
        "baseline-timeout-built": len(timeout),
        "all-built": summarize(rows, tasks),
        "baseline-timeout": summarize(rows, timeout),
        "baseline-non-timeout": summarize(rows, non_timeout),
        "rvf-class-opportunity": {
            "all-built": summarize_rvf_opportunity(rows, tasks),
            "baseline-timeout": summarize_rvf_opportunity(rows, timeout),
            "baseline-non-timeout": summarize_rvf_opportunity(rows, non_timeout),
        },
        "panel-candidates": {
            "fastest-false": [
                describe(task)
                for task in completed
                if terminals[task]["status"].startswith("false")
            ][:12],
            "fastest-true": [
                describe(task) for task in completed if terminals[task]["status"] == "true"
            ][:12],
            "slowest-completed": [describe(task) for task in reversed(completed[-20:])],
            "largest-timeout": [describe(task) for task in hard[:20]],
        },
    }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
