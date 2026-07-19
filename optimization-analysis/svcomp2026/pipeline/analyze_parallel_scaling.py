#!/usr/bin/env python3
"""Validate and summarize the GenMC parallel-scaling BenchExec matrix."""

from __future__ import annotations

import argparse
import bz2
from collections import Counter, defaultdict
import csv
from dataclasses import dataclass
from pathlib import Path
import json
import math
import re
import statistics
import xml.etree.ElementTree as ET


NAME = re.compile(
    r"(?P<method>genmc|cat|caat)-(?P<model>sc|tso|pso)\.t(?P<threads>[1248])\.r(?P<rep>\d+)\.unreach-call"
)


@dataclass(frozen=True)
class Row:
    task: str
    expected: bool
    method: str
    model: str
    threads: int
    repetition: int
    status: str
    verdict: bool | None
    walltime: float | None
    cputime: float | None
    memory: int | None
    executions: int | None
    blocked: int | None
    oracle_checks: int | None


def number(value: str | None, suffix: str = "") -> float | None:
    if not value:
        return None
    return float(value.removesuffix(suffix))


def integer(value: str | None, suffix: str = "") -> int | None:
    value = number(value, suffix)
    return None if value is None else int(value)


def verdict(status: str) -> bool | None:
    if status == "true":
        return True
    if status.startswith("false("):
        return False
    return None


def read_rows(paths: list[Path]) -> list[Row]:
    rows: list[Row] = []
    for path in paths:
        with bz2.open(path) as source:
            root = ET.parse(source).getroot()
        match = NAME.fullmatch(root.get("name", ""))
        if not match:
            raise SystemExit(f"unexpected run-set name in {path}: {root.get('name')}")
        metadata = match.groupdict()
        for run in root.findall("run"):
            columns = {
                column.get("title", ""): column.get("value", "")
                for column in run.findall("column")
            }
            status = columns.get("status", "")
            rows.append(
                Row(
                    task=Path(run.get("name", "")).name,
                    expected=run.get("expectedVerdict") == "true",
                    method=metadata["method"],
                    model=metadata["model"],
                    threads=int(metadata["threads"]),
                    repetition=int(metadata["rep"]),
                    status=status,
                    verdict=verdict(status),
                    walltime=number(columns.get("walltime"), "s"),
                    cputime=number(columns.get("cputime"), "s"),
                    memory=integer(columns.get("memory"), "B"),
                    executions=integer(columns.get("executions")),
                    blocked=integer(columns.get("blocked")),
                    oracle_checks=integer(columns.get("oracle-checks")),
                )
            )
    return rows


def geometric_mean(values: list[float]) -> float | None:
    positive = [value for value in values if value > 0 and math.isfinite(value)]
    return statistics.geometric_mean(positive) if positive else None


def quantile(values: list[float], probability: float) -> float | None:
    """Return a linearly interpolated sample quantile."""
    if not values:
        return None
    ordered = sorted(values)
    position = probability * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1 - fraction) + ordered[upper] * fraction


def metric_summary(values: list[float], prefix: str) -> dict[str, float | int | None]:
    """Summarize one metric without treating task rows as independent seeds."""
    return {
        f"{prefix}_n": len(values),
        f"{prefix}_mean": statistics.fmean(values) if values else None,
        f"{prefix}_median": statistics.median(values) if values else None,
        f"{prefix}_p95": quantile(values, 0.95),
        f"{prefix}_geomean": geometric_mean(values),
        f"{prefix}_sum": sum(values) if values else None,
        f"{prefix}_max": max(values) if values else None,
    }


def write_tsv(path: Path, rows: list[dict], fields: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("results", type=Path)
    parser.add_argument("--oracle-results", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    paths = sorted(args.results.glob("*.xml.bz2"))
    rows = read_rows(paths)
    if not rows:
        raise SystemExit("no formal XML results")
    args.output.mkdir(parents=True, exist_ok=True)

    expected_methods = {
        "sc": {"genmc", "cat", "caat"},
        "tso": {"genmc", "cat", "caat"},
        "pso": {"cat", "caat"},
    }
    expected_cells = {
        (model, method, threads, repetition)
        for model, methods in expected_methods.items()
        for method in methods
        for threads in (1, 2, 4, 8)
        for repetition in range(1, 6)
    }
    actual_cells = {
        (row.model, row.method, row.threads, row.repetition) for row in rows
    }
    duplicate_count = len(rows) - len(
        {
            (row.task, row.model, row.method, row.threads, row.repetition)
            for row in rows
        }
    )

    verdict_errors = [
        row for row in rows if row.verdict is not None and row.verdict != row.expected
    ]
    false_alarms = sum(not row.verdict and row.expected for row in verdict_errors)
    missed_bugs = sum(row.verdict and not row.expected for row in verdict_errors)

    coverage_rows: list[dict] = []
    metric_rows: list[dict] = []
    for key, group in sorted(
        defaultdict(list, {
            key: [row for row in rows if (row.model, row.method, row.threads) == key]
            for key in {(row.model, row.method, row.threads) for row in rows}
        }).items()
    ):
        model, method, threads = key
        statuses = Counter(row.status for row in group)
        solved = sum(row.verdict is not None for row in group)
        coverage_rows.append(
            {
                "model": model,
                "method": method,
                "threads": threads,
                "runs": len(group),
                "solved": solved,
                "completion_rate": solved / len(group),
                "true": statuses["true"],
                "false": sum(status.startswith("false(") for status in statuses.elements()),
                "timeout": sum("TIMEOUT" in status for status in statuses.elements()),
                "oom": sum("OUT OF MEMORY" in status for status in statuses.elements()),
                "aborted": statuses["ABORTED"],
                "other": len(group) - solved
                - sum("TIMEOUT" in status for status in statuses.elements())
                - sum("OUT OF MEMORY" in status for status in statuses.elements())
                - statuses["ABORTED"],
            }
        )
        solved_group = [row for row in group if row.verdict is not None]
        metric_row: dict[str, object] = {
            "model": model,
            "method": method,
            "threads": threads,
            "runs": len(group),
            "solved": len(solved_group),
        }
        for scope, sample in (("all", group), ("solved", solved_group)):
            for metric in ("walltime", "cputime"):
                values = [
                    float(value)
                    for row in sample
                    if (value := getattr(row, metric)) is not None
                ]
                metric_row.update(metric_summary(values, f"{scope}_{metric}_seconds"))
            rss_mib = [
                float(row.memory) / (1024 * 1024)
                for row in sample
                if row.memory is not None
            ]
            metric_row.update(metric_summary(rss_mib, f"{scope}_rss_mib"))
        metric_rows.append(metric_row)

    by_key = {
        (row.task, row.model, row.method, row.threads, row.repetition): row
        for row in rows
    }
    speed_rows: list[dict] = []
    for model, methods in expected_methods.items():
        for method in sorted(methods):
            for threads in (2, 4, 8):
                ratios = defaultdict(list)
                common = 0
                for task in {row.task for row in rows}:
                    for repetition in range(1, 6):
                        baseline = by_key.get((task, model, method, 1, repetition))
                        candidate = by_key.get((task, model, method, threads, repetition))
                        if not baseline or not candidate:
                            continue
                        if baseline.verdict is None or candidate.verdict is None:
                            continue
                        common += 1
                        for metric in ("walltime", "cputime", "memory"):
                            base_value = getattr(baseline, metric)
                            candidate_value = getattr(candidate, metric)
                            if base_value and candidate_value:
                                ratios[metric].append(base_value / candidate_value)
                speed_rows.append(
                    {
                        "model": model,
                        "method": method,
                        "threads": threads,
                        "common_solved": common,
                        "wall_speedup_gmean": geometric_mean(ratios["walltime"]),
                        "cpu_efficiency_gmean": geometric_mean(ratios["cputime"]),
                        "rss_inverse_ratio_gmean": geometric_mean(ratios["memory"]),
                    }
                )

    safe_mismatches: list[dict] = []
    safe_missing = 0
    for row in rows:
        if row.threads == 1 or not row.expected or row.verdict is not True:
            continue
        baseline = by_key.get((row.task, row.model, row.method, 1, row.repetition))
        if not baseline or baseline.verdict is not True:
            continue
        if row.executions is None or baseline.executions is None:
            safe_missing += 1
        elif (row.executions, row.blocked or 0) != (
            baseline.executions,
            baseline.blocked or 0,
        ):
            safe_mismatches.append(
                {
                    "task": row.task,
                    "model": row.model,
                    "method": row.method,
                    "threads": row.threads,
                    "repetition": row.repetition,
                    "t1_executions": baseline.executions,
                    "parallel_executions": row.executions,
                    "t1_blocked": baseline.blocked or 0,
                    "parallel_blocked": row.blocked or 0,
                }
            )

    oracle_summary = {}
    if args.oracle_results:
        oracle_paths = sorted(args.oracle_results.glob("*.xml.bz2"))
        oracle = read_rows(oracle_paths) if oracle_paths else []
        oracle_summary = {
            "xml_files": len(oracle_paths),
            "runs": len(oracle),
            "expected_xml_files": 12,
            "expected_runs": 288,
            "solved": sum(row.verdict is not None for row in oracle),
            "verdict_errors": sum(
                row.verdict is not None and row.verdict != row.expected for row in oracle
            ),
            "cells_with_oracle_count": sum(row.oracle_checks is not None for row in oracle),
            "oracle_checks": sum(row.oracle_checks or 0 for row in oracle),
        }

    summary = {
        "formal_xml_files": len(paths),
        "formal_runs": len(rows),
        "expected_runs": 15360,
        "expected_cells": len(expected_cells),
        "actual_cells": len(actual_cells),
        "missing_cells": sorted(expected_cells - actual_cells),
        "unexpected_cells": sorted(actual_cells - expected_cells),
        "duplicate_rows": duplicate_count,
        "tasks": len({row.task for row in rows}),
        "verdict_errors": len(verdict_errors),
        "false_alarms": false_alarms,
        "missed_bugs": missed_bugs,
        "safe_execution_mismatches": len(safe_mismatches),
        "safe_missing_execution_counts": safe_missing,
        "oracle": oracle_summary,
    }
    (args.output / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    write_tsv(args.output / "coverage.tsv", coverage_rows, list(coverage_rows[0]))
    write_tsv(args.output / "metric-summary.tsv", metric_rows, list(metric_rows[0]))
    write_tsv(args.output / "speedup.tsv", speed_rows, list(speed_rows[0]))
    write_tsv(
        args.output / "formal-rows.tsv",
        [
            {
                "task": row.task,
                "expected": str(row.expected).lower(),
                "model": row.model,
                "method": row.method,
                "threads": row.threads,
                "repetition": row.repetition,
                "status": row.status,
                "verdict": "" if row.verdict is None else str(row.verdict).lower(),
                "walltime_seconds": row.walltime,
                "cputime_seconds": row.cputime,
                "memory_bytes": row.memory,
                "executions": row.executions,
                "blocked": row.blocked,
            }
            for row in rows
        ],
        [
            "task", "expected", "model", "method", "threads", "repetition",
            "status", "verdict", "walltime_seconds", "cputime_seconds",
            "memory_bytes", "executions", "blocked",
        ],
    )
    write_tsv(
        args.output / "safe-execution-mismatches.tsv",
        safe_mismatches,
        [
            "task", "model", "method", "threads", "repetition",
            "t1_executions", "parallel_executions", "t1_blocked", "parallel_blocked",
        ],
    )
    print(args.output / "summary.json")
    print(json.dumps(summary, sort_keys=True))
    formal_complete = (
        len(paths) == 160
        and len(rows) == 15360
        and len({row.task for row in rows}) == 96
        and not summary["missing_cells"]
        and not summary["unexpected_cells"]
        and not duplicate_count
        and not verdict_errors
        and not safe_mismatches
        and not safe_missing
    )
    oracle_complete = not args.oracle_results or (
        oracle_summary.get("xml_files") == 12
        and oracle_summary.get("runs") == 288
        and oracle_summary.get("verdict_errors") == 0
        and oracle_summary.get("oracle_checks", 0) > 0
    )
    return 0 if formal_complete and oracle_complete else 2


if __name__ == "__main__":
    raise SystemExit(main())
