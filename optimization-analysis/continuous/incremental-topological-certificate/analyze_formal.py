#!/usr/bin/env python3

from __future__ import annotations

import bz2
import importlib.util
import json
import math
import re
import statistics
import sys
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict
from pathlib import Path


HERE = Path(__file__).resolve().parent
BASE_PATH = HERE.parent / "compiled-streamed-cycle-v2" / "analyze_formal.py"
SPEC = importlib.util.spec_from_file_location("p07g2_formal", BASE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load shared formal analyzer: {BASE_PATH}")
base = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(base)

PATTERNS = {
    "executions": r"Number of complete executions explored: ([0-9]+)",
    "snapshot-bytes": r"peak-snapshot-equivalent-bytes=([0-9]+)",
    "base-bytes": r"max-current-base-bytes=([0-9]+)",
    "lazy-checks": r"lazy-cycle-checks=([0-9]+)",
    "lazy-candidates": r"lazy-edge-candidates=([0-9]+)",
}


def load_logs(directory: Path) -> dict[str, str]:
    archives = list(directory.glob("*.logfiles.zip"))
    if len(archives) != 1:
        raise RuntimeError(f"expected one log archive in {directory}, found {len(archives)}")
    result: dict[str, str] = {}
    with zipfile.ZipFile(archives[0]) as archive:
        for member in archive.namelist():
            if not member.endswith(".log"):
                continue
            name = Path(member).name
            task = name.split(".", 2)[-1].removesuffix(".log")
            if task in result:
                raise RuntimeError(f"duplicate task log basename in {archives[0]}: {task}")
            result[task] = archive.read(member).decode(errors="replace")
    return result


def load(root: Path) -> list[dict[str, object]]:
    cells: list[dict[str, object]] = []
    for variant in base.VARIANTS:
        for model in base.MODELS:
            for repetition in base.REPETITIONS:
                directory = root / variant / model / repetition
                paths = list(directory.glob("*.xml.bz2"))
                if len(paths) != 1:
                    raise RuntimeError(f"expected one XML in {directory}, found {len(paths)}")
                with bz2.open(paths[0], "rb") as stream:
                    document = ET.parse(stream).getroot()
                logs = load_logs(directory)
                for run in document.findall("run"):
                    columns = {
                        column.attrib["title"]: column.attrib.get("value", "")
                        for column in run.findall("column")
                    }
                    task = base.task_name(run.attrib["name"])
                    task_basename = Path(task).name
                    text = logs.get(task_basename, "")
                    for title, pattern in PATTERNS.items():
                        matches = re.findall(pattern, text)
                        if matches:
                            columns[title] = matches[-1]
                    cells.append({
                        "variant": variant,
                        "model": model,
                        "repetition": repetition,
                        "task": task,
                        "status": columns.get("status", ""),
                        "category": columns.get("category", ""),
                        "cpu_s": base.numeric(columns.get("cputime")),
                        "wall_s": base.numeric(columns.get("walltime")),
                        "memory_bytes": base.numeric(columns.get("memory")),
                        "executions": columns.get("executions", ""),
                        "snapshot_bytes": columns.get("snapshot-bytes", ""),
                        "base_bytes": columns.get("base-bytes", ""),
                        "lazy_checks": columns.get("lazy-checks", ""),
                        "lazy_candidates": columns.get("lazy-candidates", ""),
                    })
    return cells


def mechanism_metrics(pairs: list[dict[str, object]]) -> dict[str, object]:
    grouped_checks: dict[tuple[str, str], list[float]] = defaultdict(list)
    grouped_candidates: dict[tuple[str, str], list[float]] = defaultdict(list)
    for pair in pairs:
        before_checks = base.numeric(str(pair["before_lazy_checks"]))
        after_checks = base.numeric(str(pair["after_lazy_checks"]))
        before_candidates = base.numeric(str(pair["before_lazy_candidates"]))
        after_candidates = base.numeric(str(pair["after_lazy_candidates"]))
        pair["full_check_ratio"] = (
            after_checks / before_checks if before_checks and after_checks is not None else None
        )
        pair["candidate_ratio"] = (
            after_candidates / before_candidates
            if before_candidates and after_candidates is not None else None
        )
        key = (str(pair["model"]), str(pair["task"]))
        if pair["full_check_ratio"] is not None:
            grouped_checks[key].append(float(pair["full_check_ratio"]))
        if pair["candidate_ratio"] is not None:
            grouped_candidates[key].append(float(pair["candidate_ratio"]))

    check_medians = {
        key: statistics.median(values)
        for key, values in grouped_checks.items() if len(values) == 4
    }
    candidate_medians = {
        key: statistics.median(values)
        for key, values in grouped_candidates.items() if len(values) == 4
    }

    def geomean(values: list[float]) -> float | None:
        positive = [value for value in values if value > 0]
        return math.exp(statistics.fmean(math.log(value) for value in positive)) \
            if positive else None

    return {
        "task_model_full_check_medians": len(check_medians),
        "full_check_geomean": geomean(list(check_medians.values())),
        "full_check_by_model": {
            model: geomean([ratio for (candidate, _), ratio in check_medians.items()
                            if candidate == model])
            for model in base.MODELS
        },
        "candidate_geomean": geomean(list(candidate_medians.values())),
        "candidate_by_model": {
            model: geomean([ratio for (candidate, _), ratio in candidate_medians.items()
                            if candidate == model])
            for model in base.MODELS
        },
    }


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: analyze_formal.py <formal-dir> <analysis-dir>")
    root, output = Path(sys.argv[1]), Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    cells = load(root)
    pairs, summary = base.analyze(cells)
    summary.update(mechanism_metrics(pairs))
    summary["status_mismatches"] = sum(
        pair["before_status"] != pair["after_status"] for pair in pairs
    )
    summary["category_mismatches"] = sum(
        pair["before_category"] != pair["after_category"] for pair in pairs
    )
    ci = summary["cpu_four_repetition_model_task_bootstrap_95_ci"]
    by_model = summary["by_model"]
    summary["formal_pass"] = (
        summary["cells"] == summary["expected_cells"]
        and summary["pairs"] == summary["expected_pairs"]
        and summary["terminal_verdict_mismatches"] == 0
        and summary["execution_mismatches"] == 0
        and summary["snapshot_mismatches"] == 0
        and summary["base_mismatches"] == 0
        and summary["coverage_losses"] == 0
        and summary["new_oom"] == 0
        and bool(ci) and float(ci[1]) < 1.0
        and all(float(by_model[model]["cpu_geomean"]) <= 1.01 for model in base.MODELS)
        and float(summary["memory_ratio_p90"]) <= 1.02
        and float(summary["memory_p90_candidate_over_baseline"]) <= 1.02
        and float(summary["large_task_rss_max_ratio"]) <= 1.02
    )

    base.write_tsv(output / "cells.tsv", cells)
    base.write_tsv(output / "pairs.tsv", pairs)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    checks = summary["full_check_by_model"]
    show = lambda value: "-" if value is None else f"{float(value):.6f}"
    lines = [
        "# P0.8b formal summary", "",
        f"- Cells/pairs: {summary['cells']}/{summary['pairs']}",
        f"- Terminal/execution mismatches: {summary['terminal_verdict_mismatches']}/{summary['execution_mismatches']}",
        f"- Snapshot/base mismatches: {summary['snapshot_mismatches']}/{summary['base_mismatches']}",
        f"- Coverage gains/losses/new OOM: {summary['coverage_gains']}/{summary['coverage_losses']}/{summary['new_oom']}",
        f"- CPU task-model geomean: {summary['cpu_geomean_of_task_model_medians']:.6f}",
        f"- CPU bootstrap 95% CI: [{ci[0]:.6f}, {ci[1]:.6f}]",
        f"- SC/TSO/PSO CPU: {by_model['sc']['cpu_geomean']:.6f}/{by_model['tso']['cpu_geomean']:.6f}/{by_model['pso']['cpu_geomean']:.6f}",
        f"- SC/TSO/PSO full-check ratios: {show(checks['sc'])}/{show(checks['tso'])}/{show(checks['pso'])}",
        f"- RSS cell P90 / P90-of-levels / large-task max: {summary['memory_ratio_p90']:.6f}/{summary['memory_p90_candidate_over_baseline']:.6f}/{summary['large_task_rss_max_ratio']:.6f}",
        f"- Frozen formal decision: {'retain' if summary['formal_pass'] else 'reject'}",
    ]
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
