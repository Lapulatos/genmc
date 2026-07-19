#!/usr/bin/env python3

from __future__ import annotations

import bz2
import csv
import json
import statistics
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


def numeric(value: str | None) -> float | None:
    if not value:
        return None
    for suffix in ("s", "B"):
        if value.endswith(suffix):
            value = value[: -len(suffix)]
            break
    try:
        return float(value)
    except ValueError:
        return None


def load(root: Path) -> dict[tuple[str, str, str], dict[str, str]]:
    rows: dict[tuple[str, str, str], dict[str, str]] = {}
    for path in sorted(root.glob("*/*.xml.bz2")):
        variant = path.parts[-3]
        repetition = path.parts[-2]
        with bz2.open(path, "rb") as stream:
            document = ET.parse(stream).getroot()
        name = document.attrib["name"]
        model = name.split(".", 1)[0].removeprefix("caat-")
        for run in document.findall("run"):
            task = Path(run.attrib["name"]).name
            columns = {
                column.attrib["title"]: column.attrib.get("value", "")
                for column in run.findall("column")
            }
            key = (repetition, model, task)
            if key in rows:
                raise RuntimeError(f"duplicate cell: {variant}/{key}")
            rows[key] = {"variant": variant, **columns}
    return rows


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: analyze_pilot.py <pilot-dir> <analysis-dir>")
    pilot = Path(sys.argv[1])
    output = Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    before = load(pilot / "before")
    after = load(pilot / "after")
    if before.keys() != after.keys():
        missing_before = sorted(after.keys() - before.keys())
        missing_after = sorted(before.keys() - after.keys())
        raise RuntimeError(
            f"unpaired cells: missing-before={missing_before}, missing-after={missing_after}"
        )

    fields = [
        "repetition", "model", "task", "before_status", "after_status",
        "before_category", "after_category", "before_cpu", "after_cpu", "cpu_ratio",
        "before_rss", "after_rss", "rss_ratio", "before_executions", "after_executions",
        "before_snapshot", "after_snapshot", "before_base", "after_base",
        "before_lazy_checks", "after_lazy_checks", "before_candidates", "after_candidates",
    ]
    pairs: list[dict[str, object]] = []
    status_mismatches = 0
    category_mismatches = 0
    execution_mismatches = 0
    candidate_mismatches = 0
    snapshot_mismatches = 0
    base_mismatches = 0
    coverage_losses = 0
    grouped: dict[tuple[str, str], list[dict[str, object]]] = defaultdict(list)

    for repetition, model, task in sorted(before):
        left = before[(repetition, model, task)]
        right = after[(repetition, model, task)]
        left_cpu, right_cpu = numeric(left.get("cputime")), numeric(right.get("cputime"))
        left_rss, right_rss = numeric(left.get("memory")), numeric(right.get("memory"))
        row: dict[str, object] = {
            "repetition": repetition,
            "model": model,
            "task": task,
            "before_status": left.get("status", ""),
            "after_status": right.get("status", ""),
            "before_category": left.get("category", ""),
            "after_category": right.get("category", ""),
            "before_cpu": left_cpu,
            "after_cpu": right_cpu,
            "cpu_ratio": right_cpu / left_cpu if left_cpu and right_cpu else None,
            "before_rss": left_rss,
            "after_rss": right_rss,
            "rss_ratio": right_rss / left_rss if left_rss and right_rss else None,
            "before_executions": left.get("executions", ""),
            "after_executions": right.get("executions", ""),
            "before_snapshot": left.get("snapshot-bytes", ""),
            "after_snapshot": right.get("snapshot-bytes", ""),
            "before_base": left.get("base-bytes", ""),
            "after_base": right.get("base-bytes", ""),
            "before_lazy_checks": left.get("lazy-checks", ""),
            "after_lazy_checks": right.get("lazy-checks", ""),
            "before_candidates": left.get("lazy-candidates", ""),
            "after_candidates": right.get("lazy-candidates", ""),
        }
        pairs.append(row)
        grouped[(model, task)].append(row)
        status_mismatches += row["before_status"] != row["after_status"]
        category_mismatches += row["before_category"] != row["after_category"]
        if row["before_executions"] and row["after_executions"]:
            execution_mismatches += row["before_executions"] != row["after_executions"]
        if row["before_candidates"] and row["after_candidates"]:
            candidate_mismatches += row["before_candidates"] != row["after_candidates"]
        if row["before_snapshot"] and row["after_snapshot"]:
            snapshot_mismatches += row["before_snapshot"] != row["after_snapshot"]
        if row["before_base"] and row["after_base"]:
            base_mismatches += row["before_base"] != row["after_base"]
        baseline_terminal = row["before_status"] not in {"TIMEOUT", "OUT OF MEMORY", ""}
        candidate_terminal = row["after_status"] not in {"TIMEOUT", "OUT OF MEMORY", ""}
        coverage_losses += baseline_terminal and not candidate_terminal

    with (output / "pairs.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(pairs)

    task_summaries: dict[str, dict[str, object]] = {}
    pilot_pass = True
    for (model, task), rows in sorted(grouped.items()):
        cpu = [float(row["cpu_ratio"]) for row in rows if row["cpu_ratio"] is not None]
        rss = [float(row["rss_ratio"]) for row in rows if row["rss_ratio"] is not None]
        before_rss = [float(row["before_rss"]) for row in rows if row["before_rss"] is not None]
        after_rss = [float(row["after_rss"]) for row in rows if row["after_rss"] is not None]
        summary = {
            "cpu_ratios": cpu,
            "rss_ratios": rss,
            "cpu_median": statistics.median(cpu) if cpu else None,
            "rss_median": statistics.median(rss) if rss else None,
            "before_rss_max": max(before_rss) if before_rss else None,
            "after_rss_max": max(after_rss) if after_rss else None,
            "rss_max_ratio": max(after_rss) / max(before_rss)
            if before_rss and after_rss else None,
        }
        task_summaries[f"{model}/{task}"] = summary
        if summary["cpu_median"] is not None and summary["cpu_median"] > 1.03:
            pilot_pass = False
        if summary["rss_median"] is not None and summary["rss_median"] > 1.02:
            pilot_pass = False
        if summary["rss_max_ratio"] is not None and summary["rss_max_ratio"] > 1.02:
            pilot_pass = False

    counters_ok = all(value == 0 for value in (
        status_mismatches, category_mismatches, execution_mismatches,
        candidate_mismatches, snapshot_mismatches, base_mismatches, coverage_losses,
    ))
    speed_witness = any(
        summary["cpu_median"] is not None and summary["cpu_median"] <= 0.98 and
        any(float(row["before_cpu"]) > 1.0 for row in grouped[tuple(key.split("/", 1))]
            if row["before_cpu"] is not None)
        for key, summary in task_summaries.items()
    )
    pilot_pass = pilot_pass and counters_ok and speed_witness
    summary = {
        "cells": len(pairs),
        "status_mismatches": status_mismatches,
        "category_mismatches": category_mismatches,
        "execution_mismatches": execution_mismatches,
        "candidate_mismatches": candidate_mismatches,
        "snapshot_mismatches": snapshot_mismatches,
        "base_mismatches": base_mismatches,
        "coverage_losses": coverage_losses,
        "speed_witness": speed_witness,
        "pilot_pass": pilot_pass,
        "tasks": task_summaries,
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")

    lines = [
        "# P0.7g2 pilot summary", "",
        f"- Paired cells: {len(pairs)}",
        f"- Status/category/execution mismatches: {status_mismatches}/{category_mismatches}/{execution_mismatches}",
        f"- Candidate/snapshot/base mismatches: {candidate_mismatches}/{snapshot_mismatches}/{base_mismatches}",
        f"- Coverage losses: {coverage_losses}",
        f"- Speed witness: {speed_witness}",
        f"- Frozen pilot decision: {'advance' if pilot_pass else 'reject'}", "",
        "| Model/task | CPU median | RSS median | RSS max ratio |",
        "|---|---:|---:|---:|",
    ]
    for key, values in task_summaries.items():
        def show(value: object) -> str:
            return "-" if value is None else f"{float(value):.6f}"
        lines.append(
            f"| {key} | {show(values['cpu_median'])} | {show(values['rss_median'])} | {show(values['rss_max_ratio'])} |"
        )
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
