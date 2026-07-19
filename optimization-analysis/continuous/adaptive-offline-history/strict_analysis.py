#!/usr/bin/env python3
"""Strict task-clustered analysis for Optimization 05b."""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import statistics
from collections import defaultdict
from pathlib import Path


METRICS = {
    "cpu": "cputime_seconds",
    "wall": "walltime_seconds",
    "memory": "memory_bytes",
}
MODELS = ("sc", "tso", "pso")


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def quantile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return math.nan
    position = fraction * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def bootstrap_geomean(values: list[float], seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    estimates = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return estimates[500], estimates[19_499]


def exact_sign_p(values: list[float]) -> tuple[int, int, int, float]:
    positive = sum(value > 1.0 for value in values)
    negative = sum(value < 1.0 for value in values)
    ties = len(values) - positive - negative
    n = positive + negative
    if n == 0:
        return positive, negative, ties, 1.0
    tail = min(positive, negative)
    probability = sum(math.comb(n, k) for k in range(tail + 1)) / (2**n)
    return positive, negative, ties, min(1.0, 2.0 * probability)


def holm(p_values: dict[str, float]) -> dict[str, float]:
    ordered = sorted(p_values, key=p_values.get)
    adjusted: dict[str, float] = {}
    running = 0.0
    total = len(ordered)
    for rank, name in enumerate(ordered):
        running = max(running, (total - rank) * p_values[name])
        adjusted[name] = min(1.0, running)
    return adjusted


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def task_ratios(rows: list[dict[str, str]], metric: str) -> dict[str, dict[str, float]]:
    by_variant = {
        variant: {
            (row["model"], row["repetition"], row["task_yaml"]): row
            for row in rows
            if row["variant"] == variant
        }
        for variant in ("before", "after")
    }
    paired: dict[tuple[str, str], list[tuple[float, float]]] = defaultdict(list)
    for key in sorted(set(by_variant["before"]) & set(by_variant["after"])):
        before, after = by_variant["before"][key], by_variant["after"][key]
        if before["category"] != "correct" or after["category"] != "correct":
            continue
        if not before[metric] or not after[metric]:
            continue
        paired[(key[0], key[2])].append((float(before[metric]), float(after[metric])))
    result: dict[str, dict[str, float]] = {model: {} for model in MODELS}
    for (model, task), observations in paired.items():
        if len(observations) != 6:
            continue
        before_median = statistics.median(value[0] for value in observations)
        after_median = statistics.median(value[1] for value in observations)
        if before_median > 0 and after_median > 0:
            result[model][task] = after_median / before_median
    return result


def summarize(values: list[float], seed: int) -> dict[str, object]:
    positive, negative, ties, p_value = exact_sign_p(values)
    low, high = bootstrap_geomean(values, seed)
    return {
        "tasks": len(values),
        "geomean_ratio": geomean(values),
        "bootstrap_95ci": [low, high],
        "mean_ratio": statistics.fmean(values),
        "std_ratio": statistics.stdev(values) if len(values) > 1 else 0.0,
        "median_ratio": statistics.median(values),
        "iqr_ratio": [quantile(values, 0.25), quantile(values, 0.75)],
        "log_ratio_std": statistics.stdev(map(math.log, values)) if len(values) > 1 else 0.0,
        "faster_over_2pct": sum(value < 0.98 for value in values),
        "within_2pct": sum(0.98 <= value <= 1.02 for value in values),
        "slower_over_2pct": sum(value > 1.02 for value in values),
        "sign_test": {
            "after_slower": positive,
            "after_faster": negative,
            "ties": ties,
            "two_sided_p": p_value,
        },
    }


def profile_pairs(before_path: Path, after_path: Path) -> list[dict[str, object]]:
    before = {row["log"]: row for row in read_rows(before_path)}
    after = {row["log"]: row for row in read_rows(after_path)}
    fields = (
        "max_history_base_bytes",
        "retained_snapshot_equivalent_bytes",
        "peak_snapshot_equivalent_bytes",
    )
    rows: list[dict[str, object]] = []
    for log in sorted(before.keys() & after.keys()):
        row: dict[str, object] = {"task_log": log}
        for field in fields:
            row[f"before_{field}"] = int(before[log].get(field, 0))
            row[f"after_{field}"] = int(after[log].get(field, 0))
        rows.append(row)
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("rows", type=Path)
    parser.add_argument("profile_before", type=Path)
    parser.add_argument("profile_after", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    rows = read_rows(args.rows)
    all_ratios: list[dict[str, object]] = []
    output: dict[str, object] = {"design": {"repetitions": 6, "bootstrap_samples": 20_000}}
    for metric_index, (metric_name, column) in enumerate(METRICS.items()):
        ratios = task_ratios(rows, column)
        common_tasks = sorted(set.intersection(*(set(ratios[model]) for model in MODELS)))
        task_aggregate = {
            task: geomean([ratios[model][task] for model in MODELS])
            for task in common_tasks
        }
        groups = {**ratios, "all": task_aggregate}
        summaries = {
            group: summarize(list(groups[group].values()), 20260715 + metric_index * 10 + index)
            for index, group in enumerate(("all", *MODELS))
        }
        adjusted = holm(
            {group: summaries[group]["sign_test"]["two_sided_p"] for group in summaries}
        )
        for group in summaries:
            summaries[group]["sign_test"]["holm_p_across_four_groups"] = adjusted[group]
        output[metric_name] = summaries
        for group, values in groups.items():
            for task, ratio in values.items():
                all_ratios.append(
                    {"metric": metric_name, "group": group, "task_yaml": task, "ratio": ratio}
                )

    profile = profile_pairs(args.profile_before, args.profile_after)
    profile_summary: dict[str, object] = {"common_tasks": len(profile)}
    for field in (
        "max_history_base_bytes",
        "retained_snapshot_equivalent_bytes",
        "peak_snapshot_equivalent_bytes",
    ):
        pairs = [(int(row[f"before_{field}"]), int(row[f"after_{field}"])) for row in profile]
        profile_summary[field] = {
            "before_sum": sum(before for before, _after in pairs),
            "after_sum": sum(after for _before, after in pairs),
            "decreased": sum(after < before for before, after in pairs),
            "same": sum(after == before for before, after in pairs),
            "increased": sum(after > before for before, after in pairs),
            "before_nonzero": sum(before > 0 for before, _after in pairs),
            "after_nonzero": sum(after > 0 for _before, after in pairs),
            "max_absolute_reduction": max((before - after for before, after in pairs), default=0),
        }
    output["profile"] = profile_summary

    with (args.output_dir / "ratios.tsv").open("w", newline="", encoding="utf-8") as sink:
        writer = csv.DictWriter(sink, fieldnames=("metric", "group", "task_yaml", "ratio"), delimiter="\t")
        writer.writeheader()
        writer.writerows(all_ratios)
    with (args.output_dir / "profile-pairs.tsv").open("w", newline="", encoding="utf-8") as sink:
        writer = csv.DictWriter(sink, fieldnames=list(profile[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(profile)
    with (args.output_dir / "metric-summary.tsv").open("w", newline="", encoding="utf-8") as sink:
        fields = ("metric", "group", "tasks", "geomean_ratio", "ci_low", "ci_high", "median_ratio", "sign_p", "holm_p")
        writer = csv.DictWriter(sink, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        for metric in METRICS:
            for group in ("all", *MODELS):
                summary = output[metric][group]
                writer.writerow({
                    "metric": metric,
                    "group": group,
                    "tasks": summary["tasks"],
                    "geomean_ratio": summary["geomean_ratio"],
                    "ci_low": summary["bootstrap_95ci"][0],
                    "ci_high": summary["bootstrap_95ci"][1],
                    "median_ratio": summary["median_ratio"],
                    "sign_p": summary["sign_test"]["two_sided_p"],
                    "holm_p": summary["sign_test"]["holm_p_across_four_groups"],
                })
    (args.output_dir / "stats-strict.json").write_text(
        json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
