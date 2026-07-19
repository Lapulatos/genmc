#!/usr/bin/env python3
"""Validate and summarize the paired V9/V10 four-repetition formal matrix."""

from __future__ import annotations

import argparse
import bz2
import json
import math
import random
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter
from pathlib import Path


PAIR_RE = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")
COMPLETE_RE = re.compile(r"Number of complete executions explored: ([0-9]+)")
SEARCH_RE = re.compile(r"Exploration statistics: (.*)")
CAT_RE = re.compile(r"CAT incremental statistics: (.*)")
SEARCH_COUNTERS = (
    "rf-offered", "rf-queued", "co-offered", "co-queued", "backward-offered",
    "backward-queued", "work-added", "work-popped", "max-retained-work",
    "validity-queries", "realized-revisit-prefixes", "inconsistent-revisit-prefixes",
)
CORE_COUNTERS = (
    "conflict-core-learn-attempts", "conflict-core-learned",
    "conflict-core-unsupported", "conflict-core-duplicate-subsumed",
    "conflict-core-evicted", "conflict-core-match-queries",
    "conflict-core-literal-checks", "conflict-core-hits",
    "conflict-core-direct-checks-avoided", "conflict-core-rf-pruned",
    "conflict-core-co-pruned", "conflict-core-max-clauses",
    "conflict-core-max-literals", "conflict-core-max-bytes",
)


def task_key(name: str) -> str:
    return Path(name).stem


def columns(run: ET.Element) -> dict[str, str]:
    return {item.get("title", ""): item.get("value", "") for item in run.findall("column")}


def load_cell(directory: Path) -> dict[str, dict[str, object]]:
    xml_path, = directory.glob("*.xml.bz2")
    zip_path, = directory.glob("*.logfiles.zip")
    root = ET.fromstring(bz2.open(xml_path, "rb").read())
    rows: dict[str, dict[str, object]] = {}
    for run in root.findall("run"):
        values = columns(run)
        key = task_key(run.get("name", ""))
        rows[key] = {
            "task": run.get("name", ""),
            "status": values.get("status", ""),
            "category": values.get("category", ""),
            "cputime": float(values.get("cputime", "0s").removesuffix("s")),
            "walltime": float(values.get("walltime", "0s").removesuffix("s")),
            "memory": int(values.get("memory", "0B").removesuffix("B")),
        }
    with zipfile.ZipFile(zip_path) as archive:
        for member in archive.namelist():
            if member.endswith("/"):
                continue
            text = archive.read(member).decode("utf-8", errors="replace")
            matches = [key for key in rows if Path(member).name.endswith(f".{key}.yml.log")]
            if len(matches) != 1:
                continue
            row = rows[matches[0]]
            complete = COMPLETE_RE.findall(text)
            if complete:
                row["complete"] = int(complete[-1])
            for pattern in (SEARCH_RE, CAT_RE):
                stats = pattern.findall(text)
                if stats:
                    row.update({key: int(value) for key, value in PAIR_RE.findall(stats[-1])})
    return rows


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int) -> dict[str, object]:
    rng = random.Random(seed)
    estimates = sorted(geomean([rng.choice(values) for _ in values]) for _ in range(20_000))
    return {
        "tasks": len(values),
        "geomean_ratio": geomean(values),
        "median_ratio": statistics.median(values),
        "bootstrap_95ci": [estimates[500], estimates[19_499]],
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    cells = {
        (rep, variant): load_cell(args.root / rep / variant)
        for rep in ("r01", "r02", "r03", "r04") for variant in ("v9", "v10")
    }
    sizes = {f"{rep}/{variant}": len(rows) for (rep, variant), rows in cells.items()}
    if any(size != 96 for size in sizes.values()):
        raise RuntimeError(f"expected 96 tasks per cell: {sizes}")

    mismatches: list[dict[str, object]] = []
    for rep in ("r01", "r02", "r03", "r04"):
        old, new = cells[(rep, "v9")], cells[(rep, "v10")]
        if old.keys() != new.keys():
            raise RuntimeError(f"task-set mismatch in {rep}")
        for task in old:
            differences: dict[str, object] = {}
            for field in ("status", "category"):
                if old[task].get(field) != new[task].get(field):
                    differences[field] = [old[task].get(field), new[task].get(field)]
            if old[task].get("category") == "correct" and old[task].get("complete") != new[task].get("complete"):
                differences["complete"] = [old[task].get("complete"), new[task].get("complete")]
            for field in SEARCH_COUNTERS:
                if old[task].get(field) != new[task].get(field):
                    differences[field] = [old[task].get(field), new[task].get(field)]
            if differences:
                mismatches.append({"repetition": rep, "task": task, "differences": differences})

    tasks = sorted(cells[("r01", "v9")])
    metrics: dict[str, object] = {}
    for index, metric in enumerate(("cputime", "walltime", "memory")):
        ratios = []
        for task in tasks:
            pairs = [(cells[(rep, "v9")][task], cells[(rep, "v10")][task])
                     for rep in ("r01", "r02", "r03", "r04")]
            if any(old["category"] != "correct" or new["category"] != "correct" for old, new in pairs):
                continue
            old_median = statistics.median(float(old[metric]) for old, _ in pairs)
            new_median = statistics.median(float(new[metric]) for _, new in pairs)
            if old_median > 0 and new_median > 0:
                ratios.append(new_median / old_median)
        metrics[metric] = bootstrap(ratios, 20260717 + index)

    core_totals = Counter()
    activated_cells = 0
    for rep in ("r01", "r02", "r03", "r04"):
        for row in cells[(rep, "v10")].values():
            if int(row.get("conflict-core-hits", 0)) > 0:
                activated_cells += 1
            for field in CORE_COUNTERS:
                core_totals[field] += int(row.get(field, 0))
    status_counts = {
        variant: dict(Counter(str(row["status"]) for rep in ("r01", "r02", "r03", "r04")
                              for row in cells[(rep, variant)].values()))
        for variant in ("v9", "v10")
    }
    result = {
        "cell_sizes": sizes,
        "rows": sum(sizes.values()),
        "mismatches": mismatches,
        "status_counts": status_counts,
        "metrics": metrics,
        "v10_activated_rows": activated_cells,
        "v10_core_totals": dict(core_totals),
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "analysis.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    cpu, wall, rss = metrics["cputime"], metrics["walltime"], metrics["memory"]
    (args.output / "analysis.md").write_text(
        "# V10 formal analysis\n\n"
        f"- Rows: {result['rows']}; semantic/search mismatches: {len(mismatches)}.\n"
        f"- V10 rows with at least one conflict-core hit: {activated_cells}/384.\n"
        f"- Core totals: `{dict(core_totals)}`.\n"
        f"- CPU V10/V9: {cpu['geomean_ratio']:.6f} "
        f"[{cpu['bootstrap_95ci'][0]:.6f}, {cpu['bootstrap_95ci'][1]:.6f}].\n"
        f"- Wall V10/V9: {wall['geomean_ratio']:.6f} "
        f"[{wall['bootstrap_95ci'][0]:.6f}, {wall['bootstrap_95ci'][1]:.6f}].\n"
        f"- RSS V10/V9: {rss['geomean_ratio']:.6f} "
        f"[{rss['bootstrap_95ci'][0]:.6f}, {rss['bootstrap_95ci'][1]:.6f}].\n"
    )


if __name__ == "__main__":
    main()
