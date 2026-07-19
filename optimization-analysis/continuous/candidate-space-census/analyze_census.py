#!/usr/bin/env python3
"""Join candidate-space counters from BenchExec log archives with XML results."""

from __future__ import annotations

import argparse
import bz2
import csv
import glob
from collections import Counter
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


STAT_RE = re.compile(r"Exploration statistics: (.*)")
PAIR_RE = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")


def columns(run: ET.Element) -> dict[str, str]:
    return {column.get("title", ""): column.get("value", "") for column in run.findall("column")}


def strip_unit(value: str, unit: str) -> float:
    return float(value.removesuffix(unit))


def key_from_name(name: str) -> str:
    return Path(name).stem


def load_model(model_dir: Path) -> list[dict[str, object]]:
    xml_path = Path(glob.glob(str(model_dir / "*.xml.bz2"))[0])
    zip_path = Path(glob.glob(str(model_dir / "*.logfiles.zip"))[0])
    root = ET.fromstring(bz2.open(xml_path, "rb").read())
    runs: dict[str, dict[str, object]] = {}
    for run in root.findall("run"):
        values = columns(run)
        name = run.get("name", "")
        runs[key_from_name(name)] = {
            "model": model_dir.name,
            "task": name,
            "status": values.get("status", ""),
            "category": values.get("category", ""),
            "cputime_s": strip_unit(values.get("cputime", "0s"), "s"),
            "walltime_s": strip_unit(values.get("walltime", "0s"), "s"),
            "memory_bytes": int(values.get("memory", "0B").removesuffix("B")),
        }
    with zipfile.ZipFile(zip_path) as archive:
        for member in archive.namelist():
            if member.endswith("/"):
                continue
            text = archive.read(member).decode("utf-8", errors="replace")
            matches = STAT_RE.findall(text)
            if not matches:
                continue
            counters = {key.replace("-", "_"): int(value) for key, value in PAIR_RE.findall(matches[-1])}
            member_name = Path(member).name
            exact = [candidate for candidate in runs if member_name.endswith(f".{candidate}.yml.log")]
            if len(exact) == 1:
                key = exact[0]
            else:
                key = key_from_name(member_name.removesuffix(".log"))
            if key not in runs:
                candidates = [candidate for candidate in runs if candidate in member or key in candidate]
                if len(candidates) != 1:
                    raise RuntimeError(f"cannot join log {member!r}: {candidates}")
                key = candidates[0]
            runs[key].update(counters)
    return list(runs.values())


def percentile(values: list[int], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return float(ordered[round((len(ordered) - 1) * fraction)])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    args = parser.parse_args()
    rows = [row for model in ("sc", "tso", "pso") for row in load_model(args.root / model)]
    counter_names = sorted({key for row in rows for key in row if key not in {"model", "task", "status", "category", "cputime_s", "walltime_s", "memory_bytes"}})
    fieldnames = ["model", "task", "status", "category", "cputime_s", "walltime_s", "memory_bytes", *counter_names]
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)

    violations: list[str] = []
    for row in rows:
        if "rf_offered" not in row:
            continue
        checks = (
            ("rf", int(row["rf_queued"]), int(row["rf_offered"])),
            ("co", int(row["co_queued"]), int(row["co_offered"])),
            ("backward", int(row["backward_queued"]), int(row["backward_offered"])),
            ("work", int(row["work_popped"]), int(row["work_added"])),
            ("prefix", int(row["inconsistent_revisit_prefixes"]), int(row["realized_revisit_prefixes"])),
        )
        for label, lower, upper in checks:
            if lower > upper:
                violations.append(f"{row['model']} {row['task']} {label}: {lower}>{upper}")
        value_checks = (
            ("same-value candidates", int(row.get("rf_same_value_candidate_upper_bound", 0)),
             int(row["rf_offered"])),
            ("same-value choice points", int(row.get("rf_same_value_choice_points", 0)),
             int(row.get("rf_value_choice_points", 0))),
        )
        for label, lower, upper in value_checks:
            if lower > upper:
                violations.append(f"{row['model']} {row['task']} {label}: {lower}>{upper}")

    lines = ["# Candidate-space census", "",
             f"Rows: {len(rows)}; accounting violations: {len(violations)}.", ""]
    for model in ("sc", "tso", "pso"):
        subset = [row for row in rows if row["model"] == model]
        counted = [row for row in subset if "rf_offered" in row]
        terminal = [
            row for row in subset
            if row["status"] in {"true", "false(unreach-call)"}
        ]
        timeouts = [row for row in subset if row["status"] == "TIMEOUT"]
        ooms = [row for row in subset if row["status"] == "OUT OF MEMORY"]
        status_counts = Counter(str(row["status"]) for row in subset)
        lines += [f"## {model.upper()}", "",
                  f"- completed verdict: {len(terminal)}/{len(subset)}; "
                  f"counter-bearing logs: {len(counted)}/{len(subset)}",
                  "- statuses: " + ", ".join(
                      f"{status or '<empty>'}={count}"
                      for status, count in sorted(status_counts.items()))]
        for name in ("rf_offered", "rf_queued", "rf_value_choice_points",
                     "rf_value_classes", "rf_same_value_candidate_upper_bound",
                     "rf_same_value_choice_points", "max_rf_same_value_class",
                     "co_offered", "co_queued", "backward_offered", "backward_queued",
                     "work_added", "work_popped", "max_retained_work",
                     "validity_queries", "realized_revisit_prefixes",
                     "inconsistent_revisit_prefixes"):
            values = [int(row.get(name, 0)) for row in counted]
            lines.append(f"- {name}: total={sum(values)}, median={statistics.median(values) if values else 0:g}, p95={percentile(values, .95):g}, max={max(values, default=0)}")
        rf_offered = sum(int(row.get("rf_offered", 0)) for row in counted)
        same_value = sum(int(row.get("rf_same_value_candidate_upper_bound", 0))
                         for row in counted)
        value_points = sum(int(row.get("rf_value_choice_points", 0)) for row in counted)
        repeated_points = sum(int(row.get("rf_same_value_choice_points", 0))
                              for row in counted)
        lines.append(
            f"- optimistic same-value RF candidate upper bound: {same_value}/{rf_offered} "
            f"= {same_value / rf_offered:.2%}" if rf_offered else
            "- optimistic same-value RF candidate upper bound: n/a"
        )
        lines.append(
            f"- RF choice points with a repeated value class: "
            f"{repeated_points}/{value_points} = {repeated_points / value_points:.2%}"
            if value_points else
            "- RF choice points with a repeated value class: n/a"
        )
        realized = sum(int(row.get("realized_revisit_prefixes", 0)) for row in counted)
        rejected = sum(int(row.get("inconsistent_revisit_prefixes", 0)) for row in counted)
        lines.append(f"- post-generation rejection fraction: {rejected}/{realized} = {rejected / realized:.2%}" if realized else "- post-generation rejection fraction: n/a")
        for label, group in (
            ("completed verdict", terminal),
            ("TIMEOUT", timeouts),
            ("OOM", ooms),
        ):
            group_counted = [row for row in group if "rf_offered" in row]
            group_rf = sum(int(row.get("rf_offered", 0)) for row in group_counted)
            group_same = sum(
                int(row.get("rf_same_value_candidate_upper_bound", 0))
                for row in group_counted
            )
            lines.append(
                f"- {label} same-value bound: {group_same}/{group_rf} = "
                f"{group_same / group_rf:.2%}; logs={len(group_counted)}/{len(group)}"
                if group_rf else
                f"- {label} same-value bound: n/a; logs={len(group_counted)}/{len(group)}"
            )
        timeout_ranked = sorted(
            (
                row for row in timeouts
                if int(row.get("rf_offered", 0)) > 0
            ),
            key=lambda row: int(row.get("rf_same_value_candidate_upper_bound", 0)),
            reverse=True,
        )[:10]
        if timeout_ranked:
            lines += ["- largest TIMEOUT same-value opportunities:"]
            for row in timeout_ranked:
                offered = int(row["rf_offered"])
                same = int(row.get("rf_same_value_candidate_upper_bound", 0))
                lines.append(
                    f"  - {row['task']}: {same}/{offered} = {same / offered:.2%}"
                )
        lines.append("")
    if violations:
        lines += ["## Accounting violations", "", *[f"- {item}" for item in violations]]
    args.summary.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
