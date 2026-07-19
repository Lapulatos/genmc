#!/usr/bin/env python3
"""Strict paired analysis for the actual regional-RVF activation cohort."""

import argparse
import bz2
import csv
import hashlib
import json
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


COUNTERS = (
    "rf-offered",
    "rf-queued",
    "work-added",
    "work-popped",
    "realized-revisit-prefixes",
    "rvf-loads-attempted",
    "rvf-loads-reduced",
    "rvf-native-only-loads",
    "rvf-native-only-mergeable-loads",
    "rvf-native-only-mergeable-sources",
    "rvf-non-atomic-reads-fixed",
    "rvf-quotient-disabled-loads",
    "rvf-fail-open",
)


def parse_number(value):
    return float(re.sub(r"[^0-9.eE+-]", "", value))


def read_xml(lane):
    paths = list(lane.glob("*.xml.bz2"))
    if len(paths) != 1:
        raise ValueError(f"{lane}: expected one result XML")
    root = ET.fromstring(bz2.open(paths[0], "rb").read())
    rows = {}
    for run in root.findall("run"):
        name = run.attrib["name"].split("/c/", 1)[-1]
        columns = {column.attrib["title"]: column.attrib.get("value", "") for column in run}
        rows[name] = {
            "status": columns.get("status", ""),
            "cpu": parse_number(columns.get("cputime", "0")),
            "wall": parse_number(columns.get("walltime", "0")),
            "memory": int(parse_number(columns.get("memory", "0"))),
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
            match = re.search(
                r"/c/([^ ]+)\.[ci](?: |$)",
                command,
            )
            if not match:
                raise ValueError(f"cannot identify source in {member}")
            name = f"{match.group(1)}.yml"
            statistics = re.findall(r"Exploration statistics: ([^\n]+)", output)
            values = dict(re.findall(r"([a-z0-9-]+)=([0-9]+)", statistics[-1])) if statistics else {}
            semantics = "\n".join(
                sorted(
                    set(
                        line
                        for line in output.splitlines()
                        if line.startswith(("Error:", "Warning:", "No errors were detected."))
                    )
                )
            )
            gate = re.search(r"SC RVF program gate: ([^\n]+)", output)
            rows[name] = {
                "gate": gate.group(1).strip() if gate else "",
                "semantic_sha256": hashlib.sha256(semantics.encode()).hexdigest(),
                **{counter: int(values.get(counter, 0)) for counter in COUNTERS},
            }
    return rows


def percent_change(candidate, baseline):
    return None if baseline == 0 else 100.0 * (candidate / baseline - 1.0)


def summarize(rows):
    terminal = [
        row
        for row in rows
        if row["status_equal"]
        and row["baseline_status"] not in ("TIMEOUT", "OUT OF MEMORY")
    ]
    result = {
        "tasks": len(rows),
        "common_terminal_tasks": len(terminal),
        "status_differences": [row["task"] for row in rows if not row["status_equal"]],
        "semantic_differences": [
            row["task"] for row in terminal if not row["semantic_equal"]
        ],
        "cpu_seconds": {
            lane: sum(row[f"{lane}_cpu"] for row in rows)
            for lane in ("baseline", "candidate")
        },
        "common_terminal_cpu_seconds": {
            lane: sum(row[f"{lane}_cpu"] for row in terminal)
            for lane in ("baseline", "candidate")
        },
        "summed_peak_memory_bytes": {
            lane: sum(row[f"{lane}_memory"] for row in rows)
            for lane in ("baseline", "candidate")
        },
        "paired_cpu_delta_percent_median": (
            statistics.median(
                100.0 * (row["candidate_cpu"] / row["baseline_cpu"] - 1.0)
                for row in terminal
                if row["baseline_cpu"] > 0
            )
            if terminal
            else None
        ),
        "counter_totals": {
            lane: {
                counter: sum(row[f"{lane}_{counter}"] for row in rows)
                for counter in COUNTERS
            }
            for lane in ("baseline", "candidate")
        },
    }
    for key in ("cpu_seconds", "common_terminal_cpu_seconds", "summed_peak_memory_bytes"):
        result[f"{key}_change_percent"] = percent_change(
            result[key]["candidate"], result[key]["baseline"]
        )
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("result_root", type=Path)
    parser.add_argument("--expected-tasks", type=int, default=51)
    args = parser.parse_args()

    lanes = {}
    for lane in ("baseline", "candidate"):
        xml = read_xml(args.result_root / lane)
        logs = read_logs(args.result_root / lane)
        if set(xml) != set(logs) or len(xml) != args.expected_tasks:
            raise SystemExit(
                f"{lane}: expected identical {args.expected_tasks}-row XML/log sets"
            )
        lanes[lane] = {name: {**xml[name], **logs[name]} for name in xml}
    if set(lanes["baseline"]) != set(lanes["candidate"]):
        raise SystemExit("baseline/candidate task sets differ")

    rows = []
    for name in sorted(lanes["baseline"]):
        baseline = lanes["baseline"][name]
        candidate = lanes["candidate"][name]
        rows.append(
            {
                "task": name,
                "baseline_status": baseline["status"],
                "candidate_status": candidate["status"],
                "status_equal": int(baseline["status"] == candidate["status"]),
                "semantic_equal": int(
                    baseline["semantic_sha256"] == candidate["semantic_sha256"]
                ),
                "baseline_cpu": baseline["cpu"],
                "candidate_cpu": candidate["cpu"],
                "baseline_memory": baseline["memory"],
                "candidate_memory": candidate["memory"],
                "candidate_gate": candidate["gate"],
                **{
                    f"baseline_{counter}": baseline[counter]
                    for counter in COUNTERS
                },
                **{
                    f"candidate_{counter}": candidate[counter]
                    for counter in COUNTERS
                },
            }
        )

    with (args.result_root / "paired.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, delimiter="\t", fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    cohorts = {
        "all": rows,
        "regional_gate": [row for row in rows if row["candidate_gate"] == "regional"],
        "actual_reduction": [
            row for row in rows if row["candidate_rvf-loads-reduced"] > 0
        ],
        "regional_without_reduction": [
            row
            for row in rows
            if row["candidate_gate"] == "regional"
            and row["candidate_rvf-loads-reduced"] == 0
        ],
        "fallback": [row for row in rows if row["candidate_gate"] != "regional"],
    }
    report = {
        "tasks": len(rows),
        "status_differences": [row["task"] for row in rows if not row["status_equal"]],
        "semantic_differences_on_common_terminals": summarize(rows)["semantic_differences"],
        "regional_gate_tasks": sum(row["candidate_gate"] == "regional" for row in rows),
        "tasks_with_reduction": sum(row["candidate_rvf-loads-reduced"] > 0 for row in rows),
        "candidate_reduced_loads": sum(row["candidate_rvf-loads-reduced"] for row in rows),
        "cohorts": {name: summarize(cohort) for name, cohort in cohorts.items()},
    }
    (args.result_root / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
