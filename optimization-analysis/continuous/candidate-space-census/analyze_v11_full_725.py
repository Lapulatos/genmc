#!/usr/bin/env python3
"""Strictly validate and summarize the paired complete V9/V11 725-task run."""

from __future__ import annotations

import argparse
import importlib.util
import json
from collections import Counter
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("formal", HERE / "analyze_v10_formal.py")
assert SPEC and SPEC.loader
FORMAL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORMAL)

FOCUS_COUNTERS = (
    "focus-reach-attempts", "focus-reach-successes", "focus-reach-fallbacks",
    "focus-reach-edge-candidates", "preventive-direct-checks",
    "preventive-direct-edge-candidates", "preventive-rf-candidates",
    "preventive-rf-pruned", "preventive-co-candidates", "preventive-co-pruned",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    old = FORMAL.load_cell(args.root / "v9")
    new = FORMAL.load_cell(args.root / "v11")
    if len(old) != 725 or len(new) != 725 or old.keys() != new.keys():
        raise RuntimeError(f"expected identical 725-task sets, got {len(old)} and {len(new)}")

    mismatches = []
    for task in old:
        differences = {}
        for field in ("status", "category"):
            if old[task].get(field) != new[task].get(field):
                differences[field] = [old[task].get(field), new[task].get(field)]
        if old[task].get("category") == new[task].get("category") == "correct" and old[task].get("complete") != new[task].get("complete"):
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
        metrics[metric] = FORMAL.bootstrap(ratios, 20260721 + index)

    totals = {
        variant: {field: sum(int(row.get(field, 0)) for row in rows.values())
                  for field in FOCUS_COUNTERS}
        for variant, rows in (("v9", old), ("v11", new))
    }
    activated = sum(int(row.get("focus-reach-successes", 0) > 0) for row in new.values())
    status_differences = sum(
        bool(set(item["differences"]) & {"status", "category"}) for item in mismatches
    )
    complete_differences = sum("complete" in item["differences"] for item in mismatches)
    # Resource-limit transitions are performance coverage changes, not semantic
    # complete-execution mismatches because one side has no completed exploration.
    v9_completed_to_resource = sum(
        old[task]["category"] == "correct" and new[task]["category"] != "correct"
        for task in old
    )
    v11_completed_to_resource = sum(
        new[task]["category"] == "correct" and old[task]["category"] != "correct"
        for task in old
    )
    common_correct_search_differences = sum(
        bool(set(item["differences"]) & set(FORMAL.SEARCH_COUNTERS))
        and old[item["task"]]["category"] == new[item["task"]]["category"] == "correct"
        for item in mismatches
    )
    result = {
        "rows": 1450,
        "tasks": 725,
        "mismatches": mismatches,
        "status_or_category_differences": status_differences,
        "complete_execution_differences": complete_differences,
        "v9_completed_to_resource": v9_completed_to_resource,
        "v11_completed_to_resource": v11_completed_to_resource,
        "common_correct_search_differences": common_correct_search_differences,
        "status_counts": {
            "v9": dict(Counter(str(row["status"]) for row in old.values())),
            "v11": dict(Counter(str(row["status"]) for row in new.values())),
        },
        "metrics": metrics,
        "v11_activated_tasks": activated,
        "counter_totals": totals,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "analysis.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    cpu, wall, rss = metrics["cputime"], metrics["walltime"], metrics["memory"]
    (args.output / "analysis.md").write_text(
        "# V11 complete 725-task analysis\n\n"
        f"- Tasks: 725 per variant; total rows: 1,450.\n"
        f"- Status/category differences: {status_differences}; common-correct "
        f"complete-execution differences: {complete_differences}; common-correct search-counter differences: "
        f"{common_correct_search_differences}.\n"
        f"- Coverage regressions (V9 completed, V11 resource-failed): "
        f"{v9_completed_to_resource}; reverse transitions: {v11_completed_to_resource}.\n"
        f"- Status counts: V9 `{result['status_counts']['v9']}`; V11 `{result['status_counts']['v11']}`.\n"
        f"- V11 tasks with successful focus reach: {activated}/725.\n"
        f"- V9/V11 focus and root totals: `{totals}`.\n"
        f"- CPU V11/V9: {cpu['geomean_ratio']:.6f} "
        f"[{cpu['bootstrap_95ci'][0]:.6f}, {cpu['bootstrap_95ci'][1]:.6f}].\n"
        f"- Wall V11/V9: {wall['geomean_ratio']:.6f} "
        f"[{wall['bootstrap_95ci'][0]:.6f}, {wall['bootstrap_95ci'][1]:.6f}].\n"
        f"- RSS V11/V9: {rss['geomean_ratio']:.6f} "
        f"[{rss['bootstrap_95ci'][0]:.6f}, {rss['bootstrap_95ci'][1]:.6f}].\n"
    )


if __name__ == "__main__":
    main()
