#!/usr/bin/env python3
"""Generate the P1 clean-candidate resource figures from strict paired TSVs."""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


def read_rows(path: Path) -> list[dict[str, str]]:
	with path.open(newline="") as stream:
		return list(csv.DictReader(stream, delimiter="\t"))


def ratio(row: dict[str, str], metric: str) -> float:
	return float(row[f"candidate_{metric}"]) / float(row[f"baseline_{metric}"])


def save(figure: plt.Figure, output: Path, stem: str) -> None:
	for suffix in ("pdf", "png"):
		figure.savefig(output / f"{stem}.{suffix}", bbox_inches="tight", dpi=220)


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("full", type=Path)
	parser.add_argument("cumulative_oom", type=Path)
	parser.add_argument("eventdeps_oom", type=Path)
	parser.add_argument("inline_oom", type=Path)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=True)

	full = read_rows(args.full)
	solved = {"true", "false(unreach-call)"}
	common = [row for row in full if row["baseline_status"] in solved and
		  row["candidate_status"] in solved]

	figure, axes = plt.subplots(1, 2, figsize=(8.4, 3.25))
	for axis, metric, label in zip(axes, ("cpu", "memory"), ("CPU time", "peak RSS")):
		values = sorted(ratio(row, metric) for row in common)
		axis.plot(range(1, len(values) + 1), values, color="#2b6cb0", linewidth=1.4)
		axis.axhline(1.0, color="#333333", linestyle="--", linewidth=1)
		axis.set_xlabel("Common-solved task (sorted)")
		axis.set_ylabel(f"candidate / baseline {label}")
		axis.set_yscale("log")
		axis.grid(alpha=0.2)
	figure.suptitle("Clean cumulative P1 candidate on 438 common-solved tasks")
	figure.tight_layout()
	save(figure, args.output, "figure-03-p1-clean-common-solved")
	plt.close(figure)

	paths = (args.cumulative_oom, args.eventdeps_oom, args.inline_oom)
	labels = ("five-item\ncumulative", "EventDeps\nonly", "inline revisit\nonly")
	cpu = []
	for path in paths:
		rows = read_rows(path)
		cpu.append(sum(float(row["candidate_cpu"]) for row in rows) /
			   sum(float(row["baseline_cpu"]) for row in rows))
	figure, axis = plt.subplots(figsize=(5.4, 3.4))
	bars = axis.bar(labels, cpu, color=("#c53030", "#dd6b20", "#718096"))
	axis.axhline(1.0, color="#333333", linestyle="--", linewidth=1)
	axis.set_ylabel("candidate / baseline total CPU")
	axis.set_title("31-task OOM gate, swapped NUMA assignment")
	axis.set_ylim(0.98, max(cpu) + 0.03)
	for bar, value in zip(bars, cpu):
		axis.text(bar.get_x() + bar.get_width() / 2, value + 0.004,
			  f"{(value - 1) * 100:+.1f}%", ha="center", va="bottom")
	axis.grid(axis="y", alpha=0.2)
	figure.tight_layout()
	save(figure, args.output, "figure-04-p1-oom-ablation")
	plt.close(figure)


if __name__ == "__main__":
	main()
