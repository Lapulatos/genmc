#!/usr/bin/env python3
"""Task-clustered paired analysis for shared dependency adjacency."""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
from pathlib import Path


def load_common():
	path = Path(__file__).parents[1] / "adaptive-offline-history" / "strict_analysis.py"
	spec = importlib.util.spec_from_file_location("caat_strict_common", path)
	if spec is None or spec.loader is None:
		raise RuntimeError(f"cannot load shared analysis helpers from {path}")
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("rows", type=Path)
	parser.add_argument("--output-dir", type=Path, required=True)
	args = parser.parse_args()
	args.output_dir.mkdir(parents=True, exist_ok=True)
	common = load_common()
	rows = common.read_rows(args.rows)
	output: dict[str, object] = {
		"design": {"repetitions": 6, "bootstrap_samples": 20_000, "unit": "task"}
	}
	all_ratios: list[dict[str, object]] = []
	for metric_index, (metric_name, column) in enumerate(common.METRICS.items()):
		ratios = common.task_ratios(rows, column)
		common_tasks = sorted(set.intersection(*(set(ratios[model]) for model in common.MODELS)))
		task_aggregate = {
			task: common.geomean([ratios[model][task] for model in common.MODELS])
			for task in common_tasks
		}
		groups = {**ratios, "all": task_aggregate}
		summaries = {
			group: common.summarize(
				list(groups[group].values()), 20260715 + metric_index * 10 + index
			)
			for index, group in enumerate(("all", *common.MODELS))
		}
		adjusted = common.holm(
			{group: summaries[group]["sign_test"]["two_sided_p"] for group in summaries}
		)
		for group in summaries:
			summaries[group]["sign_test"]["holm_p_across_four_groups"] = adjusted[group]
		output[metric_name] = summaries
		for group, values in groups.items():
			for task, ratio in values.items():
				all_ratios.append(
					{"metric": metric_name, "group": group, "task_yaml": task, "ratio": ratio}
				)

	with (args.output_dir / "ratios.tsv").open("w", newline="", encoding="utf-8") as sink:
		writer = csv.DictWriter(
			sink, fieldnames=("metric", "group", "task_yaml", "ratio"), delimiter="\t"
		)
		writer.writeheader()
		writer.writerows(all_ratios)
	with (args.output_dir / "metric-summary.tsv").open("w", newline="", encoding="utf-8") as sink:
		fields = (
			"metric", "group", "tasks", "geomean_ratio", "ci_low", "ci_high",
			"median_ratio", "sign_p", "holm_p",
		)
		writer = csv.DictWriter(sink, fieldnames=fields, delimiter="\t")
		writer.writeheader()
		for metric in common.METRICS:
			for group in ("all", *common.MODELS):
				summary = output[metric][group]
				writer.writerow({
					"metric": metric,
					"group": group,
					"tasks": summary["tasks"],
					"geomean_ratio": summary["geomean_ratio"],
					"ci_low": summary["bootstrap_95ci"][0],
					"ci_high": summary["bootstrap_95ci"][1],
					"median_ratio": summary["median_ratio"],
					"sign_p": summary["sign_test"]["two_sided_p"],
					"holm_p": summary["sign_test"]["holm_p_across_four_groups"],
				})
	(args.output_dir / "stats-strict.json").write_text(
		json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)


if __name__ == "__main__":
	main()
