#!/usr/bin/env python3
"""Analyze the four-repetition Optimization 13 BenchExec matrix."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import math
import random
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter, defaultdict
from pathlib import Path


STAT = re.compile(r"\b([a-z][a-z0-9-]*)=([0-9]+)\b")
LOG_MARKER = ".grouped-materialization."


def number(value: str, suffix: str) -> float:
    return float(value.removesuffix(suffix)) if value else 0.0


def task_name(raw: str) -> str:
    marker = "sv-benchmarks/"
    return raw.split(marker, 1)[1] if marker in raw else raw


def parse_logs(directory: Path, log_marker: str) -> dict[str, dict[str, int]]:
    profiles: dict[str, dict[str, int]] = {}
    archives = list(directory.glob("*.logfiles.zip"))
    if len(archives) != 1:
        raise ValueError(f"expected one log archive under {directory}, found {len(archives)}")
    with zipfile.ZipFile(archives[0]) as archive:
        for member in archive.namelist():
            basename = Path(member).name
            if log_marker not in basename or not basename.endswith(".log"):
                continue
            task = basename.split(log_marker, 1)[1][:-4]
            materialize = 0
            stable = 0
            current_base = 0
            history_base = 0
            final_records = 0
            text = archive.read(member).decode("utf-8", errors="replace")
            for line in text.splitlines():
                if not line.startswith("CAT incremental statistics:"):
                    continue
                values = {key: int(value) for key, value in STAT.findall(line)}
                materialize += values.get("materialize-ns", 0)
                stable = max(stable, values.get("max-stable-events", 0))
                current_base = max(current_base, values.get("max-current-base-bytes", 0))
                history_base = max(history_base, values.get("max-history-base-bytes", 0))
                final_records += 1
            profiles[task] = {
                "materialize_ns": materialize,
                "max_stable_events": stable,
                "max_current_base_bytes": current_base,
                "max_history_base_bytes": history_base,
                "cat_stat_records": final_records,
            }
    return profiles


def parse_variant(formal: Path, variant: str, log_marker: str = LOG_MARKER) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for xml_path in sorted((formal / variant).glob("*/*/*.xml.bz2")):
        model, repetition = xml_path.relative_to(formal / variant).parts[:2]
        profiles = parse_logs(xml_path.parent, log_marker)
        with bz2.open(xml_path, "rb") as source:
            root = ET.parse(source).getroot()
        for run in root.findall("run"):
            columns = {
                column.get("title", ""): column.get("value", "")
                for column in run.findall("column")
            }
            task = task_name(run.get("name", ""))
            profile = profiles.get(Path(task).name, {})
            rows.append(
                {
                    "variant": variant,
                    "model": model,
                    "repetition": repetition,
                    "task": task,
                    "expected_verdict": run.get("expectedVerdict", ""),
                    "status": columns.get("status", ""),
                    "category": columns.get("category", ""),
                    "cpu_s": number(columns.get("cputime", ""), "s"),
                    "wall_s": number(columns.get("walltime", ""), "s"),
                    "rss_bytes": int(number(columns.get("memory", ""), "B")),
                    "executions": columns.get("executions", ""),
                    "materialize_ns": profile.get("materialize_ns", 0),
                    "max_stable_events": profile.get("max_stable_events", 0),
                    "max_current_base_bytes": profile.get("max_current_base_bytes", 0),
                    "max_history_base_bytes": profile.get("max_history_base_bytes", 0),
                    "cat_stat_records": profile.get("cat_stat_records", 0),
                    "xml": str(xml_path),
                }
            )
    return rows


def key(row: dict[str, object]) -> tuple[str, str, str]:
    return str(row["model"]), str(row["repetition"]), str(row["task"])


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int = 20260716) -> list[float]:
    rng = random.Random(seed)
    estimates = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return [estimates[500], estimates[19_499]]


def clustered_metric(
    before: dict[tuple[str, str, str], dict[str, object]],
    after: dict[tuple[str, str, str], dict[str, object]],
    metric: str,
    require_correct: bool = True,
    minimum_events: int = 0,
) -> dict[str, object]:
    groups: dict[tuple[str, str], list[tuple[float, float]]] = defaultdict(list)
    for item in sorted(set(before) & set(after)):
        old, new = before[item], after[item]
        if require_correct and (old["category"] != "correct" or new["category"] != "correct"):
            continue
        if min(int(old["max_stable_events"]), int(new["max_stable_events"])) < minimum_events:
            continue
        old_value, new_value = float(old[metric]), float(new[metric])
        if old_value > 0 and new_value > 0:
            groups[(item[0], item[2])].append((old_value, new_value))

    result: dict[str, object] = {}
    for model in ("sc", "tso", "pso", "all"):
        ratios = []
        selected = {
            group: pairs
            for group, pairs in groups.items()
            if model == "all" or group[0] == model
        }
        for pairs in selected.values():
            if len(pairs) != 4:
                continue
            old_median = statistics.median(pair[0] for pair in pairs)
            new_median = statistics.median(pair[1] for pair in pairs)
            ratios.append(new_median / old_median)
        result[model] = {
            "four_repetition_model_tasks": len(ratios),
            "after_over_before_geomean": geomean(ratios) if ratios else None,
            "task_bootstrap_95ci": bootstrap(ratios) if ratios else [],
            "median_ratio": statistics.median(ratios) if ratios else None,
            "ratios_at_or_below_0_70": sum(ratio <= 0.70 for ratio in ratios),
        }
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("formal", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--log-marker", default=LOG_MARKER)
    parser.add_argument("--include-base-storage", action="store_true")
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)

    before_rows = parse_variant(arguments.formal, "before", arguments.log_marker)
    after_rows = parse_variant(arguments.formal, "after", arguments.log_marker)
    before = {key(row): row for row in before_rows}
    after = {key(row): row for row in after_rows}
    common = sorted(set(before) & set(after))
    if len(before_rows) != 1152 or len(after_rows) != 1152 or len(common) != 1152:
        raise SystemExit(
            f"incomplete matrix: before={len(before_rows)} after={len(after_rows)} paired={len(common)}"
        )

    all_rows = sorted([*before_rows, *after_rows], key=lambda row: (row["variant"], *key(row)))
    with (arguments.output / "rows.tsv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(all_rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(all_rows)

    status_changes = Counter(
        (str(before[item]["status"]), str(after[item]["status"]))
        for item in common
        if before[item]["status"] != after[item]["status"]
    )
    opposite_terminal = [
        item
        for item in common
        if before[item]["category"] == after[item]["category"] == "correct"
        and before[item]["status"] != after[item]["status"]
    ]
    execution_mismatches = [
        item
        for item in common
        if before[item]["category"] == after[item]["category"] == "correct"
        and before[item]["executions"]
        and after[item]["executions"]
        and before[item]["executions"] != after[item]["executions"]
    ]
    result = {
        "rows": {"before": len(before_rows), "after": len(after_rows), "paired": len(common)},
        "status_counts": {
            "before": dict(Counter(str(row["status"]) for row in before_rows)),
            "after": dict(Counter(str(row["status"]) for row in after_rows)),
        },
        "status_changes": {
            f"{old} -> {new}": count for (old, new), count in sorted(status_changes.items())
        },
        "opposite_common_terminal_verdicts": len(opposite_terminal),
        "common_terminal_execution_count_mismatches": len(execution_mismatches),
        "strict_common_terminal": {
            metric: clustered_metric(before, after, metric)
            for metric in ("cpu_s", "wall_s", "rss_bytes")
        },
        "large_materialization": clustered_metric(
            before, after, "materialize_ns", minimum_events=512
        ),
        "profiles": {
            "before": sum(int(row["cat_stat_records"]) > 0 for row in before_rows),
            "after": sum(int(row["cat_stat_records"]) > 0 for row in after_rows),
            "large_before": sum(int(row["max_stable_events"]) >= 512 for row in before_rows),
            "large_after": sum(int(row["max_stable_events"]) >= 512 for row in after_rows),
        },
    }
    if arguments.include_base_storage:
        result["large_current_base_storage"] = clustered_metric(
            before, after, "max_current_base_bytes", minimum_events=512
        )
        result["large_history_base_storage"] = clustered_metric(
            before, after, "max_history_base_bytes", minimum_events=512
        )
        result["large_process_rss"] = clustered_metric(
            before, after, "rss_bytes", minimum_events=512
        )
    (arguments.output / "comparison.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
