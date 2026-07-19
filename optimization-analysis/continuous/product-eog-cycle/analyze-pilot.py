#!/usr/bin/env python3
"""Summarize one balanced P0.7e before/after pilot without changing raw evidence."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


STATISTICS = re.compile(r"CAT incremental statistics: (.*)")
NUMBER_WITH_UNIT = re.compile(r"^([0-9.eE+-]+)")


def number(value: str | None) -> float | None:
	if not value:
		return None
	match = NUMBER_WITH_UNIT.match(value)
	return float(match.group(1)) if match else None


def parse_statistics(text: str) -> dict[str, int]:
	match = STATISTICS.search(text)
	if not match:
		return {}
	result: dict[str, int] = {}
	for field in match.group(1).split():
		key, separator, value = field.partition("=")
		if separator and value.isdigit():
			result[key] = int(value)
	return result


def load_variant(root: Path, variant: str) -> dict[tuple[str, str, str], dict]:
	result: dict[tuple[str, str, str], dict] = {}
	for repetition_dir in sorted((root / variant).glob("r*")):
		if not repetition_dir.is_dir():
			continue
		repetition = repetition_dir.name
		zip_paths = list(repetition_dir.glob("*.logfiles.zip"))
		if len(zip_paths) != 1:
			raise ValueError(f"expected one log archive in {repetition_dir}, found {zip_paths}")
		with zipfile.ZipFile(zip_paths[0]) as archive:
			logs = {Path(name).name: archive.read(name).decode(errors="replace") for name in archive.namelist() if name.endswith(".log")}
		for xml_path in sorted(repetition_dir.glob("*.xml.bz2")):
			with bz2.open(xml_path, "rb") as source:
				xml = ET.parse(source).getroot()
			model_match = re.search(r"caat-(sc|tso|pso)", xml.get("name", ""))
			if not model_match:
				raise ValueError(f"cannot identify model from {xml_path}")
			model = model_match.group(1)
			for run in xml.findall("run"):
				task = Path(run.get("name", "")).name
				columns = {column.get("title", ""): column.get("value", "") for column in run.findall("column")}
				log_name = f"caat-{model}.lazy-pilot.{task}.log"
				if log_name not in logs:
					raise ValueError(f"missing {log_name} in {zip_paths[0]}")
				key = (repetition, model, task)
				if key in result:
					raise ValueError(f"duplicate pilot cell {key}")
				result[key] = {
					"status": columns.get("status", ""),
					"category": columns.get("category", ""),
					"cpu": number(columns.get("cputime")),
					"wall": number(columns.get("walltime")),
					"rss": number(columns.get("memory")),
					"statistics": parse_statistics(logs[log_name]),
				}
	return result


def ratio(after: float | int | None, before: float | int | None) -> float | None:
	if after is None or before in (None, 0):
		return None
	return float(after) / float(before)


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("root", type=Path, help="pilot root containing before/ and after/")
	parser.add_argument("output", type=Path, help="output directory")
	args = parser.parse_args()

	before = load_variant(args.root, "before")
	after = load_variant(args.root, "after")
	if before.keys() != after.keys():
		raise ValueError(f"unpaired cells: before-only={before.keys() - after.keys()}, after-only={after.keys() - before.keys()}")

	rows = []
	for repetition, model, task in sorted(before):
		old = before[(repetition, model, task)]
		new = after[(repetition, model, task)]
		row = {
			"repetition": repetition,
			"model": model,
			"task": task,
			"before_status": old["status"],
			"after_status": new["status"],
			"before_category": old["category"],
			"after_category": new["category"],
			"before_cpu_s": old["cpu"],
			"after_cpu_s": new["cpu"],
			"cpu_ratio": ratio(new["cpu"], old["cpu"]),
			"before_wall_s": old["wall"],
			"after_wall_s": new["wall"],
			"wall_ratio": ratio(new["wall"], old["wall"]),
			"before_rss_b": old["rss"],
			"after_rss_b": new["rss"],
			"rss_ratio": ratio(new["rss"], old["rss"]),
		}
		for metric in (
			"lazy-cycle-checks",
			"lazy-edge-candidates",
			"lazy-depth-fallbacks",
			"lazy-product-checks",
			"lazy-product-state-visits",
			"lazy-product-transitions",
			"lazy-product-size-fallbacks",
			"sync-ns",
			"offline-ns",
			"materialize-ns",
			"copy-ns",
			"peak-snapshot-equivalent-bytes",
			"max-stable-events",
		):
			row[f"before_{metric}"] = old["statistics"].get(metric)
			row[f"after_{metric}"] = new["statistics"].get(metric)
		rows.append(row)

	args.output.mkdir(parents=True, exist_ok=True)
	with (args.output / "pairs.tsv").open("w", newline="") as output:
		writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter="\t")
		writer.writeheader()
		writer.writerows(rows)

	summary = {
		"cells": len(rows),
		"status_mismatches": sum(row["before_status"] != row["after_status"] for row in rows),
		"category_mismatches": sum(row["before_category"] != row["after_category"] for row in rows),
		"cells_by_model_task": {},
	}
	for model, task in sorted({(row["model"], row["task"]) for row in rows}):
		group = [row for row in rows if row["model"] == model and row["task"] == task]
		summary["cells_by_model_task"][f"{model}/{task}"] = {
			"cpu_ratios": [row["cpu_ratio"] for row in group],
			"rss_ratios": [row["rss_ratio"] for row in group],
			"cpu_median": statistics.median(row["cpu_ratio"] for row in group),
			"rss_median": statistics.median(row["rss_ratio"] for row in group),
		}
	(args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")

	lines = [
		"# P0.7e pilot summary",
		"",
		f"- Paired cells: {summary['cells']}",
		f"- Status mismatches: {summary['status_mismatches']}",
		f"- Category mismatches: {summary['category_mismatches']}",
		"",
		"| Model/task | CPU ratios by repetition | RSS ratios by repetition |",
		"|---|---:|---:|",
	]
	for key, values in summary["cells_by_model_task"].items():
		cpu = ", ".join(f"{value:.4f}" for value in values["cpu_ratios"])
		rss = ", ".join(f"{value:.4f}" for value in values["rss_ratios"])
		lines.append(f"| {key} | {cpu} | {rss} |")
	(args.output / "summary.md").write_text("\n".join(lines) + "\n")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
