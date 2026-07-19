#!/usr/bin/env python3
"""Validate and summarize the complete paired 725-task V9/V10 experiment."""

from __future__ import annotations

import argparse
import importlib.util
import json
from collections import Counter
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("v10_formal", HERE / "analyze_v10_formal.py")
assert SPEC and SPEC.loader
FORMAL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORMAL)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    old = FORMAL.load_cell(args.root / "v9")
    new = FORMAL.load_cell(args.root / "v10")
    if len(old) != 725 or len(new) != 725 or old.keys() != new.keys():
        raise RuntimeError(f"expected identical 725-task sets, got {len(old)} and {len(new)}")

    mismatches = []
    for task in old:
        differences = {}
        for field in ("status", "category"):
            if old[task].get(field) != new[task].get(field):
                differences[field] = [old[task].get(field), new[task].get(field)]
        if old[task].get("category") == "correct" and old[task].get("complete") != new[task].get("complete"):
            differences["complete"] = [old[task].get("complete"), new[task].get("complete")]
        for field in FORMAL.SEARCH_COUNTERS:
            if old[task].get(field) != new[task].get(field):
                differences[field] = [old[task].get(field), new[task].get(field)]
        if differences:
            mismatches.append({"task": task, "differences": differences})

    metrics = {}
    for index, metric in enumerate(("cputime", "walltime", "memory")):
        ratios = [float(new[task][metric]) / float(old[task][metric]) for task in old
                  if old[task]["category"] == new[task]["category"] == "correct"
                  and float(old[task][metric]) > 0 and float(new[task][metric]) > 0]
        metrics[metric] = FORMAL.bootstrap(ratios, 20260718 + index)

    core_totals = Counter()
    activated = 0
    for row in new.values():
        activated += int(row.get("conflict-core-hits", 0) > 0)
        for field in FORMAL.CORE_COUNTERS:
            core_totals[field] += int(row.get(field, 0))
    status_mismatches = sum(
        bool(set(item["differences"]) & {"status", "category"}) for item in mismatches
    )
    complete_mismatches = sum("complete" in item["differences"] for item in mismatches)
    common_correct_search_mismatches = sum(
        bool(set(item["differences"]) & set(FORMAL.SEARCH_COUNTERS))
        and old[item["task"]]["category"] == new[item["task"]]["category"] == "correct"
        for item in mismatches
    )
    result = {
        "rows": 1450,
        "tasks": 725,
        "mismatches": mismatches,
        "status_or_category_mismatches": status_mismatches,
        "complete_execution_mismatches": complete_mismatches,
        "common_correct_search_mismatches": common_correct_search_mismatches,
        "status_counts": {
            "v9": dict(Counter(str(row["status"]) for row in old.values())),
            "v10": dict(Counter(str(row["status"]) for row in new.values())),
        },
        "metrics": metrics,
        "v10_activated_tasks": activated,
        "v10_core_totals": dict(core_totals),
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "analysis.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    cpu, wall, rss = metrics["cputime"], metrics["walltime"], metrics["memory"]
    (args.output / "analysis.md").write_text(
        "# V10 complete 725-task analysis\n\n"
        f"- Tasks: 725 per variant; total rows: 1,450.\n"
        f"- Status/category differences: {status_mismatches}; complete-execution differences: "
        f"{complete_mismatches}; common-correct search-counter differences: "
        f"{common_correct_search_mismatches}.\n"
        f"- Status counts: V9 `{result['status_counts']['v9']}`; V10 `{result['status_counts']['v10']}`.\n"
        f"- V10 tasks with conflict-core hits: {activated}/725.\n"
        f"- Conflict-core totals: `{dict(core_totals)}`.\n"
        f"- CPU V10/V9: {cpu['geomean_ratio']:.6f} "
        f"[{cpu['bootstrap_95ci'][0]:.6f}, {cpu['bootstrap_95ci'][1]:.6f}].\n"
        f"- Wall V10/V9: {wall['geomean_ratio']:.6f} "
        f"[{wall['bootstrap_95ci'][0]:.6f}, {wall['bootstrap_95ci'][1]:.6f}].\n"
        f"- RSS V10/V9: {rss['geomean_ratio']:.6f} "
        f"[{rss['bootstrap_95ci'][0]:.6f}, {rss['bootstrap_95ci'][1]:.6f}].\n"
    )


if __name__ == "__main__":
    main()
