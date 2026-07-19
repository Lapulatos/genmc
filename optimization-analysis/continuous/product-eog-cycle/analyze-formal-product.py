#!/usr/bin/env python3
"""Analyze P0.7e product activation and large-task internal costs from raw logs."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import math
import random
import re
import statistics
import zipfile
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


STAT = re.compile(r"\b([a-z][a-z0-9-]*)=([0-9]+)\b")
EXECUTIONS = re.compile(r"Number of complete executions explored: ([0-9]+)")
MARKER = ".lazy-cycle."


def geomean(values: list[float]) -> float:
	return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int = 20260716) -> list[float]:
	rng = random.Random(seed)
	samples = sorted(geomean([rng.choice(values) for _ in values]) for _ in range(20_000))
	return [samples[500], samples[19_499]]


def read_profiles(formal: Path) -> dict[tuple[str, str, str, str], dict[str, int]]:
	result: dict[tuple[str, str, str, str], dict[str, int]] = {}
	for archive_path in sorted(formal.glob("*/*/*/*.logfiles.zip")):
		variant, model, repetition = archive_path.relative_to(formal).parts[:3]
		with zipfile.ZipFile(archive_path) as archive:
			for member in archive.namelist():
				name = Path(member).name
				if MARKER not in name or not name.endswith(".log"):
					continue
				task = name.split(MARKER, 1)[1][:-4]
				text = archive.read(member).decode(errors="replace")
				profile = {
					"copy_ns": 0,
					"offline_ns": 0,
					"peak_snapshot_bytes": 0,
					"max_stable_events": 0,
					"lazy_cycle_checks": 0,
					"lazy_edge_candidates": 0,
					"lazy_depth_fallbacks": 0,
					"product_checks": 0,
					"product_state_visits": 0,
					"product_transitions": 0,
					"product_size_fallbacks": 0,
					"executions": 0,
					"records": 0,
				}
				for line in text.splitlines():
					if not line.startswith("CAT incremental statistics:"):
						continue
					values = {key: int(value) for key, value in STAT.findall(line)}
					profile["copy_ns"] += values.get("copy-ns", 0)
					profile["offline_ns"] += values.get("offline-ns", 0)
					profile["peak_snapshot_bytes"] = max(profile["peak_snapshot_bytes"], values.get("peak-snapshot-equivalent-bytes", 0))
					profile["max_stable_events"] = max(profile["max_stable_events"], values.get("max-stable-events", 0))
					profile["lazy_cycle_checks"] += values.get("lazy-cycle-checks", 0)
					profile["lazy_edge_candidates"] += values.get("lazy-edge-candidates", 0)
					profile["lazy_depth_fallbacks"] += values.get("lazy-depth-fallbacks", 0)
					profile["product_checks"] += values.get("lazy-product-checks", 0)
					profile["product_state_visits"] += values.get("lazy-product-state-visits", 0)
					profile["product_transitions"] += values.get("lazy-product-transitions", 0)
					profile["product_size_fallbacks"] += values.get("lazy-product-size-fallbacks", 0)
					profile["records"] += 1
				executions = EXECUTIONS.search(text)
				if executions:
					profile["executions"] = int(executions.group(1))
				key = (variant, model, repetition, task)
				if key in result:
					raise ValueError(f"duplicate log profile {key}")
				result[key] = profile
	return result


def read_process_memory(formal: Path) -> dict[tuple[str, str, str, str], int]:
	result: dict[tuple[str, str, str, str], int] = {}
	for xml_path in sorted(formal.glob("*/*/*/*.xml.bz2")):
		variant, model, repetition = xml_path.relative_to(formal).parts[:3]
		with bz2.open(xml_path, "rb") as source:
			xml = ET.parse(source).getroot()
		for run in xml.findall("run"):
			columns = {column.get("title", ""): column.get("value", "") for column in run.findall("column")}
			memory = columns.get("memory", "")
			if memory.endswith("B"):
				memory = memory[:-1]
			if memory:
				result[(variant, model, repetition, Path(run.get("name", "")).name)] = int(memory)
	return result


def paired_metric(profiles: dict[tuple[str, str, str, str], dict[str, int]], metric: str) -> dict[str, object]:
	grouped: dict[tuple[str, str], list[tuple[int, int]]] = defaultdict(list)
	keys = {(model, repetition, task) for _, model, repetition, task in profiles}
	for model, repetition, task in keys:
		before = profiles.get(("before", model, repetition, task), {})
		after = profiles.get(("after", model, repetition, task), {})
		if min(before.get("max_stable_events", 0), after.get("max_stable_events", 0)) < 512:
			continue
		old, new = before.get(metric, 0), after.get(metric, 0)
		if old > 0 and new > 0:
			grouped[(model, task)].append((old, new))
	output: dict[str, object] = {}
	for selected in ("sc", "tso", "pso", "all"):
		ratios = [
			statistics.median(new for _, new in pairs) / statistics.median(old for old, _ in pairs)
			for (model, _), pairs in grouped.items()
			if len(pairs) == 4 and (selected == "all" or model == selected)
		]
		output[selected] = {
			"four_repetition_model_tasks": len(ratios),
			"after_over_before_geomean": geomean(ratios) if ratios else None,
			"task_bootstrap_95ci": bootstrap(ratios) if ratios else [],
		}
	return output


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("formal", type=Path)
	parser.add_argument("output", type=Path)
	args = parser.parse_args()
	profiles = read_profiles(args.formal)
	if not profiles:
		raise ValueError("no product formal log profiles found")
	process_memory = read_process_memory(args.formal)
	for key, profile in profiles.items():
		profile["process_rss_bytes"] = process_memory.get(key, 0)
	args.output.mkdir(parents=True, exist_ok=True)
	columns = list(next(iter(profiles.values())))
	with (args.output / "product-rows.tsv").open("w", newline="") as output:
		writer = csv.writer(output, delimiter="\t")
		writer.writerow(["variant", "model", "repetition", "task", *columns])
		for key, profile in sorted(profiles.items()):
			writer.writerow([*key, *[profile[column] for column in columns]])

	work: dict[str, dict[str, dict[str, int]]] = {}
	for variant in ("before", "after"):
		work[variant] = {}
		for selected in ("sc", "tso", "pso", "all"):
			rows = [profile for (current_variant, model, _, _), profile in profiles.items() if current_variant == variant and (selected == "all" or model == selected)]
			work[variant][selected] = {
				"profiles": len(rows),
				"profiles_with_product": sum(row["product_checks"] > 0 for row in rows),
				"profiles_with_size_fallback": sum(row["product_size_fallbacks"] > 0 for row in rows),
				"product_checks": sum(row["product_checks"] for row in rows),
				"product_state_visits": sum(row["product_state_visits"] for row in rows),
				"product_transitions": sum(row["product_transitions"] for row in rows),
				"product_size_fallbacks": sum(row["product_size_fallbacks"] for row in rows),
				"lazy_edge_candidates": sum(row["lazy_edge_candidates"] for row in rows),
			}

	execution_mismatches = []
	keys = {(model, repetition, task) for _, model, repetition, task in profiles}
	for key in sorted(keys):
		before = profiles.get(("before", *key), {})
		after = profiles.get(("after", *key), {})
		if before.get("executions", 0) and after.get("executions", 0) and before["executions"] != after["executions"]:
			execution_mismatches.append([*key, before["executions"], after["executions"]])

	result = {
		"profiles": len(profiles),
		"execution_mismatches": execution_mismatches,
		"work": work,
		"large_copy": paired_metric(profiles, "copy_ns"),
		"large_offline": paired_metric(profiles, "offline_ns"),
		"large_peak_snapshot": paired_metric(profiles, "peak_snapshot_bytes"),
		"large_process_rss": paired_metric(profiles, "process_rss_bytes"),
	}
	(args.output / "product-analysis.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
	print(json.dumps(result, indent=2, sort_keys=True))
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
