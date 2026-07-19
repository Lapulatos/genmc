#!/usr/bin/env python3
"""Analyze repeated paired before/V12 full-725 runs using per-task medians."""

from __future__ import annotations

import argparse
import importlib.util
import json
import statistics
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("formal", HERE / "analyze_v10_formal.py")
assert SPEC and SPEC.loader
FORMAL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FORMAL)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("roots", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if len(args.roots) < 2:
        raise RuntimeError("at least two repetitions are required")

    cells = []
    for root in args.roots:
        before = FORMAL.load_cell(root / "before")
        after = FORMAL.load_cell(root / "after")
        if len(before) != 725 or len(after) != 725 or before.keys() != after.keys():
            raise RuntimeError(
                f"{root}: expected identical 725-task sets, got {len(before)} and {len(after)}"
            )
        cells.append((root.name, before, after))
    reference = set(cells[0][1])
    if any(set(before) != reference for _, before, _ in cells):
        raise RuntimeError("task sets differ across repetitions")

    mismatches: list[dict[str, object]] = []
    coverage = []
    for repetition, before, after in cells:
        lost = gained = 0
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
            # Search counters are a semantic/search-preservation oracle only for
            # executions that both variants finish.  TIMEOUT/OOM rows naturally stop
            # at different prefixes even when their terminal resource status matches.
            if before[task].get("category") == after[task].get("category") == "correct":
                for field in FORMAL.SEARCH_COUNTERS:
                    if before[task].get(field) != after[task].get(field):
                        differences[field] = [before[task].get(field), after[task].get(field)]
            if differences:
                mismatches.append(
                    {"repetition": repetition, "task": task, "differences": differences}
                )
            lost += before[task]["category"] == "correct" and after[task]["category"] != "correct"
            gained += after[task]["category"] == "correct" and before[task]["category"] != "correct"
        coverage.append({"repetition": repetition, "lost": lost, "gained": gained})

    metrics = {}
    eligible_tasks = []
    for task in sorted(reference):
        if all(
            before[task]["category"] == after[task]["category"] == "correct"
            for _, before, after in cells
        ):
            eligible_tasks.append(task)
    for index, metric in enumerate(("cputime", "walltime", "memory")):
        ratios = []
        for task in eligible_tasks:
            before_median = statistics.median(
                float(before[task][metric]) for _, before, _ in cells
            )
            after_median = statistics.median(
                float(after[task][metric]) for _, _, after in cells
            )
            if before_median > 0 and after_median > 0:
                ratios.append(after_median / before_median)
        metrics[metric] = FORMAL.bootstrap(ratios, 20260728 + index)

    status_or_category_differences = sum(
        bool(set(item["differences"]) & {"status", "category"}) for item in mismatches
    )
    complete_execution_differences = sum(
        "complete" in item["differences"] for item in mismatches
    )
    search_counter_differences = sum(
        bool(set(item["differences"]) & set(FORMAL.SEARCH_COUNTERS)) for item in mismatches
    )
    result = {
        "repetitions": [name for name, _, _ in cells],
        "tasks_per_variant_per_repetition": 725,
        "rows": 725 * 2 * len(cells),
        "median_metric_tasks": len(eligible_tasks),
        "mismatches": mismatches,
        "status_or_category_differences": status_or_category_differences,
        "complete_execution_differences": complete_execution_differences,
        "common_correct_search_counter_differences": search_counter_differences,
        "coverage": coverage,
        "metrics": metrics,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "analysis.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n"
    )
    cpu, wall, rss = metrics["cputime"], metrics["walltime"], metrics["memory"]
    (args.output / "analysis.md").write_text(
        "# V12 repeated full-725 analysis\n\n"
        f"- Repetitions: {len(cells)}; rows: {result['rows']}; "
        f"per-task median cohort: {len(eligible_tasks)}.\n"
        f"- Status/category differences: {status_or_category_differences}; "
        f"complete-execution differences: {complete_execution_differences}; "
        f"common-correct search-counter differences: {search_counter_differences}.\n"
        f"- Coverage transitions: `{coverage}`.\n"
        f"- CPU after/before: {cpu['geomean_ratio']:.6f} "
        f"[{cpu['bootstrap_95ci'][0]:.6f}, {cpu['bootstrap_95ci'][1]:.6f}].\n"
        f"- Wall after/before: {wall['geomean_ratio']:.6f} "
        f"[{wall['bootstrap_95ci'][0]:.6f}, {wall['bootstrap_95ci'][1]:.6f}].\n"
        f"- RSS after/before: {rss['geomean_ratio']:.6f} "
        f"[{rss['bootstrap_95ci'][0]:.6f}, {rss['bootstrap_95ci'][1]:.6f}].\n"
    )


if __name__ == "__main__":
    main()
