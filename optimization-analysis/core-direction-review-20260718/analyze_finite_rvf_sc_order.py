#!/usr/bin/env python3
"""Strict comparison of concrete SC completion and value-class SC-order encoding."""

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
CONCRETE = re.compile(
    r"Finite skeleton SC search: candidates=([0-9]+) completed=([0-9]+) "
    r"no-witness=([0-9]+) invalid=([0-9]+) witness-rejected=([0-9]+) "
    r"solver-status=([0-9]+) check-us=([0-9]+) complete-us=([0-9]+)"
)
WITNESS = re.compile(
    r"Finite skeleton SC-order witness: abstract=(true|false) "
    r"materialization-errors=([0-9]+) evaluation-errors=([0-9]+) violations=([0-9]+)"
)


def number(text: str) -> float:
    clean = re.sub(r"[^0-9.eE+-]", "", text)
    return float(clean) if clean else 0.0


def read_xml(root: Path) -> dict[str, dict[str, dict[str, float | str]]]:
    result: dict[str, dict[str, dict[str, float | str]]] = {}
    for path in root.glob("*.xml.bz2"):
        mode = path.name.split(".results.", 1)[1].split(".", 1)[0]
        rows = {}
        tree = ET.fromstring(bz2.open(path, "rb").read())
        for run in tree.findall("run"):
            columns = {c.get("title"): c.get("value", "") for c in run}
            rows[Path(run.get("name", "")).name] = {
                "status": columns.get("status", ""),
                "cpu": number(columns.get("cputime", "0")),
                "wall": number(columns.get("walltime", "0")),
                "memory": number(columns.get("memory", "0")),
            }
        result[mode] = rows
    return result


def read_logs(root: Path) -> dict[tuple[str, str], dict[str, object]]:
    result = {}
    for path in root.rglob("*.log"):
        mode, task = path.name[:-4].split(".", 1)
        text = path.read_text(errors="replace")
        first = FIRST.search(text)
        concrete = CONCRETE.search(text)
        witness = WITNESS.search(text)
        row: dict[str, object] = {}
        if first:
            row.update(
                supported=first.group(2) == "true",
                solver_status=int(first.group(3)),
                assignment=first.group(4) == "true",
                build_us=int(first.group(5)),
                first_check_us=int(first.group(6)),
            )
        if concrete:
            row.update(
                candidates=int(concrete.group(1)),
                completed=int(concrete.group(2)),
                no_witness=int(concrete.group(3)),
                invalid=int(concrete.group(4)),
                rejected=int(concrete.group(5)),
                terminal_solver_status=int(concrete.group(6)),
                total_check_us=int(concrete.group(7)),
                completion_us=int(concrete.group(8)),
            )
        if witness:
            row.update(
                witness_abstract=witness.group(1) == "true",
                materialization_errors=int(witness.group(2)),
                evaluation_errors=int(witness.group(3)),
                violations=int(witness.group(4)),
            )
        result[(mode, task)] = row
    return result


def verdict(mode: str, row: dict[str, object]) -> str | None:
    if mode == "concrete-sc":
        if int(row.get("completed", 0)) > 0:
            return "sat"
        if int(row.get("terminal_solver_status", -1)) == 1 and not any(
            int(row.get(field, 0)) for field in ("invalid", "rejected")
        ):
            return "unsat"
        return None
    if row.get("supported") is True and row.get("solver_status") == 0 and row.get("assignment") is True:
        return "sat"
    if row.get("supported") is True and row.get("solver_status") == 1 and row.get("assignment") is False:
        return "unsat"
    return None


def geometric_mean(values: list[float]) -> float | None:
    return math.exp(statistics.fmean(math.log(x) for x in values)) if values else None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_root", type=Path)
    parser.add_argument("log_root", type=Path)
    parser.add_argument("--expected-tasks", type=int, default=15)
    args = parser.parse_args()
    xml = read_xml(args.result_root)
    logs = read_logs(args.log_root)
    if set(xml) != {"concrete-sc", "value-sc-order"}:
        raise SystemExit(f"unexpected modes: {sorted(xml)}")
    if any(len(rows) != args.expected_tasks for rows in xml.values()):
        raise SystemExit("incomplete XML mode")
    tasks = sorted(xml["concrete-sc"])
    if set(tasks) != set(xml["value-sc-order"]):
        raise SystemExit("task sets differ")
    classified = {
        mode: {task: verdict(mode, logs.get((mode, task), {})) for task in tasks}
        for mode in xml
    }
    common = [task for task in tasks if all(classified[m][task] for m in xml)]
    differences = [task for task in common if classified["concrete-sc"][task] != classified["value-sc-order"][task]]
    bad_witnesses = []
    for task in tasks:
        if classified["value-sc-order"][task] != "sat":
            continue
        row = logs.get(("value-sc-order", task), {})
        if row.get("witness_abstract") is not False or any(
            int(row.get(field, -1)) != 0
            for field in ("materialization_errors", "evaluation_errors", "violations")
        ):
            bad_witnesses.append(task)
    ratios = {}
    for metric in ("cpu", "wall", "memory"):
        values = [
            float(xml["value-sc-order"][task][metric]) / float(xml["concrete-sc"][task][metric])
            for task in common
            if float(xml["concrete-sc"][task][metric]) > 0
        ]
        ratios[metric] = {
            "geometric_mean": geometric_mean(values),
            "median": statistics.median(values) if values else None,
            "sum_ratio": (
                sum(float(xml["value-sc-order"][task][metric]) for task in common)
                / sum(float(xml["concrete-sc"][task][metric]) for task in common)
                if common else None
            ),
            "regressed": sum(value > 1 for value in values),
        }
    output = {
        "tasks": len(tasks),
        "bench_status_histograms": {
            mode: {status: sum(row["status"] == status for row in rows.values())
                   for status in sorted({str(row["status"]) for row in rows.values()})}
            for mode, rows in xml.items()
        },
        "classified_histograms": {
            mode: {
                "sat": sum(value == "sat" for value in rows.values()),
                "unsat": sum(value == "unsat" for value in rows.values()),
                "unknown": sum(value is None for value in rows.values()),
            }
            for mode, rows in classified.items()
        },
        "common_classified": len(common),
	"classified_tasks": {
	    mode: {task: value for task, value in rows.items() if value is not None}
	    for mode, rows in classified.items()
	},
        "verdict_differences": differences,
        "bad_concrete_witnesses": bad_witnesses,
        "ratios_common_classified": ratios,
	"all_task_resources": {
	    metric: {
	        mode: sum(float(xml[mode][task][metric]) for task in tasks)
	        for mode in xml
	    } | {
	        "candidate_control_ratio": (
	            sum(float(xml["value-sc-order"][task][metric]) for task in tasks)
	            / sum(float(xml["concrete-sc"][task][metric]) for task in tasks)
	        )
	    }
	    for metric in ("cpu", "wall", "memory")
	},
        "concrete_candidates": {
            task: logs[("concrete-sc", task)].get("candidates")
            for task in tasks if ("concrete-sc", task) in logs and "candidates" in logs[("concrete-sc", task)]
        },
    }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
