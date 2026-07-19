#!/usr/bin/env python3
"""Compare two four-repetition formal result tables without changing their gate."""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import statistics
from pathlib import Path


METRICS = ("cputime", "walltime", "memory")
COUNTERS = (
    "rf_offered", "rf_queued", "co_offered", "co_queued", "back_offered",
    "back_queued", "work_added", "work_popped", "peak_work", "validity_queries",
    "realized_prefixes", "rejected_prefixes",
)


def load(path: Path) -> dict[tuple[str, str, str], dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    return {(row["repetition"], row["variant"], row["task"]): row for row in rows}


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def summarize(values: list[float], seed: int) -> dict[str, object]:
    rng = random.Random(seed)
    estimates = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return {
        "tasks": len(values),
        "geomean_ratio": geomean(values),
        "bootstrap_95ci": [estimates[500], estimates[19_499]],
        "median_ratio": statistics.median(values),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("older", type=Path)
    parser.add_argument("newer", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    old = load(args.older)
    new = load(args.newer)
    if old.keys() != new.keys():
        raise RuntimeError("formal tables do not contain identical cells")
    tasks = sorted({key[2] for key in old})
    repetitions = sorted({key[0] for key in old})

    metrics: dict[str, dict[str, object]] = {}
    for variant_index, variant in enumerate(("baseline", "pruning")):
        for metric_index, metric in enumerate(METRICS):
            ratios = []
            for task in tasks:
                pairs = [(old[(rep, variant, task)], new[(rep, variant, task)])
                         for rep in repetitions]
                if any(before["category"] != after["category"] or
                       before["category"] != "correct" for before, after in pairs):
                    continue
                ratios.append(
                    statistics.median(float(after[metric]) for _, after in pairs) /
                    statistics.median(float(before[metric]) for before, _ in pairs)
                )
            metrics[f"{variant}_{metric}"] = summarize(
                ratios, 20260717 + variant_index * 10 + metric_index
            )

    status_mismatches = []
    counter_mismatches = []
    for key in sorted(old):
        if old[key]["status"] != new[key]["status"]:
            status_mismatches.append([*key, old[key]["status"], new[key]["status"]])
        if key[1] == "pruning":
            differences = {
                counter: [old[key][counter], new[key][counter]]
                for counter in COUNTERS if old[key][counter] != new[key][counter]
            }
            if differences:
                counter_mismatches.append([*key, differences])

    result = {
        "cells": len(old),
        "metrics": metrics,
        "status_mismatches": status_mismatches,
        "pruning_counter_mismatches": counter_mismatches,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "comparison.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    pruning_cpu = metrics["pruning_cputime"]
    baseline_cpu = metrics["baseline_cputime"]
    pruning_wall = metrics["pruning_walltime"]
    pruning_rss = metrics["pruning_memory"]
    (args.output / "comparison.md").write_text(
        f"""# Direct formal comparison

- New/old pruning CPU: {pruning_cpu['geomean_ratio']:.6f}
  [{pruning_cpu['bootstrap_95ci'][0]:.6f}, {pruning_cpu['bootstrap_95ci'][1]:.6f}].
- New/old pruning wall: {pruning_wall['geomean_ratio']:.6f}
  [{pruning_wall['bootstrap_95ci'][0]:.6f}, {pruning_wall['bootstrap_95ci'][1]:.6f}].
- New/old pruning RSS: {pruning_rss['geomean_ratio']:.6f}
  [{pruning_rss['bootstrap_95ci'][0]:.6f}, {pruning_rss['bootstrap_95ci'][1]:.6f}].
- Corresponding baseline CPU drift: {baseline_cpu['geomean_ratio']:.6f}
  [{baseline_cpu['bootstrap_95ci'][0]:.6f}, {baseline_cpu['bootstrap_95ci'][1]:.6f}].
- Cell status differences: {len(status_mismatches)}.
- Pruning cells with any search-counter difference: {len(counter_mismatches)}.
""",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
