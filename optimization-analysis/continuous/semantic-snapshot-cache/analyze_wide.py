#!/usr/bin/env python3
"""Analyze the paired wide semantic-snapshot-cache experiment."""

from __future__ import annotations

import csv
import json
import math
import random
import statistics
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BEFORE = ROOT / "analysis/wide-before.tsv"
AFTER = ROOT / "analysis/wide-after.tsv"
OUTPUT = ROOT / "analysis/wide-comparison.json"


def read(path: Path) -> list[dict[str, str]]:
    with path.open() as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def key(row: dict[str, str]) -> tuple[str, str, str]:
    return row["backend"], row["task_yaml"], row["repetition"]


def solved(row: dict[str, str]) -> bool:
    return row["category"] == "correct"


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def task_medians(rows: list[dict[str, str]], metric: str) -> dict[tuple[str, str], float]:
    groups: dict[tuple[str, str], list[float]] = defaultdict(list)
    for row in rows:
        value = row[metric]
        if value:
            groups[(row["backend"], row["task_yaml"])].append(float(value))
    repetitions = max(map(len, groups.values()), default=0)
    return {
        group: statistics.median(values)
        for group, values in groups.items()
        if len(values) == repetitions
    }


def bootstrap(values: list[float], seed: int = 20260715) -> list[float]:
    rng = random.Random(seed)
    estimates = []
    for _ in range(20_000):
        estimates.append(geomean([rng.choice(values) for _ in values]))
    estimates.sort()
    return [estimates[500], estimates[19_499]]


def metric_summary(before: list[dict[str, str]], after: list[dict[str, str]], metric: str) -> dict:
    bmed = task_medians(before, metric)
    amed = task_medians(after, metric)
    result = {}
    for backend in ["caat-sc", "caat-tso", "caat-pso", "all"]:
        groups = sorted(set(bmed) & set(amed))
        if backend != "all":
            groups = [group for group in groups if group[0] == backend]
        ratios = [amed[group] / bmed[group] for group in groups if bmed[group] > 0 and amed[group] > 0]
        result[backend] = {
            "model_task_pairs": len(ratios),
            "after_over_before_geomean": geomean(ratios),
            "task_bootstrap_95ci": bootstrap(ratios),
            "median_ratio": statistics.median(ratios),
        }
    return result


def main() -> None:
    before = read(BEFORE)
    after = read(AFTER)
    bmap = {key(row): row for row in before}
    amap = {key(row): row for row in after}
    common = sorted(set(bmap) & set(amap))
    status_changes = Counter((bmap[k]["status"], amap[k]["status"]) for k in common if bmap[k]["status"] != amap[k]["status"])
    verdict_mismatches = [k for k in common if solved(bmap[k]) and solved(amap[k]) and bmap[k]["status"] != amap[k]["status"]]
    execution_mismatches = [
        k for k in common
        if solved(bmap[k]) and solved(amap[k])
        and bmap[k]["expected_verdict"] == "true"
        and bmap[k]["executions"] and amap[k]["executions"]
        and bmap[k]["executions"] != amap[k]["executions"]
    ]
    output = {
        "rows": {"before": len(before), "after": len(after), "paired": len(common)},
        "correct": {
            "before": sum(map(solved, before)),
            "after": sum(map(solved, after)),
            "by_backend_before": Counter(row["backend"] for row in before if solved(row)),
            "by_backend_after": Counter(row["backend"] for row in after if solved(row)),
        },
        "status_changes": {f"{old} -> {new}": count for (old, new), count in status_changes.items()},
        "common_solved_verdict_mismatches": len(verdict_mismatches),
        "safe_execution_count_mismatches": len(execution_mismatches),
        "walltime": metric_summary(before, after, "walltime_seconds"),
        "cputime": metric_summary(before, after, "cputime_seconds"),
        "memory": metric_summary(before, after, "memory_bytes"),
    }
    OUTPUT.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n")
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
