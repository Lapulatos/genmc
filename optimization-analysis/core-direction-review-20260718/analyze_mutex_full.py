#!/usr/bin/env python3
"""Analyze the endpoint-normalized baseline/control/RVF 725-task matrix.

Logs are joined to XML rows by their source path, never by the non-unique YAML
basename used in BenchExec archive member names.
"""

from __future__ import annotations

import argparse
import bz2
import json
import random
import re
import zipfile
from collections import Counter
from pathlib import Path
from xml.etree import ElementTree


STAT_RE = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")
COMPLETE_RE = re.compile(r"Number of complete executions explored: ([0-9]+)")
SOURCE_RE = re.compile(r"(/workspace/[^\s]+?/c/[^\s]+?\.(?:c|i))(?:\s|$)")
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
    match = re.match(r"([0-9.]+)", value or "")
    return float(match.group(1)) if match else None


def canonical_source(path: str) -> str:
    normalized = path.replace("\\", "/")
    marker = "/c/"
    if marker not in normalized:
        raise ValueError(f"source path has no /c/ marker: {path}")
    relative = normalized.split(marker, 1)[1].rstrip("]")
    # The isolated endpoint pipeline rewrites SV-COMP preprocessed .i inputs to
    # .c files.  Directory plus stem remains the stable task identity.
    return str(Path(relative).with_suffix(""))


def load_configuration(directory: Path) -> dict[str, dict]:
    xml_path = next(directory.glob("*.xml.bz2"))
    root = ElementTree.parse(bz2.open(xml_path)).getroot()
    rows: dict[str, dict] = {}
    for run in root.findall(".//run"):
        source = canonical_source(run.attrib["files"])
        columns = {c.attrib["title"]: c.attrib.get("value", "")
                   for c in run.findall("column")}
        rows[source] = {
            "task": run.attrib["name"],
            "status": columns.get("status", "SKIPPED"),
            "category": columns.get("category", "missing"),
            "cputime": number(columns.get("cputime")),
            "walltime": number(columns.get("walltime")),
            "memory": number(columns.get("memory")),
            "stats": {}, "complete": None, "gate": None,
        }

    unmatched: list[str] = []
    with zipfile.ZipFile(next(directory.glob("*.logfiles.zip"))) as archive:
        for member in archive.namelist():
            text = archive.read(member).decode("utf-8", errors="replace")
            sources = SOURCE_RE.findall(text.split("\n\n", 1)[0])
            if not sources:
                unmatched.append(member)
                continue
            source = canonical_source(sources[-1])
            if source not in rows:
                unmatched.append(member)
                continue
            statistic_lines = [line for line in text.splitlines()
                               if "Exploration statistics:" in line]
            if statistic_lines:
                rows[source]["stats"] = {
                    key: int(value) for key, value in STAT_RE.findall(statistic_lines[-1])
                }
            complete = COMPLETE_RE.findall(text)
            if complete:
                rows[source]["complete"] = int(complete[-1])
            gate = re.findall(r"SC RVF program gate: ([^\n]+)", text)
            if gate:
                rows[source]["gate"] = gate[-1]
    if unmatched:
        raise RuntimeError(f"{directory.name}: {len(unmatched)} unmatched logs: {unmatched[:3]}")
    return rows


def ratio_ci(pairs: list[tuple[float, float]], seed: int = 20260718) -> dict:
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
    return {"n": len(pairs), "ratio": observed,
            "ci95": [samples[int(.025 * len(samples))], samples[int(.975 * len(samples))]]}


def compare(reference: dict[str, dict], candidate: dict[str, dict]) -> dict:
    tasks = sorted(reference.keys() & candidate.keys())
    solved = [t for t in tasks if reference[t]["category"] == "correct"
              and candidate[t]["category"] == "correct"]
    return {
        "tasks": len(tasks),
        "common_correct": len(solved),
        "status_differences": [t for t in tasks if reference[t]["status"] != candidate[t]["status"]],
        "category_differences": [t for t in tasks if reference[t]["category"] != candidate[t]["category"]],
        "complete_differences_observed_not_correctness_oracle": [
            t for t in tasks if reference[t]["complete"] is not None
            and candidate[t]["complete"] is not None
            and reference[t]["complete"] != candidate[t]["complete"]],
        "resources": {metric: ratio_ci([(reference[t][metric], candidate[t][metric]) for t in solved
                                        if reference[t][metric] is not None and candidate[t][metric] is not None])
                      for metric in ("cputime", "walltime", "memory")},
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("matrix", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = {name: load_configuration(args.matrix / name)
            for name in ("baseline", "control", "rvf")}
    def counter_differences(left: str, right: str) -> list[str]:
        differences = []
        for task in sorted(rows[left].keys() & rows[right].keys()):
            lhs, rhs = rows[left][task], rows[right][task]
            if lhs["category"] != "correct" or rhs["category"] != "correct":
                continue
            if lhs["stats"] and rhs["stats"] and any(
                    lhs["stats"].get(k, 0) != rhs["stats"].get(k, 0) for k in SEARCH_COUNTERS):
                differences.append(task)
        return differences
    result = {
        "row_counts": {k: len(v) for k, v in rows.items()},
        "status_counts": {k: dict(Counter(r["status"] for r in v.values())) for k, v in rows.items()},
        "category_counts": {k: dict(Counter(r["category"] for r in v.values())) for k, v in rows.items()},
        "baseline_vs_control": compare(rows["baseline"], rows["control"]),
        "baseline_vs_rvf": compare(rows["baseline"], rows["rvf"]),
        "control_vs_rvf": compare(rows["control"], rows["rvf"]),
        "baseline_control_search_counter_differences": counter_differences("baseline", "control"),
        "control_rvf_search_counter_differences": counter_differences("control", "rvf"),
        "rvf_gate_counts": dict(Counter(r["gate"] or "not-recorded" for r in rows["rvf"].values())),
        "rvf_totals": {k: sum(r["stats"].get(k, 0) for r in rows["rvf"].values()) for k in RVF_COUNTERS},
        "rvf_reduced_tasks": [t for t, r in rows["rvf"].items() if r["stats"].get("rvf-loads-reduced", 0)],
        "rvf_enabled_tasks": [t for t, r in rows["rvf"].items() if r["gate"] == "enabled"],
    }
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
