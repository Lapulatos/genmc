#!/usr/bin/env python3
"""Strict broad correctness/resource audit for the finite value-class SC-order lane."""

from __future__ import annotations

import argparse
import bz2
import json
import math
import re
import statistics
import xml.etree.ElementTree as ET
from pathlib import Path


FIRST = re.compile(
    r"Finite skeleton first model: mode=([^ ]+) supported=(true|false) "
    r"status=([0-9]+) assignment=(true|false) build-us=([0-9]+) check-us=([0-9]+)"
)
WITNESS = re.compile(
    r"Finite skeleton SC-order witness: abstract=(true|false) "
    r"materialization-errors=([0-9]+) evaluation-errors=([0-9]+) violations=([0-9]+)"
)


def number(text: str) -> float:
    clean = re.sub(r"[^0-9.eE+-]", "", text)
    return float(clean) if clean else 0.0


def read_xml(path: Path) -> dict[str, dict[str, float | str]]:
    tree = ET.fromstring(bz2.open(path, "rb").read())
    rows = {}
    for run in tree.findall("run"):
        columns = {c.get("title"): c.get("value", "") for c in run}
        rows[Path(run.get("name", "")).name] = {
            "status": columns.get("status", ""),
            "expected": run.get("expectedVerdict", ""),
            "cpu": number(columns.get("cputime", "0")),
            "wall": number(columns.get("walltime", "0")),
            "memory": number(columns.get("memory", "0")),
        }
    return rows


def read_logs(root: Path) -> dict[str, dict[str, object]]:
    rows = {}
    for path in root.rglob("value-sc-order.*.log"):
        task = path.name[len("value-sc-order.") : -len(".log")]
        text = path.read_text(errors="replace")
        first = FIRST.search(text)
        witness = WITNESS.search(text)
        row: dict[str, object] = {}
        if first:
            row.update(
                supported=first.group(2) == "true",
                solver_status=int(first.group(3)),
                assignment=first.group(4) == "true",
                build_us=int(first.group(5)),
                check_us=int(first.group(6)),
            )
        if witness:
            row.update(
                witness_abstract=witness.group(1) == "true",
                materialization_errors=int(witness.group(2)),
                evaluation_errors=int(witness.group(3)),
                violations=int(witness.group(4)),
            )
        rows[task] = row
    return rows


def candidate_verdict(row: dict[str, object]) -> str | None:
    if row.get("supported") is True and row.get("solver_status") == 0 and row.get("assignment") is True:
        return "sat"
    if row.get("supported") is True and row.get("solver_status") == 1 and row.get("assignment") is False:
        return "unsat"
    return None


def baseline_verdict(status: str) -> str | None:
    lowered = status.lower()
    if lowered.startswith("false"):
        return "sat"
    if lowered.startswith("true"):
        return "unsat"
    return None


def histogram(values: list[str | None]) -> dict[str, int]:
    return {
        "sat": sum(value == "sat" for value in values),
        "unsat": sum(value == "unsat" for value in values),
        "unknown": sum(value is None for value in values),
    }


def geometric_mean(values: list[float]) -> float | None:
    return math.exp(statistics.fmean(math.log(value) for value in values)) if values else None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate_xml", type=Path)
    parser.add_argument("candidate_logs", type=Path)
    parser.add_argument("baseline_xml", type=Path)
    parser.add_argument("--expected-tasks", type=int, default=283)
    args = parser.parse_args()
    candidate = read_xml(args.candidate_xml)
    baseline_all = read_xml(args.baseline_xml)
    logs = read_logs(args.candidate_logs)
    if len(candidate) != args.expected_tasks:
        raise SystemExit(f"candidate has {len(candidate)} tasks")
    missing = sorted(set(candidate) - set(baseline_all))
    if missing:
        raise SystemExit(f"baseline join missing {missing[0]}")
    tasks = sorted(candidate)
    baseline = {task: baseline_all[task] for task in tasks}
    cv = {task: candidate_verdict(logs.get(task, {})) for task in tasks}
    bv = {task: baseline_verdict(str(baseline[task]["status"])) for task in tasks}
    common = [task for task in tasks if cv[task] and bv[task]]
    differences = [task for task in common if cv[task] != bv[task]]
    bad_witnesses = []
    for task in tasks:
        if cv[task] != "sat":
            continue
        row = logs.get(task, {})
        if row.get("witness_abstract") is not False or any(
            int(row.get(field, -1)) != 0
            for field in ("materialization_errors", "evaluation_errors", "violations")
        ):
            bad_witnesses.append(task)
    expected_mismatches = []
    for task in tasks:
        if cv[task] is None:
            continue
        expected = str(candidate[task]["expected"]).lower()
        expected_class = "unsat" if expected == "true" else "sat" if expected == "false" else None
        if expected_class and cv[task] != expected_class:
            expected_mismatches.append(task)
    ratios = {}
    for metric in ("cpu", "wall", "memory"):
        values = [
            float(candidate[task][metric]) / float(baseline[task][metric])
            for task in common if float(baseline[task][metric]) > 0
        ]
        ratios[metric] = {
            "geometric_mean": geometric_mean(values),
            "median": statistics.median(values) if values else None,
            "sum_ratio": (
                sum(float(candidate[task][metric]) for task in common)
                / sum(float(baseline[task][metric]) for task in common)
                if common else None
            ),
        }
    output = {
        "tasks": len(tasks),
        "candidate_bench_statuses": {
            status: sum(str(row["status"]) == status for row in candidate.values())
            for status in sorted({str(row["status"]) for row in candidate.values()})
        },
        "candidate_classification": histogram(list(cv.values())),
        "baseline_classification": histogram(list(bv.values())),
        "common_classified": len(common),
        "verdict_differences": differences,
        "expected_verdict_mismatches": expected_mismatches,
        "bad_concrete_witnesses": bad_witnesses,
        "candidate_only_classified": {
            task: cv[task] for task in tasks if cv[task] and not bv[task]
        },
        "baseline_only_classified": {
            task: bv[task] for task in tasks if bv[task] and not cv[task]
        },
        "resources_common_classified": ratios,
        "candidate_resources_all": {
            metric: sum(float(candidate[task][metric]) for task in tasks)
            for metric in ("cpu", "wall", "memory")
        },
    }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
