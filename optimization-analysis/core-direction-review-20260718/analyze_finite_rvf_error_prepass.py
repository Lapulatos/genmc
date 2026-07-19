#!/usr/bin/env python3
"""Strict paired audit for native versus bounded SC-RVF error prepass."""

from __future__ import annotations

import argparse
import bz2
import json
import math
import re
import statistics
import xml.etree.ElementTree as ET
from pathlib import Path


PREPASS = re.compile(
    r"Finite symbolic error search: mode=rvf-sc-order timeout-ms=([0-9]+) "
    r"candidate-max=([0-9]+) terminal=([^ ]+) assignments=([0-9]+).*"
    r"replay-attempts=([0-9]+) replay-confirmed=([0-9]+) fail-open=([0-9]+)"
)


def number(text: str) -> float:
    match = re.search(r"[0-9.eE+-]+", text or "")
    return float(match.group()) if match else 0.0


def read_xml(root: Path) -> dict[str, dict[str, dict[str, float | str]]]:
    modes = {}
    for path in root.glob("*.xml.bz2"):
        mode = path.name.split(".results.", 1)[1].split(".", 1)[0]
        rows = {}
        tree = ET.fromstring(bz2.open(path, "rb").read())
        for run in tree.findall("run"):
            columns = {column.get("title"): column.get("value", "") for column in run}
            rows[Path(run.get("name", "")).name] = {
                "status": columns.get("status", ""),
                "cpu": number(columns.get("cputime", "")),
                "wall": number(columns.get("walltime", "")),
                "memory": number(columns.get("memory", "")),
            }
        modes[mode] = rows
    return modes


def geometric_mean(values: list[float]) -> float | None:
    return math.exp(statistics.fmean(math.log(value) for value in values)) if values else None


def terminal(status: str) -> bool:
    lowered = status.lower()
    return lowered.startswith("true") or lowered.startswith("false")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_root", type=Path)
    parser.add_argument("log_root", type=Path)
    parser.add_argument("--expected-tasks", type=int, default=15)
    args = parser.parse_args()

    modes = read_xml(args.result_root)
    control_mode = "native"
    candidate_mode = "rvf-prepass-5s"
    if set(modes) != {control_mode, candidate_mode}:
        raise SystemExit(f"unexpected modes: {sorted(modes)}")
    if any(len(rows) != args.expected_tasks for rows in modes.values()):
        raise SystemExit("incomplete XML mode")
    tasks = sorted(modes[control_mode])
    if set(tasks) != set(modes[candidate_mode]):
        raise SystemExit("task sets differ")

    records = []
    for path in args.log_root.rglob(f"{candidate_mode}.*.log"):
        match = PREPASS.search(path.read_text(errors="replace"))
        if match:
            records.append(match.groups())
    if len(records) != args.expected_tasks:
        raise SystemExit(f"expected {args.expected_tasks} prepass records, got {len(records)}")

    control = modes[control_mode]
    candidate = modes[candidate_mode]
    common = [
        task
        for task in tasks
        if control[task]["status"] == candidate[task]["status"]
        and terminal(str(control[task]["status"]))
    ]
    resources_all = {}
    resources_common = {}
    for metric in ("cpu", "wall", "memory"):
        control_total = sum(float(control[task][metric]) for task in tasks)
        candidate_total = sum(float(candidate[task][metric]) for task in tasks)
        resources_all[metric] = {
            control_mode: control_total,
            candidate_mode: candidate_total,
            "candidate_control_ratio": candidate_total / control_total,
        }
        ratios = [
            float(candidate[task][metric]) / float(control[task][metric])
            for task in common
            if float(control[task][metric]) > 0
        ]
        resources_common[metric] = {
            "geometric_mean": geometric_mean(ratios),
            "sum_ratio": (
                sum(float(candidate[task][metric]) for task in common)
                / sum(float(control[task][metric]) for task in common)
            ),
            "regressed": sum(ratio > 1 for ratio in ratios),
        }

    output = {
        "tasks": len(tasks),
        "status_histograms": {
            mode: {
                status: sum(str(row["status"]) == status for row in rows.values())
                for status in sorted({str(row["status"]) for row in rows.values()})
            }
            for mode, rows in modes.items()
        },
        "status_differences": [
            task for task in tasks if control[task]["status"] != candidate[task]["status"]
        ],
        "common_terminal": len(common),
        "prepass_terminals": {
            value: sum(record[2] == value for record in records)
            for value in sorted({record[2] for record in records})
        },
        "replay_confirmed": sum(int(record[5]) for record in records),
        "resources_all": resources_all,
        "resources_common": resources_common,
    }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
