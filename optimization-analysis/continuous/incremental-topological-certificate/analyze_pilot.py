#!/usr/bin/env python3

from __future__ import annotations

import bz2
import csv
import json
import re
import statistics
import sys
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict
from pathlib import Path


def numeric(value: str | None) -> float | None:
    if not value:
        return None
    for suffix in ("s", "B"):
        if value.endswith(suffix):
            value = value[: -len(suffix)]
            break
    try:
        return float(value)
    except ValueError:
        return None


def load(root: Path) -> dict[tuple[str, str, str], dict[str, str]]:
    rows: dict[tuple[str, str, str], dict[str, str]] = {}
    for path in sorted(root.glob("*/*.xml.bz2")):
        variant = path.parts[-3]
        repetition = path.parts[-2]
        with bz2.open(path, "rb") as stream:
            document = ET.parse(stream).getroot()
        model = document.attrib["name"].split(".", 1)[0].removeprefix("caat-")
        log_archive = next(path.parent.glob("*.logfiles.zip"), None)
        logs: dict[str, str] = {}
        if log_archive:
            with zipfile.ZipFile(log_archive) as archive:
                for member in archive.namelist():
                    if member.endswith(".log"):
                        logs[Path(member).name] = archive.read(member).decode(errors="replace")
        for run in document.findall("run"):
            task = Path(run.attrib["name"]).name
            columns = {column.attrib["title"]: column.attrib.get("value", "")
                       for column in run.findall("column")}
            log_name = f"caat-{model}.lazy-pilot.{task}.log"
            text = logs.get(log_name, "")
            patterns = {
                "executions": r"Number of complete executions explored: ([0-9]+)",
                "snapshot-bytes": r"peak-snapshot-equivalent-bytes=([0-9]+)",
                "base-bytes": r"max-current-base-bytes=([0-9]+)",
                "lazy-checks": r"lazy-cycle-checks=([0-9]+)",
                "lazy-candidates": r"lazy-edge-candidates=([0-9]+)",
            }
            for title, pattern in patterns.items():
                matches = re.findall(pattern, text)
                if matches:
                    columns[title] = matches[-1]
            key = (repetition, model, task)
            if key in rows:
                raise RuntimeError(f"duplicate cell: {variant}/{key}")
            rows[key] = columns
    return rows


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: analyze_pilot.py <pilot-dir> <analysis-dir>")
    pilot, output = Path(sys.argv[1]), Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    before, after = load(pilot / "before"), load(pilot / "after")
    if before.keys() != after.keys():
        raise RuntimeError("before/after pilot cells are not exactly paired")

    fields = [
        "repetition", "model", "task", "before_status", "after_status",
        "before_category", "after_category", "before_cpu", "after_cpu", "cpu_ratio",
        "before_rss", "after_rss", "rss_ratio", "before_executions", "after_executions",
        "before_snapshot", "after_snapshot", "before_base", "after_base",
        "before_lazy_checks", "after_lazy_checks", "full_check_ratio",
        "before_candidates", "after_candidates", "candidate_ratio",
    ]
    pairs: list[dict[str, object]] = []
    grouped: dict[tuple[str, str], list[dict[str, object]]] = defaultdict(list)
    mismatches = defaultdict(int)
    for repetition, model, task in sorted(before):
        left, right = before[(repetition, model, task)], after[(repetition, model, task)]
        left_cpu, right_cpu = numeric(left.get("cputime")), numeric(right.get("cputime"))
        left_rss, right_rss = numeric(left.get("memory")), numeric(right.get("memory"))
        left_checks = numeric(left.get("lazy-checks"))
        right_checks = numeric(right.get("lazy-checks"))
        left_candidates = numeric(left.get("lazy-candidates"))
        right_candidates = numeric(right.get("lazy-candidates"))
        row: dict[str, object] = {
            "repetition": repetition, "model": model, "task": task,
            "before_status": left.get("status", ""), "after_status": right.get("status", ""),
            "before_category": left.get("category", ""),
            "after_category": right.get("category", ""),
            "before_cpu": left_cpu, "after_cpu": right_cpu,
            "cpu_ratio": right_cpu / left_cpu if left_cpu and right_cpu else None,
            "before_rss": left_rss, "after_rss": right_rss,
            "rss_ratio": right_rss / left_rss if left_rss and right_rss else None,
            "before_executions": left.get("executions", ""),
            "after_executions": right.get("executions", ""),
            "before_snapshot": left.get("snapshot-bytes", ""),
            "after_snapshot": right.get("snapshot-bytes", ""),
            "before_base": left.get("base-bytes", ""), "after_base": right.get("base-bytes", ""),
            "before_lazy_checks": left_checks, "after_lazy_checks": right_checks,
            "full_check_ratio": right_checks / left_checks if left_checks else None,
            "before_candidates": left_candidates, "after_candidates": right_candidates,
            "candidate_ratio": right_candidates / left_candidates if left_candidates else None,
        }
        pairs.append(row)
        grouped[(model, task)].append(row)
        mismatches["status"] += row["before_status"] != row["after_status"]
        mismatches["category"] += row["before_category"] != row["after_category"]
        if row["before_executions"] and row["after_executions"]:
            mismatches["execution"] += row["before_executions"] != row["after_executions"]
        if row["before_snapshot"] and row["after_snapshot"]:
            mismatches["snapshot"] += row["before_snapshot"] != row["after_snapshot"]
        if row["before_base"] and row["after_base"]:
            mismatches["base"] += row["before_base"] != row["after_base"]
        left_terminal = row["before_status"] not in {"TIMEOUT", "OUT OF MEMORY", ""}
        right_terminal = row["after_status"] not in {"TIMEOUT", "OUT OF MEMORY", ""}
        mismatches["coverage_loss"] += left_terminal and not right_terminal

    with (output / "pairs.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(pairs)

    summaries: dict[str, dict[str, object]] = {}
    pilot_pass = True
    opportunity = False
    for key, rows in sorted(grouped.items()):
        cpu = [float(row["cpu_ratio"]) for row in rows if row["cpu_ratio"] is not None]
        rss = [float(row["rss_ratio"]) for row in rows if row["rss_ratio"] is not None]
        before_rss = [float(row["before_rss"]) for row in rows if row["before_rss"] is not None]
        after_rss = [float(row["after_rss"]) for row in rows if row["after_rss"] is not None]
        before_checks = [float(row["before_lazy_checks"]) for row in rows
                         if row["before_lazy_checks"] is not None]
        after_checks = [float(row["after_lazy_checks"]) for row in rows
                        if row["after_lazy_checks"] is not None]
        candidate_ratios = [float(row["candidate_ratio"]) for row in rows
                            if row["candidate_ratio"] is not None]
        summary = {
            "cpu_median": statistics.median(cpu) if cpu else None,
            "rss_median": statistics.median(rss) if rss else None,
            "rss_max_ratio": max(after_rss) / max(before_rss)
            if before_rss and after_rss else None,
            "before_full_checks_median": statistics.median(before_checks)
            if before_checks else None,
            "after_full_checks_median": statistics.median(after_checks)
            if after_checks else None,
            "full_check_ratio": statistics.median(after_checks) /
            statistics.median(before_checks) if before_checks and statistics.median(before_checks)
            else None,
            "candidate_ratio_median": statistics.median(candidate_ratios)
            if candidate_ratios else None,
        }
        summaries[f"{key[0]}/{key[1]}"] = summary
        if summary["cpu_median"] is not None and summary["cpu_median"] > 1.03:
            pilot_pass = False
        if summary["rss_median"] is not None and summary["rss_median"] > 1.02:
            pilot_pass = False
        if summary["rss_max_ratio"] is not None and summary["rss_max_ratio"] > 1.02:
            pilot_pass = False
        if key[0] in {"tso", "pso"} and summary["full_check_ratio"] is not None:
            has_long = any(float(row["before_cpu"]) > 1 for row in rows
                           if row["before_cpu"] is not None)
            opportunity |= (summary["full_check_ratio"] <= 0.80 and
                            summary["cpu_median"] <= 0.98 and has_long)

    exact = all(mismatches[name] == 0 for name in
                ("status", "category", "execution", "snapshot", "base", "coverage_loss"))
    pilot_pass = pilot_pass and exact and opportunity
    summary = {"cells": len(pairs), "mismatches": dict(mismatches),
               "opportunity": opportunity, "pilot_pass": pilot_pass, "tasks": summaries}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    lines = ["# P0.8a pilot summary", "", f"- Paired cells: {len(pairs)}",
             f"- Mismatches: {dict(mismatches)}", f"- Opportunity/speed witness: {opportunity}",
             f"- Frozen pilot decision: {'advance' if pilot_pass else 'reject'}", "",
             "| Model/task | CPU median | RSS median | RSS max | Full-check ratio | Candidate ratio |",
             "|---|---:|---:|---:|---:|---:|"]
    show = lambda value: "-" if value is None else f"{float(value):.6f}"
    for key, values in summaries.items():
        lines.append(f"| {key} | {show(values['cpu_median'])} | {show(values['rss_median'])} | "
                     f"{show(values['rss_max_ratio'])} | {show(values['full_check_ratio'])} | "
                     f"{show(values['candidate_ratio_median'])} |")
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
