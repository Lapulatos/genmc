#!/usr/bin/env python3
"""Analyze simultaneous before/after BenchExec queues for Optimization 05."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import math
import random
import statistics
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from datetime import datetime, timedelta
from pathlib import Path


def strip_unit(value: str, suffix: str) -> str:
    return value[: -len(suffix)] if value.endswith(suffix) else value


def parse_tree(root: Path, variant: str) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    variant_root = root / variant
    for xml_path in sorted(variant_root.rglob("*.xml.bz2")):
        relative = xml_path.relative_to(variant_root)
        model, repetition = relative.parts[:2]
        with bz2.open(xml_path, "rb") as source:
            xml = ET.parse(source).getroot()
        for run in xml.findall("run"):
            columns = {column.get("title", ""): column.get("value", "") for column in run.findall("column")}
            task = run.get("name", "")
            marker = "sv-benchmarks/"
            if marker in task:
                task = task.split(marker, 1)[1]
            rows.append(
                {
                    "variant": variant,
                    "model": model,
                    "repetition": repetition,
                    "task_yaml": task,
                    "expected_verdict": run.get("expectedVerdict", ""),
                    "status": columns.get("status", ""),
                    "category": columns.get("category", ""),
                    "cputime_seconds": strip_unit(columns.get("cputime", ""), "s"),
                    "walltime_seconds": strip_unit(columns.get("walltime", ""), "s"),
                    "memory_bytes": strip_unit(columns.get("memory", ""), "B"),
                    "starttime": columns.get("starttime", ""),
                    "executions": columns.get("executions", ""),
                    "xml": str(xml_path),
                }
            )
    return rows


def key(row: dict[str, str]) -> tuple[str, str, str]:
    return row["model"], row["repetition"], row["task_yaml"]


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int = 20260715) -> list[float]:
    rng = random.Random(seed)
    estimates = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return [estimates[500], estimates[19_499]]


def metric_summary(
    before: list[dict[str, str]], after: list[dict[str, str]], metric: str, strict: bool
) -> dict[str, object]:
    bmap = {key(row): row for row in before}
    amap = {key(row): row for row in after}
    grouped: dict[tuple[str, str], list[tuple[float, float]]] = defaultdict(list)
    for current in sorted(set(bmap) & set(amap)):
        b, a = bmap[current], amap[current]
        if strict and (b["category"] != "correct" or a["category"] != "correct"):
            continue
        if not b[metric] or not a[metric]:
            continue
        grouped[(current[0], current[2])].append((float(b[metric]), float(a[metric])))

    output: dict[str, object] = {}
    for model in ("sc", "tso", "pso", "all"):
        ratios: list[float] = []
        selected_groups = [
            pairs
            for (group_model, _task), pairs in grouped.items()
            if model == "all" or group_model == model
        ]
        expected_repetitions = max(map(len, selected_groups), default=0)
        for (group_model, _task), pairs in grouped.items():
            if model != "all" and group_model != model:
                continue
            if len(pairs) != expected_repetitions:
                continue
            before_median = statistics.median(pair[0] for pair in pairs)
            after_median = statistics.median(pair[1] for pair in pairs)
            if before_median > 0 and after_median > 0:
                ratios.append(after_median / before_median)
        output[model] = {
            "model_task_pairs": len(ratios),
            "required_repetitions": expected_repetitions,
            "after_over_before_geomean": geomean(ratios) if ratios else None,
            "task_bootstrap_95ci": bootstrap(ratios) if ratios else [],
            "median_ratio": statistics.median(ratios) if ratios else None,
        }
    return output


def concurrency(before: list[dict[str, str]], after: list[dict[str, str]]) -> dict[str, int]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in [*before, *after]:
        grouped[(row["model"], row["repetition"])].append(row)
    peaks: dict[str, int] = {}
    for group, rows in grouped.items():
        events: list[tuple[datetime, int]] = []
        for row in rows:
            if not row["starttime"] or not row["walltime_seconds"]:
                continue
            start = datetime.fromisoformat(row["starttime"])
            end = start + timedelta(seconds=float(row["walltime_seconds"]))
            events.extend(((start, 1), (end, -1)))
        active = peak = 0
        for _time, delta in sorted(events, key=lambda item: (item[0], item[1])):
            active += delta
            peak = max(peak, active)
        peaks[f"{group[0]}/{group[1]}"] = peak
    return peaks


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("formal", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    before = parse_tree(args.formal, "before")
    after = parse_tree(args.formal, "after")
    if not before or not after:
        raise SystemExit("missing before/after XML results")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    rows = sorted([*before, *after], key=lambda row: (row["variant"], *key(row)))
    with (args.output_dir / "rows.tsv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    bmap, amap = {key(row): row for row in before}, {key(row): row for row in after}
    common = sorted(set(bmap) & set(amap))
    status_changes = Counter(
        (bmap[item]["status"], amap[item]["status"])
        for item in common
        if bmap[item]["status"] != amap[item]["status"]
    )
    verdict_mismatches = [
        item
        for item in common
        if bmap[item]["category"] == amap[item]["category"] == "correct"
        and bmap[item]["status"] != amap[item]["status"]
    ]
    execution_mismatches = [
        item
        for item in common
        if bmap[item]["category"] == amap[item]["category"] == "correct"
        and bmap[item]["expected_verdict"] == "true"
        and bmap[item]["executions"]
        and amap[item]["executions"]
        and bmap[item]["executions"] != amap[item]["executions"]
    ]
    result = {
        "rows": {"before": len(before), "after": len(after), "paired": len(common)},
        "correct": {
            "before": sum(row["category"] == "correct" for row in before),
            "after": sum(row["category"] == "correct" for row in after),
            "by_model_before": Counter(row["model"] for row in before if row["category"] == "correct"),
            "by_model_after": Counter(row["model"] for row in after if row["category"] == "correct"),
        },
        "status_changes": {f"{old} -> {new}": count for (old, new), count in status_changes.items()},
        "common_solved_verdict_mismatches": len(verdict_mismatches),
        "safe_execution_count_mismatches": len(execution_mismatches),
        "peak_combined_task_overlap": concurrency(before, after),
        "strict_common_solved": {
            metric: metric_summary(before, after, metric, True)
            for metric in ("walltime_seconds", "cputime_seconds", "memory_bytes")
        },
        "all_observed": {
            metric: metric_summary(before, after, metric, False)
            for metric in ("walltime_seconds", "cputime_seconds", "memory_bytes")
        },
    }
    (args.output_dir / "comparison.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
