#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import math
from pathlib import Path

import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parent
ANALYSIS = ROOT / "server-results" / "formal" / "analysis"
OUT = ROOT / "analysis-output" / "figures"


def main() -> None:
	OUT.mkdir(parents=True, exist_ok=True)
	summary = json.loads((ANALYSIS / "summary.json").read_text())
	labels = ["All", "SC", "TSO", "PSO"]
	points = [summary["cpu_geomean_of_task_model_medians"]]
	intervals = [summary["cpu_four_repetition_model_task_bootstrap_95_ci"]]
	for model in ("sc", "tso", "pso"):
		points.append(summary["by_model"][model]["cpu_geomean"])
		intervals.append(summary["by_model"][model]["cpu_bootstrap_95_ci"])
	errors = [[point - interval[0] for point, interval in zip(points, intervals)],
		  [interval[1] - point for point, interval in zip(points, intervals)]]
	fig, ax = plt.subplots(figsize=(7.0, 3.8))
	colors = ["#4C78A8" if high < 1 else "#E45756" if point > 1 else "#F2CF5B"
		  for point, (_, high) in zip(points, intervals)]
	ax.bar(labels, points, color=colors, edgecolor="#333333", linewidth=0.7)
	ax.errorbar(range(len(points)), points, yerr=errors, fmt="none", ecolor="#222222",
		    capsize=5, linewidth=1.2)
	ax.axhline(1.0, color="#222222", linestyle="--", linewidth=1)
	ax.set_ylabel("CPU ratio (candidate / baseline)")
	ax.set_title("P0.7g2 four-repetition task-model CPU ratios")
	ax.set_ylim(min(low for low, _ in intervals) - 0.004,
		    max(high for _, high in intervals) + 0.004)
	ax.grid(axis="y", color="#dddddd", linewidth=0.6)
	fig.tight_layout()
	for suffix in ("pdf", "png"):
		fig.savefig(OUT / f"figure-01-cpu-ratios.{suffix}", dpi=220)
	plt.close(fig)

	memory_by_model: dict[str, list[float]] = {model: [] for model in ("sc", "tso", "pso")}
	with (ANALYSIS / "pairs.tsv").open(newline="") as handle:
		for row in csv.DictReader(handle, delimiter="\t"):
			value = row["memory_ratio"]
			if value:
				memory_by_model[row["model"]].append(float(value))
	fig, ax = plt.subplots(figsize=(7.0, 3.8))
	data = [memory_by_model[model] for model in ("sc", "tso", "pso")]
	ax.boxplot(data, tick_labels=["SC", "TSO", "PSO"], showfliers=False, whis=(10, 90),
		   medianprops={"color": "#E45756", "linewidth": 1.5},
		   boxprops={"color": "#4C78A8"}, whiskerprops={"color": "#4C78A8"},
		   capprops={"color": "#4C78A8"})
	ax.axhline(1.0, color="#222222", linestyle="--", linewidth=1)
	ax.axhline(1.02, color="#E45756", linestyle=":", linewidth=1, label="frozen 1.02 gate")
	ax.set_ylabel("RSS ratio (candidate / baseline)")
	ax.set_title("P0.7g2 paired memory ratios (10th–90th whiskers)")
	ax.legend(frameon=False, loc="upper right")
	ax.grid(axis="y", color="#dddddd", linewidth=0.6)
	fig.tight_layout()
	for suffix in ("pdf", "png"):
		fig.savefig(OUT / f"figure-02-memory-ratios.{suffix}", dpi=220)
	plt.close(fig)


if __name__ == "__main__":
	main()
