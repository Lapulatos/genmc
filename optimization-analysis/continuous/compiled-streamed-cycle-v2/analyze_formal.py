#!/usr/bin/env python3

from __future__ import annotations

import bz2
import csv
import json
import math
import random
import statistics
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


VARIANTS = ("before", "after")
MODELS = ("sc", "tso", "pso")
REPETITIONS = ("r01", "r02", "r03", "r04")


def numeric(value: str | None) -> float | None:
    if not value:
        return None
    if value[-1:] in {"s", "B"}:
        value = value[:-1]
    try:
        return float(value)
    except ValueError:
        return None


def terminal(status: str) -> bool:
    return status == "true" or status.startswith("false(")


def verdict(status: str) -> str | None:
    return status if terminal(status) else None


def geomean(values: list[float]) -> float | None:
    return math.exp(statistics.fmean(math.log(value) for value in values)) if values else None


def percentile(values: list[float], probability: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = probability * (len(ordered) - 1)
    lower, upper = math.floor(position), math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1 - weight) + ordered[upper] * weight


def bootstrap_geomean(values: list[float], samples: int = 20_000) -> list[float]:
    if not values:
        return []
    rng = random.Random(20260717)
    estimates: list[float] = []
    for _ in range(samples):
        selected = [values[rng.randrange(len(values))] for _ in values]
        estimates.append(math.exp(statistics.fmean(math.log(value) for value in selected)))
    return [percentile(estimates, 0.025), percentile(estimates, 0.975)]  # type: ignore[list-item]


def task_name(raw: str) -> str:
    marker = "sv-benchmarks/"
    return raw.split(marker, 1)[1] if marker in raw else raw


def load(root: Path) -> list[dict[str, object]]:
    cells: list[dict[str, object]] = []
    for variant in VARIANTS:
        for model in MODELS:
            for repetition in REPETITIONS:
                directory = root / variant / model / repetition
                paths = list(directory.glob("*.xml.bz2"))
                if len(paths) != 1:
                    raise RuntimeError(f"expected one XML in {directory}, found {len(paths)}")
                with bz2.open(paths[0], "rb") as stream:
                    document = ET.parse(stream).getroot()
                for run in document.findall("run"):
                    columns = {
                        column.attrib["title"]: column.attrib.get("value", "")
                        for column in run.findall("column")
                    }
                    cells.append({
                        "variant": variant,
                        "model": model,
                        "repetition": repetition,
                        "task": task_name(run.attrib["name"]),
                        "status": columns.get("status", ""),
                        "category": columns.get("category", ""),
                        "cpu_s": numeric(columns.get("cputime")),
                        "wall_s": numeric(columns.get("walltime")),
                        "memory_bytes": numeric(columns.get("memory")),
                        "executions": columns.get("executions", ""),
                        "snapshot_bytes": columns.get("snapshot-bytes", ""),
                        "base_bytes": columns.get("base-bytes", ""),
                        "lazy_checks": columns.get("lazy-checks", ""),
                        "lazy_candidates": columns.get("lazy-candidates", ""),
                    })
    return cells


def analyze(cells: list[dict[str, object]]) -> tuple[list[dict[str, object]], dict[str, object]]:
    indexed = {
        (cell["variant"], cell["model"], cell["repetition"], cell["task"]): cell
        for cell in cells
    }
    if len(indexed) != len(cells):
        raise RuntimeError("duplicate formal cell")
    pairs: list[dict[str, object]] = []
    for model in MODELS:
        for repetition in REPETITIONS:
            tasks = sorted(
                str(cell["task"]) for cell in cells
                if cell["variant"] == "before" and cell["model"] == model
                and cell["repetition"] == repetition
            )
            for task in tasks:
                left = indexed[("before", model, repetition, task)]
                right = indexed[("after", model, repetition, task)]
                common = terminal(str(left["status"])) and terminal(str(right["status"]))
                pair: dict[str, object] = {
                    "model": model,
                    "repetition": repetition,
                    "task": task,
                    "before_status": left["status"],
                    "after_status": right["status"],
                    "before_category": left["category"],
                    "after_category": right["category"],
                    "common_terminal": common,
                    "before_cpu_s": left["cpu_s"],
                    "after_cpu_s": right["cpu_s"],
                    "before_wall_s": left["wall_s"],
                    "after_wall_s": right["wall_s"],
                    "before_memory_bytes": left["memory_bytes"],
                    "after_memory_bytes": right["memory_bytes"],
                    "before_executions": left["executions"],
                    "after_executions": right["executions"],
                    "before_snapshot_bytes": left["snapshot_bytes"],
                    "after_snapshot_bytes": right["snapshot_bytes"],
                    "before_base_bytes": left["base_bytes"],
                    "after_base_bytes": right["base_bytes"],
                    "before_lazy_checks": left["lazy_checks"],
                    "after_lazy_checks": right["lazy_checks"],
                    "before_lazy_candidates": left["lazy_candidates"],
                    "after_lazy_candidates": right["lazy_candidates"],
                    "terminal_verdict_match": not common or verdict(str(left["status"])) == verdict(str(right["status"])),
                    "execution_match": not common or not left["executions"] or not right["executions"] or left["executions"] == right["executions"],
                    "candidate_match": not left["lazy_candidates"] or not right["lazy_candidates"] or left["lazy_candidates"] == right["lazy_candidates"],
                    "lazy_check_match": not left["lazy_checks"] or not right["lazy_checks"] or left["lazy_checks"] == right["lazy_checks"],
                    "snapshot_match": not left["snapshot_bytes"] or not right["snapshot_bytes"] or left["snapshot_bytes"] == right["snapshot_bytes"],
                    "base_match": not left["base_bytes"] or not right["base_bytes"] or left["base_bytes"] == right["base_bytes"],
                }
                if common:
                    pair["cpu_ratio"] = float(right["cpu_s"]) / float(left["cpu_s"])
                    pair["wall_ratio"] = float(right["wall_s"]) / float(left["wall_s"])
                    pair["memory_ratio"] = float(right["memory_bytes"]) / float(left["memory_bytes"])
                else:
                    pair["cpu_ratio"] = pair["wall_ratio"] = pair["memory_ratio"] = None
                pairs.append(pair)

    task_model_cpu: dict[tuple[str, str], list[tuple[float, float]]] = defaultdict(list)
    task_model_before_cpu: dict[tuple[str, str], list[float]] = defaultdict(list)
    task_model_before_rss: dict[tuple[str, str], list[float]] = defaultdict(list)
    task_model_after_rss: dict[tuple[str, str], list[float]] = defaultdict(list)
    for pair in pairs:
        if not pair["common_terminal"]:
            continue
        key = (str(pair["model"]), str(pair["task"]))
        task_model_cpu[key].append((float(pair["before_cpu_s"]), float(pair["after_cpu_s"])))
        task_model_before_cpu[key].append(float(pair["before_cpu_s"]))
        task_model_before_rss[key].append(float(pair["before_memory_bytes"]))
        task_model_after_rss[key].append(float(pair["after_memory_bytes"]))
    medians = {
        key: statistics.median(after for _, after in values)
        / statistics.median(before for before, _ in values)
        for key, values in task_model_cpu.items() if len(values) == 4
    }
    ci = bootstrap_geomean(list(medians.values()))
    by_model = {
        model: {
            "task_model_count": len([1 for candidate_model, _ in medians if candidate_model == model]),
            "cpu_geomean": geomean([ratio for (candidate_model, _), ratio in medians.items() if candidate_model == model]),
            "cpu_bootstrap_95_ci": bootstrap_geomean(
                [ratio for (candidate_model, _), ratio in medians.items()
                 if candidate_model == model]
            ),
        }
        for model in MODELS
    }

    common = [pair for pair in pairs if pair["common_terminal"]]
    coverage_gains = [pair for pair in pairs if not terminal(str(pair["before_status"])) and terminal(str(pair["after_status"]))]
    coverage_losses = [pair for pair in pairs if terminal(str(pair["before_status"])) and not terminal(str(pair["after_status"]))]
    large_rss_ratios = []
    for key, before_cpu in task_model_before_cpu.items():
        if len(before_cpu) == 4 and key in medians and statistics.median(before_cpu) > 1.0:
            large_rss_ratios.append(max(task_model_after_rss[key]) / max(task_model_before_rss[key]))
    memory_ratios = [float(pair["memory_ratio"]) for pair in common]
    before_memory = [float(pair["before_memory_bytes"]) for pair in common]
    after_memory = [float(pair["after_memory_bytes"]) for pair in common]
    summary: dict[str, object] = {
        "cells": len(cells),
        "pairs": len(pairs),
        "expected_cells": 2304,
        "expected_pairs": 1152,
        "common_terminal_pairs": len(common),
        "terminal_verdict_mismatches": sum(not bool(pair["terminal_verdict_match"]) for pair in pairs),
        "execution_mismatches": sum(not bool(pair["execution_match"]) for pair in pairs),
        "candidate_mismatches": sum(not bool(pair["candidate_match"]) for pair in pairs),
        "lazy_check_mismatches": sum(not bool(pair["lazy_check_match"]) for pair in pairs),
        "snapshot_mismatches": sum(not bool(pair["snapshot_match"]) for pair in pairs),
        "base_mismatches": sum(not bool(pair["base_match"]) for pair in pairs),
        "coverage_gains": len(coverage_gains),
        "coverage_losses": len(coverage_losses),
        "new_oom": sum("OUT OF MEMORY" in str(pair["after_status"]) and "OUT OF MEMORY" not in str(pair["before_status"]) for pair in pairs),
        "task_model_count": len(medians),
        "cpu_geomean_of_task_model_medians": geomean(list(medians.values())),
        "cpu_four_repetition_model_task_bootstrap_95_ci": ci,
        "by_model": by_model,
        "cell_cpu_geomean": geomean([float(pair["cpu_ratio"]) for pair in common]),
        "cell_wall_geomean": geomean([float(pair["wall_ratio"]) for pair in common]),
        "memory_ratio_p90": percentile(memory_ratios, 0.90),
        "memory_p90_candidate_over_baseline": percentile(after_memory, 0.90) / percentile(before_memory, 0.90),
        "large_task_rss_max_ratio": max(large_rss_ratios) if large_rss_ratios else None,
    }
    model_gate = all(float(by_model[model]["cpu_geomean"]) <= 1.01 for model in MODELS)
    summary["formal_pass"] = (
        summary["cells"] == summary["expected_cells"]
        and summary["pairs"] == summary["expected_pairs"]
        and summary["terminal_verdict_mismatches"] == 0
        and summary["execution_mismatches"] == 0
        and summary["candidate_mismatches"] == 0
        and summary["lazy_check_mismatches"] == 0
        and summary["snapshot_mismatches"] == 0
        and summary["base_mismatches"] == 0
        and summary["coverage_losses"] == 0
        and summary["new_oom"] == 0
        and bool(ci) and float(ci[1]) < 1.0
        and model_gate
        and float(summary["memory_ratio_p90"]) <= 1.02
        and float(summary["memory_p90_candidate_over_baseline"]) <= 1.02
        and float(summary["large_task_rss_max_ratio"]) <= 1.02
    )
    return pairs, summary


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: analyze_formal.py <formal-dir> <analysis-dir>")
    root, output = Path(sys.argv[1]), Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    cells = load(root)
    pairs, summary = analyze(cells)
    write_tsv(output / "cells.tsv", cells)
    write_tsv(output / "pairs.tsv", pairs)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    by = summary["by_model"]
    lines = [
        "# P0.7g2 formal summary", "",
        f"- Cells/pairs: {summary['cells']}/{summary['pairs']}",
        f"- Terminal/execution/candidate/lazy mismatches: {summary['terminal_verdict_mismatches']}/{summary['execution_mismatches']}/{summary['candidate_mismatches']}/{summary['lazy_check_mismatches']}",
        f"- Snapshot/base mismatches: {summary['snapshot_mismatches']}/{summary['base_mismatches']}",
        f"- Coverage gains/losses/new OOM: {summary['coverage_gains']}/{summary['coverage_losses']}/{summary['new_oom']}",
        f"- CPU task-model geomean: {summary['cpu_geomean_of_task_model_medians']:.6f}",
        f"- Four-repetition model-task bootstrap 95% CI: [{summary['cpu_four_repetition_model_task_bootstrap_95_ci'][0]:.6f}, {summary['cpu_four_repetition_model_task_bootstrap_95_ci'][1]:.6f}]",
        f"- SC/TSO/PSO CPU: {by['sc']['cpu_geomean']:.6f}/{by['tso']['cpu_geomean']:.6f}/{by['pso']['cpu_geomean']:.6f}",
        f"- RSS cell P90 / P90-of-levels / large-task max: {summary['memory_ratio_p90']:.6f}/{summary['memory_p90_candidate_over_baseline']:.6f}/{summary['large_task_rss_max_ratio']:.6f}",
        f"- Frozen formal decision: {'retain' if summary['formal_pass'] else 'reject'}",
    ]
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
