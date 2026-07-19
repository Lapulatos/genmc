#!/usr/bin/env python3
"""Strict one-repetition analysis for the cumulative P1 storage optimizations."""

import argparse
import bz2
import csv
import hashlib
import json
import math
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter
from pathlib import Path


COUNTERS = ("rf-offered", "rf-queued", "work-added", "work-popped", "realized-revisit-prefixes")
SOLVED = {"true", "false(unreach-call)"}


def number(value):
    text = re.sub(r"[^0-9.eE+-]", "", value)
    return float(text) if text else 0.0


def read_xml(lane):
    paths = list(lane.glob("*.xml.bz2"))
    if len(paths) != 1:
        raise ValueError(f"{lane}: expected one result XML")
    root = ET.fromstring(bz2.open(paths[0], "rb").read())
    rows = {}
    for run in root.findall("run"):
        name = run.attrib["name"].split("/c/", 1)[-1]
        cols = {column.attrib["title"]: column.attrib.get("value", "") for column in run}
        rows[name] = {
            "status": cols.get("status", ""),
            "cpu": number(cols.get("cputime", "0")),
            "wall": number(cols.get("walltime", "0")),
            "memory": int(number(cols.get("memory", "0"))),
        }
    return rows


def read_logs(lane):
    paths = list(lane.glob("*.logfiles.zip"))
    if len(paths) != 1:
        raise ValueError(f"{lane}: expected one log archive")
    rows = {}
    with zipfile.ZipFile(paths[0]) as archive:
        for member in archive.namelist():
            output = archive.read(member).decode(errors="replace")
            command = output.splitlines()[0] if output else ""
            match = re.search(r"/c/([^ ]+)\.[ci](?: |$)", command)
            if not match:
                raise ValueError(f"cannot identify source in {member}")
            statistics_lines = re.findall(r"Exploration statistics: ([^\n]+)", output)
            values = dict(re.findall(r"([a-z0-9-]+)=([0-9]+)", statistics_lines[-1])) if statistics_lines else {}
            complete = re.findall(r"Number of complete executions explored: ([0-9]+)", output)
            semantics = "\n".join(sorted(set(
                line for line in output.splitlines()
                if line.startswith(("Error:", "Warning:", "No errors were detected."))
            )))
            rows[f"{match.group(1)}.yml"] = {
                "complete": int(complete[-1]) if complete else None,
                "statistics_present": bool(statistics_lines),
                "semantic_sha256": hashlib.sha256(semantics.encode()).hexdigest(),
                **{counter: int(values.get(counter, 0)) for counter in COUNTERS},
            }
    return rows


def ratio_summary(rows, metric):
    ratios = [row[f"candidate_{metric}"] / row[f"baseline_{metric}"] for row in rows
              if row[f"baseline_{metric}"] > 0 and row[f"candidate_{metric}"] > 0]
    return {
        "tasks": len(ratios),
        "geometric_mean_candidate_over_baseline": math.exp(statistics.fmean(map(math.log, ratios))) if ratios else None,
        "median_candidate_over_baseline": statistics.median(ratios) if ratios else None,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("result_root", type=Path)
    parser.add_argument("--expected-tasks", type=int, default=725)
    args = parser.parse_args()
    lanes = {}
    for lane in ("baseline", "candidate"):
        xml, logs = read_xml(args.result_root / lane), read_logs(args.result_root / lane)
        if set(xml) != set(logs) or len(xml) != args.expected_tasks:
            raise SystemExit(f"{lane}: expected identical {args.expected_tasks}-row XML/log sets")
        lanes[lane] = {name: {**xml[name], **logs[name]} for name in xml}
    if set(lanes["baseline"]) != set(lanes["candidate"]):
        raise SystemExit("baseline/candidate task sets differ")
    rows = []
    for task in sorted(lanes["baseline"]):
        row = {"task": task}
        for lane in ("baseline", "candidate"):
            row.update({f"{lane}_{key}": value for key, value in lanes[lane][task].items()})
        rows.append(row)
    with (args.result_root / "paired.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, delimiter="\t", fieldnames=rows[0].keys())
        writer.writeheader(); writer.writerows(rows)
    solved = [r for r in rows if r["baseline_status"] in SOLVED and r["candidate_status"] in SOLVED]
    report = {
        "tasks": len(rows),
        "status_histograms": {lane: Counter(r[f"{lane}_status"] for r in rows) for lane in ("baseline", "candidate")},
        "status_differences": [r["task"] for r in rows if r["baseline_status"] != r["candidate_status"]],
        "semantic_differences_common_solved": [r["task"] for r in solved if r["baseline_semantic_sha256"] != r["candidate_semantic_sha256"]],
        "complete_execution_mismatches": [r["task"] for r in solved if r["baseline_complete"] != r["candidate_complete"]],
        "search_counter_comparable_tasks": sum(
            r["baseline_statistics_present"] and r["candidate_statistics_present"]
            for r in solved
        ),
        "search_counter_mismatches": [
            r["task"] for r in solved
            if r["baseline_statistics_present"] and r["candidate_statistics_present"]
            and any(r[f"baseline_{c}"] != r[f"candidate_{c}"] for c in COUNTERS)
        ],
        "common_solved_tasks": len(solved),
        "ratios_common_solved": {metric: ratio_summary(solved, metric) for metric in ("cpu", "wall", "memory")},
        "totals_all": {metric: {lane: sum(r[f"{lane}_{metric}"] for r in rows) for lane in ("baseline", "candidate")} for metric in ("cpu", "wall", "memory")},
        "totals_common_solved": {metric: {lane: sum(r[f"{lane}_{metric}"] for r in solved) for lane in ("baseline", "candidate")} for metric in ("cpu", "wall", "memory")},
    }
    (args.result_root / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
