#!/usr/bin/env python3
"""Analyze the two-repetition adapted TIMEOUT/OOM no-retention cohort."""

from __future__ import annotations

import argparse
import json
import statistics
from collections import Counter
from pathlib import Path

from analyze_no_retain import bootstrap, geomean, load_logs, load_xml, result_file


REPETITIONS = ("01", "02")
VARIANTS = ("baseline", "no-retain")
RESOURCE = {"TIMEOUT", "OUT OF MEMORY"}


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("root", type=Path)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=True)

	rows = []
	logs = {}
	for repetition in REPETITIONS:
		for variant in VARIANTS:
			directory = args.root / f"r{repetition}" / variant
			rows.extend(load_xml(result_file(directory, "*.xml*"), repetition, variant))
			logs[(repetition, variant)] = load_logs(result_file(directory, "*.logfiles.zip"))
	index = {(row["repetition"], row["variant"], row["task"]): row for row in rows}
	tasks = sorted({str(row["task"]) for row in rows})
	if len(rows) != len(REPETITIONS) * len(VARIANTS) * len(tasks):
		raise RuntimeError(f"incomplete matrix: {len(rows)} rows for {len(tasks)} tasks")

	transitions: Counter[str] = Counter()
	task_transitions = {}
	execution_mismatches = []
	for task in tasks:
		per_repetition = []
		for repetition in REPETITIONS:
			before = index[(repetition, "baseline", task)]
			after = index[(repetition, "no-retain", task)]
			transition = f"{before['status']} -> {after['status']}"
			per_repetition.append(transition)
			if before["status"] != after["status"]:
				transitions[transition] += 1
			if before["status"] == after["status"] == "true":
				before_count = logs[(repetition, "baseline")].get(Path(task).name, {}).get("executions")
				after_count = logs[(repetition, "no-retain")].get(Path(task).name, {}).get("executions")
				if before_count != after_count:
					execution_mismatches.append([repetition, task, before_count, after_count])
		task_transitions[task] = per_repetition

	repeatable_differences = {
		task: changes[0]
		for task, changes in task_transitions.items()
		if len(set(changes)) == 1 and changes[0].split(" -> ", 1)[0] != changes[0].split(" -> ", 1)[1]
	}
	repeatable_terminal_gains = {
		task: transition
		for task, transition in repeatable_differences.items()
		if transition.split(" -> ", 1)[0] in RESOURCE
		and transition.split(" -> ", 1)[1] not in RESOURCE
	}
	repeatable_oom_regressions = {
		task: transition
		for task, transition in repeatable_differences.items()
		if transition.split(" -> ", 1)[1] == "OUT OF MEMORY"
		and transition.split(" -> ", 1)[0] != "OUT OF MEMORY"
	}

	metrics = {}
	for name, column in (("cpu", "cputime"),):
		ratios = []
		for task in tasks:
			pairs = []
			for repetition in REPETITIONS:
				before = index[(repetition, "baseline", task)]
				after = index[(repetition, "no-retain", task)]
				if before["category"] != after["category"] or before["category"] != "correct":
					break
				pairs.append((float(before[column]), float(after[column])))
			if len(pairs) == len(REPETITIONS):
				ratios.append(
					statistics.median(after for _, after in pairs)
					/ statistics.median(before for before, _ in pairs)
				)
		if ratios:
			metrics[name] = {
				"tasks": len(ratios),
				"geomean_ratio": geomean(ratios),
				"bootstrap_95ci": bootstrap(ratios, 20260740),
			}

	result = {
		"design": {"tasks": len(tasks), "repetitions": 2, "cells": len(rows)},
		"status_counts": {
			variant: dict(Counter(str(row["status"]) for row in rows if row["variant"] == variant))
			for variant in VARIANTS
		},
		"status_transitions": dict(transitions),
		"repeatable_differences": repeatable_differences,
		"repeatable_terminal_gains": repeatable_terminal_gains,
		"repeatable_oom_regressions": repeatable_oom_regressions,
		"safe_execution_count_mismatches": execution_mismatches,
		"metrics": metrics,
	}
	(args.output / "stats-strict.json").write_text(
		json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	(args.output / "report.md").write_text(
		f"""# P0.4 adapted hard cohort

## Result

The two-repetition matrix contains {len(rows)} cells over {len(tasks)} tasks. It has
{len(repeatable_terminal_gains)} repeatable resource-to-terminal gains,
{len(repeatable_oom_regressions)} repeatable new OOM regressions, and
{len(execution_mismatches)} safe execution-count mismatches.

Repeatable status differences: `{json.dumps(repeatable_differences, sort_keys=True)}`.
""",
		encoding="utf-8",
	)


if __name__ == "__main__":
	main()
