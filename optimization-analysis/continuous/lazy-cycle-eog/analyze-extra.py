#!/usr/bin/env python3
"""Analyze lazy-cycle activation, copy cost, and large-graph state from raw logs."""

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
MARKER = ".lazy-cycle."


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int = 20260716) -> list[float]:
    rng = random.Random(seed)
    samples = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return [samples[500], samples[19_499]]


def read_profiles(formal: Path) -> dict[tuple[str, str, str, str], dict[str, int]]:
    result: dict[tuple[str, str, str, str], dict[str, int]] = {}
    for archive_path in sorted(formal.glob("*/*/*/*.logfiles.zip")):
        variant, model, repetition = archive_path.relative_to(formal).parts[:3]
        with zipfile.ZipFile(archive_path) as archive:
            for member in archive.namelist():
                name = Path(member).name
                if MARKER not in name or not name.endswith(".log"):
                    continue
                task = name.split(MARKER, 1)[1][:-4]
                profile = {
                    "copy_ns": 0,
                    "offline_ns": 0,
                    "peak_snapshot_bytes": 0,
                    "max_stable_events": 0,
                    "lazy_cycle_checks": 0,
                    "lazy_edge_candidates": 0,
                    "lazy_unique_edges": 0,
                    "lazy_depth_fallbacks": 0,
                    "records": 0,
                }
                text = archive.read(member).decode("utf-8", errors="replace")
                for line in text.splitlines():
                    if not line.startswith("CAT incremental statistics:"):
                        continue
                    values = {key: int(value) for key, value in STAT.findall(line)}
                    profile["copy_ns"] += values.get("copy-ns", 0)
                    profile["offline_ns"] += values.get("offline-ns", 0)
                    profile["peak_snapshot_bytes"] = max(
                        profile["peak_snapshot_bytes"],
                        values.get("peak-snapshot-equivalent-bytes", 0),
                    )
                    profile["max_stable_events"] = max(
                        profile["max_stable_events"],
                        values.get("max-stable-events", 0),
                    )
                    profile["lazy_cycle_checks"] += values.get("lazy-cycle-checks", 0)
                    profile["lazy_edge_candidates"] += values.get(
                        "lazy-edge-candidates", 0
                    )
                    profile["lazy_unique_edges"] += values.get("lazy-unique-edges", 0)
                    profile["lazy_depth_fallbacks"] += values.get(
                        "lazy-depth-fallbacks", 0
                    )
                    profile["records"] += 1
                result[(variant, model, repetition, task)] = profile
    return result


def paired_metric(
    profiles: dict[tuple[str, str, str, str], dict[str, int]], metric: str
) -> dict[str, object]:
    grouped: dict[tuple[str, str], list[tuple[int, int]]] = defaultdict(list)
    keys = {(model, repetition, task) for _, model, repetition, task in profiles}
    for model, repetition, task in keys:
        before = profiles.get(("before", model, repetition, task), {})
        after = profiles.get(("after", model, repetition, task), {})
        if min(before.get("max_stable_events", 0), after.get("max_stable_events", 0)) < 512:
            continue
        old, new = before.get(metric, 0), after.get(metric, 0)
        if old > 0 and new > 0:
            grouped[(model, task)].append((old, new))
    output: dict[str, object] = {}
    for selected in ("sc", "tso", "pso", "all"):
        ratios = [
            statistics.median(new for _, new in pairs)
            / statistics.median(old for old, _ in pairs)
            for (model, _), pairs in grouped.items()
            if len(pairs) == 4 and (selected == "all" or model == selected)
        ]
        output[selected] = {
            "four_repetition_model_tasks": len(ratios),
            "after_over_before_geomean": geomean(ratios) if ratios else None,
            "task_bootstrap_95ci": bootstrap(ratios) if ratios else [],
        }
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("formal", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    profiles = read_profiles(args.formal)
    columns = [
        "copy_ns",
        "offline_ns",
        "peak_snapshot_bytes",
        "max_stable_events",
        "lazy_cycle_checks",
        "lazy_edge_candidates",
        "lazy_unique_edges",
        "lazy_depth_fallbacks",
        "records",
    ]
    with (args.output / "extra-rows.tsv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out, delimiter="\t")
        writer.writerow(["variant", "model", "repetition", "task", *columns])
        for key, profile in sorted(profiles.items()):
            writer.writerow([*key, *[profile[column] for column in columns]])

    activation: dict[str, dict[str, int | float | None]] = {}
    for model in ("sc", "tso", "pso", "all"):
        selected = [
            profile
            for (variant, current, _, _), profile in profiles.items()
            if variant == "after" and (model == "all" or current == model)
        ]
        candidates = sum(profile["lazy_edge_candidates"] for profile in selected)
        unique = sum(profile["lazy_unique_edges"] for profile in selected)
        activation[model] = {
            "profiles": len(selected),
            "profiles_with_lazy_checks": sum(
                profile["lazy_cycle_checks"] > 0 for profile in selected
            ),
            "lazy_cycle_checks": sum(profile["lazy_cycle_checks"] for profile in selected),
            "lazy_edge_candidates": candidates,
            "lazy_unique_edges": unique,
            "lazy_depth_fallbacks": sum(
                profile["lazy_depth_fallbacks"] for profile in selected
            ),
            "candidate_over_unique": candidates / unique if unique else None,
        }
    result = {
        "profiles": len(profiles),
        "activation": activation,
        "large_copy": paired_metric(profiles, "copy_ns"),
        "large_offline": paired_metric(profiles, "offline_ns"),
        "large_peak_snapshot": paired_metric(profiles, "peak_snapshot_bytes"),
    }
    (args.output / "extra.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
