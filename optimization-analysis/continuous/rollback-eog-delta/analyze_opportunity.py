#!/usr/bin/env python3

from __future__ import annotations

import csv
import json
import re
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path


STAT_PREFIX = "CAT incremental statistics: "
TRANSITIONS = (
    "initialize", "unchanged", "insert", "rollback", "rollback-insert",
    "replace", "rebuild", "evicted",
)


def parse_statistics(text: str) -> dict[str, int] | None:
    lines = [line for line in text.splitlines() if line.startswith(STAT_PREFIX)]
    if not lines:
        return None
    return {
        key: int(value)
        for key, value in re.findall(r"([a-z][a-z0-9-]*)=([0-9]+)", lines[-1])
    }


def task_from_member(member: str) -> str:
    name = Path(member).name.removesuffix(".log")
    return name.split(".", 2)[-1]


def load_cells(analysis: Path) -> dict[tuple[str, str, str], dict[str, str]]:
    result: dict[tuple[str, str, str], dict[str, str]] = {}
    with (analysis / "cells.tsv").open() as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            if row["variant"] != "before":
                continue
            key = (row["model"], row["repetition"], Path(row["task"]).name)
            if key in result:
                raise RuntimeError(f"duplicate task basename: {key}")
            result[key] = row
    return result


def load_rows(formal: Path, analysis: Path) -> list[dict[str, object]]:
    cells = load_cells(analysis)
    rows: list[dict[str, object]] = []
    for archive in sorted((formal / "before").glob("*/*/*.logfiles.zip")):
        model, repetition = archive.parts[-3], archive.parts[-2]
        with zipfile.ZipFile(archive) as stream:
            for member in stream.namelist():
                if not member.endswith(".log"):
                    continue
                task = task_from_member(member)
                statistics = parse_statistics(stream.read(member).decode(errors="replace"))
                cell = cells[(model, repetition, task)]
                row: dict[str, object] = {
                    "model": model,
                    "repetition": repetition,
                    "task": cell["task"],
                    "status": cell["status"],
                    "cpu_s": float(cell["cpu_s"]) if cell["cpu_s"] else None,
                    "has_statistics": statistics is not None,
                }
                if statistics:
                    row.update(statistics)
                rows.append(row)
    return rows


def classify(row: dict[str, object]) -> str:
    incremental = sum(int(row.get(key, 0)) for key in
                      ("insert", "rollback", "rollback-insert", "replace"))
    rebuild = int(row.get("rebuild", 0))
    if incremental and rebuild:
        return "incremental+rebuild"
    if incremental:
        return "incremental-only"
    if rebuild:
        return "rebuild-only"
    return "initialize/unchanged-only"


def summarize(rows: list[dict[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {"cells": len(rows)}
    usable = [row for row in rows if row["has_statistics"]]
    result["cells_with_statistics"] = len(usable)
    result["cells_without_statistics"] = len(rows) - len(usable)
    result["status_counts"] = dict(Counter(str(row["status"]) for row in rows))

    by_model: dict[str, object] = {}
    for model in ("sc", "tso", "pso"):
        selected = [row for row in usable if row["model"] == model]
        categories = Counter(classify(row) for row in selected)
        transition_totals = {
            key: sum(int(row.get(key, 0)) for row in selected) for key in TRANSITIONS
        }
        total_checks = sum(int(row.get("lazy-cycle-checks", 0)) for row in selected)
        total_candidates = sum(int(row.get("lazy-edge-candidates", 0)) for row in selected)
        category_work: dict[str, dict[str, int | float | None]] = {}
        for category in (
            "incremental-only", "incremental+rebuild", "rebuild-only",
            "initialize/unchanged-only",
        ):
            group = [row for row in selected if classify(row) == category]
            checks = sum(int(row.get("lazy-cycle-checks", 0)) for row in group)
            candidates = sum(int(row.get("lazy-edge-candidates", 0)) for row in group)
            category_work[category] = {
                "cells": len(group),
                "lazy_checks": checks,
                "lazy_checks_fraction": checks / total_checks if total_checks else None,
                "lazy_candidates": candidates,
                "lazy_candidates_fraction": candidates / total_candidates
                if total_candidates else None,
                "cpu_s": sum(float(row["cpu_s"]) for row in group
                             if row["cpu_s"] is not None),
            }
        incremental_cells = [
            row for row in selected
            if classify(row) in {"incremental-only", "incremental+rebuild"}
        ]
        incremental_checks = sum(
            int(row.get("lazy-cycle-checks", 0)) for row in incremental_cells
        )
        incremental_candidates = sum(
            int(row.get("lazy-edge-candidates", 0)) for row in incremental_cells
        )
        by_model[model] = {
            "cells_with_statistics": len(selected),
            "category_counts": dict(categories),
            "transition_totals": transition_totals,
            "lazy_checks": total_checks,
            "lazy_candidates": total_candidates,
            "incremental_cell_upper_bound_check_fraction":
                incremental_checks / total_checks if total_checks else None,
            "incremental_cell_upper_bound_candidate_fraction":
                incremental_candidates / total_candidates if total_candidates else None,
            "category_work": category_work,
        }
    result["by_model"] = by_model
    return result


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    fields = sorted({key for row in rows for key in row})
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def show_fraction(value: object) -> str:
    return "-" if value is None else f"{100 * float(value):.2f}%"


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit("usage: analyze_opportunity.py <formal-dir> <analysis-dir> <output-dir>")
    formal, analysis, output = map(Path, sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    rows = load_rows(formal, analysis)
    summary = summarize(rows)
    write_tsv(output / "cells.tsv", rows)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    lines = [
        "# Rollback-safe EOG delta opportunity census", "",
        f"- Baseline cells: {summary['cells']}",
        f"- Cells with/without final CAT statistics: {summary['cells_with_statistics']}/{summary['cells_without_statistics']}",
        "- The incremental-cell fractions are strict upper bounds: mixed cells also include initialization and rebuild checks.",
        "",
        "| Model | insert | rollback | rollback-insert | replace | rebuild | incremental-cell check upper bound | candidate upper bound |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for model in ("sc", "tso", "pso"):
        data = summary["by_model"][model]
        transitions = data["transition_totals"]
        lines.append(
            f"| {model.upper()} | {transitions['insert']} | {transitions['rollback']} | "
            f"{transitions['rollback-insert']} | {transitions['replace']} | "
            f"{transitions['rebuild']} | "
            f"{show_fraction(data['incremental_cell_upper_bound_check_fraction'])} | "
            f"{show_fraction(data['incremental_cell_upper_bound_candidate_fraction'])} |"
        )
    (output / "report.md").write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
