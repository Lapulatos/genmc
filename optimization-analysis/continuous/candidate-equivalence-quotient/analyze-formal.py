#!/usr/bin/env python3
"""Strict paired analysis for the SC RVF formal matrix."""

from __future__ import annotations

import argparse
import bz2
import json
import random
import re
import statistics
import zipfile
from collections import Counter
from pathlib import Path
from xml.etree import ElementTree


STAT_RE = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")
COMPLETE_RE = re.compile(r"Number of complete executions explored: ([0-9]+)")
LOG_TASK_RE = re.compile(r"\.([^/.]+\.yml)\.log$")
SEARCH_COUNTERS = (
    "rf-offered", "rf-queued", "co-offered", "co-queued", "backward-offered",
    "backward-queued", "work-added", "work-popped", "max-retained-work",
    "validity-queries", "realized-revisit-prefixes", "inconsistent-revisit-prefixes",
)
RVF_COUNTERS = (
    "rvf-loads-attempted", "rvf-loads-reduced", "rvf-quotient-disabled-loads",
    "rvf-singleton-bypass", "rvf-native-reads-synthesized", "rvf-visible-sources",
    "rvf-value-groups", "rvf-verify-calls", "rvf-verify-states-expanded",
    "rvf-representatives-queued", "rvf-parent-continuations-queued", "rvf-fail-open",
)


def number(value: str | None) -> float | None:
    if value is None:
        return None
    match = re.match(r"([0-9.]+)", value)
    return float(match.group(1)) if match else None


def load_configuration(directory: Path) -> dict[str, dict]:
    xml_path = next(directory.glob("*.xml.bz2"))
    root = ElementTree.parse(bz2.open(xml_path)).getroot()
    rows: dict[str, dict] = {}
    for run in root.findall(".//run"):
        task = run.attrib["name"].rsplit("/", 1)[-1]
        columns = {column.attrib["title"]: column.attrib.get("value", "")
                   for column in run.findall("column")}
        rows[task] = {
            "name": run.attrib["name"],
            "status": columns.get("status", "SKIPPED"),
            "category": columns.get("category", "missing"),
            "cputime": number(columns.get("cputime")),
            "walltime": number(columns.get("walltime")),
            "memory": number(columns.get("memory")),
            "stats": {},
            "complete": None,
            "gate": None,
        }

    zip_path = next(directory.glob("*.logfiles.zip"))
    with zipfile.ZipFile(zip_path) as archive:
        for member in archive.namelist():
            match = LOG_TASK_RE.search(member)
            if not match:
                continue
            task = match.group(1)
            text = archive.read(member).decode("utf-8", errors="replace")
            statistic_lines = [line for line in text.splitlines()
                               if "Exploration statistics:" in line]
            if statistic_lines:
                rows[task]["stats"] = {
                    key: int(value) for key, value in STAT_RE.findall(statistic_lines[-1])
                }
            complete = COMPLETE_RE.findall(text)
            if complete:
                rows[task]["complete"] = int(complete[-1])
            gate = re.findall(r"SC RVF program gate: ([^\n]+)", text)
            if gate:
                rows[task]["gate"] = gate[-1]
    return rows


def ratio_ci(pairs: list[tuple[float, float]], seed: int = 20260717) -> dict:
    if not pairs:
        return {"n": 0, "ratio": None, "ci95": [None, None]}
    observed = sum(new for _, new in pairs) / sum(old for old, _ in pairs)
    rng = random.Random(seed)
    samples = []
    for _ in range(10_000):
        sample = [pairs[rng.randrange(len(pairs))] for _ in pairs]
        denominator = sum(old for old, _ in sample)
        if denominator:
            samples.append(sum(new for _, new in sample) / denominator)
    samples.sort()
    return {
        "n": len(pairs),
        "ratio": observed,
        "ci95": [samples[int(0.025 * len(samples))], samples[int(0.975 * len(samples))]],
    }


def compare(reference: dict[str, dict], candidate: dict[str, dict]) -> dict:
    tasks = sorted(reference.keys() & candidate.keys())
    status_differences = [task for task in tasks
                          if reference[task]["status"] != candidate[task]["status"]]
    category_differences = [task for task in tasks
                            if reference[task]["category"] != candidate[task]["category"]]
    complete_differences = [task for task in tasks
                            if reference[task]["complete"] is not None
                            and candidate[task]["complete"] is not None
                            and reference[task]["complete"] != candidate[task]["complete"]]
    solved = [task for task in tasks
              if reference[task]["status"] in {"true", "false(unreach-call)"}
              and candidate[task]["status"] in {"true", "false(unreach-call)"}]
    timing = {}
    for metric in ("cputime", "walltime", "memory"):
        timing[metric] = ratio_ci([
            (reference[task][metric], candidate[task][metric]) for task in solved
            if reference[task][metric] is not None and candidate[task][metric] is not None
        ])
    return {
        "tasks": len(tasks),
        "common_solved": len(solved),
        "status_differences": status_differences,
        "category_differences": category_differences,
        "complete_differences": complete_differences,
        "timing": timing,
    }


def totals(rows: dict[str, dict], counters: tuple[str, ...]) -> dict[str, int]:
    return {counter: sum(row["stats"].get(counter, 0) for row in rows.values())
            for counter in counters}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("matrix", type=Path)
    parser.add_argument("--controls", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = {name: load_configuration(args.matrix / name)
            for name in ("baseline", "control", "rvf")}
    if args.controls:
        rows.update({name: load_configuration(args.controls / name)
                     for name in ("tso", "pso-v9")})

    control_counter_mismatches = []
    for task in sorted(rows["baseline"].keys() & rows["control"].keys()):
        base, control = rows["baseline"][task], rows["control"][task]
        if base["stats"] and control["stats"] and any(
            base["stats"].get(counter, 0) != control["stats"].get(counter, 0)
            for counter in SEARCH_COUNTERS
        ):
            control_counter_mismatches.append(task)

    result = {
        "row_counts": {name: len(configuration) for name, configuration in rows.items()},
        "status_counts": {
            name: dict(Counter(row["status"] for row in configuration.values()))
            for name, configuration in rows.items()
        },
        "baseline_vs_control": compare(rows["baseline"], rows["control"]),
        "baseline_vs_rvf": compare(rows["baseline"], rows["rvf"]),
        "control_search_counter_mismatches": control_counter_mismatches,
        "search_totals": {name: totals(configuration, SEARCH_COUNTERS)
                          for name, configuration in rows.items()},
        "rvf_totals": {name: totals(configuration, RVF_COUNTERS)
                       for name, configuration in rows.items()},
        "rvf_gate_counts": dict(Counter(
            row["gate"] or "not-recorded" for row in rows["rvf"].values()
            if row["status"] != "SKIPPED"
        )),
        "complete_totals": {
            name: sum(row["complete"] or 0 for row in configuration.values())
            for name, configuration in rows.items()
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
