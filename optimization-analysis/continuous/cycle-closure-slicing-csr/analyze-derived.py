#!/usr/bin/env python3
"""Analyze P0.7a closure-state and evaluator-time counters from BenchExec logs."""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import re
import statistics
import zipfile
from collections import defaultdict
from pathlib import Path


STAT = re.compile(r"\b([a-z][a-z0-9-]*)=([0-9]+)\b")
EXECUTIONS = re.compile(r"Number of complete executions explored:\s*([0-9]+)")
MARKER = ".closure-slicing."


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int = 20260716) -> list[float]:
    rng = random.Random(seed)
    estimates = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return [estimates[500], estimates[19_499]]


def profiles(formal: Path) -> dict[tuple[str, str, str, str], dict[str, int]]:
    result: dict[tuple[str, str, str, str], dict[str, int]] = {}
    for archive_path in sorted(formal.glob("*/*/*/*.logfiles.zip")):
        variant, model, repetition = archive_path.relative_to(formal).parts[:3]
        with zipfile.ZipFile(archive_path) as archive:
            for member in archive.namelist():
                basename = Path(member).name
                if MARKER not in basename or not basename.endswith(".log"):
                    continue
                task = basename.split(MARKER, 1)[1][:-4]
                profile = {
                    "offline_ns": 0,
                    "materialize_ns": 0,
                    "peak_snapshot_bytes": 0,
                    "max_current_base_bytes": 0,
                    "max_stable_events": 0,
                    "records": 0,
                    "executions": -1,
                }
                text = archive.read(member).decode("utf-8", errors="replace")
                matches = EXECUTIONS.findall(text)
                if matches:
                    profile["executions"] = int(matches[-1])
                for line in text.splitlines():
                    if not line.startswith("CAT incremental statistics:"):
                        continue
                    values = {name: int(value) for name, value in STAT.findall(line)}
                    profile["offline_ns"] += values.get("offline-ns", 0)
                    profile["materialize_ns"] += values.get("materialize-ns", 0)
                    profile["peak_snapshot_bytes"] = max(
                        profile["peak_snapshot_bytes"],
                        values.get("peak-snapshot-equivalent-bytes", 0),
                    )
                    profile["max_current_base_bytes"] = max(
                        profile["max_current_base_bytes"],
                        values.get("max-current-base-bytes", 0),
                    )
                    profile["max_stable_events"] = max(
                        profile["max_stable_events"],
                        values.get("max-stable-events", 0),
                    )
                    profile["records"] += 1
                result[(variant, model, repetition, task)] = profile
    return result


def metric(
    data: dict[tuple[str, str, str, str], dict[str, int]], name: str
) -> dict[str, object]:
    grouped: dict[tuple[str, str], list[tuple[int, int]]] = defaultdict(list)
    keys = {(model, repetition, task) for _, model, repetition, task in data}
    for model, repetition, task in sorted(keys):
        before = data.get(("before", model, repetition, task), {})
        after = data.get(("after", model, repetition, task), {})
        if min(before.get("max_stable_events", 0), after.get("max_stable_events", 0)) < 512:
            continue
        old, new = before.get(name, 0), after.get(name, 0)
        if old > 0 and new > 0:
            grouped[(model, task)].append((old, new))

    result: dict[str, object] = {}
    for selected_model in ("sc", "tso", "pso", "all"):
        ratios = []
        for (model, _), pairs in grouped.items():
            if selected_model != "all" and model != selected_model:
                continue
            if len(pairs) != 4:
                continue
            ratios.append(
                statistics.median(new for _, new in pairs)
                / statistics.median(old for old, _ in pairs)
            )
        result[selected_model] = {
            "four_repetition_model_tasks": len(ratios),
            "after_over_before_geomean": geomean(ratios) if ratios else None,
            "task_bootstrap_95ci": bootstrap(ratios) if ratios else [],
            "median_ratio": statistics.median(ratios) if ratios else None,
        }
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("formal", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    data = profiles(args.formal)
    terminal: dict[tuple[str, str, str, str], bool] = {}
    with (args.output / "rows.tsv").open(encoding="utf-8") as source:
        for row in csv.DictReader(source, delimiter="\t"):
            terminal[(row["variant"], row["model"], row["repetition"],
                      Path(row["task"]).name)] = row["category"] == "correct"
    execution_mismatches = []
    for model, repetition, task in sorted(
        {(model, repetition, task) for _, model, repetition, task in data}
    ):
        before_key = ("before", model, repetition, task)
        after_key = ("after", model, repetition, task)
        if not terminal.get(before_key) or not terminal.get(after_key):
            continue
        old = data.get(before_key, {}).get("executions", -1)
        new = data.get(after_key, {}).get("executions", -1)
        if old < 0 or new < 0 or old != new:
            execution_mismatches.append({
                "model": model, "repetition": repetition, "task": task,
                "before": old, "after": new,
            })
    with (args.output / "derived-rows.tsv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out, delimiter="\t")
        writer.writerow(["variant", "model", "repetition", "task", "offline_ns",
                         "materialize_ns", "peak_snapshot_bytes",
                         "max_current_base_bytes", "max_stable_events", "records",
                         "executions"])
        for key, value in sorted(data.items()):
            writer.writerow([*key, *[value[name] for name in (
                "offline_ns", "materialize_ns", "peak_snapshot_bytes",
                "max_current_base_bytes", "max_stable_events", "records",
                "executions")]])
    result = {
        "profiles": len(data),
        "profiles_with_statistics": sum(value["records"] > 0 for value in data.values()),
        "common_terminal_execution_mismatches": execution_mismatches,
        "large_offline": metric(data, "offline_ns"),
        "large_materialization": metric(data, "materialize_ns"),
        "large_peak_snapshot": metric(data, "peak_snapshot_bytes"),
        "large_current_base": metric(data, "max_current_base_bytes"),
    }
    (args.output / "derived.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
