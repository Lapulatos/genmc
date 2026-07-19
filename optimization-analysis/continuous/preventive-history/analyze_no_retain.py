#!/usr/bin/env python3
"""Analyze the four-repetition preventive no-retention experiment."""

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
VARIANTS = ("baseline", "no-retain")
METRICS = {"cpu": "cputime", "wall": "walltime", "rss": "memory"}
EXECUTIONS = re.compile(r"Number of complete executions explored: ([0-9]+)")
STAT = re.compile(r"(?:^| )([a-z][a-z-]+)=([0-9]+)")
PROFILE_KEYS = (
	"offline-evals",
	"sync-ns",
	"history-ns",
	"checkpoint-ns",
	"rollback-ns",
	"retained-undo-bytes",
	"peak-undo-bytes",
	"retained-snapshot-equivalent-bytes",
	"peak-snapshot-equivalent-bytes",
	"max-history-base-bytes",
	"max-preventive-history-base-bytes",
	"preventive-retentions",
	"preventive-suppressed-retentions",
)


def numeric(value: str) -> float:
	return float(value.rstrip("sB")) if value else math.nan


def task_id(name: str) -> str:
	marker = "sv-benchmarks/c/"
	return name.split(marker, 1)[1] if marker in name else name


def read_xml(path: Path) -> bytes:
	return bz2.open(path, "rb").read() if path.suffix == ".bz2" else path.read_bytes()


def load_xml(path: Path, repetition: str, variant: str) -> list[dict[str, object]]:
	root = ET.fromstring(read_xml(path))
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


def load_logs(path: Path) -> dict[str, dict[str, int | None]]:
	records: dict[str, dict[str, int | None]] = {}
	with zipfile.ZipFile(path) as archive:
		for member in archive.namelist():
			if not member.endswith(".log"):
				continue
			text = archive.read(member).decode(errors="replace")
			lines = [line for line in text.splitlines() if "CAT incremental statistics:" in line]
			executions = EXECUTIONS.findall(text)
			record: dict[str, int | None] = {
				"executions": int(executions[-1]) if executions else None,
				"stat-lines": len(lines),
			}
			values: defaultdict[str, int] = defaultdict(int)
			for line in lines:
				for key, value in STAT.findall(line):
					values[key] += int(value)
			record.update(values)
			name = Path(member).name
			task = name.split(".preventive-pruning.", 1)[-1][:-4]
			records[task] = record
	return records


def geomean(values: list[float]) -> float:
	return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap(values: list[float], seed: int) -> list[float]:
	rng = random.Random(seed)
	estimates = sorted(geomean([rng.choice(values) for _ in values]) for _ in range(20_000))
	return [estimates[500], estimates[19_499]]


def summarize_ratios(values: list[float], seed: int) -> dict[str, object]:
	return {
		"tasks": len(values),
		"geomean_ratio": geomean(values),
		"bootstrap_95ci": bootstrap(values, seed),
		"median_ratio": statistics.median(values),
		"improved": sum(value < 1 for value in values),
		"regressed": sum(value > 1 for value in values),
	}


def summarize_profile_ratios(values: list[float]) -> dict[str, object]:
	"""Summarize mechanism ratios, which can legitimately become exactly zero."""
	return {
		"tasks": len(values),
		"median_ratio": statistics.median(values),
		"arithmetic_mean_ratio": statistics.fmean(values),
		"improved": sum(value < 1 for value in values),
		"regressed": sum(value > 1 for value in values),
		"reduced_to_zero": sum(value == 0 for value in values),
	}


def result_file(directory: Path, pattern: str) -> Path:
	paths = list(directory.glob(pattern))
	if len(paths) != 1:
		raise RuntimeError(f"expected one {pattern} in {directory}, found {len(paths)}")
	return paths[0]


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("root", type=Path)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=True)

	rows: list[dict[str, object]] = []
	logs: dict[tuple[str, str], dict[str, dict[str, int | None]]] = {}
	for repetition in REPETITIONS:
		for variant in VARIANTS:
			directory = args.root / f"r{repetition}" / variant
			xml = result_file(directory, "*.xml*")
			rows.extend(load_xml(xml, repetition, variant))
			logs[(repetition, variant)] = load_logs(result_file(directory, "*.logfiles.zip"))

	index = {(str(row["repetition"]), str(row["variant"]), str(row["task"])): row for row in rows}
	tasks = sorted({str(row["task"]) for row in rows})
	if len(rows) != len(REPETITIONS) * len(VARIANTS) * len(tasks):
		raise RuntimeError(f"incomplete matrix: {len(rows)} rows for {len(tasks)} tasks")

	status_transitions: Counter[str] = Counter()
	execution_mismatches = []
	for repetition in REPETITIONS:
		for task in tasks:
			before = index[(repetition, "baseline", task)]
			after = index[(repetition, "no-retain", task)]
			if before["status"] != after["status"]:
				status_transitions[f"{before['status']} -> {after['status']}"] += 1
			before_log = logs[(repetition, "baseline")].get(Path(task).name, {})
			after_log = logs[(repetition, "no-retain")].get(Path(task).name, {})
			if (
				before["status"] == after["status"] == "true"
				and before_log.get("executions") != after_log.get("executions")
			):
				execution_mismatches.append(
					[
						repetition,
						task,
						before_log.get("executions"),
						after_log.get("executions"),
					]
				)

	metric_summaries = {}
	for metric_index, (metric, column) in enumerate(METRICS.items()):
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
		metric_summaries[metric] = summarize_ratios(ratios, 20260716 + metric_index)

	profile = {}
	for key in PROFILE_KEYS:
		variant_totals = {}
		paired_ratios = []
		for variant in VARIANTS:
			variant_totals[variant] = sum(
				int(record.get(key, 0) or 0)
				for repetition in REPETITIONS
				for record in logs[(repetition, variant)].values()
			)
		for task in tasks:
			pairs = []
			for repetition in REPETITIONS:
				before = logs[(repetition, "baseline")].get(Path(task).name, {})
				after = logs[(repetition, "no-retain")].get(Path(task).name, {})
				before_value = int(before.get(key, 0) or 0)
				after_value = int(after.get(key, 0) or 0)
				if before_value <= 0:
					break
				pairs.append((before_value, after_value))
			if len(pairs) == len(REPETITIONS):
				paired_ratios.append(
					statistics.median(after for _, after in pairs)
					/ statistics.median(before for before, _ in pairs)
				)
		profile[key] = {
			"totals": variant_totals,
			"total_ratio": variant_totals["no-retain"] / variant_totals["baseline"]
			if variant_totals["baseline"]
			else None,
			"paired_ratio": summarize_profile_ratios(paired_ratios)
			if paired_ratios
			else None,
		}

	result = {
		"design": {"tasks": len(tasks), "repetitions": 4, "cells": len(rows)},
		"status_counts": {
			variant: dict(Counter(str(row["status"]) for row in rows if row["variant"] == variant))
			for variant in VARIANTS
		},
		"status_transitions": dict(status_transitions),
		"safe_execution_count_mismatches": execution_mismatches,
		"metrics": metric_summaries,
		"profile": profile,
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
	oom_regressions = sum(
		count
		for transition, count in status_transitions.items()
		if "OUT OF MEMORY" in transition.split(" -> ", 1)[1]
		and "OUT OF MEMORY" not in transition.split(" -> ", 1)[0]
	)
	terminal_gains = sum(
		count
		for transition, count in status_transitions.items()
		if transition.split(" -> ", 1)[0] in {"TIMEOUT", "OUT OF MEMORY"}
		and transition.split(" -> ", 1)[1] not in {"TIMEOUT", "OUT OF MEMORY"}
	)
	decision = (
		"retain"
		if not execution_mismatches
		and oom_regressions == 0
		and rss["bootstrap_95ci"][1] <= 1.02
		and (cpu["bootstrap_95ci"][1] < 1 or terminal_gains > 0)
		else "reject or narrow"
	)
	(args.output / "report.md").write_text(
		f"""# P0.4 preventive no-retention performance

## Conclusion

Decision under the frozen gate: **{decision}**. CPU no-retain/baseline is
{cpu['geomean_ratio']:.5f} with task-bootstrap 95% CI
[{cpu['bootstrap_95ci'][0]:.5f}, {cpu['bootstrap_95ci'][1]:.5f}]. RSS is
{rss['geomean_ratio']:.5f} [{rss['bootstrap_95ci'][0]:.5f},
{rss['bootstrap_95ci'][1]:.5f}].

The matrix has {len(rows)} cells, {sum(status_transitions.values())} status differences,
{terminal_gains} terminal gains, {oom_regressions} new OOM cells, and
{len(execution_mismatches)} safe execution-count mismatches. No-retention suppressed
{profile['preventive-suppressed-retentions']['totals']['no-retain']:,} checkpoint
retentions.
""",
		encoding="utf-8",
	)


if __name__ == "__main__":
	main()
