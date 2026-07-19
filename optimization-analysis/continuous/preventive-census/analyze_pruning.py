#!/usr/bin/env python3
"""Analyze one four-repetition generic preventive-pruning experiment."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import math
import random
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter, defaultdict
from pathlib import Path


REPETITIONS = ("01", "02", "03", "04")
VARIANTS = ("baseline", "pruning")
METRICS = {"cpu": "cputime", "wall": "walltime", "rss": "memory"}
SEARCH_COLUMNS = {
	"rf_offered": "rf-offered",
	"rf_queued": "rf-queued",
	"co_offered": "co-offered",
	"co_queued": "co-queued",
	"back_offered": "backward-offered",
	"back_queued": "backward-queued",
	"work_added": "work-added",
	"work_popped": "work-popped",
	"peak_work": "max-retained-work",
	"validity_queries": "validity-queries",
	"realized_prefixes": "realized-revisit-prefixes",
	"rejected_prefixes": "inconsistent-revisit-prefixes",
}
STAT = re.compile(r"preventive-([a-z-]+)=([0-9]+)")
EXECUTIONS = re.compile(r"Number of complete executions explored: ([0-9]+)")
EXPLORATION = re.compile(r"Exploration statistics: (.*)")
KEY_VALUE = re.compile(r"([a-z-]+)=([0-9]+)")


def numeric(value: str) -> float:
	return float(value.rstrip("sB")) if value else math.nan


def task_id(name: str) -> str:
	marker = "sv-benchmarks/c/"
	return name.split(marker, 1)[1] if marker in name else name


def load_xml(path: Path, repetition: str, variant: str) -> list[dict[str, object]]:
	root = ET.fromstring(bz2.open(path, "rb").read())
	rows = []
	for run in root.findall(".//run"):
		columns = {c.attrib["title"]: c.attrib.get("value", "") for c in run.findall("column")}
		rows.append(
			{
				"repetition": repetition,
				"variant": variant,
				"task": task_id(run.attrib["name"]),
				"status": columns.get("status", ""),
				"category": columns.get("category", ""),
				"cputime": numeric(columns.get("cputime", "")),
				"walltime": numeric(columns.get("walltime", "")),
				"memory": numeric(columns.get("memory", "")),
			}
		)
	return rows


def load_logs(path: Path, log_marker: str) -> dict[str, dict[str, object]]:
	records = {}
	with zipfile.ZipFile(path) as archive:
		for member in archive.namelist():
			text = archive.read(member).decode(errors="replace")
			name = Path(member).name
			marker = f".{log_marker}."
			if marker not in name or not name.endswith(".log"):
				continue
			task = name.split(marker, 1)[1][:-4]
			stats = [line for line in text.splitlines() if "CAT incremental statistics:" in line]
			exploration = EXPLORATION.findall(text)
			executions = EXECUTIONS.findall(text)
			record: dict[str, object] = {
				"executions": int(executions[-1]) if executions else None,
				"has_final_stats": bool(stats),
			}
			if stats:
				record.update({key: int(value) for key, value in STAT.findall(stats[-1])})
			if exploration:
				record.update(
					{key: int(value) for key, value in KEY_VALUE.findall(exploration[-1])}
				)
			records[task] = record
	return records


def geomean(values: list[float]) -> float:
	return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int) -> list[float]:
	rng = random.Random(seed)
	estimates = sorted(
		geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
	)
	return [estimates[500], estimates[19_499]]


def summarize_ratios(values: list[float], seed: int) -> dict[str, object]:
	return {
		"tasks": len(values),
		"geomean_ratio": geomean(values),
		"bootstrap_95ci": bootstrap(values, seed),
		"median_ratio": statistics.median(values),
		"faster": sum(value < 1 for value in values),
		"slower": sum(value > 1 for value in values),
	}


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("root", type=Path)
	parser.add_argument("--output", type=Path, required=True)
	parser.add_argument("--log-marker", default="preventive-pruning")
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=True)

	rows = []
	logs: dict[tuple[str, str], dict[str, dict[str, object]]] = {}
	for repetition in REPETITIONS:
		for variant in VARIANTS:
			directory = args.root / f"r{repetition}" / variant
			xmls = list(directory.glob("*.xml.bz2"))
			archives = list(directory.glob("*.logfiles.zip"))
			if len(xmls) != 1 or len(archives) != 1:
				raise RuntimeError(f"incomplete result directory: {directory}")
			rows.extend(load_xml(xmls[0], repetition, variant))
			logs[(repetition, variant)] = load_logs(archives[0], args.log_marker)
	for row in rows:
		record = logs[(str(row["repetition"]), str(row["variant"]))].get(
			Path(str(row["task"])).name, {}
		)
		for key, log_key in SEARCH_COLUMNS.items():
			row[key] = record.get(log_key)

	index = {(row["repetition"], row["variant"], row["task"]): row for row in rows}
	tasks = sorted({str(row["task"]) for row in rows})
	status_mismatches = []
	execution_mismatches = []
	for repetition in REPETITIONS:
		for task in tasks:
			baseline = index[(repetition, "baseline", task)]
			pruning = index[(repetition, "pruning", task)]
			if baseline["status"] != pruning["status"]:
				status_mismatches.append(
					[repetition, task, baseline["status"], pruning["status"]]
				)
			base_log = logs[(repetition, "baseline")].get(Path(task).name, {})
			prune_log = logs[(repetition, "pruning")].get(Path(task).name, {})
			if (
				baseline["status"] == pruning["status"] == "true"
				and base_log.get("executions") != prune_log.get("executions")
			):
				execution_mismatches.append(
					[
						repetition,
						task,
						base_log.get("executions"),
						prune_log.get("executions"),
					]
				)

	metric_summaries = {}
	for metric_index, (metric, column) in enumerate(METRICS.items()):
		ratios = []
		for task in tasks:
			pairs = []
			for repetition in REPETITIONS:
				baseline = index[(repetition, "baseline", task)]
				pruning = index[(repetition, "pruning", task)]
				if baseline["category"] != pruning["category"] or baseline["category"] != "correct":
					break
				pairs.append((float(baseline[column]), float(pruning[column])))
			if len(pairs) == len(REPETITIONS):
				ratios.append(
					statistics.median(after for _, after in pairs)
					/ statistics.median(before for before, _ in pairs)
				)
		metric_summaries[metric] = summarize_ratios(ratios, 20260716 + metric_index)

	# Compare only repetition/task cells with both final accounting rows. This keeps
	# a V6 coverage gain from being mistaken for more search on a baseline TIMEOUT.
	search_space = {}
	for key in SEARCH_COLUMNS:
		pairs = []
		for repetition in REPETITIONS:
			for task in tasks:
				before = index[(repetition, "baseline", task)][key]
				after = index[(repetition, "pruning", task)][key]
				if before is not None and after is not None:
					pairs.append((int(before), int(after)))
		before_total = sum(before for before, _ in pairs)
		after_total = sum(after for _, after in pairs)
		search_space[key] = {
			"paired_cells": len(pairs),
			"baseline_total": before_total,
			"pruning_total": after_total,
			"ratio": after_total / before_total if before_total else None,
			"reduction": 1 - after_total / before_total if before_total else None,
			"baseline_max": max((before for before, _ in pairs), default=None),
			"pruning_max": max((after for _, after in pairs), default=None),
		}
	for combined, components in {
		"choice_offered": ("rf_offered", "co_offered"),
		"choice_queued": ("rf_queued", "co_queued"),
	}.items():
		before_total = sum(search_space[key]["baseline_total"] for key in components)
		after_total = sum(search_space[key]["pruning_total"] for key in components)
		search_space[combined] = {
			"paired_cells": min(search_space[key]["paired_cells"] for key in components),
			"baseline_total": before_total,
			"pruning_total": after_total,
			"ratio": after_total / before_total if before_total else None,
			"reduction": 1 - after_total / before_total if before_total else None,
		}

	keys = (
		"prefix-queries",
		"prefix-inconsistent",
		"rf-candidates",
		"rf-pruned",
		"co-candidates",
		"co-pruned",
		"all-pruned-fallbacks",
		"lookup-ns",
	)
	pruning_records = [
		record
		for repetition in REPETITIONS
		for record in logs[(repetition, "pruning")].values()
		if record.get("has_final_stats")
	]
	counters = {key: sum(int(record.get(key, 0)) for record in pruning_records) for key in keys}
	counters["records_with_final_stats"] = len(pruning_records)
	candidates = counters["rf-candidates"] + counters["co-candidates"]
	pruned = counters["rf-pruned"] + counters["co-pruned"]
	counters["prune_rate"] = pruned / candidates if candidates else 0

	result = {
		"design": {"tasks": len(tasks), "repetitions": 4, "cells": len(rows)},
		"status_counts": {
			variant: dict(Counter(str(row["status"]) for row in rows if row["variant"] == variant))
			for variant in VARIANTS
		},
		"status_mismatches": status_mismatches,
		"safe_execution_count_mismatches": execution_mismatches,
		"metrics": metric_summaries,
		"search_space": search_space,
		"counters": counters,
	}
	(args.output / "stats-strict.json").write_text(
		json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	with (args.output / "formal-rows.tsv").open("w", newline="", encoding="utf-8") as sink:
		writer = csv.DictWriter(sink, fieldnames=sorted(rows[0]), delimiter="\t")
		writer.writeheader()
		writer.writerows(rows)

	cpu = metric_summaries["cpu"]
	rss = metric_summaries["rss"]
	work = search_space["work_popped"]
	queued = search_space["choice_queued"]
	peak = search_space["peak_work"]
	decision = (
		"retain"
		if not execution_mismatches
		and cpu["bootstrap_95ci"][1] < 1
		and rss["bootstrap_95ci"][1] <= 1.02
		else "reject"
	)
	(args.output / "analysis-report.md").write_text(
		f"""# Generic CAT preventive pruning

## Conclusion

Decision under the frozen gate: **{decision}**. CPU pruning/baseline is
{cpu['geomean_ratio']:.5f} with task-bootstrap 95% CI
[{cpu['bootstrap_95ci'][0]:.5f}, {cpu['bootstrap_95ci'][1]:.5f}]. RSS is
{rss['geomean_ratio']:.5f} [{rss['bootstrap_95ci'][0]:.5f},
{rss['bootstrap_95ci'][1]:.5f}].

The matrix has {len(rows)} cells, {len(status_mismatches)} resource-sensitive status
differences, and {len(execution_mismatches)} safe execution-count mismatches. Completed
pruning logs removed {pruned:,}/{candidates:,} offered RF/CO candidates
({100*counters['prune_rate']:.2f}%), with {counters['all-pruned-fallbacks']:,}
all-pruned safety fallbacks.

On {work['paired_cells']} cells with final accounting on both sides, GenMC work popped
changes from {work['baseline_total']:,} to {work['pruning_total']:,}
({100*work['reduction']:.2f}% reduction). RF/CO choices queued change from
{queued['baseline_total']:,} to {queued['pruning_total']:,}
({100*queued['reduction']:.2f}% reduction), and maximum retained work changes from
{peak['baseline_max']:,} to {peak['pruning_max']:,}. These counters distinguish
candidate-space reduction from consistency-query avoidance.
""",
		encoding="utf-8",
	)


if __name__ == "__main__":
	main()
