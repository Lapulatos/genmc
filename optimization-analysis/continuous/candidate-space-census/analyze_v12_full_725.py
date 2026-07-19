#!/usr/bin/env python3
"""Validate and summarize the paired complete before/V12 725-task run.

V12 only changes how exact structural-relation successors/predecessors are
enumerated.  It does not introduce a coarser equivalence relation, so equal
verdicts, completed-execution counts, and search counters are correctness
requirements here.  A future quotient/RVF experiment must use a different
oracle and must not reuse the equal-count requirement blindly.
"""

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

CAT_COUNTERS = (
    "preventive-direct-checks", "preventive-direct-base-candidates",
    "preventive-direct-edge-candidates", "preventive-rf-candidates",
    "preventive-rf-pruned", "preventive-co-candidates", "preventive-co-pruned",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    before = FORMAL.load_cell(args.root / "before")
    after = FORMAL.load_cell(args.root / "after")
    if len(before) != 725 or len(after) != 725 or before.keys() != after.keys():
        raise RuntimeError(
            f"expected identical 725-task sets, got {len(before)} and {len(after)}"
        )

    mismatches: list[dict[str, object]] = []
    for task in before:
        differences: dict[str, object] = {}
        for field in ("status", "category"):
            if before[task].get(field) != after[task].get(field):
                differences[field] = [before[task].get(field), after[task].get(field)]
        if (before[task].get("category") == after[task].get("category") == "correct"
                and before[task].get("complete") != after[task].get("complete")):
            differences["complete"] = [
                before[task].get("complete"), after[task].get("complete")
            ]
        for field in FORMAL.SEARCH_COUNTERS:
            if before[task].get(field) != after[task].get(field):
                differences[field] = [before[task].get(field), after[task].get(field)]
        if differences:
            mismatches.append({"task": task, "differences": differences})

    metrics = {}
    for index, metric in enumerate(("cputime", "walltime", "memory")):
        ratios = [
            float(after[task][metric]) / float(before[task][metric])
            for task in before
            if before[task]["category"] == after[task]["category"] == "correct"
            and float(before[task][metric]) > 0
            and float(after[task][metric]) > 0
        ]
        metrics[metric] = FORMAL.bootstrap(ratios, 20260724 + index)

    status_or_category_differences = sum(
        bool(set(item["differences"]) & {"status", "category"}) for item in mismatches
    )
    complete_execution_differences = sum(
        "complete" in item["differences"] for item in mismatches
    )
    common_correct_search_differences = sum(
        bool(set(item["differences"]) & set(FORMAL.SEARCH_COUNTERS))
        and before[item["task"]]["category"] == after[item["task"]]["category"] == "correct"
        for item in mismatches
    )
    before_completed_to_resource = sum(
        before[task]["category"] == "correct" and after[task]["category"] != "correct"
        for task in before
    )
    after_completed_to_resource = sum(
        after[task]["category"] == "correct" and before[task]["category"] != "correct"
        for task in before
    )
    counter_totals = {
        variant: {
            field: sum(int(row.get(field, 0)) for row in rows.values())
            for field in CAT_COUNTERS
        }
        for variant, rows in (("before", before), ("after", after))
    }
    result = {
        "rows": 1450,
        "tasks": 725,
        "mismatches": mismatches,
        "status_or_category_differences": status_or_category_differences,
        "complete_execution_differences": complete_execution_differences,
        "common_correct_search_differences": common_correct_search_differences,
        "before_completed_to_resource": before_completed_to_resource,
        "after_completed_to_resource": after_completed_to_resource,
        "status_counts": {
            "before": dict(Counter(str(row["status"]) for row in before.values())),
            "after": dict(Counter(str(row["status"]) for row in after.values())),
        },
        "metrics": metrics,
        "counter_totals": counter_totals,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "analysis.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n"
    )
    cpu, wall, rss = metrics["cputime"], metrics["walltime"], metrics["memory"]
    (args.output / "analysis.md").write_text(
        "# V12 complete 725-task analysis\n\n"
        "V12 is search-preserving, so execution/search-count equality is required; "
        "this criterion does not apply unchanged to coarser quotient experiments.\n\n"
        f"- Tasks: 725 per variant; total rows: 1,450.\n"
        f"- Status/category differences: {status_or_category_differences}; "
        f"common-correct complete-execution differences: {complete_execution_differences}; "
        f"common-correct search-counter differences: {common_correct_search_differences}.\n"
        f"- Coverage regressions (before completed, after resource-failed): "
        f"{before_completed_to_resource}; reverse transitions: {after_completed_to_resource}.\n"
        f"- Status counts: before `{result['status_counts']['before']}`; "
        f"after `{result['status_counts']['after']}`.\n"
        f"- CAT counter totals: `{counter_totals}`.\n"
        f"- CPU after/before: {cpu['geomean_ratio']:.6f} "
        f"[{cpu['bootstrap_95ci'][0]:.6f}, {cpu['bootstrap_95ci'][1]:.6f}].\n"
        f"- Wall after/before: {wall['geomean_ratio']:.6f} "
        f"[{wall['bootstrap_95ci'][0]:.6f}, {wall['bootstrap_95ci'][1]:.6f}].\n"
        f"- RSS after/before: {rss['geomean_ratio']:.6f} "
        f"[{rss['bootstrap_95ci'][0]:.6f}, {rss['bootstrap_95ci'][1]:.6f}].\n"
    )


if __name__ == "__main__":
    main()
