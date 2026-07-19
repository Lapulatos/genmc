#!/usr/bin/env python3

from __future__ import annotations

import bz2
import csv
import json
import math
from pathlib import Path
import random
import re
import statistics
import sys
import xml.etree.ElementTree as ET
import zipfile


ROOT = (
    Path(sys.argv[1]).resolve()
    if len(sys.argv) == 2
    else Path(__file__).resolve().parent / "formal-v3"
)
VARIANTS = ("baseline", "candidate")
MODELS = ("sc", "tso", "pso")
REPETITIONS = ("r01", "r02", "r03", "r04")
STAT_KEYS = (
    "lazy-cycle-checks",
    "reasoner-calls",
    "reasoner-ns",
    "reasoner-table-slots",
    "reasoner-derived-candidates",
    "reasoner-composition-middles",
    "provenance-materializations",
    "provenance-materialization-ns",
    "provenance-materialization-ops",
    "nogood-learn-attempts",
    "nogood-explanation-warmup-skips",
    "nogood-learned",
    "nogood-match-queries",
    "nogood-literal-checks",
    "nogood-hits",
    "nogood-maximum-bytes",
)


def numeric(value: str | None, suffix: str = "") -> float | None:
    if value is None:
        return None
    if suffix and value.endswith(suffix):
        value = value[: -len(suffix)]
    try:
        return float(value)
    except ValueError:
        return None


def terminal(status: str) -> bool:
    return status == "true" or status.startswith("false(")


def terminal_verdict(status: str) -> str | None:
    if status == "true":
        return "true"
    if status.startswith("false("):
        return status
    return None


def task_name(raw: str) -> str:
    return Path(raw).name


def log_metrics(directory: Path) -> dict[str, dict[str, int]]:
    archive = next(directory.glob("*.logfiles.zip"))
    result: dict[str, dict[str, int]] = {}
    with zipfile.ZipFile(archive) as logs:
        for name in logs.namelist():
            if not name.endswith(".log"):
                continue
            text = logs.read(name).decode("utf-8", errors="replace")
            key = Path(name).name.removesuffix(".log")
            if ".lazy-cycle." in key:
                key = key.split(".lazy-cycle.", 1)[1]
            values: dict[str, int] = {}
            executions = re.findall(
                r"Number of complete executions explored:\s*([0-9]+)", text
            )
            if executions:
                values["executions"] = int(executions[-1])
            for metric in STAT_KEYS:
                matches = re.findall(rf"\b{re.escape(metric)}=([0-9]+)", text)
                if matches:
                    values[metric] = sum(map(int, matches))
            result[key] = values
    return result


def load_cells() -> list[dict[str, object]]:
    cells: list[dict[str, object]] = []
    for variant in VARIANTS:
        for model in MODELS:
            for repetition in REPETITIONS:
                directory = ROOT / variant / model / repetition
                xml_path = next(directory.glob("*.xml.bz2"))
                root = ET.fromstring(bz2.open(xml_path, "rb").read())
                logs = log_metrics(directory)
                for run in root.findall("run"):
                    columns = {
                        column.attrib["title"]: column.attrib.get("value", "")
                        for column in run.findall("column")
                    }
                    task = task_name(run.attrib["name"])
                    metrics = logs.get(task, {})
                    cell: dict[str, object] = {
                        "variant": variant,
                        "model": model,
                        "repetition": repetition,
                        "task": task,
                        "status": columns.get("status", ""),
                        "category": columns.get("category", ""),
                        "cpu_s": numeric(columns.get("cputime"), "s"),
                        "wall_s": numeric(columns.get("walltime"), "s"),
                        "memory_bytes": numeric(columns.get("memory"), "B"),
                        "returnvalue": columns.get("returnvalue", ""),
                        "executions": metrics.get("executions"),
                    }
                    for metric in STAT_KEYS:
                        cell[metric] = metrics.get(metric)
                    cells.append(cell)
    return cells


def geometric_mean(values: list[float]) -> float | None:
    if not values:
        return None
    return math.exp(statistics.fmean(math.log(value) for value in values))


def percentile(values: list[float], probability: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = probability * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1 - weight) + ordered[upper] * weight


def bootstrap_task_clusters(
    ratios: dict[str, list[float]], samples: int = 10_000
) -> tuple[float, float]:
    tasks = sorted(ratios)
    rng = random.Random(20260717)
    estimates: list[float] = []
    for _ in range(samples):
        selected = [tasks[rng.randrange(len(tasks))] for _ in tasks]
        logs = [math.log(value) for task in selected for value in ratios[task]]
        estimates.append(math.exp(statistics.fmean(logs)))
    return percentile(estimates, 0.025), percentile(estimates, 0.975)  # type: ignore[return-value]


def analyze(cells: list[dict[str, object]]) -> tuple[list[dict[str, object]], dict[str, object]]:
    indexed = {
        (cell["variant"], cell["model"], cell["repetition"], cell["task"]): cell
        for cell in cells
    }
    pairs: list[dict[str, object]] = []
    for model in MODELS:
        for repetition in REPETITIONS:
            tasks = sorted(
                cell["task"]
                for cell in cells
                if cell["variant"] == "baseline"
                and cell["model"] == model
                and cell["repetition"] == repetition
            )
            for task in tasks:
                baseline = indexed[("baseline", model, repetition, task)]
                candidate = indexed[("candidate", model, repetition, task)]
                baseline_terminal = terminal(str(baseline["status"]))
                candidate_terminal = terminal(str(candidate["status"]))
                common_terminal = baseline_terminal and candidate_terminal
                pair: dict[str, object] = {
                    "model": model,
                    "repetition": repetition,
                    "task": task,
                    "baseline_status": baseline["status"],
                    "candidate_status": candidate["status"],
                    "baseline_category": baseline["category"],
                    "candidate_category": candidate["category"],
                    "baseline_terminal": baseline_terminal,
                    "candidate_terminal": candidate_terminal,
                    "common_terminal": common_terminal,
                    "terminal_verdict_match": (
                        not common_terminal
                        or terminal_verdict(str(baseline["status"]))
                        == terminal_verdict(str(candidate["status"]))
                    ),
                    "baseline_cpu_s": baseline["cpu_s"],
                    "candidate_cpu_s": candidate["cpu_s"],
                    "baseline_wall_s": baseline["wall_s"],
                    "candidate_wall_s": candidate["wall_s"],
                    "baseline_memory_bytes": baseline["memory_bytes"],
                    "candidate_memory_bytes": candidate["memory_bytes"],
                    "baseline_executions": baseline["executions"],
                    "candidate_executions": candidate["executions"],
                }
                if common_terminal:
                    pair["cpu_ratio"] = float(candidate["cpu_s"]) / float(
                        baseline["cpu_s"]
                    )
                    pair["wall_ratio"] = float(candidate["wall_s"]) / float(
                        baseline["wall_s"]
                    )
                    pair["memory_ratio"] = float(candidate["memory_bytes"]) / float(
                        baseline["memory_bytes"]
                    )
                else:
                    pair["cpu_ratio"] = None
                    pair["wall_ratio"] = None
                    pair["memory_ratio"] = None
                pair["execution_match"] = (
                    not common_terminal
                    or baseline["executions"] is None
                    or candidate["executions"] is None
                    or baseline["executions"] == candidate["executions"]
                )
                pairs.append(pair)

    common = [pair for pair in pairs if pair["common_terminal"]]
    task_model_ratios: dict[tuple[str, str], list[float]] = {}
    task_cluster_ratios: dict[str, list[float]] = {}
    for pair in common:
        ratio = float(pair["cpu_ratio"])
        task_model_ratios.setdefault((str(pair["model"]), str(pair["task"])), []).append(
            ratio
        )
    medians = {
        key: statistics.median(values) for key, values in task_model_ratios.items()
    }
    for (model, task), ratio in medians.items():
        task_cluster_ratios.setdefault(task, []).append(ratio)
    ci_low, ci_high = bootstrap_task_clusters(task_cluster_ratios)

    by_model: dict[str, object] = {}
    for model in MODELS:
        model_medians = [
            ratio for (candidate_model, _), ratio in medians.items() if candidate_model == model
        ]
        by_model[model] = {
            "task_model_count": len(model_medians),
            "cpu_geomean_of_task_medians": geometric_mean(model_medians),
        }

    baseline_timeout_candidate_terminal = [
        pair
        for pair in pairs
        if not pair["baseline_terminal"] and pair["candidate_terminal"]
    ]
    candidate_timeout_baseline_terminal = [
        pair
        for pair in pairs
        if pair["baseline_terminal"] and not pair["candidate_terminal"]
    ]
    terminal_verdict_mismatches = [
        pair for pair in pairs if not pair["terminal_verdict_match"]
    ]
    execution_mismatches = [pair for pair in pairs if not pair["execution_match"]]
    memory_baseline = [float(pair["baseline_memory_bytes"]) for pair in common]
    memory_candidate = [float(pair["candidate_memory_bytes"]) for pair in common]
    memory_ratios = [float(pair["memory_ratio"]) for pair in common]

    candidate_stats: dict[str, int] = {metric: 0 for metric in STAT_KEYS}
    candidate_stat_logs = 0
    learned_tasks: set[tuple[str, str, str]] = set()
    for cell in cells:
        if cell["variant"] != "candidate":
            continue
        if any(cell[metric] is not None for metric in STAT_KEYS):
            candidate_stat_logs += 1
        for metric in STAT_KEYS:
            if cell[metric] is not None:
                candidate_stats[metric] += int(cell[metric])
        if cell["nogood-learned"]:
            learned_tasks.add(
                (str(cell["model"]), str(cell["repetition"]), str(cell["task"]))
            )

    summary: dict[str, object] = {
        "cells": len(cells),
        "pairs": len(pairs),
        "expected_cells": 2 * 3 * 4 * 96,
        "expected_pairs": 3 * 4 * 96,
        "common_terminal_pairs": len(common),
        "terminal_verdict_mismatches": len(terminal_verdict_mismatches),
        "execution_mismatches": len(execution_mismatches),
        "baseline_nonterminal_candidate_terminal": len(
            baseline_timeout_candidate_terminal
        ),
        "candidate_nonterminal_baseline_terminal": len(
            candidate_timeout_baseline_terminal
        ),
        "task_model_count": len(medians),
        "cpu_geomean_of_task_model_medians": geometric_mean(list(medians.values())),
        "cpu_task_clustered_bootstrap_95_ci": [ci_low, ci_high],
        "by_model": by_model,
        "cell_cpu_geomean": geometric_mean(
            [float(pair["cpu_ratio"]) for pair in common]
        ),
        "cell_wall_geomean": geometric_mean(
            [float(pair["wall_ratio"]) for pair in common]
        ),
        "memory_ratio_geomean": geometric_mean(memory_ratios),
        "memory_ratio_p90": percentile(memory_ratios, 0.90),
        "baseline_memory_p90_bytes": percentile(memory_baseline, 0.90),
        "candidate_memory_p90_bytes": percentile(memory_candidate, 0.90),
        "memory_p90_candidate_over_baseline": (
            percentile(memory_candidate, 0.90) / percentile(memory_baseline, 0.90)
        ),
        "candidate_stat_logs": candidate_stat_logs,
        "candidate_learned_task_repetitions": len(learned_tasks),
        "candidate_stats": candidate_stats,
        "coverage_gains": [
            {
                "model": pair["model"],
                "repetition": pair["repetition"],
                "task": pair["task"],
                "baseline_status": pair["baseline_status"],
                "candidate_status": pair["candidate_status"],
            }
            for pair in baseline_timeout_candidate_terminal
        ],
        "coverage_losses": [
            {
                "model": pair["model"],
                "repetition": pair["repetition"],
                "task": pair["task"],
                "baseline_status": pair["baseline_status"],
                "candidate_status": pair["candidate_status"],
            }
            for pair in candidate_timeout_baseline_terminal
        ],
    }
    return pairs, summary


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    cells = load_cells()
    pairs, summary = analyze(cells)
    write_tsv(ROOT / "formal-cells.tsv", cells)
    write_tsv(ROOT / "formal-pairs.tsv", pairs)
    (ROOT / "formal-summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
